#include "driver2.h"
#include "playground.h"

#include "system.h"
#include "map.h"
#include "cell.h"
#include "spool.h"
#include "draw.h"
#include "models.h"
#include "dr2roads.h"
#include "mission.h"
#include "players.h"
#include "cars.h"
#include "camera.h"
#include "event.h"
#include "assetcatalog_game.h"
#include "main.h"

// Desktop-only developer input; Android does not add the SDL include path to
// the game target, so keep its dependency out of the shared compile.
#if !defined(PSX) && !defined(__ANDROID__)
#define PLAYGROUND_HAS_SDL_INPUT 1
#include <SDL.h>
#endif

extern char CurrentPVS[];

// The loaded level is only a resource donor. These globals are replaced with
// the generated scene's coherent structures while the playground is active.
extern int permanentModelSlotBitfield[MAX_MODEL_SLOTS / 32];
extern int startSpecSpool;

int gPlaygroundRequested = 0;
int gPlaygroundActive = 0;

namespace
{
const int kGridRadius = 9;              // cells from spawn: 19x19 tiles
const int kTileSlot = 1500;             // reserved model slots
const int kBoxSlot = 1501;
const int kBoxCount = 6;
const int kBoxSize = 1600;
const int kMaxObjects = 512;
const int kMaxCellData = 2048;

// Cell-object lists are shared by rendering, scenery collision and camera
// collision, so a single coherent generated set serves every consumer.
typedef struct
{
	int cellx;
	int cellz;
	PACKED_CELL_OBJECT object;
} PG_OBJECT_RECORD;

PACKED_CELL_OBJECT* s_cellObjects = NULL;
CELL_DATA* s_cells = NULL;

PG_OBJECT_RECORD s_records[kMaxObjects];
int s_recordCount = 0;

VECTOR s_spawn = { 0, 0, 0, 0 };
int s_spawnDir = 0;

// A region buffer whose tag is not 2 makes sdGetCell return default_plane,
// i.e. a flat concrete surface at height 0 with an up normal.
short s_flatRegion[4][2];
}

const char* Playground_GetSceneId(void)
{
	return PLAYGROUND_SCENE_ID;
}

int Playground_IsRequested(void)
{
	return gPlaygroundRequested;
}

int Playground_IsActive(void)
{
	return gPlaygroundActive;
}

void Playground_RequestLaunch(void)
{
	gPlaygroundRequested = 1;
	gPlaygroundActive = 0;

	// The donor resource set is the first single-player Chicago day drive.
	// Its car models, textures, sky and sounds are reused; its world content
	// is discarded by Playground_BuildScene.
	GameType = GAME_TAKEADRIVE;
	GameLevel = 0;
	gCurrentMissionNumber = 50;
	NumPlayers = 1;
}

void Playground_Shutdown(void)
{
	if (gPlaygroundActive)
		printInfo("Playground: shutting down scene %s\n", PLAYGROUND_SCENE_ID);

	// Returning to the frontend ends the request, so a later session does not
	// silently become a playground. Any entry point must request it again.
	gPlaygroundRequested = 0;
	gPlaygroundActive = 0;

	s_cellObjects = NULL;
	s_cells = NULL;
	s_recordCount = 0;
}

// --- generated geometry -----------------------------------------------------

// Textured, flat-shaded quad without per-vertex normals. Matches the layout
// and size (PolySizes[21]) the legacy building renderer expects.
struct PG_POLYFT4
{
	u_char id;
	u_char texture_set;
	u_char texture_id;
	u_char spare;
	u_char v0, v1, v2, v3;
	UV_INFO uv0, uv1, uv2, uv3;
	RGB color;
};

typedef struct
{
	MODEL model;
	SVECTOR vertices[8];
	SVECTOR normals[8];
	SVECTOR point_normals[8];
	PG_POLYFT4 polys[16];
	int collisionCount;
	COLLISION_PACKET collision[1];
} PG_MODEL_ARENA;

void PG_InitModel(PG_MODEL_ARENA* arena, int numVerts, int numPolys, int hasCollision)
{
	MODEL* model = &arena->model;
	ClearMem((char*)arena, sizeof(*arena));

	model->shape_flags = 0;
	model->flags2 = 0;
	model->instance_number = -1;
	model->tri_verts = 0;
	model->zBias = 64;
	model->bounding_sphere = 1600;
	model->num_point_normals = 0;
	model->num_vertices = (u_short)numVerts;
	model->num_polys = (u_short)numPolys;

	model->vertices = (int)((char*)&arena->vertices - (char*)model);
	model->normals = (int)((char*)&arena->normals - (char*)model);
	model->point_normals = (int)((char*)&arena->point_normals - (char*)model);
	model->poly_block = (int)((char*)&arena->polys[0] - (char*)model);
	model->collision_block = hasCollision ? (int)((char*)&arena->collisionCount - (char*)model) : 0;
}

void PG_SetQuad(PG_MODEL_ARENA* arena, int index, int a, int b, int c, int d)
{
	PG_POLYFT4* poly = &arena->polys[index];

	poly->id = 21;
	poly->texture_set = 0;
	poly->texture_id = 0;
	poly->spare = 0;
	poly->v0 = (u_char)a;
	poly->v1 = (u_char)b;
	poly->v2 = (u_char)d;
	poly->v3 = (u_char)c;

	poly->uv0.u = poly->uv0.v = 0;
	poly->uv1.u = poly->uv1.v = 0;
	poly->uv2.u = poly->uv2.v = 0;
	poly->uv3.u = poly->uv3.v = 0;

	poly->color.r = 0x80;
	poly->color.g = 0x80;
	poly->color.b = 0x80;
	poly->color.pad = 0;
}

// The legacy renderer culls by winding. Emitting both windings keeps the
// generated fixture visible regardless of the chosen corner order.
void PG_EmitQuadPair(PG_MODEL_ARENA* arena, int* index, int a, int b, int c, int d)
{
	PG_SetQuad(arena, (*index)++, a, b, c, d);
	PG_SetQuad(arena, (*index)++, a, d, c, b);
}

void PG_BuildTileModel(PG_MODEL_ARENA* arena)
{
	const int half = MAP_CELL_SIZE / 2;

	PG_InitModel(arena, 4, 2, 0);
	arena->model.shape_flags = SHAPE_FLAG_TILE;	// ground, drawn by DrawTILES

	arena->vertices[0].vx = (short)-half; arena->vertices[0].vy = 0; arena->vertices[0].vz = (short)-half; arena->vertices[0].pad = 0;
	arena->vertices[1].vx = (short) half; arena->vertices[1].vy = 0; arena->vertices[1].vz = (short)-half; arena->vertices[1].pad = 0;
	arena->vertices[2].vx = (short) half; arena->vertices[2].vy = 0; arena->vertices[2].vz = (short) half; arena->vertices[2].pad = 0;
	arena->vertices[3].vx = (short)-half; arena->vertices[3].vy = 0; arena->vertices[3].vz = (short) half; arena->vertices[3].pad = 0;

	int index = 0;
	PG_EmitQuadPair(arena, &index, 0, 1, 2, 3);
}

void PG_BuildBoxModel(PG_MODEL_ARENA* arena)
{
	const short h = (short)(kBoxSize / 2);

	PG_InitModel(arena, 8, 12, 1);

	const short positions[8][3] =
	{
		{ -h, -h, -h }, {  h, -h, -h }, {  h,  h, -h }, { -h,  h, -h },
		{ -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h }
	};

	for (int i = 0; i < 8; i++)
	{
		arena->vertices[i].vx = positions[i][0];
		arena->vertices[i].vy = positions[i][1];
		arena->vertices[i].vz = positions[i][2];
		arena->vertices[i].pad = 0;
	}

	int index = 0;
	PG_EmitQuadPair(arena, &index, 0, 1, 2, 3); // back
	PG_EmitQuadPair(arena, &index, 4, 7, 6, 5); // front
	PG_EmitQuadPair(arena, &index, 0, 3, 7, 4); // left
	PG_EmitQuadPair(arena, &index, 1, 5, 6, 2); // right
	PG_EmitQuadPair(arena, &index, 0, 4, 5, 1); // bottom
	PG_EmitQuadPair(arena, &index, 3, 2, 6, 7); // top

	COLLISION_PACKET* packet = &arena->collision[0];
	packet->type = COLLISION_BOX;
	packet->xpos = 0;
	packet->ypos = 0;
	packet->zpos = 0;
	packet->flags = 0;
	packet->yang = 0;
	packet->empty = 0;
	packet->xsize = (short)kBoxSize;
	packet->ysize = (short)kBoxSize;
	packet->zsize = (short)kBoxSize;

	arena->collisionCount = 1;
}

void PG_MakeObject(PACKED_CELL_OBJECT* object, int worldX, int worldY, int worldZ, int slot, int yang)
{
	object->pos.vx = (u_short)(worldX & 0xffff);
	object->pos.vz = (u_short)(worldZ & 0xffff);
	object->pos.vy = (u_short)(((worldY & 0x7fff) << 1) | ((slot >> 10) & 1));
	object->value = (u_short)(((slot & 0x3ff) << 6) | (yang & 63));
}

int PG_CellX(int worldX)
{
	return (worldX + units_across_halved) / MAP_CELL_SIZE;
}

int PG_CellZ(int worldZ)
{
	return (worldZ + units_down_halved) / MAP_CELL_SIZE;
}

int PG_CellCentreX(int cellx)
{
	return (cellx - cells_across / 2) * MAP_CELL_SIZE + MAP_CELL_SIZE / 2;
}

int PG_CellCentreZ(int cellz)
{
	return (cellz - cells_down / 2) * MAP_CELL_SIZE + MAP_CELL_SIZE / 2;
}

void PG_AssignRegion(int cellx, int cellz)
{
	const int regionIndex = (cellx / MAP_REGION_SIZE & 1) + (cellz / MAP_REGION_SIZE & 1) * 2;
	const int regionNumber = (cellx / MAP_REGION_SIZE) + (cellz / MAP_REGION_SIZE) * regions_across;

	RoadMapRegions[regionIndex] = regionNumber;
}

void PG_AddRecord(int cellx, int cellz, int worldX, int worldY, int worldZ, int slot, int yang)
{
	if (s_recordCount >= kMaxObjects)
		return;

	PG_OBJECT_RECORD* record = &s_records[s_recordCount++];
	record->cellx = cellx;
	record->cellz = cellz;
	PG_MakeObject(&record->object, worldX, worldY, worldZ, slot, yang);

	PG_AssignRegion(cellx, cellz);
}

void PG_ClearTraffic(void)
{
	for (int i = 0; i < MAX_CARS; i++)
	{
		switch (car_data[i].controlType)
		{
			case CONTROL_TYPE_CIV_AI:
			case CONTROL_TYPE_PURSUER_AI:
			case CONTROL_TYPE_LEAD_AI:
			case CONTROL_TYPE_CUTSCENE:
				car_data[i].controlType = CONTROL_TYPE_NONE;
				break;
			default:
				break;
		}
	}
}

void PG_BuildCellData(void)
{
	int cd = 0;
	int co = 0;

	// The legacy cell walkers read these globals directly; point them at the
	// generated storage so rendering and collision agree on one world.
	cell_objects = s_cellObjects;
	cells = s_cells;

	for (int i = 0; i < 4096; i++)
		cell_ptrs[i] = 0xffff;

	// Reuse the spawned cell's neighbourhood only; every object is placed in
	// a cell inside a single 32x32 region in practice.
	for (int dz = -kGridRadius; dz <= kGridRadius; dz++)
	{
		for (int dx = -kGridRadius; dx <= kGridRadius; dx++)
		{
			int cellx = PG_CellX(s_spawn.vx) + dx;
			int cellz = PG_CellZ(s_spawn.vz) + dz;
			int cbr = (cellz % MAP_REGION_SIZE) * MAP_REGION_SIZE +
				((cellx / MAP_REGION_SIZE & 1) + (cellz / MAP_REGION_SIZE & 1) * 2) * (MAP_REGION_SIZE * MAP_REGION_SIZE) +
				(cellx % MAP_REGION_SIZE);

			bool any = false;
			for (int r = 0; r < s_recordCount; r++)
			{
				if (s_records[r].cellx == cellx && s_records[r].cellz == cellz)
				{
					if (!any)
					{
						if (cd + 2 >= kMaxCellData || co >= kMaxObjects)
							return;

						cell_ptrs[cbr] = (u_short)cd;
						any = true;
					}

					s_cellObjects[co] = s_records[r].object;
					s_cells[cd].num = (u_short)co;
					cd++;
					co++;
				}
			}

			if (any)
				s_cells[cd++].num = 0x4000;
		}
	}

	if (cd < kMaxCellData)
		s_cells[cd].num = 0x8000;

	sizeof_cell_object_computed_values = 2048;
}

void Playground_BuildScene(void)
{
	if (!gPlaygroundRequested || gPlaygroundActive)
		return;

	if (PlayerStartInfo[0] == NULL)
		return;

	s_spawn.vx = PlayerStartInfo[0]->position.vx;
	s_spawn.vy = 0;
	s_spawn.vz = PlayerStartInfo[0]->position.vz;
	s_spawnDir = PlayerStartInfo[0]->rotation & 0xFFF;

	PG_MODEL_ARENA* tileArena;
	PG_MODEL_ARENA* boxArena;

	D_MALLOC_BEGIN();
	tileArena = (PG_MODEL_ARENA*)D_MALLOC(sizeof(PG_MODEL_ARENA));
	boxArena = (PG_MODEL_ARENA*)D_MALLOC(sizeof(PG_MODEL_ARENA));
	s_cellObjects = (PACKED_CELL_OBJECT*)D_MALLOC(kMaxObjects * sizeof(PACKED_CELL_OBJECT));
	s_cells = (CELL_DATA*)D_MALLOC(kMaxCellData * sizeof(CELL_DATA));
	D_MALLOC_END();

	if (!tileArena || !boxArena || !s_cellObjects || !s_cells)
		return;

	PG_BuildTileModel(tileArena);
	PG_BuildBoxModel(boxArena);

	modelpointers[kTileSlot] = &tileArena->model;
	pLodModels[kTileSlot] = &tileArena->model;
	permanentModelSlotBitfield[kTileSlot >> 5] |= 1 << (kTileSlot & 31);

	modelpointers[kBoxSlot] = &boxArena->model;
	pLodModels[kBoxSlot] = &boxArena->model;
	permanentModelSlotBitfield[kBoxSlot >> 5] |= 1 << (kBoxSlot & 31);

	// Expose the generated fixture's assets in the catalog (source unknown:
	// they are procedural, not original-game files).
	AssetCatalogGame_RegisterModel(kTileSlot);
	AssetCatalogGame_RegisterModel(kBoxSlot);

	// The box model spans [-size/2, size/2] locally, so lift it to rest on the
	// flat surface while keeping its collision box centred on the object.
	const int boxY = kBoxSize / 2;

	s_recordCount = 0;

	for (int dz = -kGridRadius; dz <= kGridRadius; dz++)
	{
		for (int dx = -kGridRadius; dx <= kGridRadius; dx++)
		{
			int cellx = PG_CellX(s_spawn.vx) + dx;
			int cellz = PG_CellZ(s_spawn.vz) + dz;

			PG_AddRecord(cellx, cellz, PG_CellCentreX(cellx), 0, PG_CellCentreZ(cellz), kTileSlot, 0);
		}
	}

	const int boxCells[kBoxCount][2] =
	{
		{  2,  1 }, { -3,  2 }, {  4, -3 }, { -4, -4 }, {  5,  4 }, {  0,  5 }
	};

	for (int i = 0; i < kBoxCount; i++)
	{
		int cellx = PG_CellX(s_spawn.vx) + boxCells[i][0];
		int cellz = PG_CellZ(s_spawn.vz) + boxCells[i][1];

		PG_AddRecord(cellx, cellz, PG_CellCentreX(cellx), boxY, PG_CellCentreZ(cellz), kBoxSlot, 0);
	}

	// Coherent empty/rewritten world data for every active consumer.
	for (int i = 0; i < 4; i++)
	{
		RoadMapDataRegions[i] = s_flatRegion[i];
		loading_region[i] = -1;
	}

	PG_BuildCellData();
	PG_ClearTraffic();

	// The donor level registers camera/event data that would otherwise drive
	// the camera and draw original-world objects. Clear it so the follow
	// camera and the generated scene are authoritative.
	InitEvents();

	doSpooling = 0;
	allowSpecSpooling = 0;
	startSpecSpool = -1;
	leadAILoaded = 0;
	pathAILoaded = 0;
	leadAIRequired = 0;

	Mission.active = 0;
	maxCivCars = 0;
	maxParkedCars = 0;
	maxCopCars = 0;
	CopsAllowed = 0;
	gDontPingInCops = 1;
	Havana3DLevelDraw = -1;

	if (setupYet != 0)
		setupYet = 0;

	gPlaygroundActive = 1;

	const int generatedBytes = (int)(2 * sizeof(PG_MODEL_ARENA) +
		kMaxObjects * sizeof(PACKED_CELL_OBJECT) + kMaxCellData * sizeof(CELL_DATA));

	printInfo("Playground: scene %s active (%d objects, spawn %d,%d, %d bytes generated)\n",
		PLAYGROUND_SCENE_ID, s_recordCount, s_spawn.vx, s_spawn.vz, generatedBytes);
}

void Playground_ResetCar(void)
{
	if (!gPlaygroundActive || player[0].playerCarId < 0)
		return;

	CAR_DATA* cp = &car_data[player[0].playerCarId];
	LONGVECTOR4 position;

	position[0] = s_spawn.vx;
	position[1] = s_spawn.vy;
	position[2] = s_spawn.vz;
	position[3] = 0;

	char padid = 0;
	InitPlayer(&player[0], cp, CONTROL_TYPE_PLAYER, s_spawnDir, &position,
		PlayerStartInfo[0]->model, PlayerStartInfo[0]->palette, &padid);
	InitCamera(&player[0]);
}

void Playground_Tick(void)
{
	if (!gPlaygroundActive)
		return;

#ifdef PLAYGROUND_HAS_SDL_INPUT
	static int resetHeld = 0;
	const Uint8* keys = SDL_GetKeyboardState(NULL);

	if (keys && keys[SDL_SCANCODE_R])
	{
		if (!resetHeld)
		{
			resetHeld = 1;
			Playground_ResetCar();
		}
	}
	else
	{
		resetHeld = 0;
	}
#endif
}

void Playground_FillVisibility(void)
{
	// A fully visible cell neighbourhood; the fixture is bounded and small.
	for (int i = 0; i < PVS_CELL_COUNT * PVS_CELL_COUNT + 3; i++)
		CurrentPVS[i] = 1;
}
