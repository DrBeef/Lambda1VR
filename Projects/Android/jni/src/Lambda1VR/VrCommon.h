

#include <xash3d_types.h>
#include <VrApi_Ext.h>
#include <VrApi_Input.h>

#include <android/log.h>

#define LOG_TAG "Lambda1VR"

#ifndef NDEBUG
#define DEBUG 1
#endif

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

#define PITCH 0
#define YAW 1
#define ROLL 2

bool xash_initialised;

float playerHeight;
float playerYaw;

bool showingScreenLayer;
float vrFOV;

vec3_t worldPosition;

vec3_t hmdPosition;
vec3_t hmdorientation;
vec3_t positionDeltaThisFrame;

vec3_t weaponangles;
vec3_t weaponoffset;
vec3_t weaponvelocity;

vec3_t flashlightangles;
vec3_t flashlightoffset;

float radians(float deg);
float degrees(float rad);
bool isMultiplayer();
double GetTimeInMilliSeconds();
float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);
void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out);
void QuatToYawPitchRoll(ovrQuatf q, vec3_t out);
bool useScreenLayer();
