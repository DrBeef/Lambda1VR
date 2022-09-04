/************************************************************************************

Filename	:	VrInputAlt2.c
Content		:	Handles controller input for the second alternative
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
#include "../Xash3D/xash3d/engine/client/client.h"

extern cvar_t	*cl_forwardspeed;
extern cvar_t	*cl_movespeedkey;

void Touch_Motion( touchEventType type, int fingerID, float x, float y, float dx, float dy );

extern float initialTouchX, initialTouchY;


void HandleInput_OneController( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )
{
	//Ensure handedness is set correctly
	const char* pszHand = (vr_control_scheme->integer >= 10) ? "1" : "0";
	Cvar_Set("hand", pszHand);


	static bool shifted = false;
	static bool dominantGripPushed = false;
	static float dominantGripPushTime = 0.0f;

	//Menu button - always on the left controller - unavoidable
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

		float distanceToHMD = sqrtf(powf(hmdPosition[0] - pDominantTracking->HeadPose.Pose.Position.x, 2) +
									powf(hmdPosition[1] - pDominantTracking->HeadPose.Pose.Position.y, 2) +
									powf(hmdPosition[2] - pDominantTracking->HeadPose.Pose.Position.z, 2));

		static float forwardYaw = 0;
		if (!isScopeEngaged())
		{
			forwardYaw = (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]) - snapTurn;
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
			QuatToYawPitchRoll(quatRemote, vr_crowbar_pitchadjust->value, weaponangles[MELEE]);

			weaponangles[ADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
			weaponangles[ADJUSTED][PITCH] *= -1.0f;

			weaponangles[UNADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
			weaponangles[UNADJUSTED][PITCH] *= -1.0f;

			weaponangles[MELEE][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
			weaponangles[MELEE][PITCH] *= -1.0f;

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

				if (!isBackpack(pDominantTracking)) {

					if (dominantGripPushed) {
						dominantGripPushTime = GetTimeInMilliSeconds();
					} else {
						if ((GetTimeInMilliSeconds() - dominantGripPushTime) <
							vr_reloadtimeoutms->integer) {
							sendButtonActionSimple("+reload");
							finishReloadNextFrame = true;
						}
					}
				}
			}
		}

		float controllerYawHeading = 0.0f;

		//still use off hand controller for flash light
        if (vr_headtorch->value == 1.0f)
        {
            VectorSet(flashlightoffset, 0, 0, 0);
            VectorCopy(hmdorientation, flashlightangles);
            VectorCopy(hmdorientation, offhandangles);
        }
        else
        {
            flashlightoffset[0] = pOffTracking->HeadPose.Pose.Position.x - hmdPosition[0];
            flashlightoffset[1] = pOffTracking->HeadPose.Pose.Position.y - hmdPosition[1];
            flashlightoffset[2] = pOffTracking->HeadPose.Pose.Position.z - hmdPosition[2];

            vec2_t v;
            rotateAboutOrigin(flashlightoffset[0], flashlightoffset[2],
                              -(cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]), v);
            flashlightoffset[0] = v[0];
            flashlightoffset[2] = v[1];

            QuatToYawPitchRoll(pOffTracking->HeadPose.Pose.Orientation, 0.0f, offhandangles);
            QuatToYawPitchRoll(pOffTracking->HeadPose.Pose.Orientation, 15.0f, flashlightangles);
        }

        flashlightangles[YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
        offhandangles[YAW] = flashlightangles[YAW];

        controllerYawHeading = 0.0f;


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

            shifted = (dominantGripPushed &&
                (GetTimeInMilliSeconds() - dominantGripPushTime) > vr_reloadtimeoutms->integer);

            static bool using = false;
            static bool weaponSwitched = false;
            if (shifted && !using) {
                if (between(0.75f, pDominantTrackedRemoteNew->Joystick.y, 1.0f) ||
                    between(-1.0f, pDominantTrackedRemoteNew->Joystick.y, -0.75f) ||
                    between(0.75f, pDominantTrackedRemoteNew->Joystick.x, 1.0f) ||
                    between(-1.0f, pDominantTrackedRemoteNew->Joystick.x, -0.75f))
                {
                    if (!weaponSwitched) {
                        if (!weaponSwitched)
                        {
                            if (between(0.75f, pDominantTrackedRemoteNew->Joystick.y, 1.0f))
                            {
                                sendButtonActionSimple("invprev"); // in this mode is makes more sense to select the next item with Joystick down; this is the mouse wheel behaviour in hl
                            }
                            else if (between(-1.0f, pDominantTrackedRemoteNew->Joystick.y, -0.75f))
                            {
                                sendButtonActionSimple("invnext");
                            }
                            else if (between(0.75f, pDominantTrackedRemoteNew->Joystick.x, 1.0f))
                            {
                                sendButtonActionSimple("invnextslot"); // not an original hl methode -> needs update from hlsdk-xash3d
                            }
                            else if (between(-1.0f, pDominantTrackedRemoteNew->Joystick.x, -0.75f))
                            {
                                sendButtonActionSimple("invprevslot");
                            }
                            weaponSwitched = true;
                        }
                        weaponSwitched = true;
                    }
                }
                else {
                    weaponSwitched = false;
                }
            }
            else {

                static float joyx[4] = {0};
                static float joyy[4] = {0};
                for (int j = 3; j > 0; --j) {
                    joyx[j] = joyx[j - 1];
                    joyy[j] = joyy[j - 1];
                }
                joyx[0] = pDominantTrackedRemoteNew->Joystick.x;
                joyy[0] = pDominantTrackedRemoteNew->Joystick.y;
                float joystickX = (joyx[0] + joyx[1] + joyx[2] + joyx[3]) / 4.0f;
                float joystickY = (joyy[0] + joyy[1] + joyy[2] + joyy[3]) / 4.0f;

                //Apply a filter and quadratic scaler so small movements are easier to make
                float dist = length(joystickX,
                                    joystickY);
                float nlf = nonLinearFilter(dist);
                float x = nlf * joystickX;
                float y = nlf * joystickY;

                player_moving = (fabs(x) + fabs(y)) > 0.01f;


                //Adjust to be off-hand controller oriented
                vec2_t v;
                rotateAboutOrigin(x, y, controllerYawHeading, v);

                remote_movementSideways = v[0];
                remote_movementForward = v[1];

                ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
                      remote_movementSideways,
                      remote_movementForward);
            }

            //Fire Primary
            if ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) !=
                (pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger)) {

                bool firingPrimary = (pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger);
                sendButtonAction("+attack", firingPrimary);
            }

            //Laser Sight
            if ((pOffTrackedRemoteNew->Buttons & ovrButton_Joystick) !=
                (pOffTrackedRemoteOld->Buttons & ovrButton_Joystick)
                && (pOffTrackedRemoteNew->Buttons & ovrButton_Joystick)) {

                Cvar_SetFloat("vr_lasersight", (int)(vr_lasersight->value + 1) % 3);

            }

            //flashlight on/off
            if (((pOffTrackedRemoteNew->Buttons & offButton2) !=
                 (pOffTrackedRemoteOld->Buttons & offButton2)) &&
                (pOffTrackedRemoteNew->Buttons & offButton2)) {
                sendButtonActionSimple("impulse 100");
            }

            static int  jumpMode = 0;
            static bool running = false;
            static int jumpTimer = 0;
            static bool firingSecondary = false;
            static bool duckToggle = false;
            if (shifted) {
                //If we were in the process of firing secondary, then just stop, otherwise it
                //can get confused
                if (firingSecondary)
                {
                    firingSecondary = false;
                    sendButtonActionSimple("-attack2");
                }

                jumpTimer = 0;
                //Switch Jump Mode
                if (((pDominantTrackedRemoteNew->Buttons & domButton1) !=
                     (pDominantTrackedRemoteOld->Buttons & domButton1)) &&
                    (pDominantTrackedRemoteNew->Buttons & domButton1)) {
                    jumpMode = 1 - jumpMode;
                    if (jumpMode == 0) {
                        CL_CenterPrint("JumpMode:  Jump {-> Duck}", -1);
                    } else {
                        CL_CenterPrint("JumpMode:  Duck -> Jump", -1);
                    }

                }

                //Run - toggle
                if ((pDominantTrackedRemoteNew->Buttons & domButton2) !=
                    (pDominantTrackedRemoteOld->Buttons & domButton2) &&
                    (pDominantTrackedRemoteNew->Buttons & domButton2)) {
                    running = !running;
                    if (running) {
                        CL_CenterPrint("Run Toggle: Enabled", -1);
                    } else {
                        CL_CenterPrint("Run Toggle: Disabled", -1);
                    }
                }

                //Use (Action)
                if ((pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) !=
                    (pDominantTrackedRemoteOld->Buttons & ovrButton_Joystick)) {

                    using = (pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick);
                    sendButtonAction("+use", (pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick));
                }
            }
            else
            {
                using = false;

                //Fire Secondary
                if ((pDominantTrackedRemoteNew->Buttons & domButton2) !=
                    (pDominantTrackedRemoteOld->Buttons & domButton2)) {

                    firingSecondary = (pDominantTrackedRemoteNew->Buttons & domButton2) > 0;
                    sendButtonAction("+attack2", (pDominantTrackedRemoteNew->Buttons & domButton2));
                }

                //Start of Jump logic - initialise timer
                if (((pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) !=
                     (pDominantTrackedRemoteOld->Buttons & ovrButton_Joystick)))
                {
                    if (pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick)
                    {
                        jumpTimer = GetTimeInMilliSeconds();
                    }
                }

                if (jumpMode == 0) {

                    //Jump Mode 0:  Jump -> {Duck}
					sendButtonAction("+jump", pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick);

                    //We are in jump macro
                    if (jumpTimer != 0) {
                        if (GetTimeInMilliSeconds() - jumpTimer > 250) {
                            duckToggle = false; // Control of ducking now down to this

                            ducked = (pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick) ? DUCK_BUTTON : DUCK_NOTDUCKED;
                            sendButtonAction("+duck", (pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick));
                        }

                        if (!(pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick)) {
                            jumpTimer = 0;
                        }
                    }

                } else {
                    //We are in jump macro
                    if (jumpTimer != 0) {
                        duckToggle = false; // Control of ducking now down to this

                        //Jump Mode 1:  Duck -> Jump
                        ducked = (pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick)
                                 ? DUCK_BUTTON : DUCK_NOTDUCKED;
                        sendButtonAction("+duck", (pDominantTrackedRemoteNew->Buttons &
                                                   ovrButton_Joystick));

                        if (GetTimeInMilliSeconds() - jumpTimer > 250) {
							sendButtonAction("+jump", pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick);
						}

                        if (!(pDominantTrackedRemoteNew->Buttons & ovrButton_Joystick)) {
                            jumpTimer = 0;
                        }
                    }
                }

                if (jumpTimer == 0) {
                    //Duck
                    if (((pDominantTrackedRemoteNew->Buttons & domButton1) != (pDominantTrackedRemoteOld->Buttons & domButton1)) &&
                        (pDominantTrackedRemoteOld->Buttons & domButton1) &&
                        ducked != DUCK_CROUCHED) {
                        ducked = (pDominantTrackedRemoteNew->Buttons & domButton1) ? DUCK_BUTTON
                                                                                   : DUCK_NOTDUCKED;

                        duckToggle = !duckToggle;
                        sendButtonAction("+duck", duckToggle ? 1 : 0);
                    }
                }
            }

            sendButtonAction("+speed", running ? 1 : 0);
		}
	}

	//Save state
	rightTrackedRemoteState_old = rightTrackedRemoteState_new;
	leftTrackedRemoteState_old = leftTrackedRemoteState_new;
}