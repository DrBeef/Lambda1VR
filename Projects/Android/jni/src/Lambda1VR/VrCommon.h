
#ifndef VRCOMMON_H
#define VRCOMMON_H

#include <xash3d_types.h>

#include <android/log.h>

#include "TBXR_Common.h"

#ifndef NDEBUG
#define DEBUG 1
#endif

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

extern ovrInputStateTrackedRemote leftTrackedRemoteState_old;
extern ovrInputStateTrackedRemote leftTrackedRemoteState_new;
extern ovrTrackedController leftRemoteTracking_new;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_old;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_new;
extern ovrTrackedController rightRemoteTracking_new;

#define PITCH 0
#define YAW 1
#define ROLL 2

extern bool xash_initialised;

extern float playerHeight;
extern float playerYaw;

extern bool showingScreenLayer;
extern float vrFOV;

extern vec3_t worldPosition;

extern vec3_t hmdPosition;
extern vec3_t hmdorientation;
extern vec3_t positionDeltaThisFrame;

#define ADJUSTED 0
#define UNADJUSTED 1
#define MELEE 2
extern vec3_t weaponangles[3];
extern vec3_t weaponoffset;
extern vec3_t weaponvelocity;

extern vec3_t offhandangles;

extern vec3_t flashlightangles;
extern vec3_t flashlightoffset;

#define DUCK_NOTDUCKED 0
#define DUCK_BUTTON 1
#define DUCK_CROUCHED 2
extern int ducked;

extern bool player_moving;

bool isMultiplayer();
bool isScopeEngaged();
bool isPlayerDead();

float length(float x, float y);
float nonLinearFilter(float in);
bool between(float min, float val, float max);

#endif //VRCOMMON_H
