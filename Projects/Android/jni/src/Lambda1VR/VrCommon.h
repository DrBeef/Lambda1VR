
#ifndef VRCOMMON_H
#define VRCOMMON_H

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

#define ADJUSTED 0
#define UNADJUSTED 1
#define MELEE 2
vec3_t weaponangles[3];
vec3_t weaponoffset;
vec3_t weaponvelocity;

vec3_t offhandangles;

vec3_t flashlightangles;
vec3_t flashlightoffset;

#define DUCK_NOTDUCKED 0
#define DUCK_BUTTON 1
#define DUCK_CROUCHED 2
int ducked;

bool player_moving;

bool isMultiplayer();
bool isScopeEngaged();
bool isPlayerDead();


float radians(float deg);
float degrees(float rad);
double GetTimeInMilliSeconds();
float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);
void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out);
void QuatToYawPitchRoll(ovrQuatf q, float pitchAdjust, vec3_t out);
bool useScreenLayer();
void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key);
void Android_Vibrate( float duration, int channel, float intensity );

#endif //VRCOMMON_H
