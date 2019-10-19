/************************************************************************************

Filename	:	VrInputDefault.c
Content		:	Handles default controller input
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
extern cvar_t	*cl_movespeedkey;

void Touch_Motion( touchEventType type, int fingerID, float x, float y, float dx, float dy );

float initialTouchX, initialTouchY;


void HandleInput_Alt( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )
{
	//Ensure handedness is set correctly
	const char* pszHand = (vr_control_scheme->integer >= 10) ? "1" : "0";
	Cvar_Set("hand", pszHand);

	static bool dominantGripPushed = false;
	static int grabMeleeWeapon = 0;
	static float dominantGripPushTime = 0.0f;
    static bool selectingWeapon = false;

	//Menu button - always on the left controller
	handleTrackedControllerButton(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, ovrButton_Enter, K_ESCAPE);

	//Menu control - Uses "touch"
	if (useScreenLayer())
	{
        interactWithTouchScreen(pDominantTracking, pDominantTrackedRemoteNew, pDominantTrackedRemoteOld);
	}
	else
	{
		//If distance to off-hand remote is less than 35cm and user pushes grip, then we enable weapon stabilisation
		float distance = sqrtf(powf(pOffTracking->HeadPose.Pose.Position.x - pDominantTracking->HeadPose.Pose.Position.x, 2) +
							   powf(pOffTracking->HeadPose.Pose.Position.y - pDominantTracking->HeadPose.Pose.Position.y, 2) +
							   powf(pOffTracking->HeadPose.Pose.Position.z - pDominantTracking->HeadPose.Pose.Position.z, 2));

		//Turn on weapon stabilisation?
		if ((pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger) !=
			(pOffTrackedRemoteOld->Buttons & ovrButton_GripTrigger)) {

			if (pOffTrackedRemoteNew->Buttons & ovrButton_GripTrigger)
			{
				if (distance < 0.50f)
				{
					Cvar_Set2("vr_weapon_stabilised", "1", true);
				}
			}
			else
			{
				Cvar_Set2("vr_weapon_stabilised", "0", true);
			}
		}

		//dominant hand stuff first
		{
			///Weapon location relative to view
			weaponoffset[0] = pDominantTracking->HeadPose.Pose.Position.x - hmdPosition[0];
			weaponoffset[1] = pDominantTracking->HeadPose.Pose.Position.y - hmdPosition[1];
			weaponoffset[2] = pDominantTracking->HeadPose.Pose.Position.z - hmdPosition[2];

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
			weaponvelocity[0] = pDominantTracking->HeadPose.LinearVelocity.x;
			weaponvelocity[1] = pDominantTracking->HeadPose.LinearVelocity.y;
			weaponvelocity[2] = pDominantTracking->HeadPose.LinearVelocity.z;

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
			const ovrQuatf quatRemote = pDominantTracking->HeadPose.Pose.Orientation;
			QuatToYawPitchRoll(quatRemote, vr_weapon_pitchadjust->value, weaponangles[ADJUSTED]);
			QuatToYawPitchRoll(quatRemote, 0.0f, weaponangles[UNADJUSTED]);
			QuatToYawPitchRoll(quatRemote, -30.0f, weaponangles[MELEE]);


			if (vr_weapon_stabilised->integer &&
				//Don't trigger stabilisation if controllers are close together (holding Glock for example)
				(distance > 0.15f))
			{
				float z = pOffTracking->HeadPose.Pose.Position.z - pDominantTracking->HeadPose.Pose.Position.z;
				float x = pOffTracking->HeadPose.Pose.Position.x - pDominantTracking->HeadPose.Pose.Position.x;
				float y = pOffTracking->HeadPose.Pose.Position.y - pDominantTracking->HeadPose.Pose.Position.y;
				float zxDist = length(x, z);

				if (zxDist != 0.0f && z != 0.0f) {
                    VectorSet(weaponangles[ADJUSTED], degrees(atanf(y / zxDist)), (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]) - degrees(atan2f(x, -z)), weaponangles[ADJUSTED][ROLL]);
                    VectorSet(weaponangles[UNADJUSTED], degrees(atanf(y / zxDist)), (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]) - degrees(atan2f(x, -z)), weaponangles[UNADJUSTED][ROLL]);
                    VectorSet(weaponangles[MELEE], degrees(atanf(y / zxDist)), (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]) - degrees(atan2f(x, -z)), weaponangles[MELEE][ROLL]);
				}
			}
			else
			{
				weaponangles[ADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[ADJUSTED][PITCH] *= -1.0f;

				weaponangles[UNADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[UNADJUSTED][PITCH] *= -1.0f;

				weaponangles[MELEE][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[MELEE][PITCH] *= -1.0f;
			}

			//Use (Action)
			if ((pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) !=
				(pDominantTrackedRemoteOld->Buttons & ovrButton_Joystick)) {

				sendButtonAction("+use", (pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick));
			}

			static bool finishReloadNextFrame = false;
			if (finishReloadNextFrame)
			{
				sendButtonActionSimple("-reload");
				finishReloadNextFrame = false;
			}

			if ((pDominantTrackedRemoteNew->Buttons & ovrButton_GripTrigger) !=
				(pDominantTrackedRemoteOld->Buttons & ovrButton_GripTrigger)) {

				dominantGripPushed = (pDominantTrackedRemoteNew->Buttons &
									  ovrButton_GripTrigger) != 0;

				if (grabMeleeWeapon == 0)
				{
					if (pDominantTracking->Status & VRAPI_TRACKING_STATUS_POSITION_TRACKED) {

						if (dominantGripPushed) {
							dominantGripPushTime = GetTimeInMilliSeconds();
						} else {
							if ((GetTimeInMilliSeconds() - dominantGripPushTime) <
								vr_reloadtimeoutms->integer) {
								sendButtonActionSimple("+reload");
								finishReloadNextFrame = true;
							}
						}
					} else{
						if (dominantGripPushed) {
							//Initiate crowbar from backpack mode
							sendButtonActionSimple("weapon_crowbar");
							int channel = (vr_control_scheme->integer >= 10) ? 0 : 1;
							Android_Vibrate(80, channel, 0.8); // vibrate to let user know they switched
							grabMeleeWeapon = 1;
						}
					}
				} else if (grabMeleeWeapon == 1 && !dominantGripPushed) {
					//Restores last used weapon
					sendButtonActionSimple("lastinv");
					grabMeleeWeapon = 0;
				}
			}
		}

		float controllerYawHeading = 0.0f;
		//off-hand stuff
		{
			flashlightoffset[0] = pOffTracking->HeadPose.Pose.Position.x - hmdPosition[0];
			flashlightoffset[1] = pOffTracking->HeadPose.Pose.Position.y - hmdPosition[1];
			flashlightoffset[2] = pOffTracking->HeadPose.Pose.Position.z - hmdPosition[2];

			vec2_t v;
			rotateAboutOrigin(flashlightoffset[0], flashlightoffset[2], -(cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]), v);
			flashlightoffset[0] = v[0];
			flashlightoffset[2] = v[1];

			QuatToYawPitchRoll(pOffTracking->HeadPose.Pose.Orientation, 15.0f, flashlightangles);

			flashlightangles[YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);

			if (vr_walkdirection->integer == 0) {
				controllerYawHeading = -cl.refdef.cl_viewangles[YAW] + flashlightangles[YAW];
			}
			else
			{
				controllerYawHeading = 0.0f;
			}
		}

		{
			//This section corrects for the fact that the controller actually controls direction of movement, but we want to move relative to the direction the
			//player is facing for positional tracking
			float multiplier = vr_positional_factor->value / (cl_forwardspeed->value *
					((pOffTrackedRemoteNew->Buttons & ovrButton_Trigger) ? cl_movespeedkey->value : 1.0f));

			//If player is ducked then multiply by 3 otherwise positional tracking feels very wrong
			if (ducked != DUCK_NOTDUCKED)
			{
				multiplier *= 3.0f;
			}

			vec2_t v;
			rotateAboutOrigin(-positionDeltaThisFrame[0] * multiplier,
							  positionDeltaThisFrame[2] * multiplier, -hmdorientation[YAW], v);
			positional_movementSideways = v[0];
			positional_movementForward = v[1];

			ALOGV("        positional_movementSideways: %f, positional_movementForward: %f",
				  positional_movementSideways,
				  positional_movementForward);

			//Jump
			handleTrackedControllerButton(pDominantTrackedRemoteNew,
										  pDominantTrackedRemoteOld, domButton2, K_SPACE);

			//We need to record if we have started firing primary so that releasing trigger will stop firing, if user has pushed grip
			//in meantime, then it wouldn't stop the gun firing and it would get stuck
			static bool firingPrimary = false;

			if (!firingPrimary && dominantGripPushed && (GetTimeInMilliSeconds() - dominantGripPushTime) > vr_reloadtimeoutms->integer)
			{
				//Fire Secondary
				if ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) !=
					(pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger)) {

					sendButtonAction("+attack2", (pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger));
				}
			}
			else
			{
				//Fire Primary
				if ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) !=
					(pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger)) {

					firingPrimary = (pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger);
					sendButtonAction("+attack", firingPrimary);
				}
			}

			//Duck
			if ((pDominantTrackedRemoteNew->Buttons & domButton1) !=
				(pDominantTrackedRemoteOld->Buttons & domButton1) &&
				ducked != DUCK_CROUCHED) {
				ducked = (pDominantTrackedRemoteNew->Buttons & domButton1) ? DUCK_BUTTON : DUCK_NOTDUCKED;
				sendButtonAction("+duck", (pDominantTrackedRemoteNew->Buttons & domButton1));
			}

			static bool sendUnAttackNextFrame = false;
			if (sendUnAttackNextFrame)
			{
				sendUnAttackNextFrame = false;
				sendButtonActionSimple("-attack");
			}

            //Weapon Chooser
            static int scrollCount = 0;
            if ((pOffTrackedRemoteNew->Buttons & offButton2) !=
                (pOffTrackedRemoteOld->Buttons & offButton2)) {

                selectingWeapon = (pOffTrackedRemoteNew->Buttons & offButton2) > 0;
                if (selectingWeapon)
                {
                    scrollCount++;
                    sendButtonActionSimple("invnext");
                } else{
                    if (scrollCount == 0)
                    {
                        sendButtonActionSimple("cancelselect");
                    } else{
                        sendUnAttackNextFrame = true;
                        sendButtonActionSimple("+attack");
                    }
                    scrollCount = 0;
                }
            }

            //right and left on left controller
            static bool weaponSwitched = false;
            if (selectingWeapon)
            {
                if (between(0.4f, pDominantTrackedRemoteNew->Joystick.x, 1.0f) ||
                    between(-1.0f, pDominantTrackedRemoteNew->Joystick.x, -0.4f))
                {
                    if (!weaponSwitched) {
                        if (between(0.4f, pDominantTrackedRemoteNew->Joystick.x, 1.0f))
                        {
                            scrollCount++;
                            sendButtonActionSimple("invnext");
                        }
                        else
                        {
                            scrollCount--;
                            sendButtonActionSimple("invprev");
                        }
                        weaponSwitched = true;
                    }
                } else {
                    weaponSwitched = false;
                }
            }
		}

		{
			//Use (Action)
			if ((pOffTrackedRemoteNew->Buttons & ovrButton_Joystick) !=
				(pOffTrackedRemoteOld->Buttons & ovrButton_Joystick)
				&& (pOffTrackedRemoteNew->Buttons & ovrButton_Joystick)) {

				Cvar_SetFloat("vr_lasersight", 1.0f - vr_lasersight->value);

			}

			//Apply a filter and quadratic scaler so small movements are easier to make
			float dist = length(pOffTrackedRemoteNew->Joystick.x, pOffTrackedRemoteNew->Joystick.y);
			float nlf = nonLinearFilter(dist);
			float x = nlf * pOffTrackedRemoteNew->Joystick.x;
			float y = nlf * pOffTrackedRemoteNew->Joystick.y;

			player_moving = (fabs(x) + fabs(y)) > 0.01f;


			//Adjust to be off-hand controller oriented
			vec2_t v;
			rotateAboutOrigin(x, y, controllerYawHeading,v);

			remote_movementSideways = v[0];
			remote_movementForward = v[1];

			ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
				  remote_movementSideways,
				  remote_movementForward);


			//flashlight on/off
			if (((pOffTrackedRemoteNew->Buttons & offButton1) !=
				 (pOffTrackedRemoteOld->Buttons & offButton1)) &&
				(pOffTrackedRemoteOld->Buttons & offButton1)) {
				sendButtonActionSimple("impulse 100");
			}


			//We need to record if we have started firing primary so that releasing trigger will stop definitely firing, if user has pushed grip
			//in meantime, then it wouldn't stop the gun firing and it would get stuck
			static bool firingPrimary = false;

			//Run
			handleTrackedControllerButton(pOffTrackedRemoteNew,
										  pOffTrackedRemoteOld,
										  ovrButton_Trigger, K_SHIFT);

			static bool increaseSnap = true;
            if (!selectingWeapon) {
				if (pDominantTrackedRemoteNew->Joystick.x > 0.6f)
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
				}
				else if (pDominantTrackedRemoteNew->Joystick.x < 0.4f) 
				{
					increaseSnap = true;
				}

				static bool decreaseSnap = true;
				if (pDominantTrackedRemoteNew->Joystick.x < -0.6f)
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
				} 
				else if (pDominantTrackedRemoteNew->Joystick.x > -0.4f)
				{
					decreaseSnap = true;
				}
			}
		}
	}

	//Save state
	leftTrackedRemoteState_old = leftTrackedRemoteState_new;
	rightTrackedRemoteState_old = rightTrackedRemoteState_new;
}