/************************************************************************************

Filename	:	Lambda1VR.c based on VrCubeWorld_SurfaceView.c
Content		:	This sample uses a plain Android SurfaceView and handles all
				Activity and Surface life cycle events in native code. This sample
				does not use the application framework and also does not use LibOVR.
				This sample only uses the VrApi.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren / Simon Brown

Copyright	:	Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

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

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GLES/gl2ext.h>

#include <common/common.h>
#include <common/library.h>
#include <common/cvardef.h>
#include <common/xash3d_types.h>
#include <engine/keydefs.h>
#include <client/touch.h>
#include <client/client.h>

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"
#include "VrApi_Types.h"

#include "../gl4es/src/gl/loader.h"

#include "VrCompositor.h"
#include "VrInput.h"

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


// Must use EGLSyncKHR because the VrApi still supports OpenGL ES 2.0
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
int CPU_LEVEL			= 3;
int GPU_LEVEL			= 3;
int NUM_MULTI_SAMPLES	= 4;
float SS_MULTIPLIER    = 1.25f;

float radians(float deg) {
	return (deg * M_PI) / 180.0;
}

float degrees(float rad) {
	return (rad * 180.0) / M_PI;
}

/* global arg_xxx structs */
struct arg_dbl *ss;
struct arg_int *cpu;
struct arg_int *gpu;
struct arg_int *msaa;
struct arg_end *end;

char **argv;
int argc=0;

extern convar_t	*r_lefthand;

enum control_scheme {
	RIGHT_HANDED_DEFAULT = 0,
	RIGHT_HANDED_ALT = 1,
	RIGHT_HANDED_ALT2 = 2,
	ONE_CONTROLLER_RIGHT = 3,
	LEFT_HANDED_DEFAULT = 10,
	LEFT_HANDED_ALT = 11,
	LEFT_HANDED_ALT_2 = 12,   //Special case for left-handers who want the right handed button mappings / thumbsticks but left handed weapon / right handed flashlight
    ONE_CONTROLLER_LEFT = 13   //Special case for left-handers who want the right handed button mappings / thumbsticks but left handed weapon / right handed flashlight
};

/*
================================================================================

System Clock Time in millis

================================================================================
*/

double GetTimeInMilliSeconds()
{
	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	return ( now.tv_sec * 1e9 + now.tv_nsec ) * (double)(1e-6);
}

/*
================================================================================

LAMBDA1VR Stuff

================================================================================
*/

typedef void (*pfnChangeGame)( const char *progname );
extern int Host_Main( int argc, const char **argv, const char *progname, int bChangeGame, pfnChangeGame func );

bool useScreenLayer()
{
	return (showingScreenLayer || cls.demoplayback || cls.state == ca_cinematic || cls.key_dest != key_game);
}

int runStatus = -1;
void L1VR_exit(int exitCode)
{
	runStatus = exitCode;
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

typedef struct
{
	EGLint		MajorVersion;
	EGLint		MinorVersion;
	EGLDisplay	Display;
	EGLConfig	Config;
	EGLSurface	TinySurface;
	EGLSurface	MainSurface;
	EGLContext	Context;
} ovrEgl;

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


static void ovrFramebuffer_Clear( ovrFramebuffer * frameBuffer )
{
	frameBuffer->Width = 0;
	frameBuffer->Height = 0;
	frameBuffer->Multisamples = 0;
	frameBuffer->TextureSwapChainLength = 0;
	frameBuffer->TextureSwapChainIndex = 0;
	frameBuffer->ColorTextureSwapChain = NULL;
	frameBuffer->DepthBuffers = NULL;
	frameBuffer->FrameBuffers = NULL;
}

static bool ovrFramebuffer_Create( ovrFramebuffer * frameBuffer, const GLenum colorFormat, const int width, const int height, const int multisamples )
{
    frameBuffer->Width = width;
	frameBuffer->Height = height;
	frameBuffer->Multisamples = multisamples;

	frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain3( VRAPI_TEXTURE_TYPE_2D, colorFormat, frameBuffer->Width, frameBuffer->Height, 1, 3 );
	frameBuffer->TextureSwapChainLength = vrapi_GetTextureSwapChainLength( frameBuffer->ColorTextureSwapChain );
	frameBuffer->DepthBuffers = (GLuint *)malloc( frameBuffer->TextureSwapChainLength * sizeof( GLuint ) );
	frameBuffer->FrameBuffers = (GLuint *)malloc( frameBuffer->TextureSwapChainLength * sizeof( GLuint ) );

	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
		(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
		(PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");

	for ( int i = 0; i < frameBuffer->TextureSwapChainLength; i++ )
	{
		// Create the color buffer texture.
		const GLuint colorTexture = vrapi_GetTextureSwapChainHandle( frameBuffer->ColorTextureSwapChain, i );
		GLenum colorTextureTarget = GL_TEXTURE_2D;
		GL( glBindTexture( colorTextureTarget, colorTexture ) );
        // Just clamp to edge. However, this requires manually clearing the border
        // around the layer to clear the edge texels.
        GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
        GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

		GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );
		GL( glTexParameteri( colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
		GL( glBindTexture( colorTextureTarget, 0 ) );

		if (multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL && glFramebufferTexture2DMultisampleEXT != NULL)
		{
			
			// Create multisampled depth buffer.
			GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
			GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
			GL(glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height));
			GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

			// Create the frame buffer.
			GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
			GL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
			GL(glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0, multisamples));
			GL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
			GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER));
			GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
			if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
			{
					ALOGE("OVRHelper::Incomplete frame buffer object: %s", GlFrameBufferStatusString(renderFramebufferStatus));
					return false;
			}
		}
		else
		{
			{
				// Create depth buffer.
				GL( glGenRenderbuffers( 1, &frameBuffer->DepthBuffers[i] ) );
				GL( glBindRenderbuffer( GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
				GL( glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, frameBuffer->Width, frameBuffer->Height ) );
				GL( glBindRenderbuffer( GL_RENDERBUFFER, 0 ) );

				// Create the frame buffer.
				GL( glGenFramebuffers( 1, &frameBuffer->FrameBuffers[i] ) );
				GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i] ) );
				GL( glFramebufferRenderbuffer( GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, frameBuffer->DepthBuffers[i] ) );
				GL( glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0 ) );
				GL( GLenum renderFramebufferStatus = glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) );
				GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 ) );
				if ( renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE )
				{
					ALOGE( "Incomplete frame buffer object: %s", GlFrameBufferStatusString( renderFramebufferStatus ) );
					return false;
				}
			}
		}
	}

	return true;
}

void ovrFramebuffer_Destroy( ovrFramebuffer * frameBuffer )
{
	GL( glDeleteFramebuffers( frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers ) );
	GL( glDeleteRenderbuffers( frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers ) );

	vrapi_DestroyTextureSwapChain( frameBuffer->ColorTextureSwapChain );

	free( frameBuffer->DepthBuffers );
	free( frameBuffer->FrameBuffers );

	ovrFramebuffer_Clear( frameBuffer );
}

void ovrFramebuffer_SetCurrent( ovrFramebuffer * frameBuffer )
{
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex] ) );
}

void ovrFramebuffer_SetNone()
{
	GL( glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 ) );
}

void ovrFramebuffer_Resolve( ovrFramebuffer * frameBuffer )
{
	// Discard the depth buffer, so the tiler won't need to write it back out to memory.
	const GLenum depthAttachment[1] = { GL_DEPTH_ATTACHMENT };
	glInvalidateFramebuffer( GL_DRAW_FRAMEBUFFER, 1, depthAttachment );

    // Flush this frame worth of commands.
    glFlush();
}

void ovrFramebuffer_Advance( ovrFramebuffer * frameBuffer )
{
	// Advance to the next texture from the set.
	frameBuffer->TextureSwapChainIndex = ( frameBuffer->TextureSwapChainIndex + 1 ) % frameBuffer->TextureSwapChainLength;
}


void ovrFramebuffer_ClearEdgeTexels( ovrFramebuffer * frameBuffer )
{
    GL( glEnable( GL_SCISSOR_TEST ) );
    GL( glViewport( 0, 0, frameBuffer->Width, frameBuffer->Height ) );

    // Explicitly clear the border texels to black because OpenGL-ES does not support GL_CLAMP_TO_BORDER.
    // Clear to fully opaque black.
    GL( glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ) );

    //Glide comfort mask in and out
    static float currentVLevel = 0.0f;

    //Hack to surround scope sight with blackness
    const float scopeSize = 0.75;
    if (isScopeEngaged())
    {
        if (currentVLevel < scopeSize)
            currentVLevel += scopeSize * 0.05;
    }
	else if (player_moving && vr_comfort_mask->value > 0.0f)
    {
        if (currentVLevel <  vr_comfort_mask->value)
            currentVLevel += vr_comfort_mask->value * 0.05;
    } else{
	    float v = (vr_comfort_mask->value == 0) ? scopeSize : vr_comfort_mask->value;
        if (currentVLevel >  0.0f)
            currentVLevel -= v * 0.05;
    }

    bool useMask = (currentVLevel > 0.0f && currentVLevel <= 1.0f);

    float width = useMask ? (frameBuffer->Width / 2.0f) * currentVLevel : 1;
    float height = useMask ? (frameBuffer->Height / 2.0f) * currentVLevel : 1;

    // bottom
    GL( glScissor( 0, 0, frameBuffer->Width, width ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );
    // top
    GL( glScissor( 0, frameBuffer->Height - height, frameBuffer->Width, height ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );
    // left
    GL( glScissor( 0, 0, width, frameBuffer->Height ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );
    // right
    GL( glScissor( frameBuffer->Width - width, 0, width, frameBuffer->Height ) );
    GL( glClear( GL_COLOR_BUFFER_BIT ) );


    GL( glScissor( 0, 0, 0, 0 ) );
    GL( glDisable( GL_SCISSOR_TEST ) );
}


/*
================================================================================

ovrRenderer

================================================================================
*/


void ovrRenderer_Clear( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer_Clear( &renderer->FrameBuffer[eye] );
	}
	renderer->ProjectionMatrix = ovrMatrix4f_CreateIdentity();
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
}


void ovrRenderer_Create( int width, int height, ovrRenderer * renderer, const ovrJava * java )
{
	renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;

	//Now using a symmetrical render target, based on the horizontal FOV
    vrFOV = vrapi_GetSystemPropertyInt( java, VRAPI_SYS_PROP_SUGGESTED_EYE_FOV_DEGREES_Y);

	// Create the render Textures.
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer_Create( &renderer->FrameBuffer[eye],
							   GL_RGBA8,
							   width,
							   height,
							   NUM_MULTI_SAMPLES );
	}

	// Setup the projection matrix.
	renderer->ProjectionMatrix = ovrMatrix4f_CreateProjectionFov(
			vrFOV, vrFOV, 0.0f, 0.0f, 1.0f, 0.0f );

}

void ovrRenderer_Destroy( ovrRenderer * renderer )
{
	for ( int eye = 0; eye < renderer->NumBuffers; eye++ )
	{
		ovrFramebuffer_Destroy( &renderer->FrameBuffer[eye] );
	}
	renderer->ProjectionMatrix = ovrMatrix4f_CreateIdentity();
}


#ifndef EPSILON
#define EPSILON 0.001f
#endif

static ovrVector3f normalizeVec(ovrVector3f vec) {
    //NOTE: leave w-component untouched
    //@@const float EPSILON = 0.000001f;
    float xxyyzz = vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;
    //@@if(xxyyzz < EPSILON)
    //@@    return *this; // do nothing if it is zero vector

    //float invLength = invSqrt(xxyyzz);
    ovrVector3f result;
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

void GetAnglesFromVectors(const ovrVector3f forward, const ovrVector3f right, const ovrVector3f up, vec3_t angles)
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

void QuatToYawPitchRoll(ovrQuatf q, float pitchAdjust, vec3_t out) {

    ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion( &q );

    if (pitchAdjust != 0.0f)
	{
		ovrMatrix4f rot = ovrMatrix4f_CreateRotation(radians(pitchAdjust), 0.0f, 0.0f);
		mat = ovrMatrix4f_Multiply(&mat, &rot);
	}

    ovrVector4f v1 = {0, 0, -1, 0};
    ovrVector4f v2 = {1, 0, 0, 0};
    ovrVector4f v3 = {0, 1, 0, 0};

    ovrVector4f forwardInVRSpace = ovrVector4f_MultiplyMatrix4f(&mat, &v1);
    ovrVector4f rightInVRSpace = ovrVector4f_MultiplyMatrix4f(&mat, &v2);
    ovrVector4f upInVRSpace = ovrVector4f_MultiplyMatrix4f(&mat, &v3);

	ovrVector3f forward = {-forwardInVRSpace.z, -forwardInVRSpace.x, forwardInVRSpace.y};
	ovrVector3f right = {-rightInVRSpace.z, -rightInVRSpace.x, rightInVRSpace.y};
	ovrVector3f up = {-upInVRSpace.z, -upInVRSpace.x, upInVRSpace.y};

	ovrVector3f forwardNormal = normalizeVec(forward);
	ovrVector3f rightNormal = normalizeVec(right);
	ovrVector3f upNormal = normalizeVec(up);

	GetAnglesFromVectors(forwardNormal, rightNormal, upNormal, out);
	return;
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

void setHMDPosition( float x, float y, float z, float yaw )
{
	static bool s_useScreen = false;

	VectorSet(hmdPosition, x, y, z);

    if (s_useScreen != useScreenLayer())
    {
		s_useScreen = useScreenLayer();

		//Record player height on transition
        playerHeight = y;
    }

	if (!useScreenLayer())
    {
    	playerYaw = yaw;

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

static inline bool isHostAlive()
{
	return (host.state != HOST_SHUTDOWN &&
			host.state != HOST_CRASHED);
}

void COM_SetRandomSeed( int lSeed );

void RenderFrame( ovrRenderer * renderer, const ovrJava * java,
											const ovrTracking2 * tracking, ovrMobile * ovr )
{
    //if we are now shutting down, drop out here
    if (isHostAlive()) {

    	//Seed the random number generator the same for each eye to ensure electricity is drawn the same
		int lSeed = rand();

		//Set everything up
		Host_BeginFrame();

		// Render the eye images.
        for (int eye = 0; eye < renderer->NumBuffers && isHostAlive(); eye++) {
            ovrFramebuffer *frameBuffer = &(renderer->FrameBuffer[eye]);
            ovrFramebuffer_SetCurrent(frameBuffer);

            {
                GL(glEnable(GL_SCISSOR_TEST));
                GL(glDepthMask(GL_TRUE));
                GL(glEnable(GL_DEPTH_TEST));
                GL(glDepthFunc(GL_LEQUAL));

                //Weusing the size of the render target
                GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
                GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));

                GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
                GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
                GL(glDisable(GL_SCISSOR_TEST));

                if (isScopeEngaged()) {
					//Now do the drawing for this eye - Force the set as it is a "read only" cvar
					char buffer[5];
					Q_snprintf(buffer, 5, "%i", eye+2);
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

            //Clear edge to prevent smearing
            ovrFramebuffer_ClearEdgeTexels(frameBuffer);
            ovrFramebuffer_Resolve(frameBuffer);
            ovrFramebuffer_Advance(frameBuffer);
        }

        Host_EndFrame();
    }

	ovrFramebuffer_SetNone();
}


/*
================================================================================

ovrRenderThread

================================================================================
*/


/*
================================================================================

ovrApp

================================================================================
*/

typedef struct
{
	ovrJava				Java;
	ovrEgl				Egl;
	ANativeWindow *		NativeWindow;
	bool				Resumed;
	ovrMobile *			Ovr;
    ovrScene			Scene;
	long long			FrameIndex;
	double 				DisplayTime;
	int					SwapInterval;
	int					CpuLevel;
	int					GpuLevel;
	int					MainThreadTid;
	int					RenderThreadTid;
	ovrLayer_Union2		Layers[ovrMaxLayerCount];
	int					LayerCount;
	ovrRenderer			Renderer;
} ovrApp;

static void ovrApp_Clear( ovrApp * app )
{
	app->Java.Vm = NULL;
	app->Java.Env = NULL;
	app->Java.ActivityObject = NULL;
	app->NativeWindow = NULL;
	app->Resumed = false;
	app->Ovr = NULL;
	app->FrameIndex = 1;
	app->DisplayTime = 0;
	app->SwapInterval = 1;
	app->CpuLevel = 2;
	app->GpuLevel = 2;
	app->MainThreadTid = 0;
	app->RenderThreadTid = 0;

	ovrEgl_Clear( &app->Egl );

	ovrScene_Clear( &app->Scene );
	ovrRenderer_Clear( &app->Renderer );
}

static void ovrApp_HandleVrModeChanges( ovrApp * app )
{
	if ( app->Resumed != false && app->NativeWindow != NULL )
	{
		if ( app->Ovr == NULL )
		{
			ovrModeParms parms = vrapi_DefaultModeParms( &app->Java );
			// Must reset the FLAG_FULLSCREEN window flag when using a SurfaceView
			parms.Flags |= VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

			parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
			parms.Display = (size_t)app->Egl.Display;
			parms.WindowSurface = (size_t)app->NativeWindow;
			parms.ShareContext = (size_t)app->Egl.Context;

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			ALOGV( "        vrapi_EnterVrMode()" );

			app->Ovr = vrapi_EnterVrMode( &parms );

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			// If entering VR mode failed then the ANativeWindow was not valid.
			if ( app->Ovr == NULL )
			{
				ALOGE( "Invalid ANativeWindow!" );
				app->NativeWindow = NULL;
			}

			// Set performance parameters once we have entered VR mode and have a valid ovrMobile.
			if ( app->Ovr != NULL )
			{
				vrapi_SetClockLevels( app->Ovr, app->CpuLevel, app->GpuLevel );

				ALOGV( "		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid );

				ALOGV( "		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid );

				vrapi_SetPerfThread( app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid );

				ALOGV( "		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid );

				vrapi_SetExtraLatencyMode(app->Ovr, VRAPI_EXTRA_LATENCY_MODE_ON);

				ALOGV( "		vrapi_SetExtraLatencyMode( %d )", VRAPI_EXTRA_LATENCY_MODE_ON );
			}
		}
	}
	else
	{
		if ( app->Ovr != NULL )
		{
			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );

			ALOGV( "        vrapi_LeaveVrMode()" );

			vrapi_LeaveVrMode( app->Ovr );
			app->Ovr = NULL;

			ALOGV( "        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface( EGL_DRAW ) );
		}
	}
}


/*
================================================================================

ovrMessageQueue

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
} ovrMessage;

static void ovrMessage_Init( ovrMessage * message, const int id, const int wait )
{
	message->Id = id;
	message->Wait = wait;
	memset( message->Parms, 0, sizeof( message->Parms ) );
}

static void		ovrMessage_SetPointerParm( ovrMessage * message, int index, void * ptr ) { *(void **)&message->Parms[index] = ptr; }
static void *	ovrMessage_GetPointerParm( ovrMessage * message, int index ) { return *(void **)&message->Parms[index]; }
static void		ovrMessage_SetIntegerParm( ovrMessage * message, int index, int value ) { message->Parms[index] = value; }
static int		ovrMessage_GetIntegerParm( ovrMessage * message, int index ) { return (int)message->Parms[index]; }
static void		ovrMessage_SetFloatParm( ovrMessage * message, int index, float value ) { *(float *)&message->Parms[index] = value; }
static float	ovrMessage_GetFloatParm( ovrMessage * message, int index ) { return *(float *)&message->Parms[index]; }

// Cyclic queue with messages.
typedef struct
{
	ovrMessage	 		Messages[MAX_MESSAGES];
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
} ovrMessageQueue;

static void ovrMessageQueue_Create( ovrMessageQueue * messageQueue )
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

static void ovrMessageQueue_Destroy( ovrMessageQueue * messageQueue )
{
	pthread_mutex_destroy( &messageQueue->Mutex );
	pthread_cond_destroy( &messageQueue->PostedCondition );
	pthread_cond_destroy( &messageQueue->ReceivedCondition );
	pthread_cond_destroy( &messageQueue->ProcessedCondition );
}

static void ovrMessageQueue_Enable( ovrMessageQueue * messageQueue, const bool set )
{
	messageQueue->EnabledFlag = set;
}

static void ovrMessageQueue_PostMessage( ovrMessageQueue * messageQueue, const ovrMessage * message )
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

static void ovrMessageQueue_SleepUntilMessage( ovrMessageQueue * messageQueue )
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

static bool ovrMessageQueue_GetNextMessage( ovrMessageQueue * messageQueue, ovrMessage * message, bool waitForMessages )
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

/*
================================================================================

ovrAppThread

================================================================================
*/

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

typedef struct
{
	JavaVM *		JavaVm;
	jobject			ActivityObject;
	pthread_t		Thread;
	ovrMessageQueue	MessageQueue;
	ANativeWindow * NativeWindow;
} ovrAppThread;

long shutdownCountdown;

int m_width;
int m_height;

void R_ChangeDisplaySettings( int width, int height, qboolean fullscreen );

void Android_GetScreenRes(int *width, int *height)
{
	*width = m_width;
	*height = m_height;
}

void Android_MessageBox(const char *title, const char *text)
{
    ALOGE("%s %s", title, text);
}


/*
========================
Android_Vibrate
========================
*/

//0 = left, 1 = right
float vibration_channel_duration[2] = {0.0f, 0.0f};
float vibration_channel_intensity[2] = {0.0f, 0.0f};

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
convar_t	*vr_legacy_ladders;
convar_t	*vr_controller_tracking_haptic;
convar_t	*vr_highlight_actionables;
convar_t	*vr_headtorch;
convar_t	*vr_quick_crouchjump;
convar_t	*vr_stereo_side;

void Android_Vibrate( float duration, int channel, float intensity )
{
	if( !vibration_enable->integer )
		return;

	if (vibration_channel_duration[channel] > 0.0f)
		return;

	if (vibration_channel_duration[channel] == -1.0f &&	duration != 0.0f)
		return;

	vibration_channel_duration[channel] = duration;
	vibration_channel_intensity[channel] = intensity;
}

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
    vr_legacy_ladders = Cvar_Get( "vr_legacy_ladders", "0", CVAR_ARCHIVE, "Set to 1 to use ladder climb direction based on HMD, otherwise off-hand controller angle is used" );
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

void * AppThreadFunction( void * parm )
{
	ovrAppThread * appThread = (ovrAppThread *)parm;

	ovrJava java;
	java.Vm = appThread->JavaVm;
	(*java.Vm)->AttachCurrentThread( java.Vm, &java.Env, NULL );
	java.ActivityObject = appThread->ActivityObject;

	// Note that AttachCurrentThread will reset the thread name.
	prctl( PR_SET_NAME, (long)"OVR::Main", 0, 0, 0 );

	xash_initialised = false;

	const ovrInitParms initParms = vrapi_DefaultInitParms( &java );
	int32_t initResult = vrapi_Initialize( &initParms );
	if ( initResult != VRAPI_INITIALIZE_SUCCESS )
	{
		// If intialization failed, vrapi_* function calls will not be available.
		exit( 0 );
	}

	ovrApp appState;
	ovrApp_Clear( &appState );
	appState.Java = java;

	// This app will handle android gamepad events itself.
	vrapi_SetPropertyInt( &appState.Java, VRAPI_EAT_NATIVE_GAMEPAD_EVENTS, 0 );

	//Using a symmetrical render target
    m_width=vrapi_GetSystemPropertyInt( &java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH ) * SS_MULTIPLIER;
    m_height=m_width;

    hmdType = vrapi_GetSystemPropertyInt(&java, VRAPI_SYS_PROP_DEVICE_TYPE);


    ovrEgl_CreateContext( &appState.Egl, NULL );

	EglInitExtensions();

	appState.CpuLevel = CPU_LEVEL;
	appState.GpuLevel = GPU_LEVEL;
	appState.MainThreadTid = gettid();

	ovrRenderer_Create( m_width, m_height, &appState.Renderer, &java );

	//Always use this folder
	chdir("/sdcard/xash");

    for ( bool destroyed = false; destroyed == false; )
	{
		for ( ; ; )
		{
			ovrMessage message;
			const bool waitForMessages = ( appState.Ovr == NULL && destroyed == false );
			if ( !ovrMessageQueue_GetNextMessage( &appThread->MessageQueue, &message, waitForMessages ) )
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
					if (!xash_initialised)
					{
						ALOGV( "    Initialising Xash3D Engine" );

#ifndef DEMO
						if (argc != 0)
						{
                            Host_Main(argc, (const char**)argv, "valve", false, NULL);
						}
						else
#endif
						{
							int argc = 1; char *argv[] = { "xash3d" };

                            Host_Main(argc, (const char**)argv, "valve", false, NULL);
						}

						xash_initialised = true;
					}
					break;
				}
				case MESSAGE_ON_RESUME:
				{
					//If we get here, then user has opted not to quit
					//jni_resumeAudio();
					appState.Resumed = true;
					break;
				}
				case MESSAGE_ON_PAUSE:
				{
					//jni_pauseAudio();
					appState.Resumed = false;
					break;
				}
				case MESSAGE_ON_STOP:
				{
					break;
				}
				case MESSAGE_ON_DESTROY:
				{
					appState.NativeWindow = NULL;
					destroyed = true;
					break;
				}
				case MESSAGE_ON_SURFACE_CREATED:	{ appState.NativeWindow = (ANativeWindow *)ovrMessage_GetPointerParm( &message, 0 ); break; }
				case MESSAGE_ON_SURFACE_DESTROYED:	{ appState.NativeWindow = NULL; break; }
			}

			ovrApp_HandleVrModeChanges( &appState );
		}

		if ( appState.Ovr == NULL )
		{
			continue;
		}

        //Use floor based tracking space
        vrapi_SetTrackingSpace(appState.Ovr, VRAPI_TRACKING_SPACE_LOCAL_FLOOR);

		// Create the scene if not yet created.
		// The scene is created here to be able to show a loading icon.
		if ( !ovrScene_IsCreated( &appState.Scene ) )
		{
			ovrScene_Create( m_width, m_height, &appState.Scene, &java );
		}

        // This is the only place the frame index is incremented, right before
        // calling vrapi_GetPredictedDisplayTime().
        appState.FrameIndex++;

        // Create the scene if not yet created.
		// The scene is created here to be able to show a loading icon.
		if (!xash_initialised || runStatus != -1)
		{
			// Show a loading icon.
			int frameFlags = 0;
			frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

			ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
			blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
			iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

			const ovrLayerHeader2 * layers[] =
			{
				&blackLayer.Header,
				&iconLayer.Header,
			};

			ovrSubmitFrameDescription2 frameDesc = {};
			frameDesc.Flags = frameFlags;
			frameDesc.SwapInterval = 1;
			frameDesc.FrameIndex = appState.FrameIndex;
			frameDesc.DisplayTime = appState.DisplayTime;
			frameDesc.LayerCount = 2;
			frameDesc.Layers = layers;

			vrapi_SubmitFrame2( appState.Ovr, &frameDesc );
		}

		//Handle haptics
		static float lastFrameTime = 0.0f;
		float timestamp = (float)(GetTimeInMilliSeconds());
		float frametime = timestamp - lastFrameTime;
		lastFrameTime = timestamp;

		for (int i = 0; i < 2; ++i) {
			if (vibration_channel_duration[i] > 0.0f ||
				vibration_channel_duration[i] == -1.0f) {
				vrapi_SetHapticVibrationSimple(appState.Ovr, controllerIDs[i],
											   vibration_channel_intensity[i]);

				if (vibration_channel_duration[i] != -1.0f) {
					vibration_channel_duration[i] -= frametime;

					if (vibration_channel_duration[i] < 0.0f) {
						vibration_channel_duration[i] = 0.0f;
						vibration_channel_intensity[i] = 0.0f;
					}
				}
			} else {
				vrapi_SetHapticVibrationSimple(appState.Ovr, controllerIDs[i], 0.0f);
			}
		}

        if (runStatus == -1) {
#ifndef NDEBUG
            if (appState.FrameIndex > 10800)
            {
                //Trigger shutdown after a couple of minutes in debug mode
                //runStatus = 0;
            }
#endif

			//Set refresh rate
            vrapi_SetDisplayRefreshRate(appState.Ovr, vr_refresh->value);

			//Enable Extra Latency Mode
			vrapi_SetExtraLatencyMode(appState.Ovr, VRAPI_EXTRA_LATENCY_MODE_ON);

			// Get the HMD pose, predicted for the middle of the time period during which
			// the new eye images will be displayed. The number of frames predicted ahead
			// depends on the pipeline depth of the engine and the synthesis rate.
			// The better the prediction, the less black will be pulled in at the edges.
			const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState.Ovr,
																			  appState.FrameIndex);
			const ovrTracking2 tracking = vrapi_GetPredictedTracking2(appState.Ovr,
																	  predictedDisplayTime);

			appState.DisplayTime = predictedDisplayTime;

            //Get orientation
            // We extract Yaw, Pitch, Roll instead of directly using the orientation
            // to allow "additional" yaw manipulation with mouse/controller.
            const ovrQuatf quatHmd = tracking.HeadPose.Pose.Orientation;
            const ovrVector3f positionHmd = tracking.HeadPose.Pose.Position;
            QuatToYawPitchRoll(quatHmd, 0.0f, hmdorientation);
            setHMDPosition(positionHmd.x, positionHmd.y, positionHmd.z, hmdorientation[YAW]);

            //TODO: fix - set to use HMD position for world position
            setWorldPosition(positionHmd.x, positionHmd.y, positionHmd.z);

            ALOGV("        HMD-Position: %f, %f, %f", positionHmd.x, positionHmd.y, positionHmd.z);

            //Get info for tracked remotes
            acquireTrackedRemotesData(appState.Ovr, appState.DisplayTime);

            //Call additional control schemes here
            switch (vr_control_scheme->integer)
			{
				case RIGHT_HANDED_DEFAULT:
					HandleInput_Default(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
										&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
										ovrButton_A, ovrButton_B, ovrButton_X, ovrButton_Y);
					break;
				case RIGHT_HANDED_ALT:
					HandleInput_Alt(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
									&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
									ovrButton_A, ovrButton_B, ovrButton_X, ovrButton_Y);
					break;
				case RIGHT_HANDED_ALT2:
					HandleInput_Alt2(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
									&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
									ovrButton_A, ovrButton_B, ovrButton_X, ovrButton_Y);
					break;
				case ONE_CONTROLLER_RIGHT:
					HandleInput_OneController(&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
									&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
									ovrButton_A, ovrButton_B, ovrButton_X, ovrButton_Y);
					break;
				case LEFT_HANDED_DEFAULT:
					HandleInput_Default(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
										&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
										ovrButton_X, ovrButton_Y, ovrButton_A, ovrButton_B);
					break;
				case LEFT_HANDED_ALT:
					HandleInput_Alt(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
									 &rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
									 ovrButton_X, ovrButton_Y, ovrButton_A, ovrButton_B);
					break;
				case LEFT_HANDED_ALT_2:
					HandleInput_Alt2(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
									&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
									ovrButton_X, ovrButton_Y, ovrButton_A, ovrButton_B);
					break;
				case ONE_CONTROLLER_LEFT:
					HandleInput_OneController(&leftTrackedRemoteState_new, &leftTrackedRemoteState_old, &leftRemoteTracking_new,
									&rightTrackedRemoteState_new, &rightTrackedRemoteState_old, &rightRemoteTracking_new,
									ovrButton_X, ovrButton_Y, ovrButton_A, ovrButton_B);
					break;
			}

			static bool usingScreenLayer = true; //Starts off using the screen layer
			if (usingScreenLayer != useScreenLayer())
			{
                usingScreenLayer = useScreenLayer();
				R_ChangeDisplaySettings(m_width, m_height, false);
			}

			ovrSubmitFrameDescription2 frameDesc = { 0 };
			if (!useScreenLayer()) {

                ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
                layer.HeadPose = tracking.HeadPose;
                for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
                {
                    ovrFramebuffer * frameBuffer = &appState.Renderer.FrameBuffer[appState.Renderer.NumBuffers == 1 ? 0 : eye];
                    layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
                    layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;

                    ovrMatrix4f projectionMatrix;
                    projectionMatrix = ovrMatrix4f_CreateProjectionFov(vrFOV, vrFOV,
                                                                       0.0f, 0.0f, 0.1f, 0.0f);

                    layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&projectionMatrix);

                    layer.Textures[eye].TextureRect.x = 0;
                    layer.Textures[eye].TextureRect.y = 0;
                    layer.Textures[eye].TextureRect.width = 1.0f;
                    layer.Textures[eye].TextureRect.height = 1.0f;
                }
                layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

                //Call the game drawing code to populate the cylinder layer texture
                RenderFrame(&appState.Renderer,
                            &appState.Java,
                            &tracking,
                            appState.Ovr);


                // Set up the description for this frame.
                const ovrLayerHeader2 *layers[] =
                        {
                                &layer.Header
                        };

                ovrSubmitFrameDescription2 frameDesc = {};
                frameDesc.Flags = 0;
                frameDesc.SwapInterval = appState.SwapInterval;
                frameDesc.FrameIndex = appState.FrameIndex;
                frameDesc.DisplayTime = appState.DisplayTime;
                frameDesc.LayerCount = 1;
                frameDesc.Layers = layers;

                // Hand over the eye images to the time warp.

                vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
            } else {
				// Set-up the compositor layers for this frame.
				// NOTE: Multiple independent layers are allowed, but they need to be added
				// in a depth consistent order.
				memset( appState.Layers, 0, sizeof( ovrLayer_Union2 ) * ovrMaxLayerCount );
				appState.LayerCount = 0;

				// Add a simple cylindrical layer
				appState.Layers[appState.LayerCount++].Cylinder =
						BuildCylinderLayer( &appState.Scene.CylinderRenderer,
											appState.Scene.CylinderWidth, appState.Scene.CylinderHeight, &tracking, radians(playerYaw) );


                //Call the game drawing code to populate the cylinder layer texture
                RenderFrame(&appState.Scene.CylinderRenderer,
                            &appState.Java,
                            &tracking,
                            appState.Ovr);

                // Compose the layers for this frame.
				const ovrLayerHeader2 * layerHeaders[ovrMaxLayerCount] = { 0 };
				for ( int i = 0; i < appState.LayerCount; i++ )
				{
					layerHeaders[i] = &appState.Layers[i].Header;
				}

				// Set up the description for this frame.
				frameDesc.Flags = 0;
				frameDesc.SwapInterval = appState.SwapInterval;
				frameDesc.FrameIndex = appState.FrameIndex;
				frameDesc.DisplayTime = appState.DisplayTime;
				frameDesc.LayerCount = appState.LayerCount;
				frameDesc.Layers = layerHeaders;

                vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
            }
        }
        else
		{
		    //We are now shutting down
		    if (runStatus == 0)
            {
                //Give us half a second (36 frames)
                shutdownCountdown = 36;
                runStatus++;
            } else	if (runStatus == 1)
            {
                if (--shutdownCountdown == 0) {
                    runStatus++;
                }
            } else	if (runStatus == 2)
            {
                Host_Shutdown();
                runStatus++;
            } else if (runStatus == 3)
            {
                ovrRenderer_Destroy( &appState.Renderer );
                ovrEgl_DestroyContext( &appState.Egl );
                (*java.Vm)->DetachCurrentThread( java.Vm );
                vrapi_Shutdown();
                exit( 0 );
            }
		}
	}

	return NULL;
}

static void ovrAppThread_Create( ovrAppThread * appThread, JNIEnv * env, jobject activityObject )
{
	(*env)->GetJavaVM( env, &appThread->JavaVm );
	appThread->ActivityObject = (*env)->NewGlobalRef( env, activityObject );
	appThread->Thread = 0;
	appThread->NativeWindow = NULL;
	ovrMessageQueue_Create( &appThread->MessageQueue );

	const int createErr = pthread_create( &appThread->Thread, NULL, AppThreadFunction, appThread );
	if ( createErr != 0 )
	{
		ALOGE( "pthread_create returned %i", createErr );
	}
}

static void ovrAppThread_Destroy( ovrAppThread * appThread, JNIEnv * env )
{
	pthread_join( appThread->Thread, NULL );
	(*env)->DeleteGlobalRef( env, appThread->ActivityObject );
	ovrMessageQueue_Destroy( &appThread->MessageQueue );
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

        if (cpu->count > 0 && cpu->ival[0] > 0 && cpu->ival[0] < 10)
        {
            CPU_LEVEL = cpu->ival[0];
        }

        if (gpu->count > 0 && gpu->ival[0] > 0 && gpu->ival[0] < 10)
        {
            GPU_LEVEL = gpu->ival[0];
        }

		if (msaa->count > 0 && msaa->ival[0] > 0 && msaa->ival[0] < 10)
        {
            NUM_MULTI_SAMPLES = msaa->ival[0];
        }
	}

	initialize_gl4es();

	ovrAppThread * appThread = (ovrAppThread *) malloc( sizeof( ovrAppThread ) );
	ovrAppThread_Create( appThread, env, activity );

	ovrMessageQueue_Enable( &appThread->MessageQueue, true );
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );

	return (jlong)((size_t)appThread);
}


JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onStart( JNIEnv * env, jobject obj, jlong handle)
{
	ALOGV( "    GLES3JNILib::onStart()" );

	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_START, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onResume( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onResume()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onPause( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onPause()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onStop( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onStop()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
}

JNIEXPORT void JNICALL Java_com_drbeef_lambda1vr_GLES3JNILib_onDestroy( JNIEnv * env, jobject obj, jlong handle )
{
	ALOGV( "    GLES3JNILib::onDestroy()" );
	ovrAppThread * appThread = (ovrAppThread *)((size_t)handle);
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	ovrMessageQueue_Enable( &appThread->MessageQueue, false );

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
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
	ovrMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
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
			ovrMessage message;
			ovrMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
			ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
			ALOGV( "        ANativeWindow_release( NativeWindow )" );
			ANativeWindow_release( appThread->NativeWindow );
			appThread->NativeWindow = NULL;
		}
		if ( newNativeWindow != NULL )
		{
			ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
			appThread->NativeWindow = newNativeWindow;
			ovrMessage message;
			ovrMessage_Init( &message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED );
			ovrMessage_SetPointerParm( &message, 0, appThread->NativeWindow );
			ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
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
	ovrMessage message;
	ovrMessage_Init( &message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED );
	ovrMessageQueue_PostMessage( &appThread->MessageQueue, &message );
	ALOGV( "        ANativeWindow_release( NativeWindow )" );
	ANativeWindow_release( appThread->NativeWindow );
	appThread->NativeWindow = NULL;
}
