#include "tr_local.h"
#include "tr_common.h"

#ifdef USE_PMLIGHT

enum {
	VP_GLOBAL_EYEPOS,
	VP_GLOBAL_MAX,
};

#if (VP_GLOBAL_MAX > 96)
#error VP_GLOBAL_MAX > MAX_PROGRAM_ENV_PARAMETERS_ARB
#endif

enum {
	DLIGHT_VERTEX,
	DLIGHT_FRAGMENT,
	SPRITE_VERTEX,
	SPRITE_FRAGMENT,
	GAMMA_VERTEX,
	GAMMA_FRAGMENT,
	PROGRAM_COUNT
};

typedef enum {
	Vertex,
	Fragment
} programType;

static GLuint programs[ PROGRAM_COUNT ];
static GLuint current_vp;
static GLuint current_fp;

static	qboolean programAvail	= qfalse;
static	qboolean programEnabled	= qfalse;

void ( APIENTRY * qglGenProgramsARB )( GLsizei n, GLuint *programs );
void ( APIENTRY * qglDeleteProgramsARB)( GLsizei n, const GLuint *programs );
void ( APIENTRY * qglProgramStringARB )( GLenum target, GLenum format, GLsizei len, const GLvoid *string );
void ( APIENTRY * qglBindProgramARB )( GLenum target, GLuint program );
void ( APIENTRY * qglProgramLocalParameter4fARB )( GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w );
void ( APIENTRY * qglProgramEnvParameter4fARB )( GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w );

qboolean fboAvailable = qfalse;
qboolean fboEnabled = qfalse;

typedef struct frameBuffer_s {
	GLuint fbo;
	GLuint color;			// renderbuffer if multisampled
	GLuint depthStencil;	// renderbuffer if multisampled
	qboolean multiSampled;
} frameBuffer_t;

static GLuint commonDepthStencil;
static frameBuffer_t frameBufferMS;
static frameBuffer_t frameBuffers[2];

static unsigned int frameBufferReadIndex = 0; // read this for the latest color/depth data
static qboolean frameBufferMultiSampling = qfalse;

qboolean blitMSfbo = qfalse;


// framebuffer functions
GLboolean (APIENTRY *qglIsRenderbuffer)( GLuint renderbuffer );
void ( APIENTRY *qglBindRenderbuffer )( GLenum target, GLuint renderbuffer );
void ( APIENTRY *qglDeleteFramebuffers )( GLsizei n, const GLuint *framebuffers );
void ( APIENTRY *qglDeleteRenderbuffers )( GLsizei n, const GLuint *renderbuffers );
void ( APIENTRY *qglGenRenderbuffers )( GLsizei n, GLuint *renderbuffers );
void ( APIENTRY *qglRenderbufferStorage )( GLenum target, GLenum internalformat, GLsizei width, GLsizei height );
void ( APIENTRY *qglGetRenderbufferParameteriv )( GLenum target, GLenum pname, GLint *params );
GLboolean ( APIENTRY *qglIsFramebuffer)( GLuint framebuffer );
void ( APIENTRY *qglBindFramebuffer)( GLenum target, GLuint framebuffer );
void ( APIENTRY *qglGenFramebuffers)( GLsizei n, GLuint *framebuffers );
GLenum ( APIENTRY *qglCheckFramebufferStatus )( GLenum target );
void ( APIENTRY *qglFramebufferTexture1D )( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level );
void ( APIENTRY *qglFramebufferTexture2D )( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level );
void ( APIENTRY *qglFramebufferTexture3D )( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset );
void ( APIENTRY *qglFramebufferRenderbuffer )( GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer );
void ( APIENTRY *qglGetFramebufferAttachmentParameteriv )( GLenum target, GLenum attachment, GLenum pname, GLint *params );
void ( APIENTRY *qglGenerateMipmap)( GLenum target );
void ( APIENTRY *qglBlitFramebuffer)( GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter );
void ( APIENTRY *qglRenderbufferStorageMultisample )(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);

qboolean GL_ProgramAvailable( void ) 
{
	return programAvail;
}


void GL_ProgramDisable( void )
{
	if ( programEnabled )
	{
		qglDisable( GL_VERTEX_PROGRAM_ARB );
		qglDisable( GL_FRAGMENT_PROGRAM_ARB );
		programEnabled = qfalse;
		current_vp = 0;
		current_fp = 0;
	}
}


void GL_ProgramEnable( void ) 
{
	if ( !programAvail )
		return;

	if ( current_vp != programs[ SPRITE_VERTEX ] ) {
		qglEnable( GL_VERTEX_PROGRAM_ARB );
		qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, programs[ SPRITE_VERTEX ] );
		current_vp = programs[ SPRITE_VERTEX ];
	}

	if ( current_fp != programs[ SPRITE_FRAGMENT ] ) {
		qglEnable( GL_FRAGMENT_PROGRAM_ARB );
		qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, programs[ SPRITE_FRAGMENT ] );
		current_fp = programs[ SPRITE_FRAGMENT ];
	}
	programEnabled = qtrue;
}


static void GL_DlightProgramEnable( void )
{
	if ( !programAvail )
		return;

	if ( current_vp != programs[ DLIGHT_VERTEX ] ) {
		qglEnable( GL_VERTEX_PROGRAM_ARB );
		qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, programs[ DLIGHT_VERTEX ] );
		current_vp = programs[ DLIGHT_VERTEX ];
	}

	if ( current_fp != programs[ DLIGHT_FRAGMENT ] ) {
		qglEnable( GL_FRAGMENT_PROGRAM_ARB );
		qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, programs[ DLIGHT_FRAGMENT ] );
		current_fp = programs[ DLIGHT_FRAGMENT ];
	}

	programEnabled = qtrue;
}

static void GL_GammaProgramEnable( void )
{
	if ( !programAvail )
		return;

	if ( current_vp != programs[ GAMMA_VERTEX ] ) {
		qglEnable( GL_VERTEX_PROGRAM_ARB );
		qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, programs[ GAMMA_VERTEX ] );
		current_vp = programs[ GAMMA_VERTEX ];
	}

	if ( current_fp != programs[ GAMMA_FRAGMENT ] ) {
		qglEnable( GL_FRAGMENT_PROGRAM_ARB );
		qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, programs[ GAMMA_FRAGMENT ] );
		current_fp = programs[ GAMMA_FRAGMENT ];
	}
	programEnabled = qtrue;
}

static void ARB_Lighting( const shaderStage_t* pStage )
{
	const dlight_t* dl;
	byte clipBits[ SHADER_MAX_VERTEXES ];
	unsigned hitIndexes[ SHADER_MAX_INDEXES ];
	int numIndexes;
	int i;
	int clip;

	backEnd.pc.c_lit_vertices_lateculltest += tess.numVertexes;

	dl = tess.light;

	for ( i = 0; i < tess.numVertexes; ++i ) {
		vec3_t dist;
		VectorSubtract( dl->transformed, tess.xyz[i], dist );

		if ( tess.surfType != SF_GRID && DotProduct( dist, tess.normal[i] ) <= 0.0f ) {
			clipBits[ i ] = 63;
			continue;
		}

		clip = 0;
		if ( dist[0] > dl->radius ) {
			clip |= 1;
		} else if ( dist[0] < -dl->radius ) {
			clip |= 2;
		}
		if ( dist[1] > dl->radius ) {
			clip |= 4;
		} else if ( dist[1] < -dl->radius ) {
			clip |= 8;
		}
		if ( dist[2] > dl->radius ) {
			clip |= 16;
		} else if ( dist[2] < -dl->radius ) {
			clip |= 32;
		}

		clipBits[i] = clip;
	}

	// build a list of triangles that need light
	numIndexes = 0;

	for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
		int		a, b, c;

		a = tess.indexes[i];
		b = tess.indexes[i+1];
		c = tess.indexes[i+2];
		if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
			continue;	// not lighted
		}
		hitIndexes[numIndexes] = a;
		hitIndexes[numIndexes+1] = b;
		hitIndexes[numIndexes+2] = c;
		numIndexes += 3;
	}

	backEnd.pc.c_lit_indices_latecull_in += numIndexes;
	backEnd.pc.c_lit_indices_latecull_out += tess.numIndexes - numIndexes;

	if ( !numIndexes )
		return;

	if ( tess.shader->sort < SS_OPAQUE ) {
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
	}

	GL_SelectTexture( 0 );

	R_BindAnimatedImage( &pStage->bundle[ 0 ] );
	
	R_DrawElements( numIndexes, hitIndexes );
}


void ARB_SetupLightParams( void )
{
	const dlight_t *dl;
	vec3_t lightRGB;
	float radius;

	if ( !programAvail )
		return;

	GL_DlightProgramEnable();

	dl = tess.light;

	if ( !glConfig.deviceSupportsGamma )
		VectorScale( dl->color, 2 * pow( r_intensity->value, r_gamma->value ), lightRGB );
	else
		VectorCopy( dl->color, lightRGB );

	radius = dl->radius * r_dlightScale->value;
	if ( r_greyscale->value > 0 ) {
		float luminance;
		luminance = LUMA( lightRGB[0], lightRGB[1], lightRGB[2] );
		lightRGB[0] = LERP( lightRGB[0], luminance, r_greyscale->value );
		lightRGB[1] = LERP( lightRGB[1], luminance, r_greyscale->value );
		lightRGB[2] = LERP( lightRGB[2], luminance, r_greyscale->value );
	}

	qglProgramLocalParameter4fARB( GL_VERTEX_PROGRAM_ARB, 0, dl->transformed[0], dl->transformed[1], dl->transformed[2], 0 );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, lightRGB[0], lightRGB[1], lightRGB[2], 1.0 );
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, 1.0 / Square( radius ), 0, 0, 0 );

	qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, VP_GLOBAL_EYEPOS,
		backEnd.or.viewOrigin[0], backEnd.or.viewOrigin[1], backEnd.or.viewOrigin[2], 0 );
}


void ARB_LightingPass( void )
{
	const shaderStage_t* pStage;

	if ( !programAvail )
		return;

	//if ( tess.shader->lightingStage == -1 )
	//	return;

	RB_DeformTessGeometry();

	GL_Cull( tess.shader->cullType );

	pStage = tess.xstages[ tess.shader->lightingStage ];

	R_ComputeTexCoords( pStage );

	// since this is guaranteed to be a single pass, fill and lock all the arrays
	
	qglDisableClientState( GL_COLOR_ARRAY );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	qglEnableClientState( GL_NORMAL_ARRAY );
	qglNormalPointer( GL_FLOAT, 16, tess.normal );

	qglVertexPointer( 3, GL_FLOAT, 16, tess.xyz );

	//if ( qglLockArraysEXT )
	//		qglLockArraysEXT( 0, tess.numVertexes );

	ARB_Lighting( pStage );

	//if ( qglUnlockArraysEXT )
	//		qglUnlockArraysEXT();

	qglDisableClientState( GL_NORMAL_ARRAY );
}

extern cvar_t *r_dlightSpecPower;
extern cvar_t *r_dlightSpecColor;

// welding these into the code to avoid having a pk3 dependency in the engine

static const char *dlightVP = {
	"!!ARBvp1.0 \n"
	"OPTION ARB_position_invariant; \n"
	"PARAM posEye = program.env[0]; \n"
	"PARAM posLight = program.local[0]; \n"
	"OUTPUT lv = result.texcoord[1]; \n" // 1
	"OUTPUT ev = result.texcoord[2]; \n" // 2
	"OUTPUT n = result.texcoord[3]; \n"  // 3
	"MOV result.texcoord[0], vertex.texcoord; \n"
	"MOV n, vertex.normal; \n"
	"SUB ev, posEye, vertex.position; \n"
	"SUB lv, posLight, vertex.position; \n"
	"END \n" 
};


// dynamically apply custom parameters
static const char *ARB_BuildDlightFragmentProgram( void  )
{
	static char program[1024];
	
	program[0] = '\0';
	strcat( program, 
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"PARAM lightRGB = program.local[0]; \n"
	"PARAM lightRange2recip = program.local[1]; \n"
	"TEMP base; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n"
	"ATTRIB dnLV = fragment.texcoord[1]; \n" // 1
	"ATTRIB dnEV = fragment.texcoord[2]; \n" // 2
	"ATTRIB n = fragment.texcoord[3]; \n"    // 3
	
	// normalize light vector
	"TEMP tmp, lv; \n"
	"DP3 tmp.w, dnLV, dnLV; \n"
	"RSQ lv.w, tmp.w; \n"
	"MUL lv.xyz, dnLV, lv.w; \n"

	// calculate light intensity
	"TEMP light; \n"
	"MUL tmp.x, tmp.w, lightRange2recip; \n"
	"SUB tmp.x, 1.0, tmp.x; \n"
	"MUL light, lightRGB, tmp.x; \n" // light.rgb
	);

	if ( r_dlightSpecColor->value > 0 ) {
		strcat( program, va( "PARAM specRGB = %1.2f; \n", r_dlightSpecColor->value ) );
	}

	strcat( program, va( "PARAM specEXP = %1.2f; \n", r_dlightSpecPower->value ) );

	strcat( program,
	// normalize eye vector
	"TEMP ev; \n"
	"DP3 ev.w, dnEV, dnEV; \n"
	"RSQ ev.w, ev.w; \n"
	"MUL ev.xyz, dnEV, ev.w; \n"

	// normalize (eye + light) vector
	"ADD tmp, lv, ev; \n"
	"DP3 tmp.w, tmp, tmp; \n"
	"RSQ tmp.w, tmp.w; \n"
	"MUL tmp.xyz, tmp, tmp.w; \n"

	// modulate specular strength
	"DP3_SAT tmp.w, n, tmp; \n"
	"POW tmp.w, tmp.w, specEXP.w; \n"
	"TEMP spec; \n" );
	if ( r_dlightSpecColor->value > 0 ) {
		// by constant
		strcat( program, "MUL spec, specRGB, tmp.w; \n" );
	} else {
		// by texture
		strcat( program, va( "MUL tmp.w, tmp.w, %1.2f; \n", -r_dlightSpecColor->value ) );
		strcat( program, "MUL spec, base, tmp.w; \n" );
	}

	strcat( program, 
	// bump color
	"TEMP bump; \n"
	"DP3_SAT bump.w, n, lv; \n"

	"MAD base, base, bump.w, spec; \n"
	"MUL result.color, base, light; \n"
	"END \n" 
	);
	
	r_dlightSpecColor->modified = qfalse;
	r_dlightSpecPower->modified = qfalse;

	return program;
}

static const char *spriteVP = {
	"!!ARBvp1.0 \n"
	"OPTION ARB_position_invariant; \n"
	"MOV result.texcoord[0], vertex.texcoord; \n"
	"END \n" 
};

static const char *spriteFP = {
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"TEMP base; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n"
	"TEMP test; \n"
	"SUB test.a, base.a, 0.85; \n"
	"KIL test.a; \n"
	"MOV base, 0.0; \n"
	"MOV result.color, base; \n"
	"MOV result.depth, fragment.position.z; \n"
	"END \n" 
};


static const char *gammaVP = {
	"!!ARBvp1.0 \n"
	"OPTION ARB_position_invariant; \n"
	"MOV result.texcoord[0], vertex.texcoord; \n"
	"END \n" 
};
static const char *gammaFP = {
	"!!ARBfp1.0 \n"
	"OPTION ARB_precision_hint_fastest; \n"
	"PARAM gamma = program.local[0]; \n"
	"TEMP base; \n"
	"TEX base, fragment.texcoord[0], texture[0], 2D; \n"
	"POW base.x, base.x, gamma.x; \n"
	"POW base.y, base.y, gamma.y; \n"
	"POW base.z, base.z, gamma.z; \n"
	"MUL base.xyz, base, gamma.w; \n"
	"MOV base.w, 1.0; \n"
	"MOV result.color, base; \n"
	"END \n" 
};


static void ARB_DeletePrograms( void ) 
{
	qglDeleteProgramsARB( ARRAY_LEN( programs ), programs );
	Com_Memset( programs, 0, sizeof( programs ) );
}


static qboolean ARB_CompileProgram( programType ptype, const char *text, GLuint program ) 
{
	GLint errorPos;
	int kind;

	if ( ptype == Fragment )
		kind = GL_FRAGMENT_PROGRAM_ARB;
	else
		kind = GL_VERTEX_PROGRAM_ARB;

	qglBindProgramARB( kind, program );
	qglProgramStringARB( kind, GL_PROGRAM_FORMAT_ASCII_ARB, strlen( text ), text );
	qglGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorPos );
	if ( qglGetError() != GL_NO_ERROR || errorPos != -1 ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "%s Compile Error: %s", (ptype == Fragment) ? "FP" : "VP", 
			qglGetString( GL_PROGRAM_ERROR_STRING_ARB ) );
		ARB_DeletePrograms();
		return qfalse;
	}

	return qtrue;
}


qboolean ARB_UpdatePrograms( void )
{
	const char *dlightFP;

	if ( !qglGenProgramsARB )
		return qfalse;

	if ( programAvail ) // delete old programs
	{
		programEnabled = qtrue; // force disable
		GL_ProgramDisable();
		ARB_DeletePrograms();
		programAvail = qfalse;
	}

	qglGenProgramsARB( ARRAY_LEN( programs ), programs );

	if ( !ARB_CompileProgram( Vertex, dlightVP, programs[ DLIGHT_VERTEX ] ) )
		return qfalse;

	dlightFP = ARB_BuildDlightFragmentProgram();

	if ( !ARB_CompileProgram( Fragment, dlightFP, programs[ DLIGHT_FRAGMENT ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Vertex, spriteVP, programs[ SPRITE_VERTEX ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Fragment, spriteFP, programs[ SPRITE_FRAGMENT ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Vertex, gammaVP, programs[ GAMMA_VERTEX ] ) )
		return qfalse;

	if ( !ARB_CompileProgram( Fragment, gammaFP, programs[ GAMMA_FRAGMENT ] ) )
		return qfalse;

	programAvail = qtrue;

	return qtrue;
}



void FBO_Clean( frameBuffer_t *fb ) 
{
	if ( fb->fbo ) 
	{
		qglBindFramebuffer( GL_FRAMEBUFFER, fb->fbo );
		if ( fb->multiSampled ) 
		{
			qglBindRenderbuffer( GL_RENDERBUFFER, 0 );
			if ( fb->color ) 
			{
				qglDeleteRenderbuffers( 1, &fb->color );
				fb->color = 0;
			}
			if ( fb->depthStencil ) 
			{
				qglDeleteRenderbuffers( 1, &fb->depthStencil );
				fb->depthStencil = 0;
			}
		}
		else 
		{
			qglBindTexture( GL_TEXTURE_2D, 0 );
			if ( fb->color ) 
			{
				qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );	
				qglDeleteTextures( 1, &fb->color );
				fb->color = 0;
			}
			if ( fb->depthStencil || commonDepthStencil ) 
			{
				qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0 );
				if ( fb->depthStencil && fb->depthStencil != commonDepthStencil ) 
				{
					qglDeleteTextures( 1, &fb->depthStencil );
					fb->depthStencil = 0;
				}
			}
		}

		qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
		qglDeleteFramebuffers( 1, &fb->fbo );
		fb->fbo = 0;
	}
}


void FBO_CleanDepth( void ) 
{
	if ( commonDepthStencil ) 
	{
		qglDeleteTextures( 1, &commonDepthStencil );
		commonDepthStencil = 0;
	}
}


static GLuint FBO_CreateDepthTexture( GLsizei width, GLsizei height ) 
{
	GLuint tex;
	qglGenTextures( 1, &tex );
	qglBindTexture( GL_TEXTURE_2D, tex );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

	if ( r_stencilbits->integer == 0 )
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL );
	else
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL );

	return tex;
}


static qboolean FBO_Create( frameBuffer_t *fb, qboolean depthStencil )
{
	int fboStatus;

	fb->multiSampled = qfalse;

	if ( frameBufferMultiSampling && r_flares->integer ) 
	{
		depthStencil = qtrue;
	}

	if ( depthStencil )
	{
		if ( !commonDepthStencil ) 
		{
			commonDepthStencil = FBO_CreateDepthTexture( glConfig.vidWidth, glConfig.vidHeight );
		}
		fb->depthStencil = commonDepthStencil;
	}

	// color texture
	qglGenTextures( 1, &fb->color );
	qglBindTexture( GL_TEXTURE_2D, fb->color );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

	qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, glConfig.vidWidth, glConfig.vidHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL );

	qglGenFramebuffers( 1, &fb->fbo );
	qglBindFramebuffer( GL_FRAMEBUFFER, fb->fbo );
	qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->color, 0 );

	if ( depthStencil ) 
	{
		if ( r_stencilbits->integer == 0 )
			qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb->depthStencil, 0 );
		else
			qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, fb->depthStencil, 0 );
	}

	fboStatus = qglCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( fboStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		ri.Printf( PRINT_ALL, "Failed to create FBO (status %d, error %d)\n", fboStatus, (int)qglGetError() );
		FBO_Clean( fb );
		return qfalse;
	}

	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glState.currenttextures[ glState.currenttmu ] = 0;
	qglBindTexture( GL_TEXTURE_2D, 0 );

	return qtrue;
}


static qboolean FBO_CreateMS( frameBuffer_t *fb )
{
	GLsizei nSamples = r_ext_multisample->integer;
	int fboStatus;
	
	fb->multiSampled = qtrue;

	if ( nSamples <= 0 )
	{
		return qfalse;
	}
	nSamples = (nSamples + 1) & ~1;

	qglGenFramebuffers( 1, &fb->fbo );
	qglBindFramebuffer( GL_FRAMEBUFFER, fb->fbo );

	qglGenRenderbuffers( 1, &fb->color );
	qglBindRenderbuffer( GL_RENDERBUFFER, fb->color );
	while ( nSamples > 0 ) {
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, nSamples, GL_RGBA8, glConfig.vidWidth, glConfig.vidHeight );
		if ( (int)qglGetError() == GL_INVALID_VALUE/* != GL_NO_ERROR */ ) {
			ri.Printf( PRINT_ALL, "...%ix MSAA is not available\n", nSamples );
			nSamples -= 2;
		} else {
			ri.Printf( PRINT_ALL, "...using %ix MSAA\n", nSamples );
			break;
		}
	}
	if ( nSamples <= 0 ) 
	{
		FBO_Clean( fb );
		return qfalse;
	}
	qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb->color );

	qglGenRenderbuffers( 1, &fb->depthStencil );
	qglBindRenderbuffer( GL_RENDERBUFFER, fb->depthStencil );
	if ( r_stencilbits->integer == 0 )
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, nSamples, GL_DEPTH_COMPONENT32, glConfig.vidWidth, glConfig.vidHeight );
	else
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, nSamples, GL_DEPTH24_STENCIL8, glConfig.vidWidth, glConfig.vidHeight );

	if ( (int)qglGetError() != GL_NO_ERROR ) 
	{
		FBO_Clean( fb );
		return qfalse;
	}

	if ( r_stencilbits->integer == 0 )
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->depthStencil );
	else
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->depthStencil );

	fboStatus = qglCheckFramebufferStatus( GL_FRAMEBUFFER );
	if ( fboStatus != GL_FRAMEBUFFER_COMPLETE )
	{
		Com_Printf( "Failed to create MS FBO (status 0x%x, error %d)\n", fboStatus, (int)qglGetError() );
		FBO_Clean( fb );
		return qfalse;
	}
	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );

	return qtrue;
}


void FBO_BindMain( void ) 
{
	if ( fboAvailable && programAvail ) 
	{
		const frameBuffer_t *fb;
		if ( frameBufferMultiSampling ) 
		{
			blitMSfbo = qtrue;
			fb = &frameBufferMS;
		} 
		else 
		{
			blitMSfbo = qfalse;
			fb = &frameBuffers[ frameBufferReadIndex ];
		}
		qglBindFramebuffer( GL_FRAMEBUFFER, fb->fbo );
	}
}


void FBO_Bind( void )
{
	if ( fboAvailable && programAvail ) 
	{
		const frameBuffer_t *fb = &frameBuffers[ frameBufferReadIndex ];
		qglBindFramebuffer( GL_FRAMEBUFFER, fb->fbo );
	}
}


static void FBO_Swap( void )
{
	frameBufferReadIndex ^= 1;
}


static void FBO_BlitToBackBuffer( void )
{
	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;

	//const frameBuffer_t *fbo = &frameBuffers[ frameBufferReadIndex ];

	//qglBindFramebuffer( GL_READ_FRAMEBUFFER, fbo->fbo );
	qglBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );

	//qglReadBuffer( GL_COLOR_ATTACHMENT0 );
	qglDrawBuffer( GL_BACK );

	qglBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR );
}


void FBO_BlitMS( qboolean depthOnly )
{
	//if ( blitMSfbo ) 
	//{
	const int w = glConfig.vidWidth;
	const int h = glConfig.vidHeight;

	const frameBuffer_t *r = &frameBufferMS;
	const frameBuffer_t *d = &frameBuffers[ frameBufferReadIndex ];

	qglBindFramebuffer( GL_READ_FRAMEBUFFER, r->fbo );
	qglBindFramebuffer( GL_DRAW_FRAMEBUFFER, d->fbo );

	if ( depthOnly ) 
	{
		qglBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST );
		qglBindFramebuffer( GL_READ_FRAMEBUFFER, d->fbo );
		return;
	}

	blitMSfbo = qfalse;
	qglReadBuffer( GL_COLOR_ATTACHMENT0 );
	qglDrawBuffer( GL_COLOR_ATTACHMENT0 );
	qglBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST );
}


void FBO_PostProcess( void )
{
	const float obScale = 1 << tr.overbrightBits;
	const float gamma = 1.0f / r_gamma->value;
	const float w = glConfig.vidWidth;
	const float h = glConfig.vidHeight;

	if ( !fboAvailable || !programAvail )
		return;

	if ( frameBufferMultiSampling && blitMSfbo )
		FBO_BlitMS( qfalse );

	FBO_Swap();
	FBO_Bind();

	GL_SelectTexture( 0 );
	qglBindTexture( GL_TEXTURE_2D, frameBuffers[ frameBufferReadIndex ^ 1 ].color );

	if ( !backEnd.projection2D )
	{
		qglMatrixMode( GL_PROJECTION );
		qglLoadIdentity();
		qglOrtho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 );
		qglMatrixMode( GL_MODELVIEW );
		qglLoadIdentity();
	}
	GL_State( GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );

	GL_GammaProgramEnable();
	qglProgramLocalParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, gamma, gamma, gamma, obScale );

	qglBegin( GL_QUADS );
		qglTexCoord2f( 0.0f, 0.0f );
		qglVertex2f( 0.0f, h );
		qglTexCoord2f( 0.0f, 1.0f );
		qglVertex2f( 0.0f, 0.0f );
		qglTexCoord2f( 1.0f, 1.0f );
		qglVertex2f( w, 0.0f );
		qglTexCoord2f( 1.0f, 0.0f );
		qglVertex2f( w, h );
	qglEnd();

	GL_ProgramDisable();

	glState.currenttextures[ glState.currenttmu ] = 0;
	qglBindTexture( GL_TEXTURE_2D, 0 );

	FBO_BlitToBackBuffer();
}


static const void *fp;
#define GPA(fn) fp = qwglGetProcAddress( #fn ); if ( !fp ) { Com_Printf( "GPA failed on '%s'\n", #fn ); goto __fail; } else { memcpy( &q##fn, &fp, sizeof( fp ) ); }

static void QGL_InitShaders( void ) 
{
	programAvail = qfalse;

	if ( !r_allowExtensions->integer )
		return;

	if ( atof( (const char *)qglGetString( GL_VERSION ) ) < 1.4 ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "...OpenGL 1.4 is not available\n" );
		return;
	}

	if ( !GLimp_HaveExtension( "GL_ARB_vertex_program" ) ) {
		return;
	}

	if ( !GLimp_HaveExtension( "GL_ARB_fragment_program" ) ) {
		return;
	}

	GPA( glGenProgramsARB );
	GPA( glBindProgramARB );
	GPA( glProgramStringARB );
	GPA( glDeleteProgramsARB );
	GPA( glProgramLocalParameter4fARB );
	GPA( glProgramEnvParameter4fARB );

	programAvail = qtrue;
__fail:
	return;
}


static void QGL_InitFBO( void ) 
{
	fboAvailable = qfalse;

	if ( !r_allowExtensions->integer )
		return;

	if ( !programAvail )
		return;

	if ( !GLimp_HaveExtension( "GL_EXT_framebuffer_object" ) )
		return;

	if ( !GLimp_HaveExtension( "GL_EXT_framebuffer_blit" ) )
		return;

	if ( !GLimp_HaveExtension( "GL_EXT_framebuffer_multisample" ) )
		return;

	GPA( glBindRenderbuffer );
	GPA( glBlitFramebuffer );
	GPA( glDeleteRenderbuffers );
	GPA( glGenRenderbuffers );
	GPA( glGetRenderbufferParameteriv );
	GPA( glIsFramebuffer );
	GPA( glBindFramebuffer );
	GPA( glDeleteFramebuffers );
	GPA( glCheckFramebufferStatus );
	GPA( glFramebufferTexture1D );
	GPA( glFramebufferTexture2D );
	GPA( glFramebufferTexture3D );
	GPA( glFramebufferRenderbuffer );
	GPA( glGenerateMipmap );
	GPA( glGenFramebuffers );
	GPA( glGetFramebufferAttachmentParameteriv );
	GPA( glIsRenderbuffer );
	GPA( glRenderbufferStorage );
	GPA( glRenderbufferStorageMultisample );
	fboAvailable = qtrue;
__fail:
	return;
}


void QGL_EarlyInitARB( void ) 
{
	QGL_InitShaders();
	QGL_InitFBO();
}


void QGL_InitARB( void )
{
	if ( ARB_UpdatePrograms() )
	{
		if ( r_fbo->integer && fboAvailable ) 
		{
			qboolean result = qfalse;
			frameBufferMultiSampling = qfalse;

			if ( FBO_CreateMS( &frameBufferMS ) ) 
			{
				frameBufferMultiSampling = qtrue;
				result = FBO_Create( &frameBuffers[ 0 ], qfalse ) && FBO_Create( &frameBuffers[ 1 ], qfalse );
				frameBufferMultiSampling = result;
			}
			else 
			{
				result = FBO_Create( &frameBuffers[ 0 ], qtrue ) && FBO_Create( &frameBuffers[ 1 ], qtrue );
			}

			if ( result ) 
			{
				FBO_BindMain();
				qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			}
			else 
			{
				FBO_Clean( &frameBufferMS );
				FBO_Clean( &frameBuffers[0] );
				FBO_Clean( &frameBuffers[1] );
				FBO_CleanDepth();
				fboAvailable = qfalse;		
			}
		}
		else 
		{
			fboAvailable = qfalse;
		}
		
		//programAvail = qtrue;
		programEnabled = qtrue; // force disable
		GL_ProgramDisable();
		ri.Printf( PRINT_ALL, "...using ARB shaders\n" );
		if ( fboAvailable ) 
		{
			ri.Printf( PRINT_ALL, "...using FBO\n" );
		}

		return; // success
	}

	ri.Printf( PRINT_ALL, "...not using ARB shaders\n" );

	qglGenProgramsARB		= NULL;
	qglDeleteProgramsARB	= NULL;
	qglProgramStringARB		= NULL;
	qglBindProgramARB		= NULL;
	qglProgramLocalParameter4fARB = NULL;
	qglProgramEnvParameter4fARB = NULL;

	fboAvailable = qfalse;
	programAvail = qfalse;
	programEnabled = qfalse;
}


void QGL_DoneARB( void )
{
	if ( programAvail )
	{
		programEnabled = qtrue; // force disable
		GL_ProgramDisable();
		ARB_DeletePrograms();
	}

	FBO_Clean( &frameBufferMS );
	FBO_Clean( &frameBuffers[0] );
	FBO_Clean( &frameBuffers[1] );
	FBO_CleanDepth();

	programAvail = qfalse;
	fboAvailable = qfalse;

	qglGenProgramsARB		= NULL;
	qglDeleteProgramsARB	= NULL;
	qglProgramStringARB		= NULL;
	qglBindProgramARB		= NULL;
	qglProgramLocalParameter4fARB = NULL;
	qglProgramEnvParameter4fARB = NULL;
}

#endif // USE_PMLIGHT
