#ifndef VRINPUT_H
#define VRINPUT_H

#include "VrCommon.h"

//New control scheme definitions to be defined L1VR_SurfaceView.c enumeration
enum control_scheme;

#define YAWDRIFTPERFRAME 0.0026F;

#define SCOPE_ENGAGE_DISTANCE 0.28

ovrInputStateTrackedRemote leftTrackedRemoteState_old;
ovrInputStateTrackedRemote leftTrackedRemoteState_new;
ovrTracking leftRemoteTracking_new;

ovrInputStateTrackedRemote rightTrackedRemoteState_old;
ovrInputStateTrackedRemote rightTrackedRemoteState_new;
ovrTracking rightRemoteTracking_new;

ovrDeviceID controllerIDs[2];

float remote_movementSideways;
float remote_movementForward;
float positional_movementSideways;
float positional_movementForward;
float snapTurn;

int hmdType;

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

bool stabiliseScope;
void updateScopeAngles(float forwardYaw);

char * g_pszBackpackWeapon;

#endif //VRINPUT_H

