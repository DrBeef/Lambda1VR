#if !defined(tbxr_common_h)
#define tbxr_common_h

//OpenXR
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <jni.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr_helpers.h>

#include <android/native_window_jni.h>
#include <android/log.h>

#include <stdbool.h>
#include <pthread.h>

#ifndef NDEBUG
#define DEBUG 1
#endif

#define LOG_TAG "TBXR"

#define DEG2RAD(a) ((a) * ((float) M_PI / 180.0f))
#define RAD2DEG(a) ((a) * (180.0f / (float) M_PI))

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

enum { ovrMaxLayerCount = 1 };
enum { ovrMaxNumEyes = 2 };

typedef enum xrButton_ {
    xrButton_A = 0x00000001, // Set for trigger pulled on the Gear VR and Go Controllers
    xrButton_B = 0x00000002,
    xrButton_RThumb = 0x00000004,
    xrButton_RShoulder = 0x00000008,
    xrButton_X = 0x00000100,
    xrButton_Y = 0x00000200,
    xrButton_LThumb = 0x00000400,
    xrButton_LShoulder = 0x00000800,
    xrButton_Up = 0x00010000,
    xrButton_Down = 0x00020000,
    xrButton_Left = 0x00040000,
    xrButton_Right = 0x00080000,
    xrButton_Enter = 0x00100000,
    xrButton_Back = 0x00200000,
    xrButton_GripTrigger = 0x04000000,
    xrButton_Trigger = 0x20000000,
    xrButton_Joystick = 0x80000000,

    //Define additional controller touch points (not button presses)
    xrButton_ThumbRest = 0x00000010,

    xrButton_EnumSize = 0x7fffffff
} xrButton;

typedef struct {
    uint32_t Buttons;
    uint32_t Touches;
    float IndexTrigger;
    float GripTrigger;
    XrVector2f Joystick;
} ovrInputStateTrackedRemote;

typedef struct {
    GLboolean Active;
    XrPosef Pose;
    XrSpaceVelocity Velocity;
} ovrTrackedController;

typedef enum control_scheme {
    RIGHT_HANDED_DEFAULT = 0,
    RIGHT_HANDED_ALT = 1,
    RIGHT_HANDED_ALT2 = 2,
    ONE_CONTROLLER_RIGHT = 3,
    LEFT_HANDED_DEFAULT = 10,
    LEFT_HANDED_ALT = 11,
    LEFT_HANDED_ALT_2 = 12,   //Special case for left-handers who want the right handed button mappings / thumbsticks but left handed weapon / right handed flashlight
    ONE_CONTROLLER_LEFT = 13   //Special case for left-handers who want the right handed button mappings / thumbsticks but left handed weapon / right handed flashlight
} control_scheme_t;

typedef struct {
    float M[4][4];
} ovrMatrix4f;


typedef struct {
    XrSwapchain Handle;
    uint32_t Width;
    uint32_t Height;
} ovrSwapChain;

typedef struct {
    int Width;
    int Height;
    int Multisamples;
    uint32_t TextureSwapChainLength;
    uint32_t TextureSwapChainIndex;
    ovrSwapChain ColorSwapChain;
    XrSwapchainImageOpenGLESKHR* ColorSwapChainImage;
    GLuint* DepthBuffers;
    GLuint* FrameBuffers;
} ovrFramebuffer;

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct
{
    ovrFramebuffer	FrameBuffer[ovrMaxNumEyes];
} ovrRenderer;

/*
================================================================================

ovrApp

================================================================================
*/


typedef enum
{
    MQ_WAIT_NONE,		// don't wait
    MQ_WAIT_RECEIVED,	// wait until the consumer thread has received the message
    MQ_WAIT_PROCESSED	// wait until the consumer thread has processed the message
} ovrMQWait;

#define MAX_MESSAGE_PARMS	8
#define MAX_MESSAGES		1024

typedef struct
{
    int			Id;
    ovrMQWait	Wait;
    long long	Parms[MAX_MESSAGE_PARMS];
} surfaceMessage;

typedef struct
{
    surfaceMessage	 		Messages[MAX_MESSAGES];
    volatile int		Head;	// dequeue at the head
    volatile int		Tail;	// enqueue at the tail
    ovrMQWait			Wait;
    volatile bool		EnabledFlag;
    volatile bool		PostedFlag;
    volatile bool		ReceivedFlag;
    volatile bool		ProcessedFlag;
    pthread_mutex_t		Mutex;
    pthread_cond_t		PostedCondition;
    pthread_cond_t		ReceivedCondition;
    pthread_cond_t		ProcessedCondition;
} surfaceMessageQueue;

typedef struct
{
    JavaVM *		JavaVm;
    jobject			ActivityObject;
    jclass          ActivityClass;
    pthread_t		Thread;
    surfaceMessageQueue	MessageQueue;
    ANativeWindow * NativeWindow;
} ovrAppThread;


typedef union {
    XrCompositionLayerProjection Projection;
    XrCompositionLayerQuad Quad;
} xrCompositorLayer_Union;

#define GL(func) func;

// Forward declarations
XrInstance TBXR_GetXrInstance();

#if defined(DEBUG)
static void
OXR_CheckErrors(XrInstance instance, XrResult result, const char* function, bool failOnError) {
    if (XR_FAILED(result)) {
        char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, errorBuffer);
        if (failOnError) {
            ALOGE("OpenXR error: %s: %s\n", function, errorBuffer);
        } else {
            ALOGV("OpenXR error: %s: %s\n", function, errorBuffer);
        }
    }
}
#endif

#if defined(DEBUG)
#define OXR(func) OXR_CheckErrors(TBXR_GetXrInstance(), func, #func, true);
#else
#define OXR(func) func;
#endif

typedef struct {
    EGLint MajorVersion;
    EGLint MinorVersion;
    EGLDisplay Display;
    EGLConfig Config;
    EGLSurface TinySurface;
    EGLSurface MainSurface;
    EGLContext Context;
} ovrEgl;

/// Java details about an activity
typedef struct ovrJava_ {
    JavaVM* Vm; //< Java Virtual Machine
    JNIEnv* Env; //< Thread specific environment
    jobject ActivityObject; //< Java activity object
} ovrJava;

typedef struct
{
    ovrJava				Java;
    ovrEgl              Egl;
    ANativeWindow* NativeWindow;
    bool				Resumed;
    bool				Focused;
    bool				Visible;
    bool                FrameSetup;
    char*               OpenXRHMD;

    float               Width;
    float               Height;

    XrInstance Instance;
    XrSession Session;
    XrViewConfigurationProperties ViewportConfig;
    XrViewConfigurationView ViewConfigurationView[ovrMaxNumEyes];
    XrSystemId SystemId;
    XrSpace HeadSpace;
    XrSpace StageSpace;
    XrSpace FakeStageSpace;
    XrSpace CurrentSpace;
    GLboolean SessionActive;
    XrPosef xfStageFromHead;
    XrView* Projections;
    XrMatrix4x4f ProjectionMatrices[2];


    float currentDisplayRefreshRate;
    float* SupportedDisplayRefreshRates;
    uint32_t RequestedDisplayRefreshRateIndex;
    uint32_t NumSupportedDisplayRefreshRates;
    PFN_xrGetDisplayRefreshRateFB pfnGetDisplayRefreshRate;
    PFN_xrRequestDisplayRefreshRateFB pfnRequestDisplayRefreshRate;

    XrFrameState        FrameState;
    int					SwapInterval;
    int					MainThreadTid;
    int					RenderThreadTid;
    xrCompositorLayer_Union		Layers[ovrMaxLayerCount];
    int					LayerCount;
    ovrRenderer			Renderer;
    ovrTrackedController TrackedController[2];
} ovrApp;


enum
{
    MESSAGE_ON_CREATE,
    MESSAGE_ON_START,
    MESSAGE_ON_RESUME,
    MESSAGE_ON_PAUSE,
    MESSAGE_ON_STOP,
    MESSAGE_ON_DESTROY,
    MESSAGE_ON_SURFACE_CREATED,
    MESSAGE_ON_SURFACE_DESTROYED
};

extern ovrAppThread * gAppThread;
extern ovrApp gAppState;
extern ovrJava java;


void ovrTrackedController_Clear(ovrTrackedController* controller);

void * AppThreadFunction(void * parm );

void ovrAppThread_Create( ovrAppThread * appThread, JNIEnv * env, jobject activityObject, jclass activityClass );
void ovrAppThread_Destroy( ovrAppThread * appThread, JNIEnv * env );


void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);

/*
 * Surface Lifecycle Message Queue
 */
void surfaceMessage_Init(surfaceMessage * message, const int id, const int wait );
void *	surfaceMessage_GetPointerParm(surfaceMessage * message, int index );
void	surfaceMessage_SetPointerParm(surfaceMessage * message, int index, void * ptr );

void surfaceMessageQueue_Create(surfaceMessageQueue * messageQueue );
void surfaceMessageQueue_Destroy(surfaceMessageQueue * messageQueue );
void surfaceMessageQueue_Enable(surfaceMessageQueue * messageQueue, const bool set );
void surfaceMessageQueue_PostMessage(surfaceMessageQueue * messageQueue, const surfaceMessage * message );

//Functions that need to be implemented by the game specific code
void VR_FrameSetup();
bool VR_UseScreenLayer();
float VR_GetScreenLayerDistance();
bool VR_GetVRProjection(int eye, float zNear, float zFar, float* projection);
void VR_HandleControllerInput();
void VR_SetHMDOrientation(float pitch, float yaw, float roll );
void VR_SetHMDPosition(float x, float y, float z );
void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight );
void VR_HapticUpdateEvent(const char* event, int intensity, float angle );
void VR_HapticEndFrame();
void VR_HapticStopEvent(const char* event);
void VR_HapticEnable();
void VR_HapticDisable();
void VR_Shutdown();


//Reusable Team Beef OpenXR stuff (in TBXR_Common.cpp)
double TBXR_GetTimeInMilliSeconds();
int TBXR_GetRefresh();
void TBXR_Recenter();
void TBXR_InitialiseOpenXR();
void TBXR_WaitForSessionActive();
void TBXR_InitRenderer();
void TBXR_EnterVR();
void TBXR_LeaveVR( );
void TBXR_GetScreenRes(int *width, int *height);
void TBXR_InitActions( void );
void TBXR_Vibrate(int duration, int channel, float intensity );
void TBXR_ProcessHaptics();
void TBXR_FrameSetup();
void TBXR_updateProjections();
void TBXR_UpdateControllers( );
void TBXR_prepareEyeBuffer(int eye );
void TBXR_finishEyeBuffer(int eye );
void TBXR_submitFrame();

#endif //vrcommon_h