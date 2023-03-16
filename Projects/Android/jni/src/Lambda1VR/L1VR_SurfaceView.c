#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h>	// for native window JNI
#include <android/input.h>

#include "argtable3.h"
#include "VrInput.h"
#include "VrCvars.h"
#include "VrCommon.h"

#include <common/common.h>
#include <common/library.h>
#include <common/cvardef.h>
#include <common/xash3d_types.h>
#include <engine/keydefs.h>
#include <client/touch.h>
#include <client/client.h>

#include "../gl4es/src/gl/loader.h"


//Let's go to the maximum!
extern float SS_MULTIPLIER;

/* global arg_xxx structs */
struct arg_dbl *ss;
struct arg_int *cpu;
struct arg_int *gpu;
struct arg_int *msaa;
struct arg_end *end;

char **argv;
int argc=0;

extern convar_t	*r_lefthand;

/*
================================================================================

LAMBDA1VR Stuff

================================================================================
*/

typedef void (*pfnChangeGame)( const char *progname );
extern int Host_Main( int argc, const char **argv, const char *progname, int bChangeGame, pfnChangeGame func );

bool VR_UseScreenLayer()
{
	return (showingScreenLayer || cls.demoplayback || cls.state == ca_cinematic || cls.key_dest != key_game);
}

extern int runStatus;
void L1VR_exit(int exitCode)
{
	runStatus = exitCode;
}

void rotateAboutOrigin(float x, float y, float rotation, vec2_t out)
{
	out[0] = cosf(DEG2RAD(-rotation)) * x  +  sinf(DEG2RAD(-rotation)) * y;
	out[1] = cosf(DEG2RAD(-rotation)) * y  -  sinf(DEG2RAD(-rotation)) * x;
}

static void UnEscapeQuotes( char *arg )
{
	char *last = NULL;
	while( *arg ) {
		if( *arg == '"' && *last == '\\' ) {
			char *c_curr = arg;
			char *c_last = last;
			while( *c_curr ) {
				*c_last = *c_curr;
				c_last = c_curr;
				c_curr++;
			}
			*c_last = '\0';
		}
		last = arg;
		arg++;
	}
}

static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	char *lastp = NULL;
	int argc, last_argc;
	argc = last_argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		while ( isspace(*bufp) ) {
			++bufp;
		}
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
				lastp = bufp;
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
		if( argv && last_argc != argc ) {
			UnEscapeQuotes( argv[last_argc] );
		}
		last_argc = argc;
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}



void setWorldPosition( float x, float y, float z )
{
    positionDeltaThisFrame[0] = (worldPosition[0] - x);
    positionDeltaThisFrame[1] = (worldPosition[1] - y);
    positionDeltaThisFrame[2] = (worldPosition[2] - z);

    worldPosition[0] = x;
    worldPosition[1] = y;
    worldPosition[2] = z;
}


void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
	VectorSet(hmdorientation, pitch, yaw, roll);

	if (!VR_UseScreenLayer())
	{
		playerYaw = yaw;
	}
}

void VR_SetHMDPosition( float x, float y, float z )
{
	static bool s_useScreen = false;
	
	setWorldPosition( x, y, z );

	VectorSet(hmdPosition, x, y, z);

    if (s_useScreen != VR_UseScreenLayer())
    {
		s_useScreen = VR_UseScreenLayer();

		//Record player height on transition
        playerHeight = y;
    }

	if (!VR_UseScreenLayer())
    {
    	if (vr_enable_crouching->value > 0.0f && vr_enable_crouching->value < 0.98f) {
            //Do we trigger crouching based on player height?
            if (hmdPosition[1] < (playerHeight * vr_enable_crouching->value) &&
                ducked == DUCK_NOTDUCKED) {
                ducked = DUCK_CROUCHED;
                sendButtonAction("+crouch", 1);
            } else if (hmdPosition[1] > (playerHeight * (vr_enable_crouching->value + 0.02f)) &&
                ducked == DUCK_CROUCHED) {
                ducked = DUCK_NOTDUCKED;
                sendButtonAction("+crouch", 0);
            }
        }
	}
}

bool isMultiplayer()
{
	return (CL_GetMaxClients() > 1);
}

bool isScopeEngaged()
{
	return (cl.scr_fov != 0 &&	cl.scr_fov  < vrFOV);
}

bool isPlayerDead()
{
    return ( cls.key_dest == key_game ) && cl.frame.client.health <= 0.0;
}

void Host_BeginFrame();
void Host_Frame();
void Host_EndFrame();

void VR_GetMove( float *forward, float *side, float *yaw, float *pitch, float *roll )
{
	//This is pretty crazy, but due to the way the angles are sent between the client and server
	//they are truncated, which results in a small rounding down, over time this leads to a yaw drift
	//which doesn't take long to start affecting the accuracy of the sniper rifle, or simply leave you
	// not facing the direction you started in!
	static float driftCorrection = 0;
	//Update our drift correction
	driftCorrection += YAWDRIFTPERFRAME;
	if (driftCorrection > 360.0F)
	{
		driftCorrection = 0;
	}

	*forward = remote_movementForward + positional_movementForward;
    *side = remote_movementSideways + positional_movementSideways;
	*yaw = hmdorientation[YAW] + snapTurn + driftCorrection;
	*pitch = hmdorientation[PITCH];
	*roll = hmdorientation[ROLL];
}

float VR_GetScreenLayerDistance()
{
	return (4.5f);
}

bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection)
{
	if (strstr(gAppState.OpenXRHMD, "pico") != NULL)
	{
		XrMatrix4x4f_CreateProjectionFov(
				&(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL_ES,
				gAppState.Projections[eye].fov, zNear, zFar);
	}

	if (strstr(gAppState.OpenXRHMD, "meta") != NULL)
	{
		XrFovf fov = {};
		for (int eye = 0; eye < ovrMaxNumEyes; eye++)
		{
			fov.angleLeft += gAppState.Projections[eye].fov.angleLeft / 2.0f;
			fov.angleRight += gAppState.Projections[eye].fov.angleRight / 2.0f;
			fov.angleUp += gAppState.Projections[eye].fov.angleUp / 2.0f;
			fov.angleDown += gAppState.Projections[eye].fov.angleDown / 2.0f;
		}
		XrMatrix4x4f_CreateProjectionFov(
				&(gAppState.ProjectionMatrices[eye]), GRAPHICS_OPENGL_ES,
				fov, zNear, zFar);
	}

	memcpy(projection, gAppState.ProjectionMatrices[eye].m, 16 * sizeof(float));
	return true;
}

void R_ChangeDisplaySettings( int width, int height, qboolean fullscreen );

//All the stuff we want to do each frame specifically for this game
void VR_FrameSetup()
{
	static bool usingScreenLayer = true; //Starts off using the screen layer
	if (usingScreenLayer != VR_UseScreenLayer())
	{
		usingScreenLayer = VR_UseScreenLayer();
		R_ChangeDisplaySettings(gAppState.Width, gAppState.Height, false);
	}	
}

void VR_Shutdown()
{
}

static inline bool isHostAlive()
{
	return (host.state != HOST_SHUTDOWN &&
			host.state != HOST_CRASHED);
}

void COM_SetRandomSeed( int lSeed );

long shutdownCountdown;


void Android_MessageBox(const char *title, const char *text)
{
    ALOGE("%s %s", title, text);
}



convar_t 	*vibration_enable;
convar_t	*vr_snapturn_angle;
convar_t	*vr_reloadtimeoutms;
convar_t	*vr_positional_factor;
convar_t	*vr_walkdirection;
convar_t	*vr_weapon_pitchadjust;
convar_t	*vr_crowbar_pitchadjust;
convar_t	*vr_weapon_recoil;
convar_t	*vr_weapon_stabilised;
convar_t	*vr_lasersight;
convar_t	*vr_quest_fov;
convar_t	*vr_refresh;
convar_t	*vr_control_scheme;
convar_t	*vr_enable_crouching;
convar_t	*vr_height_adjust;
convar_t	*vr_flashlight_model;
convar_t	*vr_hand_model;
convar_t	*vr_mirror_weapons;
convar_t	*vr_weapon_backface_culling;
convar_t	*vr_comfort_mask;
convar_t	*vr_controller_ladders;
convar_t	*vr_controller_tracking_haptic;
convar_t	*vr_highlight_actionables;
convar_t	*vr_headtorch;
convar_t	*vr_quick_crouchjump;
convar_t	*vr_stereo_side;


void initialize_gl4es();

void VR_Init()
{
	//Initialise all our variables
	playerYaw = 0.0f;
	showingScreenLayer = false;
	remote_movementSideways = 0.0f;
	remote_movementForward = 0.0f;
	positional_movementSideways = 0.0f;
	positional_movementForward = 0.0f;
	snapTurn = 0.0f;
	ducked = DUCK_NOTDUCKED;
	player_moving = false;

	//init randomiser
	srand(time(NULL));

	//Create Cvars
	vr_snapturn_angle = Cvar_Get( "vr_snapturn_angle", "45", CVAR_ARCHIVE, "Sets the angle for snap-turn, set to < 10.0 to enable smooth turning" );
	vr_reloadtimeoutms = Cvar_Get( "vr_reloadtimeoutms", "200", CVAR_ARCHIVE, "How quickly the grip trigger needs to be release to initiate a reload" );
	vr_positional_factor = Cvar_Get( "vr_positional_factor", "3000", CVAR_ARCHIVE, "Number that makes positional tracking work" );
    vr_walkdirection = Cvar_Get( "vr_walkdirection", "0", CVAR_ARCHIVE, "1 - Use HMD for direction, 0 - Use off-hand controller for direction" );
	vr_weapon_pitchadjust = Cvar_Get( "vr_weapon_pitchadjust", "-20.0", CVAR_ARCHIVE, "gun pitch angle adjust" );
	vr_crowbar_pitchadjust = Cvar_Get( "vr_crowbar_pitchadjust", "-25.0", CVAR_ARCHIVE, "crowbar pitch angle adjust" );
    vr_weapon_recoil = Cvar_Get( "vr_weapon_recoil", "0", CVAR_ARCHIVE, "Enables weapon recoil in VR, default is disabled, warning could make you sick" );
	vr_weapon_stabilised = Cvar_Get( "vr_weapon_stabilised", "0", CVAR_READ_ONLY, "Whether user has engaged weapon stabilisation or not" );
    vr_lasersight = Cvar_Get( "vr_lasersight", "0", CVAR_ARCHIVE, "Enables laser-sight" );

    char buffer[32];
    sprintf(buffer, "%d", (int)vrFOV);
    vr_quest_fov = Cvar_Get("vr_quest_fov", buffer, CVAR_LATCH, "FOV for Lambda1VR" );

    vr_refresh = Cvar_Get("vr_refresh", "72", CVAR_ARCHIVE, "Refresh Rate" );
	vr_control_scheme = Cvar_Get( "vr_control_scheme", "0", CVAR_ARCHIVE, "Controller Layout scheme" );
	vr_enable_crouching = Cvar_Get( "vr_enable_crouching", "0.85", CVAR_ARCHIVE, "To enable real-world crouching trigger, set this to a value that multiplied by the user's height will trigger crouch mechanic" );
    vr_height_adjust = Cvar_Get( "vr_height_adjust", "0.0", CVAR_ARCHIVE, "Additional height adjustment for in-game player (in metres)" );
    vr_flashlight_model = Cvar_Get( "vr_flashlight_model", "1", CVAR_ARCHIVE, "Set to 0 to prevent drawing the flashlight model" );
    vr_hand_model = Cvar_Get( "vr_hand_model", "1", CVAR_ARCHIVE, "Set to 0 to prevent drawing the hand models" );
	vr_mirror_weapons = Cvar_Get( "vr_mirror_weapons", "0", CVAR_ARCHIVE, "Set to 1 to mirror the weapon models (for left handed use)" );
    vr_weapon_backface_culling = Cvar_Get( "vr_weapon_backface_culling", "0", CVAR_ARCHIVE, "Use to enable whether back face culling is used on weapon viewmodel" );
	vr_comfort_mask = Cvar_Get( "vr_comfort_mask", "0.0", CVAR_ARCHIVE, "Use to reduce motion sickness, 0.0 is off, 1.0 is fully obscured, probably go with 0.7, anything less than 0.5 is barely visible" );
    vr_controller_ladders = Cvar_Get( "vr_controller_ladders", "0", CVAR_ARCHIVE, "Set to 0 to use ladder climb direction based on HMD, otherwise off-hand controller angle is used" );
	vr_controller_tracking_haptic = Cvar_Get( "vr_controller_tracking_haptic", "1", CVAR_ARCHIVE, "Set to 0 to disable haptic blip when dominant controller loses tracking" );
	vr_highlight_actionables = Cvar_Get( "vr_highlight_actionables", "1", CVAR_ARCHIVE, "Set to 0 to disable highlighting of actionable objects/entities" );
	vr_headtorch = Cvar_Get( "vr_headtorch", "0", CVAR_ARCHIVE, "Set to 1 to enable head-torch flashlight mode" );
    vr_quick_crouchjump = Cvar_Get( "vr_quick_crouchjump", "1", CVAR_ARCHIVE, "Set to 0 to disable quick crouch-jump mode (double clicking jump button triggers duck)" );

    //Not to be changed by users, as it will be overwritten anyway
	vr_stereo_side = Cvar_Get( "vr_stereo_side", "0", CVAR_READ_ONLY, "Eye being drawn" );

	//Set up backpack weapon string (this depends on the game)
	g_pszBackpackWeapon = getenv("VR_BACKPACK_WEAPON");
	if (g_pszBackpackWeapon == NULL)
	{
		g_pszBackpackWeapon = "weapon_crowbar";
	}
}


void VR_HandleControllerInput() {
	TBXR_UpdateControllers();
	
	//Call additional control schemes here
	switch (vr_control_scheme->integer)
	{
		case RIGHT_HANDED_DEFAULT:
			HandleInput_Default(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
								&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
								xrButton_A, xrButton_B, xrButton_X, xrButton_Y);
			break;
		case RIGHT_HANDED_ALT:
			HandleInput_Alt(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
							&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
							xrButton_A, xrButton_B, xrButton_X, xrButton_Y);
			break;
		case RIGHT_HANDED_ALT2:
			HandleInput_Alt2(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
							&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
							xrButton_A, xrButton_B, xrButton_X, xrButton_Y);
			break;
		case ONE_CONTROLLER_RIGHT:
			HandleInput_OneController(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
							&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
							xrButton_A, xrButton_B, xrButton_X, xrButton_Y);
			break;
		case LEFT_HANDED_DEFAULT:
			HandleInput_Default(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
								&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
								xrButton_X, xrButton_Y, xrButton_A, xrButton_B);
			break;
		case LEFT_HANDED_ALT:
			HandleInput_Alt(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
							 &rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
							 xrButton_X, xrButton_Y, xrButton_A, xrButton_B);
			break;
		case LEFT_HANDED_ALT_2:
			HandleInput_Alt2(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
							&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
							xrButton_X, xrButton_Y, xrButton_A, xrButton_B);
			break;
		case ONE_CONTROLLER_LEFT:
			HandleInput_OneController(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
							&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
							xrButton_X, xrButton_Y, xrButton_A, xrButton_B);
			break;
	}
}

void * AppThreadFunction( void * parm )
{
	gAppThread = (ovrAppThread *) parm;

	java.Vm = gAppThread->JavaVm;
	(*java.Vm)->AttachCurrentThread( java.Vm, &java.Env, NULL );
	java.ActivityObject = gAppThread->ActivityObject;

	prctl( PR_SET_NAME, (long)"AppThreadFunction", 0, 0, 0 );

	xash_initialised = false;
	

	TBXR_InitialiseOpenXR();

	Host_Main(argc, (const char**)argv, "valve", false, NULL);

	VR_Init();

	TBXR_EnterVR();
	TBXR_InitRenderer();
	TBXR_InitActions();
	TBXR_WaitForSessionActive();

	//Always use this folder
	chdir("/sdcard/xash");

	bool destroyed = false;
	while (!destroyed)
	{
		//We are now shutting down
		if (runStatus == 0)
		{
			destroyed = true;
		}
		else
		{
			TBXR_FrameSetup();

			//Call the game drawing code to populate the cylinder layer texture
			//if we are now shutting down, drop out here
			if (isHostAlive())
			{

				//Seed the random number generator the same for each eye to ensure electricity is drawn the same
				int lSeed = rand();

				//Set everything up
				Host_BeginFrame();

				// Render the eye images.
				for (int eye = 0; eye < ovrMaxNumEyes && isHostAlive(); eye++)
				{
					TBXR_prepareEyeBuffer(eye);

					if (gAppState.FrameState.shouldRender)
					{
						if (isScopeEngaged())
						{
							//Now do the drawing for this eye - Force the set as it is a "read only" cvar
							char buffer[5];
							Q_snprintf(buffer, 5, "%i", eye + 2);
							Cvar_Set2("vr_stereo_side", buffer, true);
						}
						else
						{
							//Now do the drawing for this eye - Force the set as it is a "read only" cvar
							char buffer[5];
							Q_snprintf(buffer, 5, "%i", eye);
							Cvar_Set2("vr_stereo_side", buffer, true);
						}


						//Sow the seed
						COM_SetRandomSeed(lSeed);

						Host_Frame();
					}

					TBXR_finishEyeBuffer(eye);
				}

				Host_EndFrame();

				TBXR_submitFrame();
			}
		}
	}

	{
		TBXR_LeaveVR();
		//Ask Java to shut down
		VR_Shutdown();

//		exit(0); // in case Java doesn't do the job
	}

	return NULL;
}

/*
================================================================================

Activity lifecycle

================================================================================
*/

int JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv *env;
	if((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
	{
		ALOGE("Failed JNI_OnLoad");
		return -1;
	}

	return JNI_VERSION_1_4;
}

JNIEXPORT jlong JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onCreate( JNIEnv * env, jclass activityClass, jobject activity,
																	   jstring commandLineParams)
{
	ALOGV( "    GLES3JNILib::onCreate()" );

	/* the global arg_xxx structs are initialised within the argtable */
	void *argtable[] = {
			ss   = arg_dbl0("s", "supersampling", "<double>", "super sampling value (e.g. 1.0)"),
            cpu   = arg_int0("c", "cpu", "<int>", "CPU perf index 1-3 (default: 2)"),
            gpu   = arg_int0("g", "gpu", "<int>", "GPU perf index 1-3 (default: 3)"),
			msaa   = arg_int0("m", "msaa", "<int>", "MSAA (default: 4)"),
			end     = arg_end(20)
	};

	jboolean iscopy;
	const char *arg = (*env)->GetStringUTFChars(env, commandLineParams, &iscopy);

	char *cmdLine = NULL;
	if (arg && strlen(arg))
	{
		cmdLine = strdup(arg);
	}

	(*env)->ReleaseStringUTFChars(env, commandLineParams, arg);

	ALOGV("Command line %s", cmdLine);
	argv = malloc(sizeof(char*) * 255);
	argc = ParseCommandLine(strdup(cmdLine), argv);

	/* verify the argtable[] entries were allocated sucessfully */
	if (arg_nullcheck(argtable) == 0) {
		/* Parse the command line as defined by argtable[] */
		arg_parse(argc, argv, argtable);

        if (ss->count > 0 && ss->dval[0] > 0.0)
        {
            SS_MULTIPLIER = ss->dval[0];
        }
	}

	initialize_gl4es();

	ovrAppThread * appThread = (ovrAppThread *) malloc( sizeof( ovrAppThread ) );
	ovrAppThread_Create( appThread, env, activity, activityClass );

	surfaceMessageQueue_Enable(&appThread->MessageQueue, true);
	surfaceMessage message;
	surfaceMessage_Init(&message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED);
	surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);

	return (jlong)((size_t)appThread);
}


JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onStart( JNIEnv * env, jobject obj, jlong handle)
{
	ALOGV( "    GLES3JNILib::onStart()" );

	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	surfaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_START, MQ_WAIT_PROCESSED );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onResume( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onResume()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	surfaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onPause( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onPause()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	surfaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onStop( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onStop()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	surfaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onDestroy( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onDestroy()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	surfaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	surfaceMessageQueue_Enable( &appThread->MessageQueue, false );

	ovrAppThread_Destroy( appThread, env );
	free( appThread );
}

/*
================================================================================

Surface lifecycle

================================================================================
*/

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onSurfaceCreated( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceCreated()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
	appThread->NativeWindow = newNativeWindow;
	surfaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
	surfaceMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onSurfaceChanged( JNIEnv * env, jobject obj, jlong handle, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceChanged()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);

	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	if ( newNativeWindow != appThread->NativeWindow )
	{
		if ( appThread->NativeWindow != NULL )
		{
			surfaceMessage message;
			surfaceMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
			surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
			ALOGV( "        ANativeWindow_release( NativeWindow )" );
			ANativeWindow_release( appThread->NativeWindow );
			appThread->NativeWindow = NULL;
		}
		if ( newNativeWindow != NULL )
		{
			ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
			appThread->NativeWindow = newNativeWindow;
			surfaceMessage message;
			surfaceMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
			surfaceMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
			surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
		}
	}
	else if ( newNativeWindow != NULL )
	{
		ANativeWindow_release( newNativeWindow );
	}
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onSurfaceDestroyed( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onSurfaceDestroyed()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	surfaceMessage message;
	surfaceMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
	surfaceMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	ALOGV( "        ANativeWindow_release( NativeWindow )" );
	ANativeWindow_release( appThread->NativeWindow );
	appThread->NativeWindow = NULL;
}
