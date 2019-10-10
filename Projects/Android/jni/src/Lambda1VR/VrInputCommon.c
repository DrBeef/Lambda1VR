/************************************************************************************

Filename	:	VrInputRight.c 
Content		:	Handles common controller input functionality
Created		:	September 2019
Authors		:	Simon Brown

*************************************************************************************/

#include <common/common.h>
#include <common/library.h>
#include <common/cvardef.h>
#include <common/xash3d_types.h>
#include <engine/keydefs.h>
#include <client/touch.h>
#include <client/client.h>

#include <VrApi.h>
#include <VrApi_Helpers.h>
#include <VrApi_SystemUtils.h>
#include <VrApi_Input.h>
#include <VrApi_Types.h>

#include "VrInput.h"


void handleTrackedControllerButton(ovrInputStateTrackedRemote * trackedRemoteState, ovrInputStateTrackedRemote * prevTrackedRemoteState, uint32_t button, int key)
{
    if ((trackedRemoteState->Buttons & button) != (prevTrackedRemoteState->Buttons & button))
    {
        Key_Event(key, (trackedRemoteState->Buttons & button) != 0);
    }
}


static void Matrix4x4_Transform (const matrix4x4 *in, const float v[3], float out[3])
{
    out[0] = v[0] * (*in)[0][0] + v[1] * (*in)[0][1] + v[2] * (*in)[0][2] + (*in)[0][3];
    out[1] = v[0] * (*in)[1][0] + v[1] * (*in)[1][1] + v[2] * (*in)[1][2] + (*in)[1][3];
    out[2] = v[0] * (*in)[2][0] + v[1] * (*in)[2][1] + v[2] * (*in)[2][2] + (*in)[2][3];
}

void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out)
{
    vec3_t temp = {0.0f, 0.0f, 0.0f};
    temp[0] = v1;
    temp[1] = v2;

    vec3_t v = {0.0f, 0.0f, 0.0f};
    matrix4x4 matrix;
    vec3_t angles = {0.0f, rotation, 0.0f};
    vec3_t origin = {0.0f, 0.0f, 0.0f};
    Matrix4x4_CreateFromEntity(matrix, angles, origin, 1.0f);
    Matrix4x4_Transform(&matrix, temp, v);

    out[0] = v[0];
    out[1] = v[1];
}

float length(float x, float y)
{
    return sqrtf(powf(x, 2.0f) + powf(y, 2.0f));
}

#define NLF_DEADZONE 0.1
#define NLF_POWER 2.2

float nonLinearFilter(float in)
{
    float val = 0.0f;
    if (in > NLF_DEADZONE)
    {
        val = in;
        val -= NLF_DEADZONE;
        val /= (1.0f - NLF_DEADZONE);
        val = powf(val, NLF_POWER);
    }
    else if (in < -NLF_DEADZONE)
    {
        val = in;
        val += NLF_DEADZONE;
        val /= (1.0f - NLF_DEADZONE);
        val = -powf(fabsf(val), NLF_POWER);
    }

    return val;
}

void sendButtonActionSimple(const char* action)
{
    char command[256];
    Q_snprintf( command, sizeof( command ), "%s\n", action );
    Cbuf_AddText( command );
}

bool between(float min, float val, float max)
{
    return (min < val) && (val < max);
}

void sendButtonAction(const char* action, long buttonDown)
{
    char command[256];
    Q_snprintf( command, sizeof( command ), "%s\n", action );
    if (!buttonDown)
    {
        command[0] = '-';
    }
    Cbuf_AddText( command );

}

void acquireTrackedRemotesData(const ovrMobile *Ovr, double displayTime) {//The amount of yaw changed by controller
    for ( int i = 0; ; i++ ) {
        ovrInputCapabilityHeader cap;
        ovrResult result = vrapi_EnumerateInputDevices(Ovr, i, &cap);
        if (result < 0) {
            break;
        }

        if (cap.Type == ovrControllerType_TrackedRemote) {
            ovrTracking remoteTracking;
            ovrInputStateTrackedRemote trackedRemoteState;
            trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
            result = vrapi_GetCurrentInputState(Ovr, cap.DeviceID, &trackedRemoteState.Header);

            if (result == ovrSuccess) {
                ovrInputTrackedRemoteCapabilities remoteCapabilities;
                remoteCapabilities.Header = cap;
                result = vrapi_GetInputDeviceCapabilities(Ovr, &remoteCapabilities.Header);

                result = vrapi_GetInputTrackingState(Ovr, cap.DeviceID, displayTime,
                                                     &remoteTracking);

                if (remoteCapabilities.ControllerCapabilities & ovrControllerCaps_RightHand) {
                    rightTrackedRemoteState_new = trackedRemoteState;
                    rightRemoteTracking_new = remoteTracking;
                    controllerIDs[1] = cap.DeviceID;
                } else{
                    leftTrackedRemoteState_new = trackedRemoteState;
                    leftRemoteTracking_new = remoteTracking;
                    controllerIDs[0] = cap.DeviceID;
                }
            }
        }
    }
}

int IN_TouchEvent( touchEventType type, int fingerID, float x, float y, float dx, float dy );

float initialTouchX, initialTouchY;

void interactWithTouchScreen(ovrTracking *tracking, ovrInputStateTrackedRemote *newState, ovrInputStateTrackedRemote *oldState) {
    float remoteAngles[3];
    QuatToYawPitchRoll(tracking->HeadPose.Pose.Orientation, 0.0f, remoteAngles);
    float yaw = remoteAngles[YAW] - playerYaw;

    //Adjust for maximum yaw values
    if (yaw >= 180.0f) yaw -= 180.0f;
    if (yaw <= -180.0f) yaw += 180.0f;

    if (yaw > -40.0f && yaw < 40.0f &&
        remoteAngles[PITCH] > -22.5f && remoteAngles[PITCH] < 22.5f) {

        int newRemoteTrigState = (newState->Buttons & ovrButton_Trigger) != 0;
        int prevRemoteTrigState = (oldState->Buttons & ovrButton_Trigger) != 0;

        touchEventType t = event_motion;

        float touchX = (-yaw + 40.0f) / 80.0f;
        float touchY = (remoteAngles[PITCH] + 22.5f) / 45.0f;
        if (newRemoteTrigState != prevRemoteTrigState)
        {
            t = newRemoteTrigState ? event_down : event_up;
            if (newRemoteTrigState)
            {
                initialTouchX = touchX;
                initialTouchY = touchY;
            }
        }

        IN_TouchEvent(t, 0, touchX, touchY, initialTouchX - touchX, initialTouchY - touchY);
    }
}