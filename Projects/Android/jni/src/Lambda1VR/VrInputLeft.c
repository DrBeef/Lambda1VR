/************************************************************************************

Filename	:	VrInputRight.c 
Content		:	Handles controller input for the left-handed
Created		:	August 2019
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
#include "VrCvars.h"

extern cvar_t	*cl_forwardspeed;

int IN_TouchEvent( touchEventType type, int fingerID, float x, float y, float dx, float dy );
void Touch_Motion( touchEventType type, int fingerID, float x, float y, float dx, float dy );

float initialTouchX, initialTouchY;


void HandleInput_Left( ovrMobile * Ovr, double displayTime )
{
	//The amount of yaw changed by controller
	//TODO: fixme
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
				} else{
					leftTrackedRemoteState_new = trackedRemoteState;
					leftRemoteTracking_new = remoteTracking;
				}
			}
		}
	}

	static bool dominantGripPushed = false;
	static float dominantGripPushTime = 0.0f;

	//Show screen view (if in multiplayer toggle scoreboard)
	if (((rightTrackedRemoteState_new.Buttons & ovrButton_B) !=
		 (rightTrackedRemoteState_old.Buttons & ovrButton_B)) &&
		(rightTrackedRemoteState_new.Buttons & ovrButton_B)) {

		showingScreenLayer = !showingScreenLayer;

		//Check we are in multiplayer
		if (CL_GetMaxClients() > 1) {
			sendButtonAction("+showscores", showingScreenLayer);
		}
	}

	//Menu button
	handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, ovrButton_Enter, K_ESCAPE);

	//Menu control - Uses "touch"
	if (useScreenLayer())
	{
		float remoteAngles[3];
		QuatToYawPitchRoll(leftRemoteTracking_new.HeadPose.Pose.Orientation, remoteAngles);
		float yaw = remoteAngles[YAW] - playerYaw;

		//Adjust for maximum yaw values
		if (yaw >= 180.0f) yaw -= 180.0f;
		if (yaw <= -180.0f) yaw += 180.0f;

		if (yaw > -40.0f && yaw < 40.0f &&
			remoteAngles[PITCH] > -22.5f && remoteAngles[PITCH] < 22.5f) {

			int newRemoteTrigState = (leftTrackedRemoteState_new.Buttons & ovrButton_Trigger) != 0;
			int prevRemoteTrigState = (leftTrackedRemoteState_old.Buttons & ovrButton_Trigger) != 0;

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
	else
	{
		static bool weaponStabilisation = false;

		//If distance to off-hand remote is less than 35cm and user pushes grip, then we enable weapon stabilisation
		float distance = sqrtf(powf(rightRemoteTracking_new.HeadPose.Pose.Position.x - leftRemoteTracking_new.HeadPose.Pose.Position.x, 2) +
							   powf(rightRemoteTracking_new.HeadPose.Pose.Position.y - leftRemoteTracking_new.HeadPose.Pose.Position.y, 2) +
							   powf(rightRemoteTracking_new.HeadPose.Pose.Position.z - leftRemoteTracking_new.HeadPose.Pose.Position.z, 2));

		//Turn on weapon stabilisation?
		if ((rightTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) !=
			(rightTrackedRemoteState_old.Buttons & ovrButton_GripTrigger)) {

			if (rightTrackedRemoteState_new.Buttons & ovrButton_GripTrigger)
			{
				if (distance < 0.50f)
				{
					weaponStabilisation = true;
				}
			}
			else
			{
				weaponStabilisation = false;
			}
		}

		//dominant hand stuff first
		{
			///Weapon location relative to view
			weaponoffset[0] = leftRemoteTracking_new.HeadPose.Pose.Position.x - hmdPosition[0];
			weaponoffset[1] = leftRemoteTracking_new.HeadPose.Pose.Position.y - hmdPosition[1];
			weaponoffset[2] = leftRemoteTracking_new.HeadPose.Pose.Position.z - hmdPosition[2];

			{
				vec2_t v;
				rotateAboutOrigin(weaponoffset[0], weaponoffset[2], -(cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]), v);
				weaponoffset[0] = v[0];
				weaponoffset[2] = v[1];

				ALOGV("        Weapon Offset: %f, %f, %f",
					  weaponoffset[0],
					  weaponoffset[1],
					  weaponoffset[2]);
			}

			//Weapon velocity
			weaponvelocity[0] = leftRemoteTracking_new.HeadPose.LinearVelocity.x;
			weaponvelocity[1] = leftRemoteTracking_new.HeadPose.LinearVelocity.y;
			weaponvelocity[2] = leftRemoteTracking_new.HeadPose.LinearVelocity.z;

			{
				vec2_t v;
				rotateAboutOrigin(weaponvelocity[0], weaponvelocity[2], -cl.refdef.cl_viewangles[YAW], v);
				weaponvelocity[0] = v[0];
				weaponvelocity[2] = v[1];

				ALOGV("        Weapon Velocity: %f, %f, %f",
					  weaponvelocity[0],
					  weaponvelocity[1],
					  weaponvelocity[2]);
			}


			//Set gun angles
			const ovrQuatf quatRemote = leftRemoteTracking_new.HeadPose.Pose.Orientation;
			QuatToYawPitchRoll(quatRemote, weaponangles);


			if (weaponStabilisation &&
				//Don't trigger stabilisation if controllers are close together (holding Glock for example)
				(distance > 0.15f))
			{
				float z = rightRemoteTracking_new.HeadPose.Pose.Position.z - leftRemoteTracking_new.HeadPose.Pose.Position.z;
				float x = rightRemoteTracking_new.HeadPose.Pose.Position.x - leftRemoteTracking_new.HeadPose.Pose.Position.x;
				float y = rightRemoteTracking_new.HeadPose.Pose.Position.y - leftRemoteTracking_new.HeadPose.Pose.Position.y;
				float zxDist = length(x, z);

				if (zxDist != 0.0f && z != 0.0f) {
					weaponangles[YAW] = (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]) - degrees(atan2f(x, -z));
					weaponangles[PITCH] = degrees(atanf(y / zxDist));
				}
			}
			else
			{
				weaponangles[YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[PITCH] *= -1.0f;

				//Slight gun angle adjustment
				weaponangles[PITCH] += vr_weapon_pitchadjust->value;
			}

			//Use (Action)
			if ((leftTrackedRemoteState_new.Buttons & ovrButton_Joystick) !=
				(leftTrackedRemoteState_old.Buttons & ovrButton_Joystick)) {

				sendButtonAction("+use", (leftTrackedRemoteState_new.Buttons & ovrButton_Joystick));
			}

			static bool finishReloadNextFrame = false;
			if (finishReloadNextFrame)
			{
				sendButtonActionSimple("-reload");
				finishReloadNextFrame = false;
			}

			if ((leftTrackedRemoteState_new.Buttons & ovrButton_GripTrigger) !=
				(leftTrackedRemoteState_old.Buttons & ovrButton_GripTrigger)) {

				dominantGripPushed = (leftTrackedRemoteState_new.Buttons & ovrButton_GripTrigger);

				if (dominantGripPushed)
				{
					dominantGripPushTime = GetTimeInMilliSeconds();
				}
				else
				{
					if ((GetTimeInMilliSeconds() - dominantGripPushTime) < vr_reloadtimeoutms->integer)
					{
						sendButtonActionSimple("+reload");
						finishReloadNextFrame = true;
					}
				}
			}
		}

		float controllerYawHeading = 0.0f;
		//off-hand stuff
		{
			flashlightoffset[0] = rightRemoteTracking_new.HeadPose.Pose.Position.x - hmdPosition[0];
			flashlightoffset[1] = rightRemoteTracking_new.HeadPose.Pose.Position.y - hmdPosition[1];
			flashlightoffset[2] = rightRemoteTracking_new.HeadPose.Pose.Position.z - hmdPosition[2];

			QuatToYawPitchRoll(rightRemoteTracking_new.HeadPose.Pose.Orientation, flashlightangles);

			flashlightangles[YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);

			if (vr_walkdirection->integer == 0) {
				controllerYawHeading = -cl.refdef.cl_viewangles[YAW] + flashlightangles[YAW];
			}
			else
			{
				controllerYawHeading = 0.0f;
			}
		}

		//Right-hand specific stuff
		{
			ALOGV("        Right-Controller-Position: %f, %f, %f",
				  leftRemoteTracking_new.HeadPose.Pose.Position.x,
				  leftRemoteTracking_new.HeadPose.Pose.Position.y,
				  leftRemoteTracking_new.HeadPose.Pose.Position.z);

			//This section corrects for the fact that the controller actually controls direction of movement, but we want to move relative to the direction the
			//player is facing for positional tracking
			float multiplier = vr_positional_factor->value / cl_forwardspeed->value;

			vec2_t v;
			rotateAboutOrigin(-positionDeltaThisFrame[0] * multiplier,
							  positionDeltaThisFrame[2] * multiplier, -hmdorientation[YAW], v);
			positional_movementSideways = v[0];
			positional_movementForward = v[1];

			ALOGV("        positional_movementSideways: %f, positional_movementForward: %f",
				  positional_movementSideways,
				  positional_movementForward);

			//Jump (Y Button)
			handleTrackedControllerButton(&leftTrackedRemoteState_new,
										  &leftTrackedRemoteState_old, ovrButton_Y, K_SPACE);

			//We need to record if we have started firing primary so that releasing trigger will stop firing, if user has pushed grip
			//in meantime, then it wouldn't stop the gun firing and it would get stuck
			static bool firingPrimary = false;

			if (!firingPrimary && dominantGripPushed && (GetTimeInMilliSeconds() - dominantGripPushTime) > vr_reloadtimeoutms->integer)
			{
				//Fire Secondary
				if ((leftTrackedRemoteState_new.Buttons & ovrButton_Trigger) !=
					(leftTrackedRemoteState_old.Buttons & ovrButton_Trigger)) {

					sendButtonAction("+attack2", (leftTrackedRemoteState_new.Buttons & ovrButton_Trigger));
				}
			}
			else
			{
				//Fire Primary
				if ((leftTrackedRemoteState_new.Buttons & ovrButton_Trigger) !=
					(leftTrackedRemoteState_old.Buttons & ovrButton_Trigger)) {

					firingPrimary = (leftTrackedRemoteState_new.Buttons & ovrButton_Trigger);
					sendButtonAction("+attack", firingPrimary);
				}
			}

			//Duck with X
			if ((leftTrackedRemoteState_new.Buttons & ovrButton_X) !=
				(leftTrackedRemoteState_old.Buttons & ovrButton_X)) {

				sendButtonAction("+duck", (leftTrackedRemoteState_new.Buttons & ovrButton_X));
			}

			//Weapon Chooser
			static bool weaponSwitched = false;
			if (between(-0.2f, leftTrackedRemoteState_new.Joystick.x, 0.2f) &&
				(between(0.8f, leftTrackedRemoteState_new.Joystick.y, 1.0f) ||
				 between(-1.0f, leftTrackedRemoteState_new.Joystick.y, -0.8f)))
			{
				if (!weaponSwitched) {
					if (between(0.8f, leftTrackedRemoteState_new.Joystick.y, 1.0f))
					{
						sendButtonActionSimple("invnext");
					}
					else
					{
						sendButtonActionSimple("invprev");
					}
					weaponSwitched = true;
				}
			} else {
				weaponSwitched = false;
			}
		}

		//Left-hand specific stuff
		{
			ALOGV("        Left-Controller-Position: %f, %f, %f",
				  rightRemoteTracking_new.HeadPose.Pose.Position.x,
				  rightRemoteTracking_new.HeadPose.Pose.Position.y,
				  rightRemoteTracking_new.HeadPose.Pose.Position.z);

			//Use (Action)
			if ((rightTrackedRemoteState_new.Buttons & ovrButton_Joystick) !=
				(rightTrackedRemoteState_old.Buttons & ovrButton_Joystick)
				&& (rightTrackedRemoteState_new.Buttons & ovrButton_Joystick)) {

				Cvar_SetFloat("vr_lasersight", 1.0f - vr_lasersight->value);

			}

			//Apply a filter and quadratic scaler so small movements are easier to make
			float dist = length(rightTrackedRemoteState_new.Joystick.x, rightTrackedRemoteState_new.Joystick.y);
			float nlf = nonLinearFilter(dist);
			float x = nlf * rightTrackedRemoteState_new.Joystick.x;
			float y = nlf * rightTrackedRemoteState_new.Joystick.y;

			//Adjust to be off-hand controller oriented
			vec2_t v;
			rotateAboutOrigin(x, y, controllerYawHeading,v);

			remote_movementSideways = v[0];
			remote_movementForward = v[1];

			ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
				  remote_movementSideways,
				  remote_movementForward);


			//flashlight on/off
			if (((rightTrackedRemoteState_new.Buttons & ovrButton_A) !=
				 (rightTrackedRemoteState_old.Buttons & ovrButton_A)) &&
				(rightTrackedRemoteState_old.Buttons & ovrButton_A)) {
				sendButtonActionSimple("impulse 100");
			}


			//We need to record if we have started firing primary so that releasing trigger will stop definitely firing, if user has pushed grip
			//in meantime, then it wouldn't stop the gun firing and it would get stuck
			static bool firingPrimary = false;

			//Run
			handleTrackedControllerButton(&rightTrackedRemoteState_new,
										  &rightTrackedRemoteState_old,
										  ovrButton_Trigger, K_SHIFT);

			static increaseSnap = true;
			if (leftTrackedRemoteState_new.Joystick.x > 0.6f)
			{
				if (increaseSnap)
				{
					snapTurn -= vr_snapturn_angle->value;
					if (vr_snapturn_angle->value > 10.0f) {
						increaseSnap = false;
					}

					if (snapTurn < -180.0f)
					{
						snapTurn += 360.f;
					}
				}
			} else if (leftTrackedRemoteState_new.Joystick.x < 0.4f) {
				increaseSnap = true;
			}

			static decreaseSnap = true;
			if (leftTrackedRemoteState_new.Joystick.x < -0.6f)
			{
				if (decreaseSnap)
				{
					snapTurn += vr_snapturn_angle->value;

					//If snap turn configured for less than 10 degrees
					if (vr_snapturn_angle->value > 10.0f) {
						decreaseSnap = false;
					}

					if (snapTurn > 180.0f)
					{
						snapTurn -= 360.f;
					}
				}
			} else if (leftTrackedRemoteState_new.Joystick.x > -0.4f)
			{
				decreaseSnap = true;
			}
		}
	}

	//Save state
	leftTrackedRemoteState_old = leftTrackedRemoteState_new;
	rightTrackedRemoteState_old = rightTrackedRemoteState_new;
}