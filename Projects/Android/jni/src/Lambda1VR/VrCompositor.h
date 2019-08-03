/************************************************************************************

Filename	:	VrCompositor.h

*************************************************************************************/

#include "VrInput.h"

#define CHECK_GL_ERRORS
#ifdef CHECK_GL_ERRORS

static const char * GlErrorString( GLenum error )
{
	switch ( error )
	{
		case GL_NO_ERROR:						return "GL_NO_ERROR";
		case GL_INVALID_ENUM:					return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:					return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:				return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION:	return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY:					return "GL_OUT_OF_MEMORY";
		default: return "unknown";
	}
}

static void GLCheckErrors( int line )
{
	for ( int i = 0; i < 10; i++ )
	{
		const GLenum error = glGetError();
		if ( error == GL_NO_ERROR )
		{
			break;
		}
		ALOGE( "GL error on line %d: %s", line, GlErrorString( error ) );
	}
}

#define GL( func )		func; GLCheckErrors( __LINE__ );

#else // CHECK_GL_ERRORS

#define GL( func )		func;

#endif // CHECK_GL_ERRORS


/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct
{
	int						Width;
	int						Height;
	int						Multisamples;
	int						TextureSwapChainLength;
	int						TextureSwapChainIndex;
	ovrTextureSwapChain *	ColorTextureSwapChain;
	GLuint *				DepthBuffers;
	GLuint *				FrameBuffers;
} ovrFramebuffer;

void ovrFramebuffer_SetCurrent( ovrFramebuffer * frameBuffer );
void ovrFramebuffer_Destroy( ovrFramebuffer * frameBuffer );
void ovrFramebuffer_SetNone();
void ovrFramebuffer_Resolve( ovrFramebuffer * frameBuffer );
void ovrFramebuffer_Advance( ovrFramebuffer * frameBuffer );
void ovrFramebuffer_ClearEdgeTexels( ovrFramebuffer * frameBuffer );

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct
{
	ovrFramebuffer	FrameBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
	ovrMatrix4f		ProjectionMatrix;
	int				NumBuffers;
} ovrRenderer;


void ovrRenderer_Clear( ovrRenderer * renderer );
void ovrRenderer_Create( int width, int height, ovrRenderer * renderer, const ovrJava * java );
void ovrRenderer_Destroy( ovrRenderer * renderer );


/*
================================================================================

renderState

================================================================================
*/

typedef struct
{
    GLuint					VertexBuffer;
    GLuint					IndexBuffer;
    GLuint					VertexArrayObject;
    GLuint	                Program;
    GLuint	                VertexShader;
    GLuint	                FragmentShader;
} renderState;

void getCurrentRenderState( renderState * state);
void restoreRenderState( renderState * state );

/*
================================================================================

ovrGeometry

================================================================================
*/

typedef struct
{
	GLuint			Index;
	GLint			Size;
	GLenum			Type;
	GLboolean		Normalized;
	GLsizei			Stride;
	const GLvoid *	Pointer;
} ovrVertexAttribPointer;

#define MAX_VERTEX_ATTRIB_POINTERS		3

typedef struct
{
	GLuint					VertexBuffer;
	GLuint					IndexBuffer;
	GLuint					VertexArrayObject;
	int						VertexCount;
	int 					IndexCount;
	ovrVertexAttribPointer	VertexAttribs[MAX_VERTEX_ATTRIB_POINTERS];
} ovrGeometry;

/*
================================================================================

ovrProgram

================================================================================
*/

#define MAX_PROGRAM_UNIFORMS	8
#define MAX_PROGRAM_TEXTURES	8

typedef struct
{
	GLuint	Program;
	GLuint	VertexShader;
	GLuint	FragmentShader;
	// These will be -1 if not used by the program.
	GLint	UniformLocation[MAX_PROGRAM_UNIFORMS];	// ProgramUniforms[].name
	GLint	UniformBinding[MAX_PROGRAM_UNIFORMS];	// ProgramUniforms[].name
	GLint	Textures[MAX_PROGRAM_TEXTURES];			// Texture%i
} ovrProgram;

/*
================================================================================

ovrScene

================================================================================
*/

typedef struct
{
	bool				CreatedScene;
	bool				CreatedVAOs;
	ovrProgram			Program;
	ovrGeometry			GroundPlane;

	//Proper renderer for stereo rendering to the cylinder layer
	ovrRenderer 		CylinderRenderer;

	int					CylinderWidth;
	int					CylinderHeight;
} ovrScene;

bool ovrScene_IsCreated( ovrScene * scene );
void ovrScene_Clear( ovrScene * scene );
void ovrScene_Create( int width, int height, ovrScene * scene, const ovrJava * java );
void ovrScene_CreateVAOs( ovrScene * scene );
void ovrScene_DestroyVAOs( ovrScene * scene );
void ovrScene_Destroy( ovrScene * scene );

/*
================================================================================

ovrRenderer

================================================================================
*/

ovrLayerProjection2 ovrRenderer_RenderGroundPlaneToEyeBuffer( ovrRenderer * renderer, const ovrJava * java,
	const ovrScene * scene, const ovrTracking2 * tracking );
	
ovrLayerProjection2 ovrRenderer_RenderToEyeBuffer( ovrRenderer * renderer, const ovrJava * java,
	const ovrTracking2 * tracking );

ovrLayerCylinder2 BuildCylinderLayer( ovrRenderer * cylinderRenderer,
	const int textureWidth, const int textureHeight,
	const ovrTracking2 * tracking, float rotateYaw );
;


