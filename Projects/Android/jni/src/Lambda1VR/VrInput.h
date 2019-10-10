
#include "VrCommon.h"

//New control scheme definitions to be defined L1VR_SurfaceView.c enumeration
enum control_scheme;

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

void sendButtonAction(const char* action, long buttonDown);
void sendButtonActionSimple(const char* action);

void interactWithTouchScreen(ovrTracking *tracking, ovrInputStateTrackedRemote *newState, ovrInputStateTrackedRemote *oldState);

void acquireTrackedRemotesData(const ovrMobile *Ovr, double displayTime);

void HandleInput_Right( ovrMobile * Ovr, double displayTime );
void HandleInput_RightAlt( ovrMobile * Ovr, double displayTime );
void HandleInput_Left( ovrMobile * Ovr, double displayTime );
void HandleInput_LeftAlt( ovrMobile * Ovr, double displayTime );


