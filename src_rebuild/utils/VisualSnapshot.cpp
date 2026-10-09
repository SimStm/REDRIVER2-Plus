#include "VisualSnapshot.h"

#include "driver2.h"
#include "C/camera.h"
#include "C/cars.h"
#include "ProjectClock.h"
#include "C/main.h"
#include "C/players.h"
#include "C/draw.h"
#include "C/director.h"
#include "C/pause.h"
#include "C/event.h"
#include "C/glaunch.h"
#include "C/mission.h"
#include "PresentationInterpolation.h"

namespace
{
const int kSlots = MAX_CARS + 2;

// A per-tick jump beyond this on any axis is treated as a teleport (spawn,
// level load, reset) and is not interpolated, so no streak is drawn.
const int kTeleportThreshold = 3000;

struct SnapshotEntry
{
	int prev[3];
	int cur[3];
	int valid;
	int model;
	int controlType;
	const CAR_COSMETICS* cosmetics;
	short prevRotation[3][3];
	short curRotation[3][3];
};

SnapshotEntry g_snapshot[kSlots];
int g_lastTick = -1;

bool IsJump(int previous, int current)
{
	const long long delta = (long long)current - previous;
	return delta > kTeleportThreshold || delta < -kTeleportThreshold;
}

struct CameraPose
{
	VECTOR position;
	SVECTOR angle;
	MATRIX inverse, world, face, faceWork;
	int projection, detailDistance, car;
};

struct CameraSnapshot
{
	CameraPose previous, current;
	int valid, carId, view;
	const EVENT* event;
};

CameraSnapshot g_camera[2];
int g_cameraView = 0;
int g_lastCameraTick = -1;

CameraPose ReadCamera()
{
	CameraPose pose;
	pose.position = camera_position;
	pose.angle = camera_angle;
	pose.inverse = inv_camera_matrix;
	pose.world = camera_matrix;
	pose.face = face_camera;
	pose.faceWork = face_camera_work;
	pose.projection = scr_z;
	pose.detailDistance = switch_detail_distance;
	pose.car = CameraCar;
	return pose;
}

void WriteCamera(const CameraPose& pose)
{
	camera_position = pose.position;
	camera_angle = pose.angle;
	inv_camera_matrix = pose.inverse;
	camera_matrix = pose.world;
	face_camera = pose.face;
	face_camera_work = pose.faceWork;
	scr_z = pose.projection;
	switch_detail_distance = pose.detailDistance;
	CameraCar = pose.car;
	SetGeomScreen(scr_z);
}

void CaptureCamera(int view)
{
	CameraSnapshot& snapshot = g_camera[view];
	const CameraPose pose = ReadCamera();
	const PLAYER& p = player[view];
	const bool cut = !snapshot.valid || g_lastCameraTick == CameraCnt || snapshot.carId != p.cameraCarId ||
		snapshot.view != p.cameraView || camera_change != 0 ||
		snapshot.event != events.cameraEvent ||
		IsJump(snapshot.current.position.vx, pose.position.vx) ||
		IsJump(snapshot.current.position.vy, pose.position.vy) ||
		IsJump(snapshot.current.position.vz, pose.position.vz);
	snapshot.previous = cut ? pose : snapshot.current;
	snapshot.current = pose;
	snapshot.valid = 1;
	snapshot.carId = p.cameraCarId;
	snapshot.view = p.cameraView;
	snapshot.event = events.cameraEvent;
	g_cameraView = view;
}
}

extern "C" void VisualSnapshot_Reset(void)
{
	for (int i = 0; i < kSlots; ++i)
		g_snapshot[i].valid = 0;

	g_lastTick = -1;
	g_lastCameraTick = -1;
	for (int i = 0; i < 2; ++i)
		g_camera[i].valid = 0;
}

extern "C" void VisualSnapshot_Capture(void)
{
	// A tick counter reset (level load, replay restart) invalidates everything.
	if (g_lastTick >= 0 && CameraCnt < g_lastTick)
		VisualSnapshot_Reset();
	if (CameraCnt == g_lastTick)
		return;

	for (int i = 0; i < kSlots; ++i)
	{
		const CAR_DATA& car = car_data[i];
		SnapshotEntry& entry = g_snapshot[i];

		if (car.controlType == CONTROL_TYPE_NONE || car.controlType == CONTROL_TYPE_CAMERACOLLIDER)
		{
			entry.valid = 0;
			continue;
		}

		const int* t = car.hd.where.t;

		if (entry.valid && entry.model == car.ap.model && entry.controlType == car.controlType && entry.cosmetics == car.ap.carCos)
		{
			if (IsJump(entry.cur[0], t[0]) || IsJump(entry.cur[1], t[1]) || IsJump(entry.cur[2], t[2]))
			{
				// Teleport: collapse both poses onto the new position so the
				// renderer shows the car at rest here instead of a streak.
				entry.prev[0] = entry.cur[0] = t[0];
				entry.prev[1] = entry.cur[1] = t[1];
				entry.prev[2] = entry.cur[2] = t[2];
				for (int row = 0; row < 3; ++row)
					for (int col = 0; col < 3; ++col)
						entry.prevRotation[row][col] = car.hd.where.m[row][col];
			}
			else
			{
				entry.prev[0] = entry.cur[0];
				entry.prev[1] = entry.cur[1];
				entry.prev[2] = entry.cur[2];
				entry.cur[0] = t[0];
				entry.cur[1] = t[1];
				entry.cur[2] = t[2];
				for (int row = 0; row < 3; ++row)
					for (int col = 0; col < 3; ++col)
						entry.prevRotation[row][col] = entry.curRotation[row][col];
			}
		}
		else
		{
			entry.prev[0] = entry.cur[0] = t[0];
			entry.prev[1] = entry.cur[1] = t[1];
			entry.prev[2] = entry.cur[2] = t[2];
			entry.valid = 1;
			for (int row = 0; row < 3; ++row)
				for (int col = 0; col < 3; ++col)
					entry.prevRotation[row][col] = car.hd.where.m[row][col];
		}
		entry.model = car.ap.model;
		entry.controlType = car.controlType;
		entry.cosmetics = car.ap.carCos;
		for (int row = 0; row < 3; ++row)
			for (int col = 0; col < 3; ++col)
				entry.curRotation[row][col] = car.hd.where.m[row][col];
	}

	g_lastTick = CameraCnt;
}

extern "C" int VisualSnapshot_GetPosition(int slot, int alphaQ8, int outXYZ[3])
{
	if (slot < 0 || slot >= kSlots || !g_snapshot[slot].valid)
		return 0;

	if (alphaQ8 < 0)
		alphaQ8 = 0;
	else if (alphaQ8 > 256)
		alphaQ8 = 256;

	const SnapshotEntry& entry = g_snapshot[slot];

	for (int i = 0; i < 3; ++i)
		outXYZ[i] = PresentationInterpolation::Lerp(entry.prev[i], entry.cur[i], alphaQ8);

	return 1;
}

extern "C" int VisualSnapshot_GetAlpha(void)
{
	// A stopped/fast-forwarded simulation must not cycle between old poses.
	if (pauseflag || FastForward)
		return 256;
	return ProjectClock_GetInterpolationAlpha();
}

extern "C" int VisualSnapshot_GetRotation(int slot, int alphaQ8, short out[3][3])
{
	if (slot < 0 || slot >= kSlots || !g_snapshot[slot].valid)
		return 0;
	const SnapshotEntry& entry = g_snapshot[slot];
	PresentationInterpolation::Matrix(entry.prevRotation, entry.curRotation, alphaQ8, out);
	return 1;
}

extern "C" void VisualSnapshot_UpdateCameras(void)
{
	// Run the original follow/head/collision logic once per scheduled tick, even
	// when several ticks share a display frame. Never advance it from DrawGame.
	// Paused replay editing is an explicit camera operation, still serviced at
	// the regional clock rate; it must not resume an old interpolation interval.
	const bool editingReplay = pauseflag && NoPlayerControl && !AttractMode;
	if (g_lastCameraTick == CameraCnt && !editingReplay)
		return;
	const int views = NumPlayers == 1 || NoPlayerControl ? 1 : 2;
	for (int view = 0; view < views; ++view)
	{
		CurrentPlayerView = view;
		InitCamera(&player[view]);
		CaptureCamera(view);
	}
	g_lastCameraTick = CameraCnt;
}

extern "C" void VisualSnapshot_RestoreCamera(void)
{
	if (g_camera[g_cameraView].valid)
		WriteCamera(g_camera[g_cameraView].current);
}

extern "C" void VisualSnapshot_ApplyCamera(int view)
{
	if (view < 0 || view >= 2)
		return;
	CameraSnapshot& snapshot = g_camera[view];
	if (!snapshot.valid)
		return; // Camera initialization belongs to GameInit/the tick path.
	const CameraPose& a = snapshot.previous;
	const CameraPose& b = snapshot.current;
	CameraPose pose = b;
	const int alpha = VisualSnapshot_GetAlpha();
	pose.position.vx = PresentationInterpolation::Lerp(a.position.vx, b.position.vx, alpha);
	pose.position.vy = PresentationInterpolation::Lerp(a.position.vy, b.position.vy, alpha);
	pose.position.vz = PresentationInterpolation::Lerp(a.position.vz, b.position.vz, alpha);
	pose.angle.vx = PresentationInterpolation::Angle(a.angle.vx, b.angle.vx, alpha);
	pose.angle.vy = PresentationInterpolation::Angle(a.angle.vy, b.angle.vy, alpha);
	pose.angle.vz = PresentationInterpolation::Angle(a.angle.vz, b.angle.vz, alpha);
	PresentationInterpolation::Matrix(a.inverse.m, b.inverse.m, alpha, pose.inverse.m);
	// InvertMatrix in the original camera path transposes the view basis,
	// including PAL's vertical aspect scale. Do not interpret that transpose
	// as a rotation with row scales (its scales live on columns).
	if (alpha == 0)
		pose.world = a.world;
	else if (alpha != 256)
		for (int row = 0; row < 3; ++row)
			for (int col = 0; col < 3; ++col)
				pose.world.m[row][col] = pose.inverse.m[col][row];
	PresentationInterpolation::Matrix(a.face.m, b.face.m, alpha, pose.face.m);
	PresentationInterpolation::Matrix(a.faceWork.m, b.faceWork.m, alpha, pose.faceWork.m);
	pose.projection = PresentationInterpolation::Lerp(a.projection, b.projection, alpha);
	WriteCamera(pose);
}
