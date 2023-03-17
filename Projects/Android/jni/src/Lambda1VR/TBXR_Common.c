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
#include "VrCommon.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#if !defined( EGL_OPENGL_ES3_BIT_KHR )
#define EGL_OPENGL_ES3_BIT_KHR		0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER			0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR		0x1004
#endif

#ifndef GLAPI
#define GLAPI extern
#endif
//#define ENABLE_GL_DEBUG
#define ENABLE_GL_DEBUG_VERBOSE 1

#define EGL_SYNC

#if defined EGL_SYNC
// EGL_KHR_reusable_sync
PFNEGLCREATESYNCKHRPROC			eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC		eglDestroySyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC		eglClientWaitSyncKHR;
PFNEGLSIGNALSYNCKHRPROC			eglSignalSyncKHR;
PFNEGLGETSYNCATTRIBKHRPROC		eglGetSyncAttribKHR;
#endif

//Let's go to the maximum!
int NUM_MULTI_SAMPLES	= 1;
int REFRESH	            = 0;
float SS_MULTIPLIER    = 1.3f;

GLboolean stageSupported = GL_FALSE;

const char* const requiredExtensionNames_meta[] = {
		XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
		XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
		XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME,
		XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,
		XR_FB_COLOR_SPACE_EXTENSION_NAME};

#define XR_PICO_CONFIGS_EXT_EXTENSION_NAME "XR_PICO_configs_ext"

enum ConfigsEXT
{
    RENDER_TEXTURE_WIDTH = 0,
    RENDER_TEXTURE_HEIGHT,
    SHOW_FPS,
    RUNTIME_LOG_LEVEL,
    PXRPLUGIN_LOG_LEVEL,
    UNITY_LOG_LEVEL,
    UNREAL_LOG_LEVEL,
    NATIVE_LOG_LEVEL,
    TARGET_FRAME_RATE,
    NECK_MODEL_X,
    NECK_MODEL_Y,
    NECK_MODEL_Z,
    DISPLAY_REFRESH_RATE,
    ENABLE_6DOF,
    CONTROLLER_TYPE,
    PHYSICAL_IPD,
    TO_DELTA_SENSOR_Y,
    GET_DISPLAY_RATE,
    FOVEATION_SUBSAMPLED_ENABLED = 18,
    TRACKING_ORIGIN_HEIGHT
};
typedef XrResult (XRAPI_PTR *PFN_xrGetConfigPICO)(
        XrSession                              session,
        enum ConfigsEXT                        configIndex,
        float *                                configData);
PFN_xrGetConfigPICO    pfnXrGetConfigPICO;


enum ConfigsSetEXT
{
	UNREAL_VERSION = 0,
	TRACKING_ORIGIN,
	OPENGL_NOERROR,
	ENABLE_SIX_DOF,
	PRESENTATION_FLAG,
	ENABLE_CPT,
	PLATFORM,
	FOVEATION_LEVEL,
	SET_DISPLAY_RATE = 8,
	MRC_TEXTURE_ID = 9,
};

typedef XrResult (XRAPI_PTR *PFN_xrSetConfigPICO) (
		XrSession                             session,
		enum ConfigsSetEXT                    configIndex,
		char *                                configData);
PFN_xrSetConfigPICO    pfnXrSetConfigPICO;

const char* const requiredExtensionNames_pico[] = {
		XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
		XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
		XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
		XR_PICO_CONFIGS_EXT_EXTENSION_NAME};


const uint32_t numRequiredExtensions_meta =
        sizeof(requiredExtensionNames_meta) / sizeof(requiredExtensionNames_meta[0]);
const uint32_t numRequiredExtensions_pico =
        sizeof(requiredExtensionNames_pico) / sizeof(requiredExtensionNames_pico[0]);


/*
================================================================================

System Clock Time in millis

================================================================================
*/

double TBXR_GetTimeInMilliSeconds()
{
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return ( now.tv_sec * 1e9 + now.tv_nsec ) * (double)(1e-6);
}

int runStatus = -1;
void TBXR_exit(int exitCode)
{
	runStatus = exitCode;
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

typedef struct
{
	bool multi_view;						// GL_OVR_multiview, GL_OVR_multiview2
	bool EXT_texture_border_clamp;			// GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
} OpenGLExtensions_t;

OpenGLExtensions_t glExtensions;

static void EglInitExtensions()
{
#if defined EGL_SYNC
	eglCreateSyncKHR		= (PFNEGLCREATESYNCKHRPROC)			eglGetProcAddress( "eglCreateSyncKHR" );
	eglDestroySyncKHR		= (PFNEGLDESTROYSYNCKHRPROC)		eglGetProcAddress( "eglDestroySyncKHR" );
	eglClientWaitSyncKHR	= (PFNEGLCLIENTWAITSYNCKHRPROC)		eglGetProcAddress( "eglClientWaitSyncKHR" );
	eglSignalSyncKHR		= (PFNEGLSIGNALSYNCKHRPROC)			eglGetProcAddress( "eglSignalSyncKHR" );
	eglGetSyncAttribKHR		= (PFNEGLGETSYNCATTRIBKHRPROC)		eglGetProcAddress( "eglGetSyncAttribKHR" );
#endif

	const char * allExtensions = (const char *)glGetString( GL_EXTENSIONS );
	if ( allExtensions != NULL )
	{
		glExtensions.multi_view = strstr( allExtensions, "GL_OVR_multiview2" ) &&
								  strstr( allExtensions, "GL_OVR_multiview_multisampled_render_to_texture" );

		glExtensions.EXT_texture_border_clamp = false;//strstr( allExtensions, "GL_EXT_texture_border_clamp" ) ||
												//strstr( allExtensions, "GL_OES_texture_border_clamp" );
	}
}

static const char * EglErrorString( const EGLint error )
{
	switch ( error )
	{
		case EGL_SUCCESS:				return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED:		return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:				return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT:			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG:			return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE:	return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY:			return "EGL_BAD_DISPLAY";
		case EGL_BAD_SURFACE:			return "EGL_BAD_SURFACE";
		case EGL_BAD_MATCH:				return "EGL_BAD_MATCH";
		case EGL_BAD_PARAMETER:			return "EGL_BAD_PARAMETER";
		case EGL_BAD_NATIVE_PIXMAP:		return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:		return "EGL_BAD_NATIVE_WINDOW";
		case EGL_CONTEXT_LOST:			return "EGL_CONTEXT_LOST";
		default:						return "unknown";
	}
}

static const char * GlFrameBufferStatusString( GLenum status )
{
	switch ( status )
	{
		case GL_FRAMEBUFFER_UNDEFINED:						return "GL_FRAMEBUFFER_UNDEFINED";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:			return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:	return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_UNSUPPORTED:					return "GL_FRAMEBUFFER_UNSUPPORTED";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:			return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
		default:											return "unknown";
	}
}


/*
================================================================================

ovrEgl

================================================================================
*/

static void ovrEgl_Clear( ovrEgl * egl )
{
	egl->MajorVersion = 0;
	egl->MinorVersion = 0;
	egl->Display = 0;
	egl->Config = 0;
	egl->TinySurface = EGL_NO_SURFACE;
	egl->MainSurface = EGL_NO_SURFACE;
	egl->Context = EGL_NO_CONTEXT;
}

static void ovrEgl_CreateContext( ovrEgl * egl, const ovrEgl * shareEgl )
{
	if ( egl->Display != 0 )
	{
		return;
	}

	egl->Display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
	ALOGV( "        eglInitialize( Display, &MajorVersion, &MinorVersion )" );
	eglInitialize( egl->Display, &egl->MajorVersion, &egl->MinorVersion );
	// Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
	// flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
	// settings, and that is completely wasted for our warp target.
	const int MAX_CONFIGS = 1024;
	EGLConfig configs[MAX_CONFIGS];
	EGLint numConfigs = 0;
	if ( eglGetConfigs( egl->Display, configs, MAX_CONFIGS, &numConfigs ) == EGL_FALSE )
	{
		ALOGE( "        eglGetConfigs() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint configAttribs[] =
	{
		EGL_RED_SIZE,		8,
		EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,		8,
		EGL_ALPHA_SIZE,		8, // need alpha for the multi-pass timewarp compositor
		EGL_DEPTH_SIZE,		0,
		EGL_STENCIL_SIZE,	0,
		EGL_SAMPLES,		0,
		EGL_NONE
	};
	egl->Config = 0;
	for ( int i = 0; i < numConfigs; i++ )
	{
		EGLint value = 0;

		eglGetConfigAttrib( egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value );
		if ( ( value & EGL_OPENGL_ES3_BIT_KHR ) != EGL_OPENGL_ES3_BIT_KHR )
		{
			continue;
		}

		// The pbuffer config also needs to be compatible with normal window rendering
		// so it can share textures with the window context.
		eglGetConfigAttrib( egl->Display, configs[i], EGL_SURFACE_TYPE, &value );
		if ( ( value & ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) ) != ( EGL_WINDOW_BIT | EGL_PBUFFER_BIT ) )
		{
			continue;
		}

		int	j = 0;
		for ( ; configAttribs[j] != EGL_NONE; j += 2 )
		{
			eglGetConfigAttrib( egl->Display, configs[i], configAttribs[j], &value );
			if ( value != configAttribs[j + 1] )
			{
				break;
			}
		}
		if ( configAttribs[j] == EGL_NONE )
		{
			egl->Config = configs[i];
			break;
		}
	}
	if ( egl->Config == 0 )
	{
		ALOGE( "        eglChooseConfig() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	EGLint contextAttribs[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE
	};
	ALOGV( "        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )" );
	egl->Context = eglCreateContext( egl->Display, egl->Config, ( shareEgl != NULL ) ? shareEgl->Context : EGL_NO_CONTEXT, contextAttribs );
	if ( egl->Context == EGL_NO_CONTEXT )
	{
		ALOGE( "        eglCreateContext() failed: %s", EglErrorString( eglGetError() ) );
		return;
	}
	const EGLint surfaceAttribs[] =
	{
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
		EGL_NONE
	};
	ALOGV( "        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )" );
	egl->TinySurface = eglCreatePbufferSurface( egl->Display, egl->Config, surfaceAttribs );
	if ( egl->TinySurface == EGL_NO_SURFACE )
	{
		ALOGE( "        eglCreatePbufferSurface() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
	ALOGV( "        eglMakeCurrent( Display, TinySurface, TinySurface, Context )" );
	if ( eglMakeCurrent( egl->Display, egl->TinySurface, egl->TinySurface, egl->Context ) == EGL_FALSE )
	{
		ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		eglDestroySurface( egl->Display, egl->TinySurface );
		eglDestroyContext( egl->Display, egl->Context );
		egl->Context = EGL_NO_CONTEXT;
		return;
	}
}

static void ovrEgl_DestroyContext( ovrEgl * egl )
{
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )" );
		if ( eglMakeCurrent( egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) == EGL_FALSE )
		{
			ALOGE( "        eglMakeCurrent() failed: %s", EglErrorString( eglGetError() ) );
		}
	}
	if ( egl->Context != EGL_NO_CONTEXT )
	{
		ALOGE( "        eglDestroyContext( Display, Context )" );
		if ( eglDestroyContext( egl->Display, egl->Context ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroyContext() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Context = EGL_NO_CONTEXT;
	}
	if ( egl->TinySurface != EGL_NO_SURFACE )
	{
		ALOGE( "        eglDestroySurface( Display, TinySurface )" );
		if ( eglDestroySurface( egl->Display, egl->TinySurface ) == EGL_FALSE )
		{
			ALOGE( "        eglDestroySurface() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->TinySurface = EGL_NO_SURFACE;
	}
	if ( egl->Display != 0 )
	{
		ALOGE( "        eglTerminate( Display )" );
		if ( eglTerminate( egl->Display ) == EGL_FALSE )
		{
			ALOGE( "        eglTerminate() failed: %s", EglErrorString( eglGetError() ) );
		}
		egl->Display = 0;
	}
}

/*
================================================================================

ovrFramebuffer

================================================================================
*/

static void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer) {
    frameBuffer->Width = 0;
    frameBuffer->Height = 0;
    frameBuffer->Multisamples = 0;
    frameBuffer->TextureSwapChainLength = 0;
    frameBuffer->TextureSwapChainIndex = 0;
    frameBuffer->ColorSwapChain.Handle = XR_NULL_HANDLE;
    frameBuffer->ColorSwapChain.Width = 0;
    frameBuffer->ColorSwapChain.Height = 0;
    frameBuffer->ColorSwapChainImage = NULL;
    frameBuffer->DepthBuffers = NULL;
    frameBuffer->FrameBuffers = NULL;
}

typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);


static bool ovrFramebuffer_Create(
        XrSession session,
        ovrFramebuffer* frameBuffer,
        const GLenum colorFormat,
        const int width,
        const int height,
        const int multisamples) {
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
            (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress(
                    "glRenderbufferStorageMultisampleEXT");
    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
            (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress(
                    "glFramebufferTexture2DMultisampleEXT");

    frameBuffer->Width = width;
    frameBuffer->Height = height;
    frameBuffer->Multisamples = multisamples;

    XrSwapchainCreateInfo swapChainCreateInfo;
    memset(&swapChainCreateInfo, 0, sizeof(swapChainCreateInfo));
    swapChainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    swapChainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.format = colorFormat;
    swapChainCreateInfo.sampleCount = 1;
    swapChainCreateInfo.width = width;
    swapChainCreateInfo.height = height;
    swapChainCreateInfo.faceCount = 1;
    swapChainCreateInfo.arraySize = 1;
    swapChainCreateInfo.mipCount = 1;

    frameBuffer->ColorSwapChain.Width = swapChainCreateInfo.width;
    frameBuffer->ColorSwapChain.Height = swapChainCreateInfo.height;

    // Create the swapchain.
    OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &frameBuffer->ColorSwapChain.Handle));
    // Get the number of swapchain images.
    OXR(xrEnumerateSwapchainImages(
            frameBuffer->ColorSwapChain.Handle, 0, &frameBuffer->TextureSwapChainLength, NULL));
    // Allocate the swapchain images array.
    frameBuffer->ColorSwapChainImage = (XrSwapchainImageOpenGLESKHR*)malloc(
            frameBuffer->TextureSwapChainLength * sizeof(XrSwapchainImageOpenGLESKHR));

    // Populate the swapchain image array.
    for (uint32_t i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        frameBuffer->ColorSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
        frameBuffer->ColorSwapChainImage[i].next = NULL;
    }
    OXR(xrEnumerateSwapchainImages(
            frameBuffer->ColorSwapChain.Handle,
            frameBuffer->TextureSwapChainLength,
            &frameBuffer->TextureSwapChainLength,
            (XrSwapchainImageBaseHeader*)frameBuffer->ColorSwapChainImage));

    frameBuffer->DepthBuffers =
            (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
    frameBuffer->FrameBuffers =
            (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

    for (uint32_t i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        // Create the color buffer texture.
        const GLuint colorTexture = frameBuffer->ColorSwapChainImage[i].image;

		GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        GLenum colorTextureTarget = GL_TEXTURE_2D;
		GL(glTexParameterfv(colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor));
        GL(glBindTexture(colorTextureTarget, colorTexture));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glBindTexture(colorTextureTarget, 0));

        if (glRenderbufferStorageMultisampleEXT != NULL &&
            glFramebufferTexture2DMultisampleEXT != NULL) {
            // Create multisampled depth buffer.
            GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
            GL(glRenderbufferStorageMultisampleEXT(
                    GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height));
            GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

            // Create the frame buffer.
            // NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
            GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
            GL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
            GL(glFramebufferTexture2DMultisampleEXT(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D,
                    colorTexture,
                    0,
                    multisamples));
            GL(glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER,
                    frameBuffer->DepthBuffers[i]));
            GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER));
            GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                ALOGE(
                        "Incomplete frame buffer object: %s",
                        GlFrameBufferStatusString(renderFramebufferStatus));
                return false;
            }
        } else {
			return false;
		}
    }

    return true;
}

void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer) {
    GL(glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers));
    GL(glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));
    OXR(xrDestroySwapchain(frameBuffer->ColorSwapChain.Handle));
    free(frameBuffer->ColorSwapChainImage);

    free(frameBuffer->DepthBuffers);
    free(frameBuffer->FrameBuffers);

    ovrFramebuffer_Clear(frameBuffer);
}

void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer) {
    GL(glBindFramebuffer(
            GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex]));
}

void ovrFramebuffer_SetNone() {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer) {

    // Discard the depth buffer, so the tiler won't need to write it back out to memory.
    const GLenum depthAttachment[1] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

    // We now let the resolve happen implicitly.
}

void ovrFramebuffer_Acquire(ovrFramebuffer* frameBuffer) {
    // Acquire the swapchain image
    XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
    OXR(xrAcquireSwapchainImage(
            frameBuffer->ColorSwapChain.Handle, &acquireInfo, &frameBuffer->TextureSwapChainIndex));

    XrSwapchainImageWaitInfo waitInfo;
    waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    waitInfo.next = NULL;
    waitInfo.timeout = 1000000000; /* timeout in nanoseconds */
    XrResult res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
    int i = 0;
    while (res == XR_TIMEOUT_EXPIRED) {
        res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
        i++;
        ALOGV(
                " Retry xrWaitSwapchainImage %d times due to XR_TIMEOUT_EXPIRED (duration %f seconds)",
                i,
                waitInfo.timeout * (1E-9));
    }
}

void ovrFramebuffer_Release(ovrFramebuffer* frameBuffer) {
    XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
    OXR(xrReleaseSwapchainImage(frameBuffer->ColorSwapChain.Handle, &releaseInfo));
}


/*
================================================================================

ovrRenderer

================================================================================
*/

void ovrRenderer_Clear(ovrRenderer* renderer) {
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		ovrFramebuffer_Clear(&renderer->FrameBuffer[eye]);
	}
}

void ovrRenderer_Create(
		XrSession session,
		ovrRenderer* renderer,
		int suggestedEyeTextureWidth,
		int suggestedEyeTextureHeight) {
	// Create the frame buffers.
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		ovrFramebuffer_Create(
				session,
				&renderer->FrameBuffer[eye],
				GL_SRGB8_ALPHA8,
				suggestedEyeTextureWidth,
				suggestedEyeTextureHeight,
				NUM_MULTI_SAMPLES);
	}
}

void ovrRenderer_Destroy(ovrRenderer* renderer) {
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
	}
}



/*
================================================================================

ovrMatrix4f

================================================================================
*/

ovrMatrix4f ovrMatrix4f_CreateFromQuaternion(const XrQuaternionf* q) {
	const float ww = q->w * q->w;
	const float xx = q->x * q->x;
	const float yy = q->y * q->y;
	const float zz = q->z * q->z;

	ovrMatrix4f out;
	out.M[0][0] = ww + xx - yy - zz;
	out.M[0][1] = 2 * (q->x * q->y - q->w * q->z);
	out.M[0][2] = 2 * (q->x * q->z + q->w * q->y);
	out.M[0][3] = 0;

	out.M[1][0] = 2 * (q->x * q->y + q->w * q->z);
	out.M[1][1] = ww - xx + yy - zz;
	out.M[1][2] = 2 * (q->y * q->z - q->w * q->x);
	out.M[1][3] = 0;

	out.M[2][0] = 2 * (q->x * q->z - q->w * q->y);
	out.M[2][1] = 2 * (q->y * q->z + q->w * q->x);
	out.M[2][2] = ww - xx - yy + zz;
	out.M[2][3] = 0;

	out.M[3][0] = 0;
	out.M[3][1] = 0;
	out.M[3][2] = 0;
	out.M[3][3] = 1;
	return out;
}


/// Use left-multiplication to accumulate transformations.
ovrMatrix4f ovrMatrix4f_Multiply(const ovrMatrix4f* a, const ovrMatrix4f* b) {
	ovrMatrix4f out;
	out.M[0][0] = a->M[0][0] * b->M[0][0] + a->M[0][1] * b->M[1][0] + a->M[0][2] * b->M[2][0] +
				  a->M[0][3] * b->M[3][0];
	out.M[1][0] = a->M[1][0] * b->M[0][0] + a->M[1][1] * b->M[1][0] + a->M[1][2] * b->M[2][0] +
				  a->M[1][3] * b->M[3][0];
	out.M[2][0] = a->M[2][0] * b->M[0][0] + a->M[2][1] * b->M[1][0] + a->M[2][2] * b->M[2][0] +
				  a->M[2][3] * b->M[3][0];
	out.M[3][0] = a->M[3][0] * b->M[0][0] + a->M[3][1] * b->M[1][0] + a->M[3][2] * b->M[2][0] +
				  a->M[3][3] * b->M[3][0];

	out.M[0][1] = a->M[0][0] * b->M[0][1] + a->M[0][1] * b->M[1][1] + a->M[0][2] * b->M[2][1] +
				  a->M[0][3] * b->M[3][1];
	out.M[1][1] = a->M[1][0] * b->M[0][1] + a->M[1][1] * b->M[1][1] + a->M[1][2] * b->M[2][1] +
				  a->M[1][3] * b->M[3][1];
	out.M[2][1] = a->M[2][0] * b->M[0][1] + a->M[2][1] * b->M[1][1] + a->M[2][2] * b->M[2][1] +
				  a->M[2][3] * b->M[3][1];
	out.M[3][1] = a->M[3][0] * b->M[0][1] + a->M[3][1] * b->M[1][1] + a->M[3][2] * b->M[2][1] +
				  a->M[3][3] * b->M[3][1];

	out.M[0][2] = a->M[0][0] * b->M[0][2] + a->M[0][1] * b->M[1][2] + a->M[0][2] * b->M[2][2] +
				  a->M[0][3] * b->M[3][2];
	out.M[1][2] = a->M[1][0] * b->M[0][2] + a->M[1][1] * b->M[1][2] + a->M[1][2] * b->M[2][2] +
				  a->M[1][3] * b->M[3][2];
	out.M[2][2] = a->M[2][0] * b->M[0][2] + a->M[2][1] * b->M[1][2] + a->M[2][2] * b->M[2][2] +
				  a->M[2][3] * b->M[3][2];
	out.M[3][2] = a->M[3][0] * b->M[0][2] + a->M[3][1] * b->M[1][2] + a->M[3][2] * b->M[2][2] +
				  a->M[3][3] * b->M[3][2];

	out.M[0][3] = a->M[0][0] * b->M[0][3] + a->M[0][1] * b->M[1][3] + a->M[0][2] * b->M[2][3] +
				  a->M[0][3] * b->M[3][3];
	out.M[1][3] = a->M[1][0] * b->M[0][3] + a->M[1][1] * b->M[1][3] + a->M[1][2] * b->M[2][3] +
				  a->M[1][3] * b->M[3][3];
	out.M[2][3] = a->M[2][0] * b->M[0][3] + a->M[2][1] * b->M[1][3] + a->M[2][2] * b->M[2][3] +
				  a->M[2][3] * b->M[3][3];
	out.M[3][3] = a->M[3][0] * b->M[0][3] + a->M[3][1] * b->M[1][3] + a->M[3][2] * b->M[2][3] +
				  a->M[3][3] * b->M[3][3];
	return out;
}

ovrMatrix4f ovrMatrix4f_CreateRotation(const float radiansX, const float radiansY, const float radiansZ) {
	const float sinX = sinf(radiansX);
	const float cosX = cosf(radiansX);
	const ovrMatrix4f rotationX = {
			{{1, 0, 0, 0}, {0, cosX, -sinX, 0}, {0, sinX, cosX, 0}, {0, 0, 0, 1}}};
	const float sinY = sinf(radiansY);
	const float cosY = cosf(radiansY);
	const ovrMatrix4f rotationY = {
			{{cosY, 0, sinY, 0}, {0, 1, 0, 0}, {-sinY, 0, cosY, 0}, {0, 0, 0, 1}}};
	const float sinZ = sinf(radiansZ);
	const float cosZ = cosf(radiansZ);
	const ovrMatrix4f rotationZ = {
			{{cosZ, -sinZ, 0, 0}, {sinZ, cosZ, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
	const ovrMatrix4f rotationXY = ovrMatrix4f_Multiply(&rotationY, &rotationX);
	return ovrMatrix4f_Multiply(&rotationZ, &rotationXY);
}

XrVector4f XrVector4f_MultiplyMatrix4f(const ovrMatrix4f* a, const XrVector4f* v) {
	XrVector4f out;
	out.x = a->M[0][0] * v->x + a->M[0][1] * v->y + a->M[0][2] * v->z + a->M[0][3] * v->w;
	out.y = a->M[1][0] * v->x + a->M[1][1] * v->y + a->M[1][2] * v->z + a->M[1][3] * v->w;
	out.z = a->M[2][0] * v->x + a->M[2][1] * v->y + a->M[2][2] * v->z + a->M[2][3] * v->w;
	out.w = a->M[3][0] * v->x + a->M[3][1] * v->y + a->M[3][2] * v->z + a->M[3][3] * v->w;
	return out;
}

#ifndef EPSILON
#define EPSILON 0.001f
#endif

static XrVector3f normalizeVec(XrVector3f vec) {
    //NOTE: leave w-component untouched
    //@@const float EPSILON = 0.000001f;
    float xxyyzz = vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;
    //@@if(xxyyzz < EPSILON)
    //@@    return *this; // do nothing if it is zero vector

    //float invLength = invSqrt(xxyyzz);
	XrVector3f result;
    float invLength = 1.0f / sqrtf(xxyyzz);
    result.x = vec.x * invLength;
    result.y = vec.y * invLength;
    result.z = vec.z * invLength;
    return result;
}

void NormalizeAngles(vec3_t angles)
{
	while (angles[0] >= 90) angles[0] -= 180;
	while (angles[1] >= 180) angles[1] -= 360;
	while (angles[2] >= 180) angles[2] -= 360;
	while (angles[0] < -90) angles[0] += 180;
	while (angles[1] < -180) angles[1] += 360;
	while (angles[2] < -180) angles[2] += 360;
}

void GetAnglesFromVectors(const XrVector3f forward, const XrVector3f right, const XrVector3f up, vec3_t angles)
{
	float sr, sp, sy, cr, cp, cy;

	sp = -forward.z;

	float cp_x_cy = forward.x;
	float cp_x_sy = forward.y;
	float cp_x_sr = -right.z;
	float cp_x_cr = up.z;

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (fabs(cy) > EPSILON)
	{
	cp = cp_x_cy / cy;
	}
	else if (fabs(sy) > EPSILON)
	{
	cp = cp_x_sy / sy;
	}
	else if (fabs(sr) > EPSILON)
	{
	cp = cp_x_sr / sr;
	}
	else if (fabs(cr) > EPSILON)
	{
	cp = cp_x_cr / cr;
	}
	else
	{
	cp = cos(asin(sp));
	}

	float pitch = atan2(sp, cp);

	angles[0] = pitch / (M_PI*2.f / 360.f);
	angles[1] = yaw / (M_PI*2.f / 360.f);
	angles[2] = roll / (M_PI*2.f / 360.f);

	NormalizeAngles(angles);
}


void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out) {

	ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion( &q );

	if (rotation[0] != 0.0f || rotation[1] != 0.0f || rotation[2] != 0.0f)
	{
		ovrMatrix4f rot = ovrMatrix4f_CreateRotation(DEG2RAD(rotation[0]), DEG2RAD(rotation[1]), DEG2RAD(rotation[2]));
		mat = ovrMatrix4f_Multiply(&mat, &rot);
	}

	XrVector4f v1 = {0, 0, -1, 0};
	XrVector4f v2 = {1, 0, 0, 0};
	XrVector4f v3 = {0, 1, 0, 0};

	XrVector4f forwardInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v1);
	XrVector4f rightInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v2);
	XrVector4f upInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v3);

	XrVector3f forward = {-forwardInVRSpace.z, -forwardInVRSpace.x, forwardInVRSpace.y};
	XrVector3f right = {-rightInVRSpace.z, -rightInVRSpace.x, rightInVRSpace.y};
	XrVector3f up = {-upInVRSpace.z, -upInVRSpace.x, upInVRSpace.y};

	XrVector3f forwardNormal = normalizeVec(forward);
	XrVector3f rightNormal = normalizeVec(right);
	XrVector3f upNormal = normalizeVec(up);

	GetAnglesFromVectors(forwardNormal, rightNormal, upNormal, out);
}

/*
========================
TBXR_Vibrate
========================
*/

void TBXR_Vibrate( int duration, int chan, float intensity );

/*
================================================================================

ovrRenderThread

================================================================================
*/



void ovrApp_Clear(ovrApp* app) {
	app->Focused = false;
	app->Visible = false;
	app->Instance = XR_NULL_HANDLE;
	app->Session = XR_NULL_HANDLE;
	memset(&app->ViewportConfig, 0, sizeof(XrViewConfigurationProperties));
	memset(&app->ViewConfigurationView, 0, ovrMaxNumEyes * sizeof(XrViewConfigurationView));
	app->SystemId = XR_NULL_SYSTEM_ID;
	app->HeadSpace = XR_NULL_HANDLE;
	app->StageSpace = XR_NULL_HANDLE;
	app->FakeStageSpace = XR_NULL_HANDLE;
	app->CurrentSpace = XR_NULL_HANDLE;
	app->SessionActive = false;
	app->SupportedDisplayRefreshRates = NULL;
	app->RequestedDisplayRefreshRateIndex = 0;
	app->NumSupportedDisplayRefreshRates = 0;
	app->pfnGetDisplayRefreshRate = NULL;
	app->pfnRequestDisplayRefreshRate = NULL;
	app->SwapInterval = 1;
	memset(app->Layers, 0, sizeof(xrCompositorLayer_Union) * ovrMaxLayerCount);
	app->LayerCount = 0;
	app->MainThreadTid = 0;
	app->RenderThreadTid = 0;
	ovrEgl_Clear( &app->Egl );
	ovrRenderer_Clear(&app->Renderer);
}



void ovrApp_HandleSessionStateChanges(ovrApp* app, XrSessionState state) {
	if (state == XR_SESSION_STATE_READY) {
		assert(app->SessionActive == false);

		XrSessionBeginInfo sessionBeginInfo;
		memset(&sessionBeginInfo, 0, sizeof(sessionBeginInfo));
		sessionBeginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
		sessionBeginInfo.next = NULL;
		sessionBeginInfo.primaryViewConfigurationType = app->ViewportConfig.viewConfigurationType;

		XrResult result;
		OXR(result = xrBeginSession(app->Session, &sessionBeginInfo));

		app->SessionActive = (result == XR_SUCCESS);

		// Set session state once we have entered VR mode and have a valid session object.
		if (app->SessionActive)
		{
			XrPerfSettingsLevelEXT cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
			XrPerfSettingsLevelEXT gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;

			PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
			OXR(xrGetInstanceProcAddr(
					app->Instance,
					"xrPerfSettingsSetPerformanceLevelEXT",
					(PFN_xrVoidFunction * )(&pfnPerfSettingsSetPerformanceLevelEXT)));

			OXR(pfnPerfSettingsSetPerformanceLevelEXT(
					app->Session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, cpuPerfLevel));
			OXR(pfnPerfSettingsSetPerformanceLevelEXT(
					app->Session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, gpuPerfLevel));

            if (strstr(gAppState.OpenXRHMD, "meta") != NULL)
            {
                PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
                OXR(xrGetInstanceProcAddr(
                        app->Instance,
                        "xrSetAndroidApplicationThreadKHR",
                        (PFN_xrVoidFunction * )(&pfnSetAndroidApplicationThreadKHR)));

                OXR(pfnSetAndroidApplicationThreadKHR(
                        app->Session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, app->MainThreadTid));
                OXR(pfnSetAndroidApplicationThreadKHR(
                        app->Session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR, app->RenderThreadTid));
            }
		}
	} else if (state == XR_SESSION_STATE_STOPPING) {
		assert(app->SessionActive);

		OXR(xrEndSession(app->Session));
		app->SessionActive = false;
	}
}

GLboolean ovrApp_HandleXrEvents(ovrApp* app) {
	XrEventDataBuffer eventDataBuffer = {};
	GLboolean recenter = GL_FALSE;

	// Poll for events
	for (;;) {
		XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
		baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
		baseEventHeader->next = NULL;
		XrResult r;
		OXR(r = xrPollEvent(app->Instance, &eventDataBuffer));
		if (r != XR_SUCCESS) {
			break;
		}

		switch (baseEventHeader->type) {
			case XR_TYPE_EVENT_DATA_EVENTS_LOST:
				ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
				break;
			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
				const XrEventDataInstanceLossPending* instance_loss_pending_event =
						(XrEventDataInstanceLossPending*)(baseEventHeader);
				ALOGV(
						"xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event: time %f",
						FromXrTime(instance_loss_pending_event->lossTime));
			} break;
			case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
				ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
				break;
			case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
				const XrEventDataPerfSettingsEXT* perf_settings_event =
						(XrEventDataPerfSettingsEXT*)(baseEventHeader);
				ALOGV(
						"xrPollEvent: received XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event: type %d subdomain %d : level %d -> level %d",
						perf_settings_event->type,
						perf_settings_event->subDomain,
						perf_settings_event->fromLevel,
						perf_settings_event->toLevel);
			} break;
			case XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB: {
				const XrEventDataDisplayRefreshRateChangedFB* refresh_rate_changed_event =
						(XrEventDataDisplayRefreshRateChangedFB*)(baseEventHeader);
				ALOGV(
						"xrPollEvent: received XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB event: fromRate %f -> toRate %f",
						refresh_rate_changed_event->fromDisplayRefreshRate,
						refresh_rate_changed_event->toDisplayRefreshRate);
			} break;
			case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
				XrEventDataReferenceSpaceChangePending* ref_space_change_event =
						(XrEventDataReferenceSpaceChangePending*)(baseEventHeader);
				ALOGV(
						"xrPollEvent: received XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING event: changed space: %d for session %p at time %f",
						ref_space_change_event->referenceSpaceType,
						(void*)ref_space_change_event->session,
						FromXrTime(ref_space_change_event->changeTime));
				recenter = GL_TRUE;
			} break;
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
				const XrEventDataSessionStateChanged* session_state_changed_event =
						(XrEventDataSessionStateChanged*)(baseEventHeader);
				ALOGV(
						"xrPollEvent: received XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: %d for session %p at time %f",
						session_state_changed_event->state,
						(void*)session_state_changed_event->session,
						FromXrTime(session_state_changed_event->time));

				switch (session_state_changed_event->state) {
					case XR_SESSION_STATE_FOCUSED:
						app->Focused = true;
						break;
					case XR_SESSION_STATE_VISIBLE:
						app->Visible = true;
						break;
					case XR_SESSION_STATE_READY:
					case XR_SESSION_STATE_STOPPING:
						ovrApp_HandleSessionStateChanges(app, session_state_changed_event->state);
						break;
					default:
						break;
				}
			} break;
			default:
				ALOGV("xrPollEvent: Unknown event");
				break;
		}
	}
	return recenter;
}

void ovrAppThread_Create( ovrAppThread * appThread, JNIEnv * env, jobject activityObject, jclass activityClass )
{
	(*env)->GetJavaVM( env, &appThread->JavaVm );
	appThread->ActivityObject = (*env)->NewGlobalRef( env, activityObject );
	appThread->ActivityClass = (*env)->NewGlobalRef( env, activityClass );
	appThread->Thread = 0;
	appThread->NativeWindow = NULL;
	surfaceMessageQueue_Create(&appThread->MessageQueue);

	const int createErr = pthread_create( &appThread->Thread, NULL, AppThreadFunction, appThread );
	if ( createErr != 0 )
	{
		ALOGE( "pthread_create returned %i", createErr );
	}
}

void ovrAppThread_Destroy( ovrAppThread * appThread, JNIEnv * env )
{
	pthread_join( appThread->Thread, NULL );
	(*env)->DeleteGlobalRef( env, appThread->ActivityObject );
	(*env)->DeleteGlobalRef( env, appThread->ActivityClass );
	surfaceMessageQueue_Destroy(&appThread->MessageQueue);
}


/*
================================================================================

surfaceMessageQueue

================================================================================
*/


void surfaceMessage_Init(surfaceMessage * message, const int id, const int wait )
{
	message->Id = id;
	message->Wait = (ovrMQWait)wait;
	memset( message->Parms, 0, sizeof( message->Parms ) );
}

void		surfaceMessage_SetPointerParm(surfaceMessage * message, int index, void * ptr ) { *(void **)&message->Parms[index] = ptr; }
void *	surfaceMessage_GetPointerParm(surfaceMessage * message, int index ) { return *(void **)&message->Parms[index]; }


void surfaceMessageQueue_Create(surfaceMessageQueue * messageQueue )
{
	messageQueue->Head = 0;
	messageQueue->Tail = 0;
	messageQueue->Wait = MQ_WAIT_NONE;
	messageQueue->EnabledFlag = false;
	messageQueue->PostedFlag = false;
	messageQueue->ReceivedFlag = false;
	messageQueue->ProcessedFlag = false;

	pthread_mutexattr_t	attr;
	pthread_mutexattr_init( &attr );
	pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
	pthread_mutex_init( &messageQueue->Mutex, &attr );
	pthread_mutexattr_destroy( &attr );
	pthread_cond_init( &messageQueue->PostedCondition, NULL );
	pthread_cond_init( &messageQueue->ReceivedCondition, NULL );
	pthread_cond_init( &messageQueue->ProcessedCondition, NULL );
}

void surfaceMessageQueue_Destroy(surfaceMessageQueue * messageQueue )
{
	pthread_mutex_destroy( &messageQueue->Mutex );
	pthread_cond_destroy( &messageQueue->PostedCondition );
	pthread_cond_destroy( &messageQueue->ReceivedCondition );
	pthread_cond_destroy( &messageQueue->ProcessedCondition );
}

void surfaceMessageQueue_Enable(surfaceMessageQueue * messageQueue, const bool set )
{
	messageQueue->EnabledFlag = set;
}

void surfaceMessageQueue_PostMessage(surfaceMessageQueue * messageQueue, const surfaceMessage * message )
{
	if ( !messageQueue->EnabledFlag )
	{
		return;
	}
	while ( messageQueue->Tail - messageQueue->Head >= MAX_MESSAGES )
	{
		usleep( 1000 );
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	messageQueue->Messages[messageQueue->Tail & ( MAX_MESSAGES - 1 )] = *message;
	messageQueue->Tail++;
	messageQueue->PostedFlag = true;
	pthread_cond_broadcast( &messageQueue->PostedCondition );
	if ( message->Wait == MQ_WAIT_RECEIVED )
	{
		while ( !messageQueue->ReceivedFlag )
		{
			pthread_cond_wait( &messageQueue->ReceivedCondition, &messageQueue->Mutex );
		}
		messageQueue->ReceivedFlag = false;
	}
	else if ( message->Wait == MQ_WAIT_PROCESSED )
	{
		while ( !messageQueue->ProcessedFlag )
		{
			pthread_cond_wait( &messageQueue->ProcessedCondition, &messageQueue->Mutex );
		}
		messageQueue->ProcessedFlag = false;
	}
	pthread_mutex_unlock( &messageQueue->Mutex );
}

static void ovrMessageQueue_SleepUntilMessage(surfaceMessageQueue * messageQueue )
{
	if ( messageQueue->Wait == MQ_WAIT_PROCESSED )
	{
		messageQueue->ProcessedFlag = true;
		pthread_cond_broadcast( &messageQueue->ProcessedCondition );
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	if ( messageQueue->Tail > messageQueue->Head )
	{
		pthread_mutex_unlock( &messageQueue->Mutex );
		return;
	}
	while ( !messageQueue->PostedFlag )
	{
		pthread_cond_wait( &messageQueue->PostedCondition, &messageQueue->Mutex );
	}
	messageQueue->PostedFlag = false;
	pthread_mutex_unlock( &messageQueue->Mutex );
}

static bool surfaceMessageQueue_GetNextMessage(surfaceMessageQueue * messageQueue, surfaceMessage * message, bool waitForMessages )
{
	if ( messageQueue->Wait == MQ_WAIT_PROCESSED )
	{
		messageQueue->ProcessedFlag = true;
		pthread_cond_broadcast( &messageQueue->ProcessedCondition );
		messageQueue->Wait = MQ_WAIT_NONE;
	}
	if ( waitForMessages )
	{
		ovrMessageQueue_SleepUntilMessage( messageQueue );
	}
	pthread_mutex_lock( &messageQueue->Mutex );
	if ( messageQueue->Tail <= messageQueue->Head )
	{
		pthread_mutex_unlock( &messageQueue->Mutex );
		return false;
	}
	*message = messageQueue->Messages[messageQueue->Head & ( MAX_MESSAGES - 1 )];
	messageQueue->Head++;
	pthread_mutex_unlock( &messageQueue->Mutex );
	if ( message->Wait == MQ_WAIT_RECEIVED )
	{
		messageQueue->ReceivedFlag = true;
		pthread_cond_broadcast( &messageQueue->ReceivedCondition );
	}
	else if ( message->Wait == MQ_WAIT_PROCESSED )
	{
		messageQueue->Wait = MQ_WAIT_PROCESSED;
	}
	return true;
}

ovrAppThread * gAppThread = NULL;
ovrApp gAppState;
ovrJava java;
bool destroyed = false;

void TBXR_GetScreenRes(int *width, int *height)
{
	*width = gAppState.Width;
	*height = gAppState.Height;
}

XrInstance TBXR_GetXrInstance() {
	return gAppState.Instance;
}

static void TBXR_ProcessMessageQueue() {
	for ( ; ; )
	{
		surfaceMessage message;
		if ( !surfaceMessageQueue_GetNextMessage(&gAppThread->MessageQueue, &message, false) )
		{
			break;
		}

		switch ( message.Id )
		{
			case MESSAGE_ON_CREATE:
			{
				break;
			}
			case MESSAGE_ON_START:
			{
				break;
			}
			case MESSAGE_ON_RESUME:
			{
				//If we get here, then user has opted not to quit
				gAppState.Resumed = true;
				break;
			}
			case MESSAGE_ON_PAUSE:
			{
				gAppState.Resumed = false;
				break;
			}
			case MESSAGE_ON_STOP:
			{
				break;
			}
			case MESSAGE_ON_DESTROY:
			{
				gAppState.NativeWindow = NULL;
				destroyed = true;
				//shutdown = true;
				break;
			}
			case MESSAGE_ON_SURFACE_CREATED:	{ gAppState.NativeWindow = (ANativeWindow *) surfaceMessage_GetPointerParm(
						&message, 0); break; }
			case MESSAGE_ON_SURFACE_DESTROYED:	{ gAppState.NativeWindow = NULL; break; }
		}
	}
}

void ovrTrackedController_Clear(ovrTrackedController* controller) {
	controller->Active = false;
	controller->Pose = XrPosef_Identity();
}

void TBXR_InitialiseResolution()
{
	// Enumerate the viewport configurations.
	uint32_t viewportConfigTypeCount = 0;
	OXR(xrEnumerateViewConfigurations(
			gAppState.Instance, gAppState.SystemId, 0, &viewportConfigTypeCount, NULL));

	XrViewConfigurationType* viewportConfigurationTypes =
			(XrViewConfigurationType*)malloc(viewportConfigTypeCount * sizeof(XrViewConfigurationType));

	OXR(xrEnumerateViewConfigurations(
			gAppState.Instance,
			gAppState.SystemId,
			viewportConfigTypeCount,
			&viewportConfigTypeCount,
			viewportConfigurationTypes));

	ALOGV("Available Viewport Configuration Types: %d", viewportConfigTypeCount);

	for (uint32_t i = 0; i < viewportConfigTypeCount; i++) {
		const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

		ALOGV(
				"Viewport configuration type %d : %s",
				viewportConfigType,
				viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO ? "Selected" : "");

		XrViewConfigurationProperties viewportConfig;
		viewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
		OXR(xrGetViewConfigurationProperties(
				gAppState.Instance, gAppState.SystemId, viewportConfigType, &viewportConfig));
		ALOGV(
				"FovMutable=%s ConfigurationType %d",
				viewportConfig.fovMutable ? "true" : "false",
				viewportConfig.viewConfigurationType);

		uint32_t viewCount;
		OXR(xrEnumerateViewConfigurationViews(
				gAppState.Instance, gAppState.SystemId, viewportConfigType, 0, &viewCount, NULL));

		if (viewCount > 0) {
			XrViewConfigurationView* elements =
					(XrViewConfigurationView*)malloc(viewCount * sizeof(XrViewConfigurationView));

			for (uint32_t e = 0; e < viewCount; e++) {
				elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
				elements[e].next = NULL;
			}

			OXR(xrEnumerateViewConfigurationViews(
					gAppState.Instance,
					gAppState.SystemId,
					viewportConfigType,
					viewCount,
					&viewCount,
					elements));

			// Cache the view config properties for the selected config type.
			if (viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
				assert(viewCount == ovrMaxNumEyes);
				for (uint32_t e = 0; e < viewCount; e++) {
					gAppState.ViewConfigurationView[e] = elements[e];
				}
			}

			free(elements);
		} else {
			ALOGE("Empty viewport configuration type: %d", viewCount);
		}
	}

	free(viewportConfigurationTypes);

	//Shortcut to width and height
	gAppState.Width = gAppState.ViewConfigurationView[0].recommendedImageRectWidth * SS_MULTIPLIER;
	gAppState.Height = gAppState.ViewConfigurationView[0].recommendedImageRectHeight * SS_MULTIPLIER;
}

void TBXR_EnterVR( ) {

	if (gAppState.Session) {
		//Com_Printf("TBXR_EnterVR called with existing session");
		return;
	}

	// Create the OpenXR Session.
	XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingAndroidGLES = {};
	graphicsBindingAndroidGLES.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
	graphicsBindingAndroidGLES.next = NULL;
	graphicsBindingAndroidGLES.display = eglGetCurrentDisplay();
	graphicsBindingAndroidGLES.config = eglGetCurrentSurface(EGL_DRAW);
	graphicsBindingAndroidGLES.context = eglGetCurrentContext();

	XrSessionCreateInfo sessionCreateInfo = {};
	memset(&sessionCreateInfo, 0, sizeof(sessionCreateInfo));
	sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	sessionCreateInfo.next = &graphicsBindingAndroidGLES;
	sessionCreateInfo.createFlags = 0;
	sessionCreateInfo.systemId = gAppState.SystemId;

	XrResult initResult;
	OXR(initResult = xrCreateSession(gAppState.Instance, &sessionCreateInfo, &gAppState.Session));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to create XR session: %d.", initResult);
		exit(1);
	}

	// Create a space to the first path
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	OXR(xrCreateReferenceSpace(gAppState.Session, &spaceCreateInfo, &gAppState.HeadSpace));
}

void TBXR_LeaveVR( ) {
	if (gAppState.Session) {
		OXR(xrDestroySpace(gAppState.HeadSpace));
		// StageSpace is optional.
		if (gAppState.StageSpace != XR_NULL_HANDLE) {
			OXR(xrDestroySpace(gAppState.StageSpace));
		}
		OXR(xrDestroySpace(gAppState.FakeStageSpace));
		gAppState.CurrentSpace = XR_NULL_HANDLE;
		OXR(xrDestroySession(gAppState.Session));
		gAppState.Session = NULL;
	}

	ovrRenderer_Destroy( &gAppState.Renderer );
	ovrEgl_DestroyContext( &gAppState.Egl );
	(*java.Vm)->DetachCurrentThread( java.Vm );
}

void TBXR_InitRenderer(  ) {
	// Get the viewport configuration info for the chosen viewport configuration type.
	gAppState.ViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;

	OXR(xrGetViewConfigurationProperties(
			gAppState.Instance, gAppState.SystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &gAppState.ViewportConfig));


    if (strstr(gAppState.OpenXRHMD, "meta") != NULL)
    {
        XrSystemColorSpacePropertiesFB colorSpacePropertiesFB = {};
        colorSpacePropertiesFB.type = XR_TYPE_SYSTEM_COLOR_SPACE_PROPERTIES_FB;

        XrSystemProperties systemProperties = {};
        systemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
        systemProperties.next = &colorSpacePropertiesFB;
        OXR(xrGetSystemProperties(gAppState.Instance, gAppState.SystemId, &systemProperties));

        // Enumerate the supported color space options for the system.
        {
            PFN_xrEnumerateColorSpacesFB pfnxrEnumerateColorSpacesFB = NULL;
            OXR(xrGetInstanceProcAddr(
                    gAppState.Instance,
                    "xrEnumerateColorSpacesFB",
                    (PFN_xrVoidFunction*)(&pfnxrEnumerateColorSpacesFB)));

            uint32_t colorSpaceCountOutput = 0;
            OXR(pfnxrEnumerateColorSpacesFB(gAppState.Session, 0, &colorSpaceCountOutput, NULL));

            XrColorSpaceFB* colorSpaces =
                    (XrColorSpaceFB*)malloc(colorSpaceCountOutput * sizeof(XrColorSpaceFB));

            OXR(pfnxrEnumerateColorSpacesFB(
                    gAppState.Session, colorSpaceCountOutput, &colorSpaceCountOutput, colorSpaces));
            ALOGV("Supported ColorSpaces:");

            for (uint32_t i = 0; i < colorSpaceCountOutput; i++) {
                ALOGV("%d:%d", i, colorSpaces[i]);
            }

            const XrColorSpaceFB requestColorSpace = XR_COLOR_SPACE_REC2020_FB;

            PFN_xrSetColorSpaceFB pfnxrSetColorSpaceFB = NULL;
            OXR(xrGetInstanceProcAddr(
                    gAppState.Instance, "xrSetColorSpaceFB", (PFN_xrVoidFunction*)(&pfnxrSetColorSpaceFB)));

            OXR(pfnxrSetColorSpaceFB(gAppState.Session, requestColorSpace));

            free(colorSpaces);
        }

        // Get the supported display refresh rates for the system.
        {
            PFN_xrEnumerateDisplayRefreshRatesFB pfnxrEnumerateDisplayRefreshRatesFB = NULL;
            OXR(xrGetInstanceProcAddr(
                    gAppState.Instance,
                    "xrEnumerateDisplayRefreshRatesFB",
                    (PFN_xrVoidFunction*)(&pfnxrEnumerateDisplayRefreshRatesFB)));

            OXR(pfnxrEnumerateDisplayRefreshRatesFB(
                    gAppState.Session, 0, &gAppState.NumSupportedDisplayRefreshRates, NULL));

            gAppState.SupportedDisplayRefreshRates =
                    (float*)malloc(gAppState.NumSupportedDisplayRefreshRates * sizeof(float));
            OXR(pfnxrEnumerateDisplayRefreshRatesFB(
                    gAppState.Session,
                    gAppState.NumSupportedDisplayRefreshRates,
                    &gAppState.NumSupportedDisplayRefreshRates,
                    gAppState.SupportedDisplayRefreshRates));
            ALOGV("Supported Refresh Rates:");
            for (uint32_t i = 0; i < gAppState.NumSupportedDisplayRefreshRates; i++) {
                ALOGV("%d:%f", i, gAppState.SupportedDisplayRefreshRates[i]);
            }

            OXR(xrGetInstanceProcAddr(
                    gAppState.Instance,
                    "xrGetDisplayRefreshRateFB",
                    (PFN_xrVoidFunction*)(&gAppState.pfnGetDisplayRefreshRate)));

            OXR(gAppState.pfnGetDisplayRefreshRate(gAppState.Session, &gAppState.currentDisplayRefreshRate));
            ALOGV("Current System Display Refresh Rate: %f", gAppState.currentDisplayRefreshRate);

            OXR(xrGetInstanceProcAddr(
                    gAppState.Instance,
                    "xrRequestDisplayRefreshRateFB",
                    (PFN_xrVoidFunction*)(&gAppState.pfnRequestDisplayRefreshRate)));

            // Test requesting the system default.
            OXR(gAppState.pfnRequestDisplayRefreshRate(gAppState.Session, 0.0f));
            ALOGV("Requesting system default display refresh rate");
        }
    }

	uint32_t numOutputSpaces = 0;
	OXR(xrEnumerateReferenceSpaces(gAppState.Session, 0, &numOutputSpaces, NULL));

	XrReferenceSpaceType* referenceSpaces =
			(XrReferenceSpaceType*)malloc(numOutputSpaces * sizeof(XrReferenceSpaceType));

	OXR(xrEnumerateReferenceSpaces(
			gAppState.Session, numOutputSpaces, &numOutputSpaces, referenceSpaces));

	for (uint32_t i = 0; i < numOutputSpaces; i++) {
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
			stageSupported = GL_TRUE;
			break;
		}
	}

	free(referenceSpaces);

	if (gAppState.CurrentSpace == XR_NULL_HANDLE) {
		TBXR_Recenter();
	}

    gAppState.Projections = (XrView*)(malloc(ovrMaxNumEyes * sizeof(XrView)));
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		memset(&gAppState.Projections[eye], 0, sizeof(XrView));
        gAppState.Projections[eye].type = XR_TYPE_VIEW;
	}

    if (strstr(gAppState.OpenXRHMD, "pico") != NULL)
    {
        xrGetInstanceProcAddr(gAppState.Instance,"xrSetConfigPICO", (PFN_xrVoidFunction*)(&pfnXrSetConfigPICO));
        xrGetInstanceProcAddr(gAppState.Instance,"xrGetConfigPICO", (PFN_xrVoidFunction*)(&pfnXrGetConfigPICO));

        pfnXrSetConfigPICO(gAppState.Session,TRACKING_ORIGIN,"0");
        pfnXrSetConfigPICO(gAppState.Session,TRACKING_ORIGIN,"1");

        pfnXrGetConfigPICO(gAppState.Session, GET_DISPLAY_RATE, &gAppState.currentDisplayRefreshRate);
    }

	ovrRenderer_Create(
			gAppState.Session,
			&gAppState.Renderer,
			gAppState.Width,
			gAppState.Height);
}

void VR_DestroyRenderer(  )
{
	ovrRenderer_Destroy(&gAppState.Renderer);
	free(gAppState.Projections);
}

void TBXR_InitialiseOpenXR()
{
	ovrApp_Clear(&gAppState);
	gAppState.Java = java;

	ovrEgl_CreateContext(&gAppState.Egl, NULL);
	EglInitExtensions();

    //First, find out which HMD we are using
    gAppState.OpenXRHMD = (char*)getenv("OPENXR_HMD");

	PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
	xrGetInstanceProcAddr(
			XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
	if (xrInitializeLoaderKHR != NULL) {
		XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid;
		memset(&loaderInitializeInfoAndroid, 0, sizeof(loaderInitializeInfoAndroid));
		loaderInitializeInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
		loaderInitializeInfoAndroid.next = NULL;
		loaderInitializeInfoAndroid.applicationVM = java.Vm;
		loaderInitializeInfoAndroid.applicationContext = java.ActivityObject;
		xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
	}

	// Create the OpenXR instance.
	XrApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	strcpy(appInfo.applicationName, "QuakeQuest");
	appInfo.applicationVersion = 0;
	strcpy(appInfo.engineName, "QuakeQuest");
	appInfo.engineVersion = 0;
	appInfo.apiVersion = XR_CURRENT_API_VERSION;

	XrInstanceCreateInfo instanceCreateInfo;
	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;

    XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
	instanceCreateInfoAndroid.applicationVM = java.Vm;
	instanceCreateInfoAndroid.applicationActivity = java.ActivityObject;

	instanceCreateInfo.next = (XrBaseInStructure*)&instanceCreateInfoAndroid;

	instanceCreateInfo.createFlags = 0;
	instanceCreateInfo.applicationInfo = appInfo;
	instanceCreateInfo.enabledApiLayerCount = 0;
	instanceCreateInfo.enabledApiLayerNames = NULL;

    if (strstr(gAppState.OpenXRHMD, "meta") != NULL)
    {
        instanceCreateInfo.enabledExtensionCount = numRequiredExtensions_meta;
        instanceCreateInfo.enabledExtensionNames = requiredExtensionNames_meta;
    }
    else
    {
        instanceCreateInfo.enabledExtensionCount = numRequiredExtensions_pico;
        instanceCreateInfo.enabledExtensionNames = requiredExtensionNames_pico;
    }

	XrResult initResult;
	OXR(initResult = xrCreateInstance(&instanceCreateInfo, &gAppState.Instance));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to create XR instance: %d.", initResult);
		exit(1);
	}

	XrInstanceProperties instanceInfo;
	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	OXR(xrGetInstanceProperties(gAppState.Instance, &instanceInfo));
	ALOGV(
			"Runtime %s: Version : %u.%u.%u",
			instanceInfo.runtimeName,
			XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
			XR_VERSION_MINOR(instanceInfo.runtimeVersion),
			XR_VERSION_PATCH(instanceInfo.runtimeVersion));

	XrSystemGetInfo systemGetInfo;
	memset(&systemGetInfo, 0, sizeof(systemGetInfo));
	systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	systemGetInfo.next = NULL;
	systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

	OXR(initResult = xrGetSystem(gAppState.Instance, &systemGetInfo, &gAppState.SystemId));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to get system.");
		exit(1);
	}

	// Get the graphics requirements.
	PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
	OXR(xrGetInstanceProcAddr(
			gAppState.Instance,
			"xrGetOpenGLESGraphicsRequirementsKHR",
			(PFN_xrVoidFunction * )(&pfnGetOpenGLESGraphicsRequirementsKHR)));

	XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {};
	graphicsRequirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
	OXR(pfnGetOpenGLESGraphicsRequirementsKHR(gAppState.Instance, gAppState.SystemId,
											  &graphicsRequirements));

    if (strstr(gAppState.OpenXRHMD, "meta") != NULL)
    {
        XrSystemColorSpacePropertiesFB colorSpacePropertiesFB = {};
        colorSpacePropertiesFB.type = XR_TYPE_SYSTEM_COLOR_SPACE_PROPERTIES_FB;

        XrSystemProperties systemProperties = {};
        systemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
        systemProperties.next = &colorSpacePropertiesFB;
        OXR(xrGetSystemProperties(gAppState.Instance, gAppState.SystemId, &systemProperties));

        ALOGV("System Color Space Properties: colorspace=%d", colorSpacePropertiesFB.colorSpace);
    }

	TBXR_InitialiseResolution();
}

void TBXR_Recenter() {

	// Calculate recenter reference
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	if (gAppState.CurrentSpace != XR_NULL_HANDLE) {
		vec3_t rotation = {0, 0, 0};
		XrSpaceLocation loc = {};
		loc.type = XR_TYPE_SPACE_LOCATION;
		OXR(xrLocateSpace(gAppState.HeadSpace, gAppState.CurrentSpace, gAppState.FrameState.predictedDisplayTime, &loc));
		QuatToYawPitchRoll(loc.pose.orientation, rotation, hmdorientation);

		spaceCreateInfo.poseInReferenceSpace.orientation.x = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.y = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.z = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.w = 1;
	}

	// Delete previous space instances
	if (gAppState.StageSpace != XR_NULL_HANDLE) {
		OXR(xrDestroySpace(gAppState.StageSpace));
	}
	if (gAppState.FakeStageSpace != XR_NULL_HANDLE) {
		OXR(xrDestroySpace(gAppState.FakeStageSpace));
	}

	// Create a default stage space to use if SPACE_TYPE_STAGE is not
	// supported, or calls to xrGetReferenceSpaceBoundsRect fail.
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
	OXR(xrCreateReferenceSpace(gAppState.Session, &spaceCreateInfo, &gAppState.FakeStageSpace));
	ALOGV("Created fake stage space from local space with offset");
	gAppState.CurrentSpace = gAppState.FakeStageSpace;

	if (stageSupported) {
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
		OXR(xrCreateReferenceSpace(gAppState.Session, &spaceCreateInfo, &gAppState.StageSpace));
		ALOGV("Created stage space");
		gAppState.CurrentSpace = gAppState.StageSpace;
	}
}

void TBXR_UpdateStageBounds() {
	XrExtent2Df stageBounds = {};

	XrResult result;
	OXR(result = xrGetReferenceSpaceBoundsRect(
			gAppState.Session, XR_REFERENCE_SPACE_TYPE_STAGE, &stageBounds));
	if (result != XR_SUCCESS) {
		ALOGE("Stage bounds query failed: using small defaults");
		stageBounds.width = 1.0f;
		stageBounds.height = 1.0f;

		gAppState.CurrentSpace = gAppState.FakeStageSpace;
	}
}

void TBXR_WaitForSessionActive()
{//Now wait for the session to be ready
	while (!gAppState.SessionActive) {
		TBXR_ProcessMessageQueue();
		if (ovrApp_HandleXrEvents(&gAppState)) {
			TBXR_Recenter();
		}
	}
}

static void TBXR_GetHMDOrientation() {

	if (gAppState.FrameState.predictedDisplayTime == 0)
	{
		return;
	}

	// Get the HMD pose, predicted for the middle of the time period during which
	// the new eye images will be displayed. The number of frames predicted ahead
	// depends on the pipeline depth of the engine and the synthesis rate.
	// The better the prediction, the less black will be pulled in at the edges.
	XrSpaceLocation loc = {};
	loc.type = XR_TYPE_SPACE_LOCATION;
	OXR(xrLocateSpace(gAppState.HeadSpace, gAppState.CurrentSpace, gAppState.FrameState.predictedDisplayTime, &loc));
	gAppState.xfStageFromHead = loc.pose;

	const XrQuaternionf quatHmd = gAppState.xfStageFromHead.orientation;
	const XrVector3f positionHmd = gAppState.xfStageFromHead.position;

	vec3_t rotation = {0, 0, 0};
	vec3_t hmdorientation = {0, 0, 0};
	QuatToYawPitchRoll(quatHmd, rotation, hmdorientation);
	VR_SetHMDPosition(positionHmd.x, positionHmd.y, positionHmd.z);
	VR_SetHMDOrientation(hmdorientation[0], hmdorientation[1], hmdorientation[2]);
}


//All the stuff we want to do each frame
void TBXR_FrameSetup()
{
	if (gAppState.FrameSetup)
	{
		return;
	}

    while (!destroyed)
    {
		TBXR_ProcessMessageQueue();

        GLboolean stageBoundsDirty = GL_TRUE;
        if (ovrApp_HandleXrEvents(&gAppState))
        {
			TBXR_Recenter();
        }

        if (gAppState.SessionActive == GL_FALSE)
        {
            continue;
        }

        if (stageBoundsDirty)
        {
			TBXR_UpdateStageBounds();
            stageBoundsDirty = GL_FALSE;
        }

        break;
    }

    if (destroyed)
    {
		TBXR_LeaveVR();
		//Ask Java to shut down
		VR_Shutdown();

		exit(0); // in case Java doesn't do the job
	}


	// NOTE: OpenXR does not use the concept of frame indices. Instead,
	// XrWaitFrame returns the predicted display time.
	//XrFrameWaitInfo waitFrameInfo = {};
	//waitFrameInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	//waitFrameInfo.next = NULL;

	memset(&(gAppState.FrameState), 0, sizeof(XrFrameState));
	gAppState.FrameState.type = XR_TYPE_FRAME_STATE;

	ALOGV("xrWaitFrame");
	OXR(xrWaitFrame(gAppState.Session, NULL, &gAppState.FrameState));

	// Get the HMD pose, predicted for the middle of the time period during which
	// the new eye images will be displayed. The number of frames predicted ahead
	// depends on the pipeline depth of the engine and the synthesis rate.
	// The better the prediction, the less black will be pulled in at the edges.
	XrFrameBeginInfo beginFrameDesc = {};
	beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
	beginFrameDesc.next = NULL;
	ALOGV("xrBeginFrame");
	OXR(xrBeginFrame(gAppState.Session, &beginFrameDesc));

	//Game specific frame setup stuff called here
	VR_FrameSetup();

	//Get controller state here
	TBXR_GetHMDOrientation();
	VR_HandleControllerInput();

	TBXR_ProcessHaptics();

	gAppState.FrameSetup = true;
}

int TBXR_GetRefresh()
{
	return gAppState.currentDisplayRefreshRate;
}

#define GL_FRAMEBUFFER_SRGB               0x8DB9

void TBXR_ClearFrameBuffer(int width, int height)
{
	glEnable( GL_SCISSOR_TEST );
	glViewport( 0, 0, width, height );

	//Black
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );

	glScissor( 0, 0, width, height );
	glClear( GL_COLOR_BUFFER_BIT );

	glScissor( 0, 0, 0, 0 );
	glDisable( GL_SCISSOR_TEST );

	//This is a bit of a hack, but we need to do this to correct for the fact that the engine uses linear RGB colorspace
	//but openxr uses SRGB (or something, must admit I don't really understand, but adding this works to make it look good again)
	glDisable( GL_FRAMEBUFFER_SRGB );
}

void TBXR_prepareEyeBuffer(int eye )
{
	ovrFramebuffer* frameBuffer = &(gAppState.Renderer.FrameBuffer[eye]);
	ovrFramebuffer_Acquire(frameBuffer);
	ovrFramebuffer_SetCurrent(frameBuffer);
	TBXR_ClearFrameBuffer(frameBuffer->ColorSwapChain.Width, frameBuffer->ColorSwapChain.Height);
}

void TBXR_finishEyeBuffer(int eye )
{
	ovrRenderer *renderer = &gAppState.Renderer;

	ovrFramebuffer *frameBuffer = &(renderer->FrameBuffer[eye]);

	// Clear the alpha channel, other way OpenXR would not transfer the framebuffer fully
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	//Clear edge to prevent smearing
	ovrFramebuffer_Resolve(frameBuffer);
	ovrFramebuffer_Release(frameBuffer);
	ovrFramebuffer_SetNone();
}

void TBXR_updateProjections()
{
	XrViewLocateInfo projectionInfo = {};
	projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	projectionInfo.viewConfigurationType = gAppState.ViewportConfig.viewConfigurationType;
	projectionInfo.displayTime = gAppState.FrameState.predictedDisplayTime;
	projectionInfo.space = gAppState.HeadSpace;

	XrViewState viewState = {XR_TYPE_VIEW_STATE, NULL};

	uint32_t projectionCapacityInput = ovrMaxNumEyes;
	uint32_t projectionCountOutput = projectionCapacityInput;

	OXR(xrLocateViews(
			gAppState.Session,
			&projectionInfo,
			&viewState,
			projectionCapacityInput,
			&projectionCountOutput,
			gAppState.Projections));
}

float fov_y = 0.0;

float GetFOV()
{
	return fov_y;
}

void TBXR_submitFrame()
{
	if (gAppState.SessionActive == GL_FALSE) {
		return;
	}

	TBXR_updateProjections();

	XrFovf fov = {};
	XrPosef viewTransform[2];

	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		XrPosef xfHeadFromEye = gAppState.Projections[eye].pose;
		XrPosef xfStageFromEye = XrPosef_Multiply(gAppState.xfStageFromHead, xfHeadFromEye);
		viewTransform[eye] = XrPosef_Inverse(xfStageFromEye);
        fov.angleLeft += gAppState.Projections[eye].fov.angleLeft / 2.0f;
        fov.angleRight += gAppState.Projections[eye].fov.angleRight / 2.0f;
        fov.angleUp += gAppState.Projections[eye].fov.angleUp / 2.0f;
        fov.angleDown += gAppState.Projections[eye].fov.angleDown / 2.0f;
	}
	vrFOV = (fabs(fov.angleLeft) + fabs(fov.angleRight)) * 180.0f / M_PI;


	gAppState.LayerCount = 0;
	memset(gAppState.Layers, 0, sizeof(xrCompositorLayer_Union) * ovrMaxLayerCount);

	XrCompositionLayerProjectionView projection_layer_elements[2] = {};
	if (!VR_UseScreenLayer()) {
		XrCompositionLayerProjection projection_layer = {};
		projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
		projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		projection_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
		projection_layer.space = gAppState.CurrentSpace;
		projection_layer.viewCount = ovrMaxNumEyes;
		projection_layer.views = projection_layer_elements;

		for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
			ovrFramebuffer* frameBuffer = &gAppState.Renderer.FrameBuffer[eye];

			memset(&projection_layer_elements[eye], 0, sizeof(XrCompositionLayerProjectionView));
			projection_layer_elements[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
			projection_layer_elements[eye].pose = gAppState.xfStageFromHead;
			projection_layer_elements[eye].fov = fov;
			memset(&projection_layer_elements[eye].subImage, 0, sizeof(XrSwapchainSubImage));
			projection_layer_elements[eye].subImage.swapchain =
					frameBuffer->ColorSwapChain.Handle;
			projection_layer_elements[eye].subImage.imageRect.offset.x = 0;
			projection_layer_elements[eye].subImage.imageRect.offset.y = 0;
			projection_layer_elements[eye].subImage.imageRect.extent.width =
					frameBuffer->ColorSwapChain.Width;
			projection_layer_elements[eye].subImage.imageRect.extent.height =
					frameBuffer->ColorSwapChain.Height;
			projection_layer_elements[eye].subImage.imageArrayIndex = 0;
		}

		gAppState.Layers[gAppState.LayerCount++].Projection = projection_layer;
	} else {

		// Build the quad layer
		XrCompositionLayerQuad quad_layer = {};
		int width = gAppState.Renderer.FrameBuffer[0].ColorSwapChain.Width;
		int height = gAppState.Renderer.FrameBuffer[0].ColorSwapChain.Height;
		quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
		quad_layer.next = NULL;
		quad_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		quad_layer.space = gAppState.CurrentSpace;
		quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
		memset(&quad_layer.subImage, 0, sizeof(XrSwapchainSubImage));
		quad_layer.subImage.swapchain = gAppState.Renderer.FrameBuffer[0].ColorSwapChain.Handle;
		quad_layer.subImage.imageRect.offset.x = 0;
		quad_layer.subImage.imageRect.offset.y = 0;
		quad_layer.subImage.imageRect.extent.width = width;
		quad_layer.subImage.imageRect.extent.height = height;
		quad_layer.subImage.imageArrayIndex = 0;
		const XrVector3f axis = {0.0f, 1.0f, 0.0f};
		XrVector3f pos = {
				gAppState.xfStageFromHead.position.x - sin(DEG2RAD(playerYaw)) * VR_GetScreenLayerDistance(),
				1.0f,
				gAppState.xfStageFromHead.position.z - cos(DEG2RAD(playerYaw)) * VR_GetScreenLayerDistance()
		};
		quad_layer.pose.orientation = XrQuaternionf_CreateFromVectorAngle(axis, DEG2RAD(playerYaw));
		quad_layer.pose.position = pos;
		XrExtent2Df size = {5.0f, 4.5f};
		quad_layer.size = size;

		gAppState.Layers[gAppState.LayerCount++].Quad = quad_layer;
	}

	// Compose the layers for this frame.
	const XrCompositionLayerBaseHeader* layers[ovrMaxLayerCount] = {};
	for (int i = 0; i < gAppState.LayerCount; i++) {
		layers[i] = (const XrCompositionLayerBaseHeader*)&gAppState.Layers[i];
	}

	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = gAppState.FrameState.predictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = gAppState.LayerCount;
	endFrameInfo.layers = layers;

	ALOGV("xrEndFrame");
	OXR(xrEndFrame(gAppState.Session, &endFrameInfo));

	gAppState.FrameSetup = false;
}
