#ifndef VRINPUT_H
#define VRINPUT_H

#include "VrCommon.h"

//New control scheme definitions to be defined L1VR_SurfaceView.c enumeration
enum control_scheme;

#define YAWDRIFTPERFRAME 0.0026F;

#define SCOPE_ENGAGE_DISTANCE 0.28

extern ovrInputStateTrackedRemote leftTrackedRemoteState_old;
extern ovrInputStateTrackedRemote leftTrackedRemoteState_new;
extern ovrTracking leftRemoteTracking_new;

extern ovrInputStateTrackedRemote rightTrackedRemoteState_old;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_new;
extern ovrTracking rightRemoteTracking_new;

extern ovrDeviceID controllerIDs[2];

extern float remote_movementSideways;
extern float remote_movementForward;
extern float positional_movementSideways;
extern float positional_movementForward;
extern float snapTurn;

extern int hmdType;

void sendButtonAction(const char* action, long buttonDown);
void sendButtonActionSimple(const char* action);

void interactWithTouchScreen(ovrTracking *tracking, ovrInputStateTrackedRemote *newState, ovrInputStateTrackedRemote *oldState);

void acquireTrackedRemotesData(const ovrMobile *Ovr, double displayTime);


void HandleInput_Default( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 );
void HandleInput_Alt( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                       ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                       int domButton1, int domButton2, int offButton1, int offButton2 );
void HandleInput_Alt2( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                      ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                      int domButton1, int domButton2, int offButton1, int offButton2 );
void HandleInput_OneController( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                      ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                      int domButton1, int domButton2, int offButton1, int offButton2 );

extern bool stabiliseScope;
void updateScopeAngles(float forwardYaw);

bool isBackpack(ovrTracking* pTracking);

extern char * g_pszBackpackWeapon;

#endif //VRINPUT_H

