#ifndef CARS_H
#define CARS_H

#if defined(_WIN32) && !defined(PSX)
#include <vector>
#endif


#define CAR_INDEX(cp)  (int)(cp-car_data)

#define IS_ROADBLOCK_CAR(cp) (cp->controlType == CONTROL_TYPE_CIV_AI && (cp->controlFlags & CONTROL_FLAG_COP_SLEEPING))

// PHYSICS
extern CAR_DATA car_data[MAX_CARS + 2];	// all cars + Tanner cbox + Camera cbox

// active cars
extern CAR_DATA* active_car_list[MAX_CARS];
extern unsigned char lightsOnDelay[MAX_CARS];

extern CAR_MODEL NewCarModel[MAX_CAR_RESIDENT_MODELS];
extern CAR_MODEL NewLowCarModel[MAX_CAR_RESIDENT_MODELS];

extern MODEL* gCarLowModelPtr[MAX_CAR_RESIDENT_MODELS];
extern MODEL* gCarDamModelPtr[MAX_CAR_RESIDENT_MODELS];
extern MODEL* gCarCleanModelPtr[MAX_CAR_RESIDENT_MODELS];

extern int whichCP;		// car poly counter
extern int baseSpecCP;	// special car poly counter

extern SVECTOR gTempCarVertDump[MAX_CARS][MAX_DENTING_VERTS];
extern DENTUVS gTempHDCarUVDump[MAX_CARS][MAX_DENTING_UVS];
extern DENTUVS gTempLDCarUVDump[MAX_CARS][MAX_DENTING_LOD_UVS];

extern MODEL *gHubcapModelPtr;
extern MODEL *gCleanWheelModelPtr;
extern MODEL *gFastWheelModelPtr;
extern MODEL *gDamWheelModelPtr;

extern short FrontWheelRotation[MAX_CARS];
extern short BackWheelRotation[MAX_CARS];

extern char LeftLight;
extern char RightLight;

extern u_short civ_clut[8][32][6];

extern void StepCarPresentation();
extern void DrawCar(CAR_DATA *cp, int view); // 0x000210B8

#ifndef PSX
// [A] loads car model from file
char* LoadCarModelFromFile(char* dest, int modelNumber, int type);
#ifdef _WIN32
struct WorldMeshConversionInput;
struct NativeCarSourceIdentity;
struct NativeCarBodyCapture;
struct NativeWheelSourceIdentity;
struct NativeWheelCapture;
bool Cars_GetNativeSourceIdentity(int carId, NativeCarSourceIdentity* out);
bool Cars_CaptureNativeBody(int carId, bool lowDetail, NativeCarBodyCapture* out, char* error, int capacity);
bool Cars_GetNativeBodyLowDetail(int carId, bool* lowDetail);
bool Cars_CaptureNativeDamage(int carId, const NativeCarBodyCapture& capture,
	std::vector<float>* xyz, std::vector<float>* uvs, char* error, int capacity);
bool Cars_GetNativeTransform(int carId, float out[16]);
bool Cars_GetNativeWheelSource(int carId, int wheelSlot, NativeWheelSourceIdentity* out);
bool Cars_CaptureNativeWheel(int carId, int wheelSlot, NativeWheelCapture* out, char* error, int capacity);
bool Cars_GetNativeWheelPose(int carId, int wheelSlot, float transform[16], int* spinSin, int* spinCos);
#endif
bool Cars_ExportInspectorModel(int carId, const char* modId, char* status, int statusCapacity);
bool Cars_ConvertInspectorModel(int carId, const char* modId, char* status, int statusCapacity);
#endif

extern MODEL* GetCarModel(char *src, char **dest, int KeepNormals); // 0x00065134
extern void startBuildNewCars(int isSpecial); // 0x00022860
extern void buildNewCarFromModel(int index, int detail, char* polySrc, MODEL* model); // 0x00022960

extern void MangleWheelModels(); // 0x000230C8

extern char GetCarPalIndex(int tpage); // 0x00023390


#endif
