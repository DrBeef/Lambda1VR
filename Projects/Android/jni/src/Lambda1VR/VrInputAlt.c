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

extern float initialTouchX, initialTouchY;


void HandleInput_Alt( ovrInputStateTrackedRemote *pDominantTrackedRemoteNew, ovrInputStateTrackedRemote *pDominantTrackedRemoteOld, ovrTracking* pDominantTracking,
                          ovrInputStateTrackedRemote *pOffTrackedRemoteNew, ovrInputStateTrackedRemote *pOffTrackedRemoteOld, ovrTracking* pOffTracking,
                          int domButton1, int domButton2, int offButton1, int offButton2 )
{
	//Ensure handedness is set correctly
	const char* pszHand = (vr_control_scheme->integer >= 10) ? "1" : "0";
	Cvar_Set("hand", pszHand);

	static bool dominantGripPushed = false;
	static int grabMeleeWeapon = 0;
	static bool canUseBackpack = false;
	static float dominantGripPushTime = 0.0f;
    static bool selectingWeapon = false;

	static bool canUseQuickSave = false;
	if (!isBackpack(pOffTracking)) {
		canUseQuickSave = false;
	}
	else if (!canUseQuickSave) {
		int channel = (vr_control_scheme->integer >= 10) ? 1 : 0;
		if (vr_controller_tracking_haptic->value == 1.0f) {
			Android_Vibrate(40, channel,
							0.5); // vibrate to let user know they can switch
		}
		canUseQuickSave = true;
	}

	if (canUseQuickSave && !isMultiplayer())
	{
		if (((pOffTrackedRemoteNew->Buttons & offButton1) !=
			 (pOffTrackedRemoteOld->Buttons & offButton1)) &&
			(pOffTrackedRemoteNew->Buttons & offButton1)) {
			sendButtonActionSimple("savequick");
		}

		if (((pOffTrackedRemoteNew->Buttons & offButton2) !=
			 (pOffTrackedRemoteOld->Buttons & offButton2)) &&
			(pOffTrackedRemoteNew->Buttons & offButton2)) {
			sendButtonActionSimple("loadquick");
		}
	}


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

		float distanceToHMD = sqrtf(powf(hmdPosition[0] - pDominantTracking->HeadPose.Pose.Position.x, 2) +
									powf(hmdPosition[1] - pDominantTracking->HeadPose.Pose.Position.y, 2) +
									powf(hmdPosition[2] - pDominantTracking->HeadPose.Pose.Position.z, 2));

		static float forwardYaw = 0;
		if (!isScopeEngaged())
		{
			forwardYaw = (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]) - snapTurn;
		}

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

		//Engage scope if conditions are right
		if (vr_weapon_stabilised->value == 1 && !isScopeEngaged() && distanceToHMD < SCOPE_ENGAGE_DISTANCE)
		{
			sendButtonActionSimple("+alt1");
		}
		else if (isScopeEngaged() && (distanceToHMD > SCOPE_ENGAGE_DISTANCE || vr_weapon_stabilised->value != 1))
		{
			sendButtonActionSimple("-alt1");
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
				}
			}
			else
			{
				weaponangles[ADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[ADJUSTED][PITCH] *= -1.0f;

				weaponangles[UNADJUSTED][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
				weaponangles[UNADJUSTED][PITCH] *= -1.0f;
			}

			weaponangles[MELEE][YAW] += (cl.refdef.cl_viewangles[YAW] - hmdorientation[YAW]);
			weaponangles[MELEE][PITCH] *= -1.0f;

			// Use (Action gesture)
			if (vr_gesture_triggered_use->integer) {
				bool gestureUseAllowed = !vr_weapon_stabilised->value;
				// Off-hand gesture
				float xOffset = hmdPosition[0] - pOffTracking->HeadPose.Pose.Position.x;
				float zOffset = hmdPosition[2] - pOffTracking->HeadPose.Pose.Position.z;
				float distanceToBody = sqrtf((xOffset * xOffset) + (zOffset * zOffset));
				if (gestureUseAllowed && (distanceToBody > vr_use_gesture_boundary->value)) {
					if (!(use_gesture_state & VR_USE_GESTURE_OFF_HAND)) {
						sendButtonAction("+use2", true);
					}
					use_gesture_state |= VR_USE_GESTURE_OFF_HAND;
				} else {
					if (use_gesture_state & VR_USE_GESTURE_OFF_HAND) {
						sendButtonAction("+use2", false);
					}
					use_gesture_state &= ~VR_USE_GESTURE_OFF_HAND;
				}
				// Weapon-hand gesture
				xOffset = hmdPosition[0] - pDominantTracking->HeadPose.Pose.Position.x;
				zOffset = hmdPosition[2] - pDominantTracking->HeadPose.Pose.Position.z;
				distanceToBody = sqrtf((xOffset * xOffset) + (zOffset * zOffset));
				if (gestureUseAllowed && (distanceToBody > vr_use_gesture_boundary->value)) {
					if (!(use_gesture_state & VR_USE_GESTURE_WEAPON_HAND)) {
						sendButtonAction("+use", true);
					}
					use_gesture_state |= VR_USE_GESTURE_WEAPON_HAND;
				} else {
					if (use_gesture_state & VR_USE_GESTURE_WEAPON_HAND) {
						sendButtonAction("+use", false);
					}
					use_gesture_state &= ~VR_USE_GESTURE_WEAPON_HAND;
				}
			} else {
				if (use_gesture_state & VR_USE_GESTURE_OFF_HAND) {
					sendButtonAction("+use2", false);
				}
				if (use_gesture_state & VR_USE_GESTURE_WEAPON_HAND) {
					sendButtonAction("+use", false);
				}
				use_gesture_state = 0;
			}

			// Use (Action button)
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

			if (!isBackpack(pDominantTracking)) {
				canUseBackpack = false;
			}
			else if (!canUseBackpack && grabMeleeWeapon == 0) {
				int channel = (vr_control_scheme->integer >= 10) ? 0 : 1;
				if (vr_controller_tracking_haptic->value == 1.0f) {
					Android_Vibrate(40, channel,
									0.5); // vibrate to let user know they can switch
				}
				canUseBackpack = true;
			}

			if ((pDominantTrackedRemoteNew->Buttons & ovrButton_GripTrigger) !=
				(pDominantTrackedRemoteOld->Buttons & ovrButton_GripTrigger)) {

				dominantGripPushed = (pDominantTrackedRemoteNew->Buttons &
									  ovrButton_GripTrigger) != 0;

				if (grabMeleeWeapon == 0)
				{
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
					} else{
						if (dominantGripPushed) {
							//Initiate crowbar from backpack mode
							sendButtonActionSimple(g_pszBackpackWeapon);
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

		if (vr_walkdirection->integer == 0) {
			controllerYawHeading = -cl.refdef.cl_viewangles[YAW] + flashlightangles[YAW];
		}
		else
		{
			controllerYawHeading = 0.0f;
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

			if (vr_quick_crouchjump->value != 0.0) {
				//Jump (and crouch if double clicked quick enough)
				static bool quickCrouchJumped = false;
				static double jumpTimer = 0;
				if ((pDominantTrackedRemoteNew->Buttons & domButton2) !=
					(pDominantTrackedRemoteOld->Buttons & domButton2)) {
					if ((pDominantTrackedRemoteNew->Buttons & domButton2) && jumpTimer == 0) {
						//Jump and start timer
						sendButtonActionSimple("+jump");
						jumpTimer = GetTimeInMilliSeconds();
					} else {
						//Is jump button released?
						if ((pDominantTrackedRemoteNew->Buttons & domButton2) == 0) {
							if (quickCrouchJumped) {
								ducked = DUCK_NOTDUCKED;
								sendButtonActionSimple("-duck");
								jumpTimer = 0;
								quickCrouchJumped = false;
							} else {
								sendButtonActionSimple("-jump");
							}
						}
							//Jump button is therefore pushed, was it pushed quick enough to trigger duck?
						else if (between(1, (float) (GetTimeInMilliSeconds() - jumpTimer), 300)) {
							ducked = DUCK_BUTTON;
							sendButtonActionSimple("+duck");
							quickCrouchJumped = true;
						}
							//Nope
						else {
							//Just jump and reset timer
							sendButtonActionSimple("+jump");
							jumpTimer = GetTimeInMilliSeconds();
						}
					}
				}
			} else {
				//Jump
				sendButtonAction("+jump", (pDominantTrackedRemoteNew->Buttons & domButton2));
			}

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
				// we need to release secondary fire if dominantGripPushed has been released before releasing trigger -> should fix the gun jamming and non stop firing secondary attack bug
				if ((pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) !=
					(pDominantTrackedRemoteOld->Buttons & ovrButton_Trigger) &&
					(pDominantTrackedRemoteNew->Buttons & ovrButton_Trigger) == false)
				{
					sendButtonAction("+attack2", false);
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
                    sendButtonActionSimple("invprev");
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
                            sendButtonActionSimple("invprev");
                        }
                        else
                        {
                            scrollCount--;
                            sendButtonActionSimple("invnext");
                        }
                        weaponSwitched = true;
                    }
                } else {
                    weaponSwitched = false;
                }
            }
		}

		{
            //Laser Sight / Scope stabilisation
			if ((pOffTrackedRemoteNew->Buttons & ovrButton_Joystick) !=
				(pOffTrackedRemoteOld->Buttons & ovrButton_Joystick)
				&& (pOffTrackedRemoteNew->Buttons & ovrButton_Joystick)) {

                if (!isScopeEngaged()) {
                    Cvar_SetFloat("vr_lasersight", (int)(vr_lasersight->value + 1) % 3);
                }
                else {
                    stabiliseScope = !stabiliseScope;
                }
			}

			if (!isScopeEngaged()) {
				static float joyx[4] = {0};
				static float joyy[4] = {0};
				for (int j = 3; j > 0; --j) {
					joyx[j] = joyx[j - 1];
					joyy[j] = joyy[j - 1];
				}
				joyx[0] = pOffTrackedRemoteNew->Joystick.x;
				joyy[0] = pOffTrackedRemoteNew->Joystick.y;
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
				rotateAboutOrigin(x, y, controllerYawHeading,v);

				remote_movementSideways = v[0];
				remote_movementForward = v[1];

				ALOGV("        remote_movementSideways: %f, remote_movementForward: %f",
					  remote_movementSideways,
					  remote_movementForward);
			} else {
				static bool scopeZoomed = false;
				if (between(0.75f, pOffTrackedRemoteNew->Joystick.y, 1.0f) ||
					between(-1.0f, pOffTrackedRemoteNew->Joystick.y, -0.75f))
				{
					if (!scopeZoomed)
					{
						if (between(0.75f, pOffTrackedRemoteNew->Joystick.y, 1.0f))
						{
							sendButtonActionSimple("impulse 104"); //zoom in
						}
						else if (between(-1.0f, pOffTrackedRemoteNew->Joystick.y, -0.75f))
						{
							sendButtonActionSimple("impulse 105"); //zoom out
						}
						scopeZoomed = true;
					}
				}
				else
				{
					scopeZoomed = false;
				}
			}


			//flashlight on/off
			if (!canUseQuickSave &&
                ((pOffTrackedRemoteNew->Buttons & offButton1) !=
				 (pOffTrackedRemoteOld->Buttons & offButton1)) &&
				(pOffTrackedRemoteOld->Buttons & offButton1)) {
				sendButtonActionSimple("impulse 100");
			}


			//We need to record if we have started firing primary so that releasing trigger will stop definitely firing, if user has pushed grip
			//in meantime, then it wouldn't stop the gun firing and it would get stuck
			static bool firingPrimary = false;

			//Run
			sendButtonAction("+speed", pOffTrackedRemoteNew->Buttons & ovrButton_Trigger);

			static float joyx[4] = {0};
			for (int j = 3; j > 0; --j)
				joyx[j] = joyx[j-1];
			joyx[0] = pDominantTrackedRemoteNew->Joystick.x;
			float joystickX = (joyx[0] + joyx[1] + joyx[2] + joyx[3]) / 4.0f;


			static bool increaseSnap = true;
            if (!selectingWeapon) {

                //engage comfort mask if using smooth rotation
                player_moving |= (vr_snapturn_angle->value <= 10.0f &&
                                  fabs(joystickX) > 0.7f);

				if (joystickX > 0.7f)
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
				else if (joystickX < 0.1f)
				{
					increaseSnap = true;
				}

				static bool decreaseSnap = true;
				if (joystickX < -0.7f)
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
				else if (joystickX > -0.1f)
				{
					decreaseSnap = true;
				}
			}
		}

        updateScopeAngles(forwardYaw);
    }

	//Save state
	leftTrackedRemoteState_old = leftTrackedRemoteState_new;
	rightTrackedRemoteState_old = rightTrackedRemoteState_new;
}