#include "driver2.h"
#include "pause.h"
#include "cars.h"
#include "texture.h"
#include "overmap.h"
#include "draw.h"
#include "mission.h"
#include "system.h"
#include "debris.h"
#include "main.h"
#include "camera.h"
#include "handling.h"
#include "cosmetic.h"
#include "shadow.h"
#include "civ_ai.h"
#include "mc_snd.h"
#include "gamesnd.h"
#include "players.h"
#include "cutscene.h"
#include "convert.h"
#include "glaunch.h"
#include "../utils/VisualSnapshot.h"
#include "ASM/rndrasm.h"

#ifndef PSX
#include "assetcatalog.h"
#include "PsyX/PsyX_public.h"
#include <string>
#include <vector>
#include "../../utils/InspectorExport.h"
#include "../../utils/CarMeshSerializer.h"
#ifdef _WIN32
#include "../../utils/NativeCarMesh.h"
#include "../../utils/NativeWheelMesh.h"
#include "PsyX/PsyX_render.h"
static unsigned int s_nativeCarModelRevision[MAX_CAR_RESIDENT_MODELS] = {};
#endif
#ifdef _WIN32
#include <windows.h>
#ifdef LoadImage
#undef LoadImage
#endif
#endif
#endif

#ifndef PSX
// Shared by DrawCarObject and DrawCarWheels so a wheel component's logical
// parent is exactly the car instance key its body uses.
static void Cars_BuildInspectorKey(const CAR_DATA* cp, char* out, int capacity);
#endif

struct plotCarGlobals
{
	u_char* primptr;
	OTTYPE* ot;
	u_int intensity;
	u_short* pciv_clut;
	u_char* damageLevel;
};


#ifndef PSX
#define CAR_LOD_SWITCH_DISTANCE switch_detail_distance
#else
#define CAR_LOD_SWITCH_DISTANCE 5500
#endif

MATRIX light_matrix =
{ 
	{ 
		{ 4096, 0, 0 }, 
		{ 0, 0, 0 }, 
		{ 0, 0, 0 }
	}, 
	{ 0, 0, 0 } 
};

MATRIX colour_matrix =
{ 
	{ 
		{ 4032, 0, 0 }, 
		{ 3936, 0, 0 }, 
		{ 3520, 0, 0 }
	}, 
	{ 0, 0, 0 } 
};

// PHYSICS
CAR_DATA car_data[MAX_CARS + 2];	// all cars + Tanner cbox + Camera cbox

HUBCAP gHubcap;

MODEL* gHubcapModelPtr;
MODEL* gCleanWheelModelPtr;
MODEL* gFastWheelModelPtr;
MODEL* gDamWheelModelPtr;

// active cars
CAR_DATA* active_car_list[MAX_CARS];

u_char lightsOnDelay[MAX_CARS];
short FrontWheelRotation[MAX_CARS]; // offset 0x0
short BackWheelRotation[MAX_CARS]; // offset 0x30

SVECTOR gTempCarVertDump[MAX_CARS][MAX_DENTING_VERTS];

DENTUVS *gTempCarUVPtr;
DENTUVS gTempHDCarUVDump[MAX_CARS][MAX_DENTING_UVS];
DENTUVS gTempLDCarUVDump[MAX_CARS][MAX_DENTING_LOD_UVS];

CAR_MODEL NewCarModel[MAX_CAR_RESIDENT_MODELS];
CAR_MODEL NewLowCarModel[MAX_CAR_RESIDENT_MODELS];

MODEL* gCarLowModelPtr[MAX_CAR_RESIDENT_MODELS];
MODEL* gCarDamModelPtr[MAX_CAR_RESIDENT_MODELS];
MODEL* gCarCleanModelPtr[MAX_CAR_RESIDENT_MODELS];

// pedestrian palette at 0 and next are cars
// model_id, texture_number, palette
u_short civ_clut[8][32][6];

#define MAX_CAR_POLYS	(200 * 2) * MAX_CAR_RESIDENT_MODELS

int whichCP = 0;
int baseSpecCP = 0;
CAR_POLY carPolyBuffer[MAX_CAR_POLYS + 1];

char LeftLight = 0;
char RightLight = 0;
char TransparentObject = 0;

// [D] [T]
void plotCarPolyB3(int numTris, CAR_POLY *src, SVECTOR *vlist, plotCarGlobals *pg)
{
	int Z;
	int indices;
	u_int FT3rgb;
	SVECTOR *v2;
	SVECTOR *v1;
	SVECTOR *v0;
	POLY_F3 *prim;
	OTTYPE *ot;

	prim = (POLY_F3 *)pg->primptr;
	FT3rgb = pg->intensity;
	ot = pg->ot;

	while (numTris > 0)
	{
		indices = src->vindices;
		v0 = vlist + (indices & 0xff);
		v1 = vlist + (indices >> 8 & 0xff);
		v2 = vlist + (indices >> 16 & 0xff);

		gte_ldv3(v0, v1, v2);

		gte_rtpt();

		gte_nclip();
		gte_stopz(&Z);
		gte_avsz3();

		if (Z > -1) 
		{
			*(u_int*)&prim->r0 = FT3rgb | 0x20000000;

			gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);

			gte_stotz(&Z);

			setPolyF3(prim);
			addPrim(ot + (Z >> 1), prim);

			prim++;
		}

		numTris--;
		src++;
	}

	pg->primptr = (unsigned char*)prim;
}

// [D] [T]
void plotCarPolyFT3(int numTris, CAR_POLY *src, SVECTOR *vlist, plotCarGlobals *pg)
{
	int indices;
	int ofse;
	SVECTOR *v2;
	SVECTOR *v1;
	SVECTOR *v0;
	int Z;
	POLY_FT3 *prim;
	OTTYPE *ot;
	int FT3rgb;
	int reg;

	prim = (POLY_FT3 *)pg->primptr;
	FT3rgb = pg->intensity | 0x24000000;
	ot = pg->ot;

	gte_ldrgb(&FT3rgb);

	while (numTris > 0)
	{
		indices = src->vindices;
		v0 = vlist + (indices & 0xff);
		v1 = vlist + (indices >> 8 & 0xff);
		v2 = vlist + (indices >> 16 & 0xff);

		gte_ldv3(v0, v1, v2);

		gte_rtpt();

		gte_nclip();
		gte_stopz(&Z);
		gte_avsz3();

		if (Z > -1) 
		{
			ofse = pg->damageLevel[src->originalindex];
			*(u_int*)&prim->r0 = FT3rgb;
			*(u_int*)&prim->u0 = src->clut_uv0 + ofse;
			*(u_int*)&prim->u1 = src->tpage_uv1 + ofse;
			*(u_int*)&prim->u2 = src->uv3_uv2 + ofse;

			gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);

			gte_stotz(&Z);

			setPolyFT3(prim);
			addPrim(ot + (Z >> 1), prim);

			prim++;
		}
		numTris--;
		src++;
	}

	pg->primptr = (unsigned char*)prim;
}

// [D] [T]
void plotCarPolyGT3(int numTris, CAR_POLY *src, SVECTOR *vlist, SVECTOR *nlist, plotCarGlobals *pg, int palette)
{
	int Z;
	int otz;	
	SVECTOR* v2;
	SVECTOR *v1;
	SVECTOR *v0;
	u_int indices;
	POLY_GT3 *prim;
	u_int r0,r1,r2;
	int ofse;

	prim = (POLY_GT3 *)pg->primptr;

	int GT3rgb = pg->intensity | 0x34000000;
	gte_ldrgb(&GT3rgb);

	while (numTris > 0)
	{
		indices = src->vindices;

		v0 = vlist + (indices & 0xff);
		v1 = vlist + (indices >> 8 & 0xff);
		v2 = vlist + (indices >> 16 & 0xff);

		gte_ldv3(v0, v1, v2);

		gte_rtpt();
		gte_nclip();

		gte_stopz(&Z);

		gte_avsz3();
		
		gte_stotz(&otz);

		if (Z > -1 && otz > 0)
		{
			indices = src->nindices;

			r0 = (u_int)(ushort)nlist[indices & 0xff].pad;
			r1 = (u_int)(ushort)nlist[indices >> 8 & 0xff].pad;
			r2 = (u_int)(ushort)nlist[indices >> 16 & 0xff].pad;

			*(u_int*)&prim->r0 = (r0 & 0xff) << 0x10 | r0;
			*(u_int*)&prim->r1 = (r1 & 0xff) << 0x10 | r1;
			*(u_int*)&prim->r2 = (r2 & 0xff) << 0x10 | r2;

			ofse = pg->damageLevel[src->originalindex];

			*(u_int*)&prim->u0 = pg->pciv_clut[(src->clut_uv0 >> 0x10) + palette] << 0x10 | (src->clut_uv0 & 0xffff) + ofse;
			*(u_int*)&prim->u1 = src->tpage_uv1 + ofse;
			*(u_int*)&prim->u2 = src->uv3_uv2 + ofse;

			gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);

			setPolyGT3(prim);
			addPrim(pg->ot + (otz >> 1), prim);

			prim++;
		}

		src++;
		numTris--;
	}

	pg->primptr = (unsigned char*)prim;
}


#ifdef DYNAMIC_LIGHTING
void plotCarPolyGT3Lit(int numTris, CAR_POLY* src, SVECTOR* vlist, SVECTOR* nlist, plotCarGlobals* pg, int palette)
{
	int Z;
	int otz;
	SVECTOR* v2;
	SVECTOR* v1;
	SVECTOR* v0;
	u_int indices;
	POLY_GT3* prim;
	u_int r0, r1, r2;
	int ofse;

	prim = (POLY_GT3*)pg->primptr;

	int GT3rgb = pg->intensity | 0x34000000;
	gte_ldrgb(&GT3rgb);

	while (numTris > 0)
	{
		indices = src->vindices;

		v0 = vlist + (indices & 0xff);
		v1 = vlist + (indices >> 8 & 0xff);
		v2 = vlist + (indices >> 16 & 0xff);

		gte_ldv3(v0, v1, v2);

		gte_rtpt();
		gte_nclip();

		gte_stopz(&Z);

		gte_avsz3();

		gte_stotz(&otz);

		if (Z > -1 && otz > 0)
		{
			indices = src->nindices;

			r0 = (u_int)(ushort)nlist[indices & 0xff].pad;
			r1 = (u_int)(ushort)nlist[indices >> 8 & 0xff].pad;
			r2 = (u_int)(ushort)nlist[indices >> 16 & 0xff].pad;

			*(u_int*)&prim->r0 = (r0 & 0xff) << 0x10 | r0;
			*(u_int*)&prim->r1 = (r1 & 0xff) << 0x10 | r1;
			*(u_int*)&prim->r2 = (r2 & 0xff) << 0x10 | r2;

			ofse = pg->damageLevel[src->originalindex];

			*(u_int*)&prim->u0 = pg->pciv_clut[(src->clut_uv0 >> 0x10) + palette] << 0x10 | (src->clut_uv0 & 0xffff) + ofse;
			*(u_int*)&prim->u1 = src->tpage_uv1 + ofse;
			*(u_int*)&prim->u2 = src->uv3_uv2 + ofse;

			gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);

			SVECTOR tmpPos;
			gte_ldv0(v0);
			gte_rtps();
			gte_stsv(&tmpPos);
			GetDLightLevel(&tmpPos, (u_int*)&prim->r0);

			gte_ldv0(v1);
			gte_rtps();
			gte_stsv(&tmpPos);
			GetDLightLevel(&tmpPos, (u_int*)&prim->r1);

			gte_ldv0(v2);
			gte_rtps();
			gte_stsv(&tmpPos);
			GetDLightLevel(&tmpPos, (u_int*)&prim->r2);

			setPolyGT3(prim);
			addPrim(pg->ot + (otz >> 1), prim);

			prim++;
		}

		src++;
		numTris--;
	}

	pg->primptr = (unsigned char*)prim;
}
#endif // DYNAMIC_LIGHTING


// [D] [T]
void plotCarPolyGT3nolight(int numTris, CAR_POLY *src, SVECTOR *vlist, plotCarGlobals *pg, int palette)
{
	int Z;
	int otz;
	SVECTOR* v2;
	SVECTOR* v1;
	SVECTOR* v0;
	u_int indices;
	POLY_FT3* prim;
	int ofse;

	prim = (POLY_FT3*)pg->primptr;

	int GT3rgb = pg->intensity | 0x34000000;
	gte_ldrgb(&GT3rgb);

	while (numTris > 0)
	{
		indices = src->vindices;

		v0 = vlist + (indices & 0xff);
		v1 = vlist + (indices >> 8 & 0xff);
		v2 = vlist + (indices >> 16 & 0xff);

		gte_ldv3(v0, v1, v2);

		gte_rtpt();
		gte_nclip();
		gte_stopz(&Z);

		gte_avsz3();
		gte_stotz(&otz);

		if (Z > -1 && otz > 0)
		{
			*(u_int*)&prim->r0 = GT3rgb;

			ofse = pg->damageLevel[src->originalindex];

			*(u_int*)&prim->u0 = pg->pciv_clut[(src->clut_uv0 >> 0x10) + palette] << 0x10 | (src->clut_uv0 & 0xffff) + ofse;
			*(u_int*)&prim->u1 = src->tpage_uv1 + ofse;
			*(u_int*)&prim->u2 = src->uv3_uv2 + ofse;

			gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);

			setPolyFT3(prim);
			addPrim(pg->ot + (otz >> 1), prim);

			prim++;
		}

		src++;
		numTris--;
	}

	pg->primptr = (unsigned char*)prim;
}

MATRIX save_colour_matrix;
MATRIX save_light_matrix;

// [D] [T]
void setupLightingMatrices(void)
{
	gte_ReadColorMatrix(&save_colour_matrix);
	gte_ReadLightMatrix(&save_light_matrix);

	gte_SetColorMatrix(&colour_matrix);
	gte_SetLightMatrix(&light_matrix);

	if (gTimeOfDay == TIME_NIGHT)
	{
		gte_SetBackColor(48, 48, 48);
	}
	else
	{
		gte_SetBackColor(140, 140, 140);
	}
}

// [D] [T]
void restoreLightingMatrices(void)
{
	gte_SetColorMatrix(&save_colour_matrix);
	gte_SetLightMatrix(&save_light_matrix);
}

// [D] [T]
void ComputeCarLightingLevels(CAR_DATA* cp, char detail)
{
#ifdef PSX
	MATRIX& scratchPadMat = *(MATRIX*)((u_char*)getScratchAddr(0) + 0x100);
#else
	MATRIX scratchPadMat;
#endif

	int doLight;
	int orW, orY;
	MODEL* model;
	int num_norms, count;
	SVECTOR* ppads;
	SVECTOR* norms;
	SVECTOR colour, lightsourcevector;
	CVECTOR c0, c1, c2;
	u_int GT3rgb;

	if (gTimeOfDay == TIME_NIGHT)
	{
		lightsourcevector = night_vectors[GameLevel];
		colour = night_colours[GameLevel];
	}
	else
	{
		lightsourcevector = day_vectors[GameLevel];
		colour = day_colours[GameLevel];
	}

	InvertMatrix(&cp->hd.where, &scratchPadMat);
	SetRotMatrix(&scratchPadMat);

	gte_ldv0(&lightsourcevector);
	gte_rtv0();
	gte_stsv(light_matrix.m[0]);

	colour_matrix.m[0][0] = colour.vx;
	colour_matrix.m[1][0] = colour.vy;
	colour_matrix.m[2][0] = colour.vz;

	orY = ABS(cp->st.n.orientation[1] - cp->ap.qy);
	orW = ABS(cp->st.n.orientation[3] - cp->ap.qw);

	doLight = 0;

	if ((orY + orW > 200) || (cp->lowDetail != (detail | lightning)))
		doLight = 1;

	if ((M_BIT(gTimeOfDay) & (M_BIT(TIME_DAWN) | M_BIT(TIME_DUSK))) && (cp->id & 15) == (CameraCnt & 15))
		doLight = 1;

	if (doLight)
	{
		setupLightingMatrices();

		GT3rgb = combointensity & 0xffffffU | 0x34000000;
		gte_ldrgb(&GT3rgb);

		cp->ap.qy = cp->st.n.orientation[1];
		cp->ap.qw = cp->st.n.orientation[3];
		cp->lowDetail = detail | lightning;

		model = detail ? gCarCleanModelPtr[cp->ap.model] : gCarLowModelPtr[cp->ap.model];
		num_norms = model->num_point_normals / 3;
		norms = GET_MODEL_DATA(SVECTOR, model, point_normals);

		ppads = gTempCarVertDump[cp->id];
		count = num_norms;// +1;

		while (count >= 0)
		{
			gte_ldv3(&norms[0], &norms[1], &norms[2]);
			gte_ncct();
			gte_strgb3(&c0, &c1, &c2);

			ppads[0].pad = *(short*)&c0;
			ppads[1].pad = *(short*)&c1;
			ppads[2].pad = *(short*)&c2;

			count--;
			norms += 3;
			ppads += 3;
		}

		restoreLightingMatrices();
	}
}

// [D] [T]
void DrawWheelObject(MODEL* model, SVECTOR* verts, int transparent, int wheelnum)
{
	ushort clut;
	ushort tpage;
	int i;
	int Z;
	int otZ;
	int combo;
	POLY_FT4* poly;
	POLYFT4* src;
	u_int dim;
	u_int bright;

	src = GET_MODEL_DATA(POLYFT4, model, poly_block);
	poly = (POLY_FT4*)current->primptr;

	clut = texture_cluts[src->texture_set][src->texture_id];
	tpage = texture_pages[src->texture_set] | 0x20;

	if (gTimeOfDay > -1)
	{
		if (gTimeOfDay < TIME_NIGHT)
		{
			bright = combointensity & 0xffffffU | 0x2c000000;
			dim = (combointensity & 0xfcfcfcU) >> 2 | 0x2c000000;
		}
		else if (gTimeOfDay == TIME_NIGHT)
		{
			combo = (combointensity & 0xffU) / 3;
			combo = combo << 0x10 | combo << 8 | combo;

			bright = combo | 0x2c000000;
			dim = (combo & 0xfcfcfc) >> 2 | 0x2c000000;
		}
	}

	i = model->num_polys;

	while (i-- != -1)
	{
		gte_ldv3(&verts[src->v0], &verts[src->v1], &verts[src->v2]);

		gte_rtpt();
		gte_nclip();

		gte_stopz(&Z);
		gte_stsxy0(&poly->x0);

		gte_ldv0(&verts[src->v3]);

		gte_rtps();
		gte_avsz4();

		gte_stotz(&otZ);

		if (otZ > 2)
		{
			setPolyFT4(poly);
			addPrim(current->ot + (otZ >> 1) + 5, poly);

			if (i < 2 || Z < 0)
				*(u_int*)&poly->r0 = 0x2c000000;
			else if (((i ^ wheelnum >> 1) & 1) == 0)
				*(u_int*)&poly->r0 = dim;
			else
				*(u_int*)&poly->r0 = bright;

			setSemiTrans(poly, transparent);

			gte_stsxy3(&poly->x1, &poly->x3, &poly->x2);

			*(u_short*)&poly->u0 = *(u_short*)&src->uv0;
			*(u_short*)&poly->u1 = *(u_short*)&src->uv1;
			*(u_short*)&poly->u2 = *(u_short*)&src->uv3;
			*(u_short*)&poly->u3 = *(u_short*)&src->uv2;
			poly->clut = clut;
			poly->tpage = tpage;

			poly++;
		}
		src++;
	}
	current->primptr = (char*)poly;
}

// [D] [T]
void DrawCarWheels(CAR_DATA *cp, MATRIX *RearMatrix, VECTOR *pos, int zclip)
{
	short wheelSize;
	int FW1z, FW2z;
	int BW1z, BW2z;
	int FrontWheelIncrement, BackWheelIncrement;
	int sizeScale;
	int wheelnum;
	SVECTOR* VertPtr;
	SVECTOR* wheelDisp;
	WHEEL* wheel;
	int car_id;
	MODEL* WheelModelBack;
	MODEL* WheelModelFront;
	MODEL* model;

#ifdef PSX
	MATRIX& FrontMatrix = *(MATRIX*)(u_char*)getScratchAddr(0);
	MATRIX& SteerMatrix = *(MATRIX*)((u_char*)getScratchAddr(0) + sizeof(MATRIX));
	VECTOR& WheelPos = *(VECTOR*)((u_char*)getScratchAddr(0) + sizeof(MATRIX) * 2);
	SVECTOR& sWheelPos = *(SVECTOR*)((u_char*)getScratchAddr(0) + sizeof(MATRIX) * 2 + sizeof(VECTOR));
	static_assert(sizeof(MATRIX) * 2 + sizeof(VECTOR) + sizeof(SVECTOR) * 25 < 1024 - sizeof(_pct), "Scratchpad overflow");
#else
	MATRIX FrontMatrix;
	MATRIX SteerMatrix;
	VECTOR WheelPos;
	SVECTOR sWheelPos;
#endif

	D_CHECK_ERROR(cp < car_data, "Invalid car");

	car_id = CAR_INDEX(cp);

	BackWheelIncrement = FrontWheelIncrement = cp->hd.wheel_speed >> 8;

	if (cp->wheelspin != 0)
		BackWheelIncrement = 700;

	if (cp->hd.wheel[0].locked != 0)
		FrontWheelIncrement = 0;

	if (cp->hd.wheel[3].locked != 0)
		BackWheelIncrement = 0;

	if (FrontWheelIncrement + 400U < 801)
		WheelModelFront = gCleanWheelModelPtr;
	else
		WheelModelFront = gFastWheelModelPtr;

	if (BackWheelIncrement + 400U < 801)
		WheelModelBack = gCleanWheelModelPtr;
	else
		WheelModelBack = gFastWheelModelPtr;

	wheelSize = car_cosmetics[cp->ap.model].wheelSize;
	
	sizeScale = (wheelSize * 14142) / 10000;

	// rotate wheel verts

	FW1z = FIXEDH(RSIN(FrontWheelRotation[car_id]) * sizeScale);
	FW2z = FIXEDH(RCOS(FrontWheelRotation[car_id]) * sizeScale);

	VertPtr = GET_MODEL_DATA(SVECTOR, WheelModelFront, vertices);

	VertPtr[8].vz = FW1z;
	VertPtr[15].vz = FW1z;
	VertPtr[11].vy = FW1z;
	VertPtr[12].vy = FW1z;

	VertPtr[9].vy = -FW1z;
	VertPtr[14].vy = -FW1z;
	VertPtr[10].vz = -FW1z;
	VertPtr[13].vz = -FW1z;

	VertPtr[8].vy = FW2z;
	VertPtr[15].vy = FW2z;
	VertPtr[9].vz = FW2z;
	VertPtr[14].vz = FW2z;

	VertPtr[10].vy = -FW2z;
	VertPtr[13].vy = -FW2z;
	VertPtr[11].vz = -FW2z;
	VertPtr[12].vz = -FW2z;

	VertPtr[23].vz = 0;
	VertPtr[22].vz = 0;
	VertPtr[21].vy = 0;
	VertPtr[20].vy = 0;
	VertPtr[19].vy = 0;
	VertPtr[18].vy = 0;
	VertPtr[17].vz = 0;
	VertPtr[16].vz = 0;

	VertPtr[23].vy = wheelSize;
	VertPtr[22].vy = wheelSize;
	VertPtr[21].vz = wheelSize;
	VertPtr[20].vz = wheelSize;

	VertPtr[19].vz = -wheelSize;
	VertPtr[18].vz = -wheelSize;
	VertPtr[17].vy = -wheelSize;
	VertPtr[16].vy = -wheelSize;

	BW1z = FIXEDH(RSIN(BackWheelRotation[car_id]) * sizeScale);
	BW2z = FIXEDH(RCOS(BackWheelRotation[car_id]) * sizeScale);

	VertPtr = GET_MODEL_DATA(SVECTOR, WheelModelBack, vertices);
	
	VertPtr[8].vz = BW1z;
	VertPtr[15].vz = BW1z;
	VertPtr[11].vy = BW1z;
	VertPtr[12].vy = BW1z;

	VertPtr[9].vy = -BW1z;
	VertPtr[14].vy = -BW1z;
	VertPtr[10].vz = -BW1z;
	VertPtr[13].vz = -BW1z;

	VertPtr[8].vy = BW2z;
	VertPtr[15].vy = BW2z;
	VertPtr[9].vz = BW2z;
	VertPtr[14].vz = BW2z;

	VertPtr[10].vy = -BW2z;
	VertPtr[13].vy = -BW2z;
	VertPtr[11].vz = -BW2z;
	VertPtr[12].vz = -BW2z;

	VertPtr[23].vz = 0;
	VertPtr[22].vz = 0;
	VertPtr[21].vy = 0;
	VertPtr[20].vy = 0;
	VertPtr[19].vy = 0;
	VertPtr[18].vy = 0;
	VertPtr[17].vz = 0;
	VertPtr[16].vz = 0;

	VertPtr[23].vy = wheelSize;
	VertPtr[22].vy = wheelSize;
	VertPtr[21].vz = wheelSize;
	VertPtr[20].vz = wheelSize;

	VertPtr[19].vz = -wheelSize;
	VertPtr[18].vz = -wheelSize;
	VertPtr[17].vy = -wheelSize;
	VertPtr[16].vy = -wheelSize;

	SteerMatrix.m[0][0] = RCOS(cp->wheel_angle);
	SteerMatrix.m[0][2] = RSIN(cp->wheel_angle);
	SteerMatrix.m[1][1] = ONE;
	SteerMatrix.m[2][1] = 0;
	SteerMatrix.m[1][2] = 0;
	SteerMatrix.m[1][0] = 0;
	SteerMatrix.m[0][1] = 0;
	SteerMatrix.m[2][0] = -SteerMatrix.m[0][2];
	SteerMatrix.m[2][2] = SteerMatrix.m[0][0];

	MulMatrix0(RearMatrix, &SteerMatrix, &FrontMatrix);

	wheelDisp = car_cosmetics[cp->ap.model].wheelDisp;
	wheel = cp->hd.wheel;
	wheelnum = 0;

#ifndef PSX
	char carInspectorKey[ASSET_CATALOG_ID_CAPACITY] = {};
	Cars_BuildInspectorKey(cp, carInspectorKey, sizeof(carInspectorKey));
#endif

	do {
		if ((wheelnum & 1) != 0)
			model = WheelModelBack;
		else
			model = WheelModelFront;

		VertPtr = GET_MODEL_DATA(SVECTOR, model, vertices);

		if (cp->ap.flags & (1 << wheelnum)) // [A] used appearance flags to store hubcap presence
		{
			model = gDamWheelModelPtr;
		}

		if ((wheelnum & 2) == 0) 
			sWheelPos.vx = 17 - wheelDisp->vx;
		else
			sWheelPos.vx = -17 - wheelDisp->vx;

		sWheelPos.vz = -wheelDisp->vz;
		sWheelPos.vy = (-wheelSize - wheelDisp->vy) - wheel->susCompression + 14;

		gte_SetRotMatrix(RearMatrix);
		gte_ldv0(&sWheelPos);

		gte_rtv0();

		gte_stlvl(&WheelPos);

		WheelPos.vx += pos->vx;
		WheelPos.vy += pos->vy;
		WheelPos.vz += pos->vz;

		gte_SetTransVector(&WheelPos);

		if ((wheelnum & 1) == 0) 
		{
			gte_SetRotMatrix(&FrontMatrix);
		}

#ifndef PSX
		const void* wheelBegin = current->primptr;
#endif
		DrawWheelObject(model, VertPtr, TransparentObject, wheelnum);
#ifndef PSX
		{
			// A wheel is a component of the car instance, not a level model slot.
			char wheelKey[ASSET_CATALOG_ID_CAPACITY];
			if (AssetCatalog_MakeComponentKey(carInspectorKey, "wheel", wheelnum, wheelKey, sizeof(wheelKey)))
			{
				char inspectorLabel[PSYX_INSPECTOR_LABEL_LENGTH];
				PsyXInspectorObject inspectorObject = {};
				snprintf(inspectorObject.key, sizeof(inspectorObject.key), "%s", wheelKey);
				snprintf(inspectorObject.modelName, sizeof(inspectorObject.modelName), "Wheel %d", wheelnum);
				inspectorObject.modelIndex = -1;
				inspectorObject.position[0] = cp->hd.where.t[0];
				inspectorObject.position[1] = cp->hd.where.t[1];
				inspectorObject.position[2] = cp->hd.where.t[2];
				snprintf(inspectorLabel, sizeof(inspectorLabel), "Wheel %d of Car #%d", wheelnum, cp->id);
				PsyX_Inspector_RegisterObjectRange(wheelBegin, current->primptr, inspectorLabel, &inspectorObject);
			}
		}
#endif

		wheelDisp++;
		wheel++;
		wheelnum++;
	} while (wheelnum < 4);
}

// [D] [T]
void PlayerCarFX(CAR_DATA *cp)
{
	int WheelSpeed;
	WheelSpeed = cp->hd.wheel_speed;

	if (WheelSpeed + 199U < 0x4b0c7)
	{
		if (cp->wheel_angle < -200)
			AddIndicatorLight(cp, 0);
		else if (cp->wheel_angle > 200)
			AddIndicatorLight(cp, 1);
	}

	if( WheelSpeed < 0 && cp->thrust > 0 ||
		WheelSpeed > 0 && cp->thrust < 0)
	{
		AddBrakeLight(cp);
	}

	if (WheelSpeed < 0 && cp->thrust < 0)
		AddReverseLight(cp);
}

// [D] [T]
void plotNewCarModel(CAR_MODEL* car, int palette)
{
#ifdef PSX
	plotCarGlobals& _pg = *(plotCarGlobals*)((u_char*)getScratchAddr(0) + 1024 - sizeof(plotCarGlobals) - sizeof(_pct));
#else
	plotCarGlobals _pg;
#endif

	u_int lightlevel;
	u_int underIntensity;
	SVECTOR v = { 0, -4096, 0 };

	lightlevel = combointensity | 0x3000000;

	if (car == NULL)
		return;

	if (gTimeOfDay > -1)
	{
		if (gTimeOfDay < TIME_NIGHT)
			lightlevel = combointensity | 0x30000000;
		else if (gTimeOfDay == TIME_NIGHT)
			lightlevel = 0x302a2a2a;
	}

	setupLightingMatrices();

	gte_ldv0(&v);
	gte_ldrgb(&lightlevel);

	gte_nccs();

	_pg.primptr = (u_char*)current->primptr;
	_pg.intensity = 0;
	_pg.pciv_clut = (u_short*)&civ_clut[1];
	_pg.damageLevel = (u_char*)gTempCarUVPtr;

	_pg.ot = (OTTYPE*)(current->ot + 28);

	gte_strgb(&underIntensity);

	// draw wheel arcs
	plotCarPolyB3(car->numB3, car->pB3, car->vlist, &_pg);
	_pg.intensity = underIntensity & 0xffffff;

	// draw car bottom
	_pg.ot = (OTTYPE*)(current->ot + 16);
	plotCarPolyFT3(car->numFT3, car->pFT3, car->vlist, &_pg);

	// draw car body
	_pg.ot = (OTTYPE*)(current->ot + 4);
	if (gTimeOfDay == TIME_NIGHT)
	{
		_pg.intensity = (combointensity & 0xfcfcf0U) >> 2;
#ifdef DYNAMIC_LIGHTING
		(gEnableDlights ? plotCarPolyGT3Lit : plotCarPolyGT3)(car->numGT3, car->pGT3, car->vlist, car->nlist, &_pg, palette);
#else
		plotCarPolyGT3nolight(car->numGT3, car->pGT3, car->vlist, &_pg, palette);
#endif // DYNAMIC_LIGHTING
	}
	else
	{
		_pg.intensity = combointensity & 0xffffff;
#ifdef DYNAMIC_LIGHTING
		(gEnableDlights ? plotCarPolyGT3Lit : plotCarPolyGT3)(car->numGT3, car->pGT3, car->vlist, car->nlist, &_pg, palette);
#else
		plotCarPolyGT3(car->numGT3, car->pGT3, car->vlist, car->nlist, &_pg, palette);
#endif
	}

	current->primptr = (char*)_pg.primptr;

	restoreLightingMatrices();
}

// [A]
void startBuildNewCars(int isSpecial)
{
	if(isSpecial)
		baseSpecCP = whichCP;
	else
		whichCP = 0;
}


#if USE_PC_FILESYSTEM
static char* CarModelTypeNames[] = {
	"CLEAN",
	"DAMAGED",
	"LOW",
};

// [A] loads car model from file
char* LoadCarModelFromFile(char* dest, int modelNumber, int type)
{
	char* mem;
	char filename[64];

	sprintf(filename, "LEVELS\\%s\\CARMODEL_%d_%s.MDL", LevelNames[GameLevel], modelNumber, CarModelTypeNames[type - 1]);
	if (FileExists(filename))
	{
		mem = (char*)(dest ? dest : (_other_buffer + modelNumber * 0x10000 + (type - 1) * 0x4000));

		// get model from file
		Loadfile(filename, mem);
		return mem;
	}

	return NULL;
}
#endif

// [D] [T]
MODEL* GetCarModel(char* src, char** dest, int KeepNormals)
{
	int size;
	MODEL* model;
	char* mem;

	model = (MODEL*)*dest;

	if (KeepNormals == 0)
		size = ((MODEL*)src)->normals;
	else
		size = ((MODEL*)src)->poly_block;

	// if loaded externally don't copy from source lump
	memcpy((u_char*)*dest, (u_char*)src, size);

	if (KeepNormals == 0)
		size = model->normals;
	else
		size = model->poly_block;

	//*dest += size + 2;
	*dest = (char*)((u_intptr)model + size + 3 & ~3);

#if MODEL_RELOCATE_POINTERS
	model->vertices += (int)model;
	model->normals += (int)model;
	model->poly_block = (int)src + model->poly_block;

	if (KeepNormals == 0)
		model->point_normals = 0;
	else
		model->point_normals += (int)model;
#else
	if (KeepNormals == 0)
		model->point_normals = 0;

	model->instance_number = -1;
#endif
	return model;
}

// [D] [T] [A]
void buildNewCarFromModel(int index, int detail, char* polySrc, MODEL* model)
{
#if defined(_WIN32) && !defined(PSX)
	if (index >= 0 && index < MAX_CAR_RESIDENT_MODELS) ++s_nativeCarModelRevision[index];
#endif

	int newNumPolys;
	int i, pass;
	ushort clut;
	u_char ptype, carid;
	u_char *polyList;
	CAR_POLY* cp;
	CAR_MODEL* car;
	MODEL* polySrcModel;

	polySrcModel = (MODEL*)polySrc;
	car = detail ? &NewCarModel[index] : &NewLowCarModel[index];

	if (polySrcModel == NULL || (polySrcModel->shape_flags & 0xfffffff) > 0x800000)
	{
		car->numGT3 = 0;
		car->numFT3 = 0;
		car->numB3 = 0;
		return;
	}

	model->instance_number = -1;
	polySrcModel->instance_number = -1;

	car->vlist = GET_MODEL_DATA(SVECTOR, model, vertices);
	car->nlist = GET_MODEL_DATA(SVECTOR, model, point_normals);
;
	for (pass = 0; pass < 3; pass++)
	{
		polyList = GET_RELOC_MODEL_DATA(u_char, polySrcModel, poly_block);

		if (pass == 1)
			car->pFT3 = carPolyBuffer + whichCP;
		else if (pass == 0)
			car->pGT3 = carPolyBuffer + whichCP;
		else if (pass == 2)
			car->pB3 = carPolyBuffer + whichCP;

		newNumPolys = whichCP;

		for (i = 0; newNumPolys < MAX_CAR_POLYS && i < model->num_polys; i++)
		{
			ptype = *polyList;

			cp = carPolyBuffer + newNumPolys;

			switch (ptype & 0x1f) 
			{
				case 0:
				case 18:
					if (pass == 2)	// F3
					{
						cp->vindices = M_INT_4R(polyList[1], polyList[2], polyList[3], 0);
						cp->originalindex = i;

						newNumPolys++;
					}
					break;
				case 1:
				case 19:
					if (pass == 2)	// F4
					{
						cp->vindices = M_INT_4R(polyList[4], polyList[5], polyList[6], 0); 
						cp->originalindex = i;

						cp++;

						cp->vindices = M_INT_4R(polyList[4], polyList[6], polyList[7], 0);
						cp->originalindex = i;

						newNumPolys += 2;
					}
					break;
				case 20:
					if (pass == 1)	// FT3
					{
						POLYFT3* pft3 = (POLYFT3*)polyList;
									
						cp->vindices = M_INT_4R(pft3->v0, pft3->v1, pft3->v2, 0);
						cp->clut_uv0 = M_INT_2(texture_cluts[pft3->texture_set][pft3->texture_id], *(ushort*)&pft3->uv0);
						cp->tpage_uv1 = M_INT_2(texture_pages[pft3->texture_set], *(ushort*)&pft3->uv1);
						cp->uv3_uv2 = *(ushort*)&pft3->uv2;
						cp->originalindex = i;

						newNumPolys++;
					}
					break;
				case 21:
					if (pass == 1)	// FT4
					{
						POLYFT4* pft4 = (POLYFT4*)polyList;

						cp->vindices = M_INT_4R(pft4->v0, pft4->v1, pft4->v2, 0);
						cp->clut_uv0 = M_INT_2(texture_cluts[pft4->texture_set][pft4->texture_id], *(ushort *)&pft4->uv0);
						cp->tpage_uv1 = M_INT_2(texture_pages[pft4->texture_set], *(ushort*)&pft4->uv1);
						cp->uv3_uv2 = *(ushort*)&pft4->uv2;
						cp->originalindex = i;

						cp++;
						
						cp->vindices = M_INT_4R(pft4->v0, pft4->v2, pft4->v3, 0);
						cp->clut_uv0 = M_INT_2(texture_cluts[polyList[1]][polyList[2]], *(ushort*)&pft4->uv0);
						cp->tpage_uv1 = M_INT_2(texture_pages[polyList[1]], *(ushort*)&pft4->uv2);
						cp->uv3_uv2 = *(ushort*)&pft4->uv3;
						cp->originalindex = i;

						newNumPolys += 2;
					}
					break;
				case 22:
					if (pass == 0) // GT3
					{
						POLYGT3* pgt3 = (POLYGT3*)polyList;
							
						carid = GetCarPalIndex(pgt3->texture_set);
						clut = (carid - 1) * 6 * 32 + pgt3->texture_id * 6;

						civ_clut[carid][pgt3->texture_id][0] = texture_cluts[pgt3->texture_set][pgt3->texture_id];
						
						cp->vindices = M_INT_4R(pgt3->v0, pgt3->v1, pgt3->v2, 0);
						cp->nindices = M_INT_4R(pgt3->n0, pgt3->n1, pgt3->n2, 0);
						cp->clut_uv0 = M_INT_2(clut, *(ushort*)&pgt3->uv0);
						cp->tpage_uv1 = M_INT_2(texture_pages[pgt3->texture_set], *(ushort *)&pgt3->uv1);
						cp->uv3_uv2 = *(ushort *)&pgt3->uv2;
						cp->originalindex = i;

						newNumPolys++;
					}
					break;
				case 23:
					if (pass == 0)  // GT4
					{
						POLYGT4* pgt4 = (POLYGT4*)polyList;

						carid = GetCarPalIndex(pgt4->texture_set);
						clut = (carid - 1) * 6 * 32 + pgt4->texture_id * 6;

						civ_clut[carid][pgt4->texture_id][0] = texture_cluts[pgt4->texture_set][pgt4->texture_id];

						cp->vindices = M_INT_4R(pgt4->v0, pgt4->v1, pgt4->v2, 0);
						cp->nindices = M_INT_4R(pgt4->n0, pgt4->n1, pgt4->n2, 0);
						cp->clut_uv0 = M_INT_2(clut, *(ushort*)&pgt4->uv0);
						cp->tpage_uv1 = M_INT_2(texture_pages[pgt4->texture_set], *(ushort*)&pgt4->uv1);
						cp->uv3_uv2 = *(ushort*)&pgt4->uv2;
						cp->originalindex = i;

						cp++;

						cp->vindices = M_INT_4R(pgt4->v0, pgt4->v2, pgt4->v3, 0);
						cp->nindices = M_INT_4R(pgt4->n0, pgt4->n2, pgt4->n3, 0);
						cp->clut_uv0 = M_INT_2(clut, *(ushort*)&pgt4->uv0);
						cp->tpage_uv1 = M_INT_2(texture_pages[pgt4->texture_set], *(ushort *)&pgt4->uv2);
						cp->uv3_uv2 = *(ushort *)&pgt4->uv3;
						cp->originalindex = i;

						newNumPolys += 2;
					}
			}

			polyList += PolySizes[ptype & 0x1f];
		}

		if (pass == 1) 
			car->numFT3 = newNumPolys - whichCP;
		else if (pass == 0)
			car->numGT3 = newNumPolys - whichCP;
		else if (pass == 2) 
			car->numB3 = newNumPolys - whichCP;

		whichCP = newNumPolys;
	}
}

// [D] [T]
void MangleWheelModels(void)
{
	UV_INFO tmpUV2;
	u_char tmpUV;
	u_int v0, v1, v2;
	POLYFT4*src;
	MODEL *m;
	int i, j;

	for (i = 0; i < 3; i++)
	{
		if (i == 1)
			m = gFastWheelModelPtr;
		else if (i == 2)
			m = gDamWheelModelPtr;
		else
			m = gCleanWheelModelPtr;

		// do some fuckery swaps
		src = GET_MODEL_DATA(POLYFT4, m, poly_block);
		
		v0 = *(u_int *)&src[2].v0;
		v1 = *(u_int *)&src[2].uv0;
		v2 = *(u_int *)&src[2].uv2;
		*(u_int *)src = *(u_int *)(src + 2);
		*(u_int *)&src->v0 = v0;
		*(u_int *)&src->uv0 = v1;
		*(u_int *)&src->uv2 = v2;
		src->color = src[2].color;

		v0 = *(u_int *)&src[3].v0;
		v1 = *(u_int *)&src[3].uv0;
		v2 = *(u_int *)&src[3].uv2;
		*(u_int *)(src + 1) = *(u_int *)(src + 3);
		*(u_int *)&src[1].v0 = v0;
		*(u_int *)&src[1].uv0 = v1;
		*(u_int *)&src[1].uv2 = v2;
		src[1].color = src[3].color;

		v0 = *(u_int *)&src[4].v0;
		v1 = *(u_int *)&src[4].uv0;
		v2 = *(u_int *)&src[4].uv2;
		*(u_int *)(src + 2) = *(u_int *)(src + 4);
		*(u_int *)&src[2].v0 = v0;
		*(u_int *)&src[2].uv0 = v1;
		*(u_int *)&src[2].uv2 = v2;
		src[2].color = src[4].color;

		v0 = *(u_int *)&src[6].v0;
		v1 = *(u_int *)&src[6].uv0;
		v2 = *(u_int *)&src[6].uv2;
		*(u_int *)(src + 3) = *(u_int *)(src + 6);
		*(u_int *)&src[3].v0 = v0;
		*(u_int *)&src[3].uv0 = v1;
		*(u_int *)&src[3].uv2 = v2;
		src[3].color = src[6].color;

		src[2].v2 = 23;
		src[2].v3 = 22;
		src[3].v0 = 21;
		src[3].v1 = 20;
		src[3].v2 = 19;
		src[3].v3 = 18;
		src[2].v1 = 17;
		src[2].v0 = 16;

		tmpUV = (src->uv0.u + src->uv2.u) / 2;
		src[3].uv3.u = tmpUV;
		src[3].uv2.u = tmpUV;
		src[3].uv1.u = tmpUV;
		src[3].uv0.u = tmpUV;
		src[2].uv3.u = tmpUV;
		src[2].uv2.u = tmpUV;
		src[2].uv1.u = tmpUV;
		src[2].uv0.u = tmpUV;

		tmpUV = (src->uv0.v + src->uv2.v) / 2;
		src[3].uv3.v = tmpUV;
		src[3].uv2.v = tmpUV;
		src[3].uv1.v = tmpUV;
		src[3].uv0.v = tmpUV;
		src[2].uv3.v = tmpUV;
		src[2].uv2.v = tmpUV;
		src[2].uv1.v = tmpUV;
		src[2].uv0.v = tmpUV;

		m->num_polys = 4;

		for (j = 0; j < 2; j++)
		{
			tmpUV2 = src->uv0;
			
			src->uv0 = src->uv1;
			src->uv1 = src->uv2;
			src->uv2 = src->uv3;
			src->uv3 = tmpUV2;

			src++;
		}

	}

	// HACK: Show clean model only in Rio.
	//if (GameLevel == 3) 
	//	gFastWheelModelPtr = gCleanWheelModelPtr;
}

// [D] [T]
void ProcessPalletLump(char *lump_ptr, int lump_size)
{
	ushort clutValue;
	int *buffPtr;
	int texnum;
	int palette;
	u_short *clutTablePtr;
	u_short clutTable[320];
	int tpageindex;
	int total_cluts;
	int clut_number;

	total_cluts = *(int*)lump_ptr;
	
	if (total_cluts == 0)
		return;

	buffPtr = (int*)(lump_ptr + 4);
	clutTablePtr = (u_short*)clutTable;

	while (*buffPtr != -1)
	{
		palette = buffPtr[0];
		texnum = buffPtr[1];
		tpageindex = buffPtr[2];
		clut_number = buffPtr[3];
		buffPtr += 4;

		if (clut_number == -1)
		{
			// store clut
			LoadImage(&clutpos, (u_long*)buffPtr);
			buffPtr += 8;

			clutValue = GetClut(clutpos.x, clutpos.y);
			IncrementClutNum(&clutpos);

			*clutTablePtr++ = clutValue;			
		}
		else
		{
			// use stored clut
			clutValue = clutTable[clut_number];
		}

		civ_clut[GetCarPalIndex(tpageindex)][texnum][palette + 1] = clutValue;
	}
}

#ifndef PSX
// Source identity (model number) is distinct from the live car slot; the key
// keeps both and the city variant, so a reused slot cannot alias a key.
static void Cars_BuildInspectorKey(const CAR_DATA* cp, char* out, int capacity)
{
	AssetCatalogContext catalogContext;
	const int cityVariant = AssetCatalog_GetContext(&catalogContext) ? catalogContext.variant : (int)GetCityType();
	int carModelNumber = -1;
	const int carRecord = AssetCatalog_FindCar(cp->ap.model);
	if (carRecord >= 0)
		AssetCatalog_GetCar(carRecord, NULL, &carModelNumber, NULL, 0, NULL);
	else if (cp->ap.model >= 0 && cp->ap.model < MAX_CAR_RESIDENT_MODELS)
		carModelNumber = residentCarModels[cp->ap.model];

	snprintf(out, capacity, "car:%d:%d:%d:%d:%d",
		GameLevel, cityVariant, cp->id, cp->ap.model, carModelNumber);
}
#endif

// [D] [T]
void DrawCarObject(CAR_MODEL* car, MATRIX* matrix, VECTOR* pos, int palette, CAR_DATA* cp, int detail)
{
	VECTOR modelLocation;
	SVECTOR cog;

#ifndef PSX
	const void* primitiveBegin = current->primptr;
#endif

	cog = cp->ap.carCos->cog;

	// [A] mini cars correct position
	if (ActiveCheats.cheat13 != 0)
	{
		cog.vx <<= 1;
		cog.vy <<= 1;
		cog.vz <<= 1;
	}

	gte_SetRotMatrix(matrix);
	gte_SetTransVector(pos);

	gte_ldv0(&cog);

	gte_rtv0tr();

	gte_stlvnl(&modelLocation);

	gte_SetTransVector(&modelLocation);

	plotNewCarModel(car, palette);

#ifndef PSX
	char inspectorLabel[PSYX_INSPECTOR_LABEL_LENGTH];
	snprintf(inspectorLabel, sizeof(inspectorLabel), "Car #%d | model %d | %s detail | palette %d",
		cp->id, cp->ap.model, detail ? "high" : "low", palette);
	PsyXInspectorObject inspectorObject = {};

	Cars_BuildInspectorKey(cp, inspectorObject.key, sizeof(inspectorObject.key));
	snprintf(inspectorObject.modelName, sizeof(inspectorObject.modelName), "Resident car %d", cp->ap.model);
	inspectorObject.modelIndex = cp->ap.model;
	inspectorObject.polygonCount = car->numFT3 + car->numGT3 + car->numB3;
	for (int i = 0; i < 3; ++i) inspectorObject.position[i] = cp->hd.where.t[i];
	PsyX_Inspector_RegisterObjectRange(primitiveBegin, current->primptr, inspectorLabel, &inspectorObject);
#endif
}

#ifndef PSX
static bool IsInspectorModId(const char* modId)
{
	if (!modId || modId[0] == '\0')
		return false;
	for (const char* character = modId; *character; ++character)
	{
		if (!((*character >= 'a' && *character <= 'z') || (*character >= 'A' && *character <= 'Z') ||
			(*character >= '0' && *character <= '9') || *character == '-' || *character == '_'))
			return false;
	}
	return true;
}

static int CarModelMaximumVertex(const CAR_POLY* polygons, int count)
{
	int maximum = 0;
	for (int polygonIndex = 0; polygonIndex < count; ++polygonIndex)
	{
		const int indices = polygons[polygonIndex].vindices;
		for (int corner = 0; corner < 3; ++corner)
		{
			const int index = (indices >> (corner * 8)) & 0xff;
			if (index > maximum) maximum = index;
		}
	}
	return maximum;
}

static void WriteCarModelFaces(std::string& output, const CAR_POLY* polygons, int count)
{
	for (int polygonIndex = 0; polygonIndex < count; ++polygonIndex)
	{
		const int indices = polygons[polygonIndex].vindices;
		char line[96];
		snprintf(line, sizeof(line), "f %d %d %d\n", (indices & 0xff) + 1, ((indices >> 8) & 0xff) + 1,
			((indices >> 16) & 0xff) + 1);
		output += line;
	}
}

static void AppendCarModelTriangleIndices(std::vector<unsigned int>& indices, const CAR_POLY* polygons, int count)
{
	for (int polygonIndex = 0; polygonIndex < count; ++polygonIndex)
	{
		const int packed = polygons[polygonIndex].vindices;
		indices.push_back((unsigned int)(packed & 0xff));
		indices.push_back((unsigned int)((packed >> 8) & 0xff));
		indices.push_back((unsigned int)((packed >> 16) & 0xff));
	}
}

bool Cars_ExportInspectorModel(int carId, const char* modId, char* status, int statusCapacity)
{
	if (!status || statusCapacity <= 0)
		return false;
	status[0] = '\0';
	if (carId < 0 || carId >= MAX_CARS || !IsInspectorModId(modId))
	{
		snprintf(status, statusCapacity, "Choose a valid car selection and mod id");
		return false;
	}
#ifndef _WIN32
	snprintf(status, statusCapacity, "Model export is currently available on Windows only");
	return false;
#else
	const int modelNumber = car_data[carId].ap.model;
	if (modelNumber < 0 || modelNumber >= MAX_CAR_RESIDENT_MODELS)
	{
		snprintf(status, statusCapacity, "The selected car has no resident model");
		return false;
	}
	CAR_MODEL model = NewCarModel[modelNumber];
	model.vlist = gTempCarVertDump[carId];
	if (!model.vlist)
	{
		snprintf(status, statusCapacity, "The selected car model is not currently available");
		return false;
	}
	int maximumVertex = CarModelMaximumVertex(model.pFT3, model.numFT3);
	const int maximumGtVertex = CarModelMaximumVertex(model.pGT3, model.numGT3);
	const int maximumBottomVertex = CarModelMaximumVertex(model.pB3, model.numB3);
	if (maximumGtVertex > maximumVertex) maximumVertex = maximumGtVertex;
	if (maximumBottomVertex > maximumVertex) maximumVertex = maximumBottomVertex;
	if (maximumVertex < 0 || maximumVertex >= MAX_DENTING_VERTS)
	{
		snprintf(status, statusCapacity, "The selected car model has unsupported vertex indices");
		return false;
	}
	const DWORD modsAttributes = GetFileAttributesA("mods");
	const DWORD parentModsAttributes = GetFileAttributesA("../mods");
	const char* modsRoot = modsAttributes != INVALID_FILE_ATTRIBUTES && (modsAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "mods" :
		parentModsAttributes != INVALID_FILE_ATTRIBUTES && (parentModsAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "../mods" : "mods";
	char modDirectory[256];
	char assetsDirectory[320];
	char modelsDirectory[384];
	char outputPath[448];
	snprintf(modDirectory, sizeof(modDirectory), "%s/%s", modsRoot, modId);
	snprintf(assetsDirectory, sizeof(assetsDirectory), "%s/assets", modDirectory);
	snprintf(modelsDirectory, sizeof(modelsDirectory), "%s/models", assetsDirectory);
	snprintf(outputPath, sizeof(outputPath), "%s/car_%d_id_%d.obj", modelsDirectory, modelNumber, carId);
	CreateDirectoryA(modsRoot, NULL);
	CreateDirectoryA(modDirectory, NULL);
	CreateDirectoryA(assetsDirectory, NULL);
	CreateDirectoryA(modelsDirectory, NULL);
	char line[192];
	snprintf(line, sizeof(line), "# REDRIVER2-Plus 3D inspector car export\n# car id: %d\n# model: %d\n", carId, modelNumber);
	std::string output(line);
	for (int vertexIndex = 0; vertexIndex <= maximumVertex; ++vertexIndex)
	{
		const SVECTOR& vertex = model.vlist[vertexIndex];
		snprintf(line, sizeof(line), "v %d %d %d\n", vertex.vx, vertex.vy, vertex.vz);
		output += line;
	}
	WriteCarModelFaces(output, model.pFT3, model.numFT3);
	WriteCarModelFaces(output, model.pGT3, model.numGT3);
	WriteCarModelFaces(output, model.pB3, model.numB3);
	return InspectorExport_WriteText(outputPath, output.c_str(), status, statusCapacity);
#endif
}

// E02-B: converts the selected resident car model through the native GLB
// pipeline and leaves the cached artifact under the target mod. The geometry is
// the same one Cars_ExportInspectorModel writes as OBJ; the conversion is keyed
// by the target mod (the only active mod) so two mods keep separate cached
// entries. The OBJ export above is untouched.
bool Cars_ConvertInspectorModel(int carId, const char* modId, char* status, int statusCapacity)
{
	if (!status || statusCapacity <= 0)
		return false;
	status[0] = '\0';
	if (carId < 0 || carId >= MAX_CARS || !IsInspectorModId(modId))
	{
		snprintf(status, statusCapacity, "Choose a valid car selection and mod id");
		return false;
	}
#ifndef _WIN32
	snprintf(status, statusCapacity, "Model export is currently available on Windows only");
	return false;
#else
	const int modelNumber = car_data[carId].ap.model;
	if (modelNumber < 0 || modelNumber >= MAX_CAR_RESIDENT_MODELS)
	{
		snprintf(status, statusCapacity, "The selected car has no resident model");
		return false;
	}
	CAR_MODEL model = NewCarModel[modelNumber];
	model.vlist = gTempCarVertDump[carId];
	if (!model.vlist)
	{
		snprintf(status, statusCapacity, "The selected car model is not currently available");
		return false;
	}
	int maximumVertex = CarModelMaximumVertex(model.pFT3, model.numFT3);
	const int maximumGtVertex = CarModelMaximumVertex(model.pGT3, model.numGT3);
	const int maximumBottomVertex = CarModelMaximumVertex(model.pB3, model.numB3);
	if (maximumGtVertex > maximumVertex) maximumVertex = maximumGtVertex;
	if (maximumBottomVertex > maximumVertex) maximumVertex = maximumBottomVertex;
	if (maximumVertex < 0 || maximumVertex >= MAX_DENTING_VERTS)
	{
		snprintf(status, statusCapacity, "The selected car model has unsupported vertex indices");
		return false;
	}

	const DWORD modsAttributes = GetFileAttributesA("mods");
	const DWORD parentModsAttributes = GetFileAttributesA("../mods");
	const char* modsRoot = modsAttributes != INVALID_FILE_ATTRIBUTES && (modsAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "mods" :
		parentModsAttributes != INVALID_FILE_ATTRIBUTES && (parentModsAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "../mods" : "mods";
	char modDirectory[256];
	char assetsDirectory[320];
	char cacheDirectory[384];
	snprintf(modDirectory, sizeof(modDirectory), "%s/%s", modsRoot, modId);
	snprintf(assetsDirectory, sizeof(assetsDirectory), "%s/assets", modDirectory);
	snprintf(cacheDirectory, sizeof(cacheDirectory), "%s/cache", assetsDirectory);
	CreateDirectoryA(modsRoot, NULL);
	CreateDirectoryA(modDirectory, NULL);
	CreateDirectoryA(assetsDirectory, NULL);
	CreateDirectoryA(cacheDirectory, NULL);

	const int vertexCount = maximumVertex + 1;
	std::vector<float> positions((size_t)vertexCount * 3);
	for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
	{
		const SVECTOR& vertex = model.vlist[vertexIndex];
		positions[(size_t)vertexIndex * 3 + 0] = (float)vertex.vx;
		positions[(size_t)vertexIndex * 3 + 1] = (float)vertex.vy;
		positions[(size_t)vertexIndex * 3 + 2] = (float)vertex.vz;
	}

	std::vector<unsigned int> triangleIndices;
	triangleIndices.reserve((size_t)(model.numFT3 + model.numGT3 + model.numB3) * 3);
	AppendCarModelTriangleIndices(triangleIndices, model.pFT3, model.numFT3);
	AppendCarModelTriangleIndices(triangleIndices, model.pGT3, model.numGT3);
	AppendCarModelTriangleIndices(triangleIndices, model.pB3, model.numB3);
	const int triangleCount = (int)(triangleIndices.size() / 3);

	char sourceIdentity[64];
	snprintf(sourceIdentity, sizeof(sourceIdentity), "car_%d_id_%d", modelNumber, carId);

	// The live conversion is keyed by the target mod: the artifact lands in that
	// mod's cache and a different mod id produces a different cache entry.
	const char* activeMods[1] = { modId };

	AssetConverterResult result;
	char error[256];
	const int resultCode = CarMeshSerializer_Export(cacheDirectory, sourceIdentity,
		positions.data(), vertexCount, triangleIndices.data(), triangleCount,
		activeMods, 1, &result, error, sizeof(error));
	if (resultCode != ASSET_CONVERTER_OK)
	{
		snprintf(status, statusCapacity, "GLB conversion failed: %s",
			error[0] ? error : AssetConverter_ErrorName(resultCode));
		return false;
	}

	snprintf(status, statusCapacity, "Converted car GLB %s (%s): %s",
		result.glbPath, result.converted ? "converted" : "reused", result.contentHash);
	return true;
#endif
}
#endif

#if defined(_WIN32) && !defined(PSX)
bool Cars_GetNativeSourceIdentity(int carId, NativeCarSourceIdentity* out)
{
	if (!out || carId < 0 || carId >= MAX_CARS) return false;
	const CAR_DATA& car = car_data[carId];
	const int model = car.ap.model;
	if (car.controlType == CONTROL_TYPE_NONE || car.controlType == CONTROL_TYPE_CAMERACOLLIDER ||
		model < 0 || model >= MAX_CAR_RESIDENT_MODELS || !car.ap.carCos ||
		!gCarCleanModelPtr[model] || car.ap.palette >= 6) return false;
	out->model = model; out->palette = car.ap.palette;
	out->revision = s_nativeCarModelRevision[model];
	out->cleanModel = gCarCleanModelPtr[model];
	out->lowModel = gCarLowModelPtr[model];
	return true;
}

bool Cars_CaptureNativeBody(int carId, bool lowDetail, NativeCarBodyCapture* out, char* error, int capacity)
{
	NativeCarSourceIdentity identity = {};
	if (!out || !Cars_GetNativeSourceIdentity(carId, &identity))
	{
		if (out) *out = NativeCarBodyCapture();
		if (error && capacity > 0) snprintf(error, capacity, "original car body is not resident");
		return false;
	}
	*out = NativeCarBodyCapture();
	out->lowDetail = lowDetail;
	const MODEL* clean = lowDetail ? identity.lowModel : identity.cleanModel;
	const CAR_MODEL& model = lowDetail ? NewLowCarModel[identity.model] : NewCarModel[identity.model];
	if (!clean || clean->num_vertices <= 0 || clean->num_vertices > MAX_DENTING_VERTS ||
		model.numFT3 < 0 || model.numGT3 < 0 || model.numB3 < 0 ||
		model.numFT3 > 1024 || model.numGT3 > 1024 || model.numB3 > 1024 ||
		model.numFT3 + model.numGT3 + model.numB3 > 1024 ||
		(model.numFT3 && !model.pFT3) || (model.numGT3 && !model.pGT3) || (model.numB3 && !model.pB3))
	{
		if (error && capacity > 0) snprintf(error, capacity, "original car body inventory is invalid");
		return false;
	}
	// The legacy relocation getter takes MODEL*, but capture only reads it.
	const SVECTOR* vertices = GET_MODEL_DATA(SVECTOR, const_cast<MODEL*>(clean), vertices);
	std::vector<float> positions;
	positions.reserve((size_t)clean->num_vertices * 3);
	for (int v = 0; v < clean->num_vertices; ++v)
	{
		positions.push_back((float)vertices[v].vx);
		positions.push_back((float)vertices[v].vy);
		positions.push_back((float)vertices[v].vz);
	}
	std::vector<NativeCarTriangle> triangles;
	const CAR_POLY* lists[3] = { model.pFT3, model.pGT3, model.pB3 };
	const int counts[3] = { model.numFT3, model.numGT3, model.numB3 };
	for (int list = 0; list < 3; ++list)
	{
		for (int p = 0; p < counts[list]; ++p)
		{
			const CAR_POLY& polygon = lists[list][p];
			NativeCarTriangle triangle = {};
			for (int c = 0; c < 3; ++c) triangle.vertices[c] = ((unsigned int)polygon.vindices >> (c * 8)) & 255;
			triangle.textured = list != 2;
			if (triangle.textured)
			{
				const int damageCapacity = lowDetail ? MAX_DENTING_LOD_UVS : MAX_DENTING_UVS;
				if (polygon.originalindex < 0 || polygon.originalindex >= damageCapacity)
				{
					if (error && capacity > 0) snprintf(error, capacity, "original car damage polygon index is invalid");
					return false;
				}
				triangle.originalIndex = (unsigned)polygon.originalindex;
				triangle.paletteClut = list == 1;
				triangle.tpage = (unsigned short)((unsigned int)polygon.tpage_uv1 >> 16);
				triangle.clut = (unsigned short)((unsigned int)polygon.clut_uv0 >> 16);
				if (list == 1)
				{
					const int offset = triangle.clut + identity.palette;
					if (offset < 0 || offset >= 7 * 32 * 6)
					{
						if (error && capacity > 0) snprintf(error, capacity, "original car palette index is invalid");
						return false;
					}
					triangle.clut = ((const unsigned short*)&civ_clut[1])[offset];
				}
				const unsigned int packed[3] = { (unsigned int)polygon.clut_uv0,
					(unsigned int)polygon.tpage_uv1, (unsigned int)polygon.uv3_uv2 };
				for (int c = 0; c < 3; ++c)
				{
					triangle.uv[c * 2] = (unsigned char)packed[c];
					triangle.uv[c * 2 + 1] = (unsigned char)(packed[c] >> 8);
				}
			}
			triangles.push_back(triangle);
		}
	}
	std::vector<unsigned short> vram(1024u * 512u);
	GR_ReadVRAM(vram.data(), 0, 0, 1024, 512);
	return NativeCarMesh_Capture(positions, triangles, vram, &out->conversion, error, capacity, &out->bindings);
}

bool Cars_GetNativeBodyLowDetail(int carId, bool* lowDetail)
{
	NativeCarSourceIdentity identity = {};
	if (!lowDetail || !Cars_GetNativeSourceIdentity(carId, &identity)) return false;
	const CAR_DATA& car = car_data[carId];
	int position[3] = { car.hd.where.t[0], car.hd.where.t[1], car.hd.where.t[2] };
	VisualSnapshot_GetPosition(carId, VisualSnapshot_GetAlpha(), position);
	VECTOR relative = { position[0] - camera_position.vx, -position[1] - camera_position.vy,
		position[2] - camera_position.vz, 0 };
	// Desktop implementation only reads inv_camera_matrix and the local vector.
	Apply_Inv_CameraMatrix(&relative);
	*lowDetail = NativeCarMesh_SelectLowDetail(relative.vz, CAR_LOD_SWITCH_DISTANCE,
		gForceLowDetailCars != 0, car.controlType == CONTROL_TYPE_PLAYER);
	return true;
}

bool Cars_CaptureNativeDamage(int carId, const NativeCarBodyCapture& capture,
	std::vector<float>* xyz, std::vector<float>* uvs, char* error, int capacity)
{
	NativeCarSourceIdentity identity = {};
	if (!Cars_GetNativeSourceIdentity(carId, &identity))
	{
		if (xyz) xyz->clear();
		if (uvs) uvs->clear();
		if (error && capacity > 0) snprintf(error, capacity, "original per-car damage source is not resident");
		return false;
	}
	const MODEL* source = capture.lowDetail ? identity.lowModel : identity.cleanModel;
	const int damageSlot = car_data[carId].id;
	if (damageSlot < 0 || damageSlot >= MAX_CARS || !source || source->num_vertices <= 0 || source->num_vertices > MAX_DENTING_VERTS)
	{
		if (xyz) xyz->clear();
		if (uvs) uvs->clear();
		if (error && capacity > 0) snprintf(error, capacity, "original per-car damage vertex inventory is invalid");
		return false;
	}
	const SVECTOR* vertices = capture.lowDetail ? GET_MODEL_DATA(SVECTOR, const_cast<MODEL*>(source), vertices) : gTempCarVertDump[damageSlot];
	std::vector<float> positions;
	positions.reserve((size_t)source->num_vertices * 3);
	for (int v = 0; v < source->num_vertices; ++v)
	{
		positions.push_back((float)vertices[v].vx);
		positions.push_back((float)vertices[v].vy);
		positions.push_back((float)vertices[v].vz);
	}
	const int count = capture.lowDetail ? MAX_DENTING_LOD_UVS : MAX_DENTING_UVS;
	const DENTUVS* bytes = capture.lowDetail ? gTempLDCarUVDump[damageSlot] : gTempHDCarUVDump[damageSlot];
	std::vector<unsigned char> damage;
	damage.reserve(count);
	for (int p = 0; p < count; ++p) damage.push_back((unsigned char)bytes[p].u3);
	return NativeCarMesh_ExpandDamage(capture, positions, damage, xyz, uvs, error, capacity);
}

bool Cars_GetNativeTransform(int carId, float out[16])
{
	NativeCarSourceIdentity identity = {};
	if (!out || !Cars_GetNativeSourceIdentity(carId, &identity)) return false;
	const CAR_DATA& car = car_data[carId];
	int position[3] = { car.hd.where.t[0], car.hd.where.t[1], car.hd.where.t[2] };
	short rotation[3][3];
	memcpy(rotation, car.hd.where.m, sizeof(rotation));
	const int alpha = VisualSnapshot_GetAlpha();
	if (VisualSnapshot_GetPosition(carId, alpha, position))
		VisualSnapshot_GetRotation(carId, alpha, rotation);
	const SVECTOR& cog = car.ap.carCos->cog;
	const short offset[3] = { cog.vx, cog.vy, cog.vz };
	NativeCarMesh_ComposeTransform(position, rotation, offset, ActiveCheats.cheat13 != 0, out);
	return true;
}

bool Cars_GetNativeWheelSource(int carId, int slot, NativeWheelSourceIdentity* out)
{
	if (!out || slot < 0 || slot >= 4 || !Cars_GetNativeSourceIdentity(carId, &out->car)) return false;
	const CAR_DATA& car = car_data[carId];
	int increment = car.hd.wheel_speed >> 8;
	if (slot & 1) { if (car.wheelspin) increment = 700; if (car.hd.wheel[3].locked) increment = 0; }
	else if (car.hd.wheel[0].locked) increment = 0;
	out->vertices = increment + 400U < 801 ? gCleanWheelModelPtr : gFastWheelModelPtr;
	out->polygons = car.ap.flags & (1 << slot) ? gDamWheelModelPtr : out->vertices;
	out->wheelSize = car_cosmetics[car.ap.model].wheelSize;
	return out->vertices && out->polygons && out->wheelSize > 0 && out->wheelSize <= 4096;
}

bool Cars_CaptureNativeWheel(int carId, int slot, NativeWheelCapture* out, char* error, int capacity)
{
	NativeWheelSourceIdentity identity = {};
	if (!out || !Cars_GetNativeWheelSource(carId, slot, &identity))
	{
		if (out) *out = NativeWheelCapture();
		if (error && capacity > 0) snprintf(error, capacity, "original wheel source is not resident");
		return false;
	}
	*out = NativeWheelCapture();
	const MODEL* verticesModel = identity.vertices;
	const MODEL* polygonsModel = identity.polygons;
	const int quads = polygonsModel->num_polys + 1; // DrawWheelObject includes the retained fifth quad.
	if (verticesModel->num_vertices < 24 || verticesModel->num_vertices > 256 || quads <= 0 || quads > 16)
	{
		if (error && capacity > 0) snprintf(error, capacity, "original wheel inventory is invalid");
		return false;
	}
	const SVECTOR* vertices = GET_MODEL_DATA(SVECTOR, const_cast<MODEL*>(verticesModel), vertices);
	const POLYFT4* polygons = GET_MODEL_DATA(POLYFT4, const_cast<MODEL*>(polygonsModel), poly_block);
	for (int v = 0; v < verticesModel->num_vertices; ++v)
	{
		out->originalPositions.push_back((float)vertices[v].vx);
		out->originalPositions.push_back((float)vertices[v].vy);
		out->originalPositions.push_back((float)vertices[v].vz);
	}
	if (polygons[0].texture_set >= 128 || polygons[0].texture_id >= 32)
	{
		if (error && capacity > 0) snprintf(error, capacity, "original wheel texture identity is invalid");
		return false;
	}
	const unsigned short page = texture_pages[polygons[0].texture_set] | 0x20;
	const unsigned short clut = texture_cluts[polygons[0].texture_set][polygons[0].texture_id];
	std::vector<NativeCarTriangle> triangles;
	for (int q = 0; q < quads; ++q)
	{
		const POLYFT4& polygon = polygons[q];
		const unsigned int indices[4] = { polygon.v0, polygon.v1, polygon.v2, polygon.v3 };
		const unsigned char uv[8] = { polygon.uv0.u, polygon.uv0.v, polygon.uv1.u, polygon.uv1.v,
			polygon.uv2.u, polygon.uv2.v, polygon.uv3.u, polygon.uv3.v };
		NativeCarTriangle emitted[2];
		NativeWheelMesh_TriangulateQuad(indices, uv, page, clut, emitted);
		for (int t = 0; t < 2; ++t)
		{
			const NativeCarTriangle& triangle = emitted[t];
			for (int c = 0; c < 3; ++c)
			{
				out->cornerVertices.push_back(triangle.vertices[c]);
			}
			triangles.push_back(triangle);
		}
	}
	std::vector<unsigned short> vram(1024u * 512u);
	GR_ReadVRAM(vram.data(), 0, 0, 1024, 512);
	if (!NativeCarMesh_Capture(out->originalPositions, triangles, vram, &out->conversion, error, capacity) ||
		!NativeWheelMesh_ApplyConstantColors(quads, &out->conversion.legacyColors, error, capacity) ||
		!NativeWheelMesh_Deform(*out, identity.wheelSize, 0, 4096, &out->conversion.source.positions, error, capacity))
	{
		*out = NativeWheelCapture();
		return false;
	}
	return true;
}

bool Cars_GetNativeWheelPose(int carId, int slot, float transform[16], int* spinSin, int* spinCos)
{
	NativeWheelSourceIdentity source = {};
	if (!transform || !spinSin || !spinCos || !Cars_GetNativeWheelSource(carId, slot, &source)) return false;
	const CAR_DATA& car = car_data[carId];
	int position[3] = { car.hd.where.t[0], car.hd.where.t[1], car.hd.where.t[2] };
	short rotation[3][3]; memcpy(rotation, car.hd.where.m, sizeof(rotation));
	const int alpha = VisualSnapshot_GetAlpha();
	if (VisualSnapshot_GetPosition(carId, alpha, position)) VisualSnapshot_GetRotation(carId, alpha, rotation);
	const short zero[3] = {};
	float body[16]; NativeCarMesh_ComposeTransform(position, rotation, zero, false, body);
	const SVECTOR& displacement = car_cosmetics[car.ap.model].wheelDisp[slot];
	const short local[3] = { displacement.vx, displacement.vy, displacement.vz };
	// Wheel steering/compression/spin remain current-tick values until their snapshot seam migrates.
	NativeWheelMesh_ComposeTransform(body, local, source.wheelSize, car.hd.wheel[slot].susCompression,
		slot, RSIN(car.wheel_angle), RCOS(car.wheel_angle), transform);
	const int spin = slot & 1 ? BackWheelRotation[carId] : FrontWheelRotation[carId];
	*spinSin = RSIN(spin); *spinCos = RCOS(spin);
	return true;
}
#endif

void StepCarPresentation(void)
{
	if (pauseflag)
		return;
	for (int slot = 0; slot < MAX_CARS; ++slot)
	{
		CAR_DATA* cp = &car_data[slot];
		if (cp->controlType == CONTROL_TYPE_NONE || cp->controlType == CONTROL_TYPE_CAMERACOLLIDER || !cp->ap.carCos)
			continue;
		int WheelSpeed, maxDamage;
		int doSmoke = 0;

		WheelSpeed = cp->hd.speed * 8192;
		maxDamage = MaxPlayerDamage[0];

		if (cp->controlType == CONTROL_TYPE_PLAYER)
		{
			if(*cp->ai.padid >= 0 && *cp->ai.padid < 2)
				maxDamage = MaxPlayerDamage[*cp->ai.padid];
		}

		if (cp->totalDamage >= maxDamage)
		{
			if (WheelSpeed + 59999U < 119999)
				AddFlamingEngine(cp);

			doSmoke = 2;
		}
		else
		{
			if (cp->ap.damage[0] > 2000 || cp->ap.damage[1] > 2000)
			{
				if (cp->ap.damage[0] > 3000 || cp->ap.damage[1] > 3000)
					doSmoke = 2;
				else
					doSmoke = 1;
			}
		}

		if (doSmoke && WheelSpeed + 399999U < 1199999)
			AddSmokingEngine(cp, doSmoke - 1, WheelSpeed);

#if ENABLE_GAME_ENCHANCEMENTS
		AddExhaustSmoke(cp, doSmoke > 1, WheelSpeed);
#endif

		cp->ap.life += 8;
		const int copPhaseStep = (GameLevel == 1 || GameLevel == 3) ? 3 * 48 :
			(GameLevel == 2 ? 8 * 16 : 6 * 16);
		cp->ap.coplife += copPhaseStep;
	}
}

// [D] [T] [A]
void DrawCar(CAR_DATA* cp, int view)
{
	int yVal;
	int maxDamage;
	int WheelSpeed;
	int oboxLenSq;
	CAR_MODEL* CarModelPtr;
	int model;
	CVECTOR col;
	SVECTOR d;
	VECTOR pos, dist;
	VECTOR corners[4];
	MATRIX workmatrix;
#ifndef PSX
	MATRIX visualPose = cp->hd.where;
	MATRIX visualDrawMatrix = cp->hd.drawCarMat;
#endif

	D_CHECK_ERROR(cp < car_data, "Invalid car");

	model = cp->ap.model;
#ifndef PSX
	BeginCarPresentation(cp);
#endif

	// draw car lights in for InCar camera
	if (player[view].cameraView == 2 && cp->id == player[view].cameraCarId)
	{
		if (cp->ap.damage[0] < 500)
			LeftLight = 2;
		else if (cp->ap.damage[0] < 1000)
			LeftLight = 1;
		else
			LeftLight = 0;

		if (cp->ap.damage[1] < 500)
			RightLight = 2;
		else if (cp->ap.damage[1] < 1000)
			RightLight = 1;
		else
			RightLight = 0;

		if (gLightsOn == 0)
			return;

		if (lightsOnDelay[cp->id])
			return;

		col.r = 128;
		col.g = 120;
		col.b = 110;

		PlacePoolForCar(cp, &col, 1, 1);
		return;
	}

	pos.vx = cp->hd.where.t[0];
	pos.vz = cp->hd.where.t[2];
	pos.vy = -cp->hd.where.t[1];

#ifndef PSX
	// E01: render the car between its previous and current tick poses. This only
	// affects presentation; the authoritative pose is untouched, and the culling
	// corners, shadow and wheels below all use the same interpolated position.
	{
		int interpolated[3];
		const int alpha = VisualSnapshot_GetAlpha();

		if (VisualSnapshot_GetPosition((int)(cp - car_data), alpha, interpolated))
		{
			pos.vx = interpolated[0];
			pos.vy = -interpolated[1];
			pos.vz = interpolated[2];
			for (int axis = 0; axis < 3; ++axis)
				visualPose.t[axis] = interpolated[axis];
			VisualSnapshot_GetRotation((int)(cp - car_data), alpha, visualPose.m);
			for (int row = 0; row < 3; ++row)
				for (int col = 0; col < 3; ++col)
					visualDrawMatrix.m[row][col] = row == 1 ? visualPose.m[row][col] : -visualPose.m[row][col];
		}
	}
#endif

	SetFrustrumMatrix();

	if (FrustrumCheck(&pos, 800) == -1)
		return;

	// corners for frustrum checking of big cars
	corners[0].vx = corners[1].vx = pos.vx + cp->hd.oBox.radii[0].vx;
	corners[0].vx += cp->hd.oBox.radii[2].vx;
	corners[1].vx -= cp->hd.oBox.radii[2].vx;

	corners[2].vx = corners[3].vx = pos.vx - cp->hd.oBox.radii[0].vx;
	corners[2].vx += cp->hd.oBox.radii[2].vx;
	corners[3].vx -= cp->hd.oBox.radii[2].vx;

	corners[0].vz = corners[1].vz = pos.vz + cp->hd.oBox.radii[0].vz;
	corners[0].vz += cp->hd.oBox.radii[2].vz;
	corners[1].vz -= cp->hd.oBox.radii[2].vz;

	corners[2].vz = corners[3].vz = pos.vz - cp->hd.oBox.radii[0].vz;
	corners[2].vz += cp->hd.oBox.radii[2].vz;
	corners[3].vz -= cp->hd.oBox.radii[2].vz;

	corners[3].vy = pos.vy;
	corners[2].vy = pos.vy;
	corners[1].vy = pos.vy;
	corners[0].vy = pos.vy;

	if (FrustrumCheck(corners, 0) == -1 &&
		FrustrumCheck(corners + 1, 0) == -1 &&
		FrustrumCheck(corners + 2, 0) == -1 &&
		FrustrumCheck(corners + 3, 0) == -1)
	{
		return;
	}

	d.vx = cp->hd.oBox.location.vx - camera_position.vx;
	d.vy = -camera_position.vy - cp->hd.oBox.location.vy;
	d.vz = cp->hd.oBox.location.vz - camera_position.vz;
#ifndef PSX
	d.vx += visualPose.t[0] - cp->hd.where.t[0];
	d.vy -= visualPose.t[1] - cp->hd.where.t[1];
	d.vz += visualPose.t[2] - cp->hd.where.t[2];
#endif

	dist.vx = d.vx * cp->hd.oBox.radii[0].vx + d.vy * cp->hd.oBox.radii[0].vy + d.vz * cp->hd.oBox.radii[0].vz;

	if (dist.vx < 0)
		dist.vx = -dist.vx;

	dist.vy = d.vx * cp->hd.oBox.radii[1].vx + d.vy * cp->hd.oBox.radii[1].vy + d.vz * cp->hd.oBox.radii[1].vz;
	if (dist.vy < 0)
		dist.vy = -dist.vy;

	dist.vz = d.vx * cp->hd.oBox.radii[2].vx + d.vy * cp->hd.oBox.radii[2].vy + d.vz * cp->hd.oBox.radii[2].vz;
	if (dist.vz < 0)
		dist.vz = -dist.vz;

	oboxLenSq = cp->hd.oBox.length[0];

	if (dist.vx < oboxLenSq * oboxLenSq)
	{
		oboxLenSq = cp->hd.oBox.length[1];

		if (dist.vy < oboxLenSq * oboxLenSq)
		{
			oboxLenSq = cp->hd.oBox.length[2];

			if (dist.vz < oboxLenSq * oboxLenSq)
				return;
		}
	}

	pos.vx -= camera_position.vx;
	pos.vy -= camera_position.vy;
	pos.vz -= camera_position.vz;

	Apply_Inv_CameraMatrix(&pos);

	num_cars_drawn++;

#ifdef PSX
	MulMatrix0(&inv_camera_matrix, &cp->hd.drawCarMat, &workmatrix);
#else
	MulMatrix0(&inv_camera_matrix, &visualDrawMatrix, &workmatrix);
#endif

	// [A] there was mini cars cheat
	// we need full blown mini cars with physics support
	if (ActiveCheats.cheat13 != 0)
	{
		int i;
		for (i = 0; i < 3; i++)
		{
			workmatrix.m[i][0] >>= 1;
			workmatrix.m[i][1] >>= 1;
			workmatrix.m[i][2] >>= 1;
		}
	}

	// to check if car is flipped
	yVal = cp->hd.where.m[1][1];

	// LOD switching
	if (pos.vz <= CAR_LOD_SWITCH_DISTANCE && gForceLowDetailCars == 0 || cp->controlType == CONTROL_TYPE_PLAYER)
	{
#ifdef PSX
		int doSmoke = 0;

		WheelSpeed = cp->hd.speed * 8192;
		maxDamage = MaxPlayerDamage[0];

		if (cp->controlType == CONTROL_TYPE_PLAYER)
		{
			if(*cp->ai.padid >= 0 && *cp->ai.padid < 2)
				maxDamage = MaxPlayerDamage[*cp->ai.padid];
		}

		if (cp->totalDamage >= maxDamage)
		{
			if (WheelSpeed + 59999U < 119999)
				AddFlamingEngine(cp);

			doSmoke = 2;
		}
		else
		{
			if (cp->ap.damage[0] > 2000 || cp->ap.damage[1] > 2000)
			{
				if (cp->ap.damage[0] > 3000 || cp->ap.damage[1] > 3000)
					doSmoke = 2;
				else
					doSmoke = 1;
			}
		}

		if (doSmoke && WheelSpeed + 399999U < 1199999)
			AddSmokingEngine(cp, doSmoke - 1, WheelSpeed);

#if ENABLE_GAME_ENCHANCEMENTS
		AddExhaustSmoke(cp, doSmoke > 1, WheelSpeed);
#endif

#endif

#ifdef PSX
		SetShadowPoints(cp, corners);
#else
		SetShadowPointsForMatrix(&visualPose, model, corners);
#endif
		PlaceShadowForCar(corners, 4, 10, yVal < 0 ? 0 : 2);

		ComputeCarLightingLevels(cp, 1);

		gTempCarUVPtr = gTempHDCarUVDump[cp->id];
		CarModelPtr = &NewCarModel[model];
		CarModelPtr->vlist = gTempCarVertDump[cp->id];
		CarModelPtr->nlist = gTempCarVertDump[cp->id];

		FindCarLightFade(&workmatrix);

		DrawCarObject(CarModelPtr, &workmatrix, &pos, cp->ap.palette, cp, 1);

		if (ActiveCheats.cheat13 != 0)
		{
#ifdef PSX
			MulMatrix0(&inv_camera_matrix, &cp->hd.drawCarMat, &workmatrix);
#else
			MulMatrix0(&inv_camera_matrix, &visualDrawMatrix, &workmatrix);
#endif
		}

		DrawCarWheels(cp, &workmatrix, &pos, view);
	}
	else
	{
		CarModelPtr = &NewLowCarModel[model];
		CarModelPtr->nlist = gTempCarVertDump[cp->id];

		gTempCarUVPtr = gTempLDCarUVDump[cp->id];

		if (pos.vz < 8000)
		{
#ifdef PSX
			SetShadowPoints(cp, corners);
#else
			SetShadowPointsForMatrix(&visualPose, model, corners);
#endif
			PlaceShadowForCar(corners, 0, 0, yVal < 0 ? 0 : 2);
		}

		ComputeCarLightingLevels(cp, 0);

		FindCarLightFade(&workmatrix);

		DrawCarObject(CarModelPtr, &workmatrix, &pos, cp->ap.palette, cp, 0);
	}

	TransparentObject = 0;

	if (cp->controlType == CONTROL_TYPE_PLAYER)
		PlayerCarFX(cp);
	else if (cp->controlType == CONTROL_TYPE_CIV_AI)
		CivCarFX(cp);

	if (gLightsOn && !lightsOnDelay[cp->id])
	{
		if (cp->controlType == CONTROL_TYPE_CIV_AI)
		{
			if (cp->ai.c.thrustState != CIV_AI_THRUST_STOP || (cp->ai.c.ctrlState != CIV_AI_CTRL_PARKED && cp->ai.c.ctrlState != CIV_AI_CTRL_EMPTY && cp->ai.c.ctrlState != CIV_AI_CTRL_STOP_AT_NODE))
				AddNightLights(cp);
		}
		else if (SilenceThisCar(cp->id) == 0)
			AddNightLights(cp);
	}

	if(CarHasSiren(cp->ap.model))
	{
		if ((IS_ROADBLOCK_CAR(cp) || cp->controlType == CONTROL_TYPE_PURSUER_AI) ||		// any regular cop car including roadblock
			gInGameCutsceneActive && cp->controlType == CONTROL_TYPE_CUTSCENE && force_siren[CAR_INDEX(cp)] != 0 ||		// any car with siren in cutscene
			gCurrentMissionNumber == 26 && cp->controlType == CONTROL_TYPE_CUTSCENE && cp->ap.model == SPECIAL_CAR_SLOT)				// Vegas ambulance
		{
			if (cp->ai.p.dying < 75)
				AddCopCarLight(cp);
		}
	}
}

// [D] [T]
char GetCarPalIndex(int tpage)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		if (tpage == carTpages[GameLevel][i])
			return i;
	}

	return 0;
}
