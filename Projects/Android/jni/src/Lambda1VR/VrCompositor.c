/************************************************************************************

Filename	:	VrCompositor.c

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/window.h>				// for AWINDOW_FLAG_KEEP_SCREEN_ON

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>



#include <VrApi.h>
#include <VrApi_Helpers.h>

#include "../gl4es/src/gl/loader.h"
#include "VrCompositor.h"



/*
================================================================================

renderState

================================================================================
*/


void getCurrentRenderState( renderState * state)
{
	LOAD_GLES2(glGetIntegerv);

	state->VertexBuffer = 0;
	state->IndexBuffer = 0;
	state->VertexArrayObject = 0;
	state->Program = 0;

    gles_glGetIntegerv(GL_ARRAY_BUFFER, &state->VertexBuffer );
	gles_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER, &state->IndexBuffer );
    gles_glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state->VertexArrayObject );
	gles_glGetIntegerv(GL_CURRENT_PROGRAM, &state->Program );
}

void restoreRenderState( renderState * state )
{
	LOAD_GLES2(glBindBuffer);
	LOAD_GLES2(glUseProgram);

    GL( gles_glUseProgram( state->Program ) );
    GL( glBindVertexArray( state->VertexArrayObject ) );
	GL( gles_glBindBuffer( GL_ARRAY_BUFFER, state->VertexBuffer ) );
	GL( gles_glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, state->IndexBuffer ) );
}


/*
================================================================================

ovrGeometry

================================================================================
*/

enum VertexAttributeLocation
{
	VERTEX_ATTRIBUTE_LOCATION_POSITION,
	VERTEX_ATTRIBUTE_LOCATION_COLOR,
	VERTEX_ATTRIBUTE_LOCATION_UV,
};

typedef struct
{
	enum VertexAttributeLocation location;
	const char *			name;
} ovrVertexAttribute;

static ovrVertexAttribute ProgramVertexAttributes[] =
{
	{ VERTEX_ATTRIBUTE_LOCATION_POSITION,	"vertexPosition" },
	{ VERTEX_ATTRIBUTE_LOCATION_COLOR,		"vertexColor" },
	{ VERTEX_ATTRIBUTE_LOCATION_UV,			"vertexUv" },
};

static void ovrGeometry_Clear( ovrGeometry * geometry )
{
	geometry->VertexBuffer = 0;
	geometry->IndexBuffer = 0;
	geometry->VertexArrayObject = 0;
	geometry->VertexCount = 0;
	geometry->IndexCount = 0;
	for ( int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++ )
	{
		memset( &geometry->VertexAttribs[i], 0, sizeof( geometry->VertexAttribs[i] ) );
		geometry->VertexAttribs[i].Index = -1;
	}
}

static void ovrGeometry_CreateGroundPlane( ovrGeometry * geometry )
{
	LOAD_GLES2(glGenBuffers);
	LOAD_GLES2(glBindBuffer);
	LOAD_GLES2(glBufferData);

	typedef struct
	{
		float positions[4][4];
		unsigned char colors[4][4];
	} ovrCubeVertices;

	static const ovrCubeVertices cubeVertices =
	{
		// positions
		{
			{  4.5f, -1.2f,  4.5f, 1.0f },
			{  4.5f, -1.2f, -4.5f, 1.0f },
			{ -4.5f, -1.2f, -4.5f, 1.0f },
			{ -4.5f, -1.2f,  4.5f, 1.0f }
		},
		// colors
		{
			{ 255,   0,   0, 255 },
			{   0, 255,   0, 255 },
			{   0,   0, 255, 255 },
			{ 255, 255,   0, 255 },
		},
	};

	static const unsigned short cubeIndices[6] =
	{
		0, 1, 2,
		0, 2, 3,
	};

	geometry->VertexCount = 4;
	geometry->IndexCount = 6;

	geometry->VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
	geometry->VertexAttribs[0].Size = 4;
	geometry->VertexAttribs[0].Type = GL_FLOAT;
	geometry->VertexAttribs[0].Normalized = false;
	geometry->VertexAttribs[0].Stride = sizeof( cubeVertices.positions[0] );
 	geometry->VertexAttribs[0].Pointer = (const GLvoid *)offsetof( ovrCubeVertices, positions );

	geometry->VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
	geometry->VertexAttribs[1].Size = 4;
	geometry->VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
	geometry->VertexAttribs[1].Normalized = true;
	geometry->VertexAttribs[1].Stride = sizeof( cubeVertices.colors[0] );
 	geometry->VertexAttribs[1].Pointer = (const GLvoid *)offsetof( ovrCubeVertices, colors );

	renderState state;
	getCurrentRenderState(&state);

	GL( gles_glGenBuffers( 1, &geometry->VertexBuffer ) );
	GL( gles_glBindBuffer( GL_ARRAY_BUFFER, geometry->VertexBuffer ) );
	GL( gles_glBufferData( GL_ARRAY_BUFFER, sizeof( cubeVertices ), &cubeVertices, GL_STATIC_DRAW ) );

	GL( gles_glGenBuffers( 1, &geometry->IndexBuffer ) );
	GL( gles_glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer ) );
	GL( gles_glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( cubeIndices ), cubeIndices, GL_STATIC_DRAW ) );

	restoreRenderState(&state);
}

static void ovrGeometry_Destroy( ovrGeometry * geometry )
{
	LOAD_GLES2(glDeleteBuffers);

	GL( gles_glDeleteBuffers( 1, &geometry->IndexBuffer ) );
	GL( gles_glDeleteBuffers( 1, &geometry->VertexBuffer ) );

	ovrGeometry_Clear( geometry );
}

static void ovrGeometry_CreateVAO( ovrGeometry * geometry )
{
	LOAD_GLES2(glBindBuffer);

	renderState state;
	getCurrentRenderState(&state);

	GL( glGenVertexArrays( 1, &geometry->VertexArrayObject ) );
	GL( glBindVertexArray( geometry->VertexArrayObject ) );

	GL( gles_glBindBuffer( GL_ARRAY_BUFFER, geometry->VertexBuffer ) );

	for ( int i = 0; i < MAX_VERTEX_ATTRIB_POINTERS; i++ )
	{
		if ( geometry->VertexAttribs[i].Index != -1 )
		{
			GL( glEnableVertexAttribArray( geometry->VertexAttribs[i].Index ) );
			GL( glVertexAttribPointer( geometry->VertexAttribs[i].Index, geometry->VertexAttribs[i].Size,
				geometry->VertexAttribs[i].Type, geometry->VertexAttribs[i].Normalized,
				geometry->VertexAttribs[i].Stride, geometry->VertexAttribs[i].Pointer ) );
		}
	}

    GL( gles_glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, geometry->IndexBuffer ) );

	restoreRenderState(&state);
}

static void ovrGeometry_DestroyVAO( ovrGeometry * geometry )
{
	GL( glDeleteVertexArrays( 1, &geometry->VertexArrayObject ) );
}

/*
================================================================================

ovrProgram

================================================================================
*/


typedef struct
{
	enum
	{
		UNIFORM_VIEW_PROJ_MATRIX,
	}				index;
	enum
	{
		UNIFORM_TYPE_VECTOR4,
		UNIFORM_TYPE_MATRIX4X4,
		UNIFORM_TYPE_INT,
		UNIFORM_TYPE_BUFFER,
	}				type;
	const char *	name;
} ovrUniform;

static ovrUniform ProgramUniforms[] =
{
	{ UNIFORM_VIEW_PROJ_MATRIX,			UNIFORM_TYPE_MATRIX4X4,	"viewProjectionMatrix"	},
};

static void ovrProgram_Clear( ovrProgram * program )
{
	program->Program = 0;
	program->VertexShader = 0;
	program->FragmentShader = 0;
	memset( program->UniformLocation, 0, sizeof( program->UniformLocation ) );
	memset( program->UniformBinding, 0, sizeof( program->UniformBinding ) );
	memset( program->Textures, 0, sizeof( program->Textures ) );
}

static bool ovrProgram_Create( ovrProgram * program, const char * vertexSource, const char * fragmentSource )
{
    LOAD_GLES2(glGetIntegerv);
    LOAD_GLES2(glCreateShader);
    LOAD_GLES2(glShaderSource);
    LOAD_GLES2(glCompileShader);
    LOAD_GLES2(glGetShaderiv);
    LOAD_GLES2(glGetShaderInfoLog);
    LOAD_GLES2(glAttachShader);
    LOAD_GLES2(glBindAttribLocation);
    LOAD_GLES2(glLinkProgram);
    LOAD_GLES2(glGetProgramiv);
    LOAD_GLES2(glGetUniformLocation);
    LOAD_GLES2(glCreateProgram);
    LOAD_GLES2(glUseProgram);
    LOAD_GLES2(glUniform1i);
    LOAD_GLES2(glGetProgramInfoLog);

	GLint r;

	GL( program->VertexShader = gles_glCreateShader( GL_VERTEX_SHADER ) );

	GL( gles_glShaderSource( program->VertexShader, 1, &vertexSource, 0 ) );
	GL( gles_glCompileShader( program->VertexShader ) );
	GL( gles_glGetShaderiv( program->VertexShader, GL_COMPILE_STATUS, &r ) );
	if ( r == GL_FALSE )
	{
		GLchar msg[4096];
		GL( gles_glGetShaderInfoLog( program->VertexShader, sizeof( msg ), 0, msg ) );
		ALOGE( "%s\n%s\n", vertexSource, msg );
		return false;
	}

	GL( program->FragmentShader = gles_glCreateShader( GL_FRAGMENT_SHADER ) );
	GL( gles_glShaderSource( program->FragmentShader, 1, &fragmentSource, 0 ) );
	GL( gles_glCompileShader( program->FragmentShader ) );
	GL( gles_glGetShaderiv( program->FragmentShader, GL_COMPILE_STATUS, &r ) );
	if ( r == GL_FALSE )
	{
		GLchar msg[4096];
		GL( gles_glGetShaderInfoLog( program->FragmentShader, sizeof( msg ), 0, msg ) );
		ALOGE( "%s\n%s\n", fragmentSource, msg );
		return false;
	}

	GL( program->Program = gles_glCreateProgram() );
	GL( gles_glAttachShader( program->Program, program->VertexShader ) );
	GL( gles_glAttachShader( program->Program, program->FragmentShader ) );

	// Bind the vertex attribute locations.
	for ( int i = 0; i < sizeof( ProgramVertexAttributes ) / sizeof( ProgramVertexAttributes[0] ); i++ )
	{
		GL( gles_glBindAttribLocation( program->Program, ProgramVertexAttributes[i].location, ProgramVertexAttributes[i].name ) );
	}

	GL( gles_glLinkProgram( program->Program ) );
	GL( gles_glGetProgramiv( program->Program, GL_LINK_STATUS, &r ) );
	if ( r == GL_FALSE )
	{
		GLchar msg[4096];
		GL( gles_glGetProgramInfoLog( program->Program, sizeof( msg ), 0, msg ) );
		ALOGE( "Linking program failed: %s\n", msg );
		return false;
	}

	int numBufferBindings = 0;

	// Get the uniform locations.
	memset( program->UniformLocation, -1, sizeof( program->UniformLocation ) );
	for ( int i = 0; i < sizeof( ProgramUniforms ) / sizeof( ProgramUniforms[0] ); i++ )
	{
		const int uniformIndex = ProgramUniforms[i].index;
		if ( ProgramUniforms[i].type == UNIFORM_TYPE_BUFFER )
		{
			GL( program->UniformLocation[uniformIndex] = glGetUniformBlockIndex( program->Program, ProgramUniforms[i].name ) );
			program->UniformBinding[uniformIndex] = numBufferBindings++;
			GL( glUniformBlockBinding( program->Program, program->UniformLocation[uniformIndex], program->UniformBinding[uniformIndex] ) );
		}
		else
		{
			GL( program->UniformLocation[uniformIndex] = gles_glGetUniformLocation( program->Program, ProgramUniforms[i].name ) );
			program->UniformBinding[uniformIndex] = program->UniformLocation[uniformIndex];
		}
	}

	renderState state;
	getCurrentRenderState(&state);

	GL( gles_glUseProgram( program->Program ) );

	// Get the texture locations.
	for ( int i = 0; i < MAX_PROGRAM_TEXTURES; i++ )
	{
		char name[32];
		sprintf( name, "Texture%i", i );
		program->Textures[i] = gles_glGetUniformLocation( program->Program, name );
		if ( program->Textures[i] != -1 )
		{
			GL( gles_glUniform1i( program->Textures[i], i ) );
		}
	}

	restoreRenderState(&state);

	return true;
}

static void ovrProgram_Destroy( ovrProgram * program )
{
    LOAD_GLES2(glDeleteProgram);
    LOAD_GLES2(glDeleteShader);

	if ( program->Program != 0 )
	{
		GL( gles_glDeleteProgram( program->Program ) );
		program->Program = 0;
	}
	if ( program->VertexShader != 0 )
	{
		GL( gles_glDeleteShader( program->VertexShader ) );
		program->VertexShader = 0;
	}
	if ( program->FragmentShader != 0 )
	{
		GL( gles_glDeleteShader( program->FragmentShader ) );
		program->FragmentShader = 0;
	}
}

static const char VERTEX_SHADER[] =
	"#version 300 es\n"
	"in vec3 vertexPosition;\n"
	"in vec4 vertexColor;\n"
	"uniform mat4 viewProjectionMatrix;\n"
	"out vec4 fragmentColor;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = viewProjectionMatrix * vec4( vertexPosition, 1.0 );\n"
	"	fragmentColor = vertexColor;\n"
	"}\n";

static const char FRAGMENT_SHADER[] =
	"#version 300 es\n"
	"in lowp vec4 fragmentColor;\n"
	"out lowp vec4 outColor;\n"
	"void main()\n"
	"{\n"
	"	outColor = fragmentColor;\n"
	"}\n";

/*
================================================================================

ovrScene

================================================================================
*/

void ovrScene_Clear( ovrScene * scene )
{
	scene->CreatedScene = false;
	scene->CreatedVAOs = false;
	ovrProgram_Clear( &scene->Program );
	ovrGeometry_Clear( &scene->GroundPlane );	
	ovrRenderer_Clear( &scene->CylinderRenderer );

	scene->CylinderWidth = 0;
	scene->CylinderHeight = 0;
}

bool ovrScene_IsCreated( ovrScene * scene )
{
	return scene->CreatedScene;
}

void ovrScene_CreateVAOs( ovrScene * scene )
{
	if ( !scene->CreatedVAOs )
	{
		ovrGeometry_CreateVAO( &scene->GroundPlane );
		scene->CreatedVAOs = true;
	}
}

void ovrScene_DestroyVAOs( ovrScene * scene )
{
	if ( scene->CreatedVAOs )
	{
		ovrGeometry_DestroyVAO( &scene->GroundPlane );
		scene->CreatedVAOs = false;
	}
}

void ovrScene_Create( int width, int height, ovrScene * scene, const ovrJava * java )
{
	// Simple ground plane geometry.
	{
		ovrProgram_Create( &scene->Program, VERTEX_SHADER, FRAGMENT_SHADER );
		ovrGeometry_CreateGroundPlane( &scene->GroundPlane );
		ovrScene_CreateVAOs( scene );
	}

	// Create Cylinder renderer
	{
		scene->CylinderWidth = width;
		scene->CylinderHeight = height;
		
		//Create cylinder renderer
		ovrRenderer_Create( width, height, &scene->CylinderRenderer, java );
	}
	
	scene->CreatedScene = true;
}

void ovrScene_Destroy( ovrScene * scene )
{
	ovrScene_DestroyVAOs( scene );
	ovrProgram_Destroy( &scene->Program );
	ovrGeometry_Destroy( &scene->GroundPlane );
	ovrRenderer_Destroy( &scene->CylinderRenderer );

	scene->CreatedScene = false;
}

/*
================================================================================

ovrRenderer

================================================================================
*/

ovrLayerProjection2 ovrRenderer_RenderGroundPlaneToEyeBuffer( ovrRenderer * renderer, const ovrJava * java,
	const ovrScene * scene, const ovrTracking2 * tracking )
{
	LOAD_GLES2(glEnable);
	LOAD_GLES2(glDepthMask);
	LOAD_GLES2(glDepthFunc);
	LOAD_GLES2(glViewport);
	LOAD_GLES2(glScissor);
	LOAD_GLES2(glClearColor);
	LOAD_GLES2(glClear);
	LOAD_GLES2(glDisable);
	LOAD_GLES2(glGetIntegerv);
	LOAD_GLES2(glBindBuffer);
	LOAD_GLES2(glCullFace);
	LOAD_GLES2(glDrawElements);
	LOAD_GLES2(glUseProgram);
    LOAD_GLES(glUniformMatrix4fv);

	ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
	layer.HeadPose = tracking->HeadPose;
	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer * frameBuffer = &renderer->FrameBuffer[eye];
		layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
		layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;
		layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection( &tracking->Eye[eye].ProjectionMatrix );
	}
	layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer * frameBuffer = &renderer->FrameBuffer[eye];
		ovrFramebuffer_SetCurrent( frameBuffer );

		renderState state;
		getCurrentRenderState(&state);

        GL( gles_glUseProgram( scene->Program.Program ) );

		ovrMatrix4f viewProjMatrix = ovrMatrix4f_Multiply( &tracking->Eye[eye].ProjectionMatrix, &tracking->Eye[eye].ViewMatrix );
		glUniformMatrix4fv(	scene->Program.UniformLocation[UNIFORM_VIEW_PROJ_MATRIX], 1, GL_TRUE, &viewProjMatrix.M[0][0] );

		GL( gles_glEnable( GL_SCISSOR_TEST ) );
		GL( gles_glDepthMask( GL_TRUE ) );
		GL( gles_glEnable( GL_DEPTH_TEST ) );
		GL( gles_glDepthFunc( GL_LEQUAL ) );
		GL( gles_glEnable( GL_CULL_FACE ) );
		GL( gles_glCullFace( GL_BACK ) );
		GL( gles_glViewport( 0, 0, frameBuffer->Width, frameBuffer->Height ) );
		GL( gles_glScissor( 0, 0, frameBuffer->Width, frameBuffer->Height ) );
		GL( gles_glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ) );
		GL( gles_glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) );

		//bind buffers
		GL( gles_glBindBuffer( GL_ARRAY_BUFFER, scene->GroundPlane.VertexBuffer ) );
		GL( gles_glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, scene->GroundPlane.IndexBuffer ) );
		GL( glBindVertexArray( scene->GroundPlane.VertexArrayObject ) );

		GL( gles_glDrawElements( GL_TRIANGLES, scene->GroundPlane.IndexCount, GL_UNSIGNED_SHORT, NULL ) );

		restoreRenderState(&state);

		// Explicitly clear the border texels to black when GL_CLAMP_TO_BORDER is not available.
		ovrFramebuffer_ClearEdgeTexels( frameBuffer );

		ovrFramebuffer_Resolve( frameBuffer );
		ovrFramebuffer_Advance( frameBuffer );
	}

	ovrFramebuffer_SetNone();

	return layer;
}

// Assumes landscape cylinder shape.
static ovrMatrix4f CylinderModelMatrix( const int texWidth, const int texHeight,
										const ovrVector3f translation,
										const float rotateYaw,
										const float rotatePitch,
										const float radius,
										const float density )
{
	const ovrMatrix4f scaleMatrix = ovrMatrix4f_CreateScale( radius, radius * (float)texHeight * VRAPI_PI / density, radius );
	const ovrMatrix4f transMatrix = ovrMatrix4f_CreateTranslation( translation.x, translation.y, translation.z );
	const ovrMatrix4f rotXMatrix = ovrMatrix4f_CreateRotation( rotateYaw, 0.0f, 0.0f );
	const ovrMatrix4f rotYMatrix = ovrMatrix4f_CreateRotation( 0.0f, rotatePitch, 0.0f );

	const ovrMatrix4f m0 = ovrMatrix4f_Multiply( &transMatrix, &scaleMatrix );
	const ovrMatrix4f m1 = ovrMatrix4f_Multiply( &rotXMatrix, &m0 );
	const ovrMatrix4f m2 = ovrMatrix4f_Multiply( &rotYMatrix, &m1 );

	return m2;
}

ovrLayerCylinder2 BuildCylinderLayer( ovrRenderer * cylinderRenderer,
	const int textureWidth, const int textureHeight,
	const ovrTracking2 * tracking, float rotatePitch )
{
	ovrLayerCylinder2 layer = vrapi_DefaultLayerCylinder2();

	const float fadeLevel = 1.0f;
	layer.Header.ColorScale.x =
		layer.Header.ColorScale.y =
		layer.Header.ColorScale.z =
		layer.Header.ColorScale.w = fadeLevel;
	layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
	layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

	//layer.Header.Flags = VRAPI_FRAME_LAYER_FLAG_CLIP_TO_TEXTURE_RECT;

	layer.HeadPose = tracking->HeadPose;

	const float density = 4500.0f;
	const float rotateYaw = 0.0f;
	const float radius = 2.0f;
	const ovrVector3f translation = { 0.0f, 0.0f, -0.5f };

	ovrMatrix4f cylinderTransform = 
		CylinderModelMatrix( textureWidth, textureHeight, translation,
			rotateYaw, rotatePitch, radius, density );

	const float circScale = density * 0.5f / textureWidth;
	const float circBias = -circScale * ( 0.5f * ( 1.0f - 1.0f / circScale ) );

	for ( int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++ )
	{
		ovrFramebuffer * cylinderFrameBuffer = &cylinderRenderer->FrameBuffer[eye];
		
		ovrMatrix4f modelViewMatrix = ovrMatrix4f_Multiply( &tracking->Eye[eye].ViewMatrix, &cylinderTransform );
		layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_Inverse( &modelViewMatrix );
		layer.Textures[eye].ColorSwapChain = cylinderFrameBuffer->ColorTextureSwapChain;
		layer.Textures[eye].SwapChainIndex = cylinderFrameBuffer->TextureSwapChainIndex;

		// Texcoord scale and bias is just a representation of the aspect ratio. The positioning
		// of the cylinder is handled entirely by the TexCoordsFromTanAngles matrix.

		const float texScaleX = circScale;
		const float texBiasX = circBias;
		const float texScaleY = -0.5f;
		const float texBiasY = texScaleY * ( 0.5f * ( 1.0f - ( 1.0f / texScaleY ) ) );

		layer.Textures[eye].TextureMatrix.M[0][0] = texScaleX;
		layer.Textures[eye].TextureMatrix.M[0][2] = texBiasX;
		layer.Textures[eye].TextureMatrix.M[1][1] = texScaleY;
		layer.Textures[eye].TextureMatrix.M[1][2] = -texBiasY;

		layer.Textures[eye].TextureRect.width = 1.0f;
		layer.Textures[eye].TextureRect.height = 1.0f;
	}

	return layer;
}
