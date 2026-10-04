// tr_postfx.cpp -- offscreen scene rendering and post-process effects
//
// r_fbo 1 renders each frame into a framebuffer object (multisampled when
// MSAA is on) and presents it with the post-process gamma pass, sampling the
// texture instead of copying the back buffer. On top of it:
//   r_hdr 1:   16-bit float scene, so additive light (sabers, bolts,
//              explosions) builds up above full brightness; presenting it
//              rolls highlights off smoothly instead of clipping them.
//   r_bloom 1: threshold bloom of the 3D view (not the HUD) through a chain of
//              downsampled buffers.
// Everything is off by default; with r_fbo 1 and no effect the image is the
// same as without it. Programs are ARB assembly, the level the gamma and glow
// passes already need, embedded in the code so they also work on pure servers.

#include "tr_local.h"

cvar_t	*r_fbo;
cvar_t	*r_hdr;
cvar_t	*r_bloom;
cvar_t	*r_bloomIntensity;
cvar_t	*r_bloomThreshold;
cvar_t	*r_exposure;

static PFNGLGENFRAMEBUFFERSPROC						qglGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC					qglDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC						qglBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC				qglFramebufferTexture2D;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC				qglFramebufferRenderbuffer;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC				qglCheckFramebufferStatus;
static PFNGLGENRENDERBUFFERSPROC					qglGenRenderbuffers;
static PFNGLDELETERENDERBUFFERSPROC					qglDeleteRenderbuffers;
static PFNGLBINDRENDERBUFFERPROC					qglBindRenderbuffer;
static PFNGLRENDERBUFFERSTORAGEPROC					qglRenderbufferStorage;
static PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC		qglRenderbufferStorageMultisample;
static PFNGLBLITFRAMEBUFFERPROC						qglBlitFramebuffer;
static PFNGLCLAMPCOLORARBPROC						qglClampColorARB;

#define BLOOM_LEVELS	5

typedef struct {
	GLuint	fbo;
	GLuint	texture;	// rectangle texture
	int		width, height;
} renderTarget_t;

static struct {
	qboolean		available;		// extensions present
	qboolean		active;			// r_fbo and everything created
	qboolean		hdr;
	qboolean		bloom;
	qboolean		drawing;		// the frame goes to the offscreen target
	int				width, height;
	int				samples;
	GLenum			format;			// scene color format

	renderTarget_t	scene;			// single sampled, sampled by the post passes
	GLuint			msaaFbo;		// multisampled draw target when MSAA is on
	GLuint			msaaColor, msaaDepth;
	GLuint			sceneDepth;
	renderTarget_t	blur[BLOOM_LEVELS];	// bloom chain, half size and down
	renderTarget_t	post;			// HDR: tone mapped view, copied back

	GLuint			tapProgram;		// 4 bilinear taps, threshold, scale
	GLuint			copyProgram;
	GLuint			toneMapProgram;	// HDR: exposure, bloom and highlight roll-off
} pfx;

static const char *pfxTapFP =
	"!!ARBfp1.0\n"
	"PARAM off = program.env[0];\n"		// (dx, dy, -dx, -dy) in source texels
	"PARAM ts = program.env[1];\n"		// (threshold, scale, 0, 0)
	"PARAM quarter = { 0.25, 0.25, 0.25, 0.25 };\n"
	"PARAM zero = { 0, 0, 0, 0 };\n"
	"PARAM one = { 1, 1, 1, 1 };\n"
	"TEMP t0, t1, t2, t3, sum;\n"
	"ADD t0.xy, fragment.texcoord[0], off.xyxx;\n"
	"ADD t1.xy, fragment.texcoord[0], off.zwzz;\n"
	"ADD t2.xy, fragment.texcoord[0], off.xwxx;\n"
	"ADD t3.xy, fragment.texcoord[0], off.zyzz;\n"
	"TEX t0, t0, texture[0], RECT;\n"
	"TEX t1, t1, texture[0], RECT;\n"
	"TEX t2, t2, texture[0], RECT;\n"
	"TEX t3, t3, texture[0], RECT;\n"
	"ADD sum, t0, t1;\n"
	"ADD sum, sum, t2;\n"
	"ADD sum, sum, t3;\n"
	"MUL sum, sum, quarter;\n"
	"SUB sum.xyz, sum, ts.x;\n"
	"MAX sum.xyz, sum, zero;\n"
	"MUL result.color.xyz, sum, ts.y;\n"
	"MOV result.color.w, one.x;\n"
	"END\n";

static const char *pfxCopyFP =
	"!!ARBfp1.0\n"
	"TEX result.color, fragment.texcoord[0], texture[0], RECT;\n"
	"END\n";

// Scene times exposure plus bloom, then the highlights: x below the knee is
// kept, above it an exponential shoulder with slope 1 at the knee approaches
// the white point (the LUT input that saturates) instead of clipping
static const char *pfxToneMapFP =
	"!!ARBfp1.0\n"
	"PARAM tm = program.env[0];\n"		// (knee, white - knee, log2(e) / (white - knee), exposure)
	"PARAM bl = program.env[1];\n"		// (bloom scale, 0, 0, 0)
	"PARAM bc = program.env[2];\n"		// bloom texcoord = (texcoord - bc.xy) * bc.zw
	"PARAM zero = { 0, 0, 0, 0 };\n"
	"PARAM one = { 1, 1, 1, 1 };\n"
	"TEMP c, b, d, e, s;\n"
	"TEX c, fragment.texcoord[0], texture[0], RECT;\n"
	"SUB b.xy, fragment.texcoord[0], bc;\n"
	"MUL b.xy, b, bc.zwzz;\n"
	"TEX b, b, texture[1], RECT;\n"
	"MUL c.xyz, c, tm.w;\n"
	"MAD c.xyz, b, bl.x, c;\n"
	"SUB d.xyz, c, tm.x;\n"
	"MAX e.xyz, d, zero;\n"
	"MUL s.xyz, e, -tm.z;\n"
	"EX2 s.x, s.x;\n"
	"EX2 s.y, s.y;\n"
	"EX2 s.z, s.z;\n"
	"SUB s.xyz, one, s;\n"
	"MAD s.xyz, s, tm.y, tm.x;\n"
	"CMP result.color.xyz, d, c, s;\n"
	"MOV result.color.w, c.w;\n"
	"END\n";

static GLuint R_PostFXProgram( const char *text ) {
	GLuint	program = 0;
	GLint	errorPos = -1;

	qglGenProgramsARB( 1, &program );
	qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, program );
	qglProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (int)strlen( text ), text );
	qglGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorPos );
	if ( errorPos != -1 ) {
		ri.Printf( PRINT_WARNING, "post-process program error at %i: %s\n", errorPos,
			(const char *)qglGetString( GL_PROGRAM_ERROR_STRING_ARB ) );
		qglDeleteProgramsARB( 1, &program );
		return 0;
	}
	return program;
}

static qboolean R_CreateTarget( renderTarget_t *rt, int width, int height, GLenum format, GLuint depth ) {
	rt->width = width;
	rt->height = height;
	rt->texture = R_AllocTextureName();

	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, rt->texture );
	qglTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, format, width, height, 0, GL_RGBA,
		( format == GL_RGBA16F || format == GL_RGB16F ) ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE, NULL );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	qglGenFramebuffers( 1, &rt->fbo );
	qglBindFramebuffer( GL_FRAMEBUFFER, rt->fbo );
	qglFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_ARB, rt->texture, 0 );
	if ( depth ) {
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth );
	}
	return (qboolean)( qglCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
}

static void R_DestroyTargets( void ) {
	if ( !qglDeleteFramebuffers ) {
		return;
	}
	if ( pfx.scene.fbo ) {
		qglDeleteFramebuffers( 1, &pfx.scene.fbo );
		qglDeleteTextures( 1, &pfx.scene.texture );
	}
	for ( int i = 0; i < BLOOM_LEVELS; i++ ) {
		if ( pfx.blur[i].fbo ) {
			qglDeleteFramebuffers( 1, &pfx.blur[i].fbo );
			qglDeleteTextures( 1, &pfx.blur[i].texture );
		}
	}
	if ( pfx.post.fbo ) {
		qglDeleteFramebuffers( 1, &pfx.post.fbo );
		qglDeleteTextures( 1, &pfx.post.texture );
	}
	if ( pfx.msaaFbo ) {
		qglDeleteFramebuffers( 1, &pfx.msaaFbo );
		qglDeleteRenderbuffers( 1, &pfx.msaaColor );
		qglDeleteRenderbuffers( 1, &pfx.msaaDepth );
	}
	if ( pfx.sceneDepth ) {
		qglDeleteRenderbuffers( 1, &pfx.sceneDepth );
	}
	Com_Memset( &pfx.scene, 0, sizeof( pfx.scene ) );
	Com_Memset( pfx.blur, 0, sizeof( pfx.blur ) );
	Com_Memset( &pfx.post, 0, sizeof( pfx.post ) );
	pfx.msaaFbo = pfx.msaaColor = pfx.msaaDepth = pfx.sceneDepth = 0;
}

static qboolean R_CreateTargetsFormat( GLenum format ) {
	pfx.format = format;
	pfx.width = glConfig.vidWidth;
	pfx.height = glConfig.vidHeight;
	pfx.samples = tr.msaaSamples > 1 ? tr.msaaSamples : 0;

	if ( pfx.samples ) {
		// draw into multisampled storage, resolve into the scene texture
		qglGenRenderbuffers( 1, &pfx.msaaColor );
		qglBindRenderbuffer( GL_RENDERBUFFER, pfx.msaaColor );
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, pfx.samples, format, pfx.width, pfx.height );
		qglGenRenderbuffers( 1, &pfx.msaaDepth );
		qglBindRenderbuffer( GL_RENDERBUFFER, pfx.msaaDepth );
		qglRenderbufferStorageMultisample( GL_RENDERBUFFER, pfx.samples, GL_DEPTH24_STENCIL8, pfx.width, pfx.height );

		qglGenFramebuffers( 1, &pfx.msaaFbo );
		qglBindFramebuffer( GL_FRAMEBUFFER, pfx.msaaFbo );
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, pfx.msaaColor );
		qglFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pfx.msaaDepth );
		if ( qglCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			return qfalse;
		}
		if ( !R_CreateTarget( &pfx.scene, pfx.width, pfx.height, format, 0 ) ) {
			return qfalse;
		}
	} else {
		qglGenRenderbuffers( 1, &pfx.sceneDepth );
		qglBindRenderbuffer( GL_RENDERBUFFER, pfx.sceneDepth );
		qglRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, pfx.width, pfx.height );
		if ( !R_CreateTarget( &pfx.scene, pfx.width, pfx.height, format, pfx.sceneDepth ) ) {
			return qfalse;
		}
	}
	qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );

	if ( pfx.bloom ) {
		int w = pfx.width, h = pfx.height;

		for ( int i = 0; i < BLOOM_LEVELS; i++ ) {
			w = MAX( 1, w / 2 );
			h = MAX( 1, h / 2 );
			// the blur chain holds no alpha
			if ( !R_CreateTarget( &pfx.blur[i], w, h, pfx.hdr ? GL_RGBA16F : GL_RGBA8, 0 ) ) {
				return qfalse;
			}
		}
	}
	// tone mapped values fit in 8 bits
	if ( pfx.hdr && !R_CreateTarget( &pfx.post, pfx.width, pfx.height, GL_RGBA8, 0 ) ) {
		return qfalse;
	}
	return qtrue;
}

// The scene keeps an alpha channel only if the window has one, so blends
// with the destination alpha work as they do on screen: without alpha bits
// it reads as 1. Float RGB isn't always renderable, then RGBA is used.
static qboolean R_CreateTargets( void ) {
	GLint alphaBits = 0;

	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
	qglGetIntegerv( GL_ALPHA_BITS, &alphaBits );

	const GLenum preferred = pfx.hdr ? ( alphaBits ? GL_RGBA16F : GL_RGB16F ) : ( alphaBits ? GL_RGBA8 : GL_RGB8 );
	if ( R_CreateTargetsFormat( preferred ) ) {
		return qtrue;
	}
	R_DestroyTargets();
	if ( preferred == GL_RGB16F || preferred == GL_RGB8 ) {
		return R_CreateTargetsFormat( pfx.hdr ? GL_RGBA16F : GL_RGBA8 );
	}
	return qfalse;
}

/*
==================
R_InitPostFX

Called at the end of R_Init, after the shaders (gamma program) exist
==================
*/
void R_InitPostFX( void ) {
	r_fbo = ri.Cvar_Get( "r_fbo", "0", CVAR_ARCHIVE | CVAR_GLOBAL | CVAR_LATCH );
	r_hdr = ri.Cvar_Get( "r_hdr", "0", CVAR_ARCHIVE | CVAR_GLOBAL | CVAR_LATCH );
	r_bloom = ri.Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE | CVAR_GLOBAL | CVAR_LATCH );
	r_bloomIntensity = ri.Cvar_Get( "r_bloomIntensity", "0.5", CVAR_ARCHIVE | CVAR_GLOBAL );
	r_bloomThreshold = ri.Cvar_Get( "r_bloomThreshold", "0.75", CVAR_ARCHIVE | CVAR_GLOBAL );
	r_exposure = ri.Cvar_Get( "r_exposure", "1", CVAR_ARCHIVE | CVAR_GLOBAL );

	Com_Memset( &pfx, 0, sizeof( pfx ) );

	if ( !r_fbo->integer ) {
		return;
	}

	const qboolean core = (qboolean)( GL_CheckForExtension( "GL_ARB_framebuffer_object" ) );
	if ( !core && !( GL_CheckForExtension( "GL_EXT_framebuffer_object" ) && GL_CheckForExtension( "GL_EXT_framebuffer_blit" ) &&
		GL_CheckForExtension( "GL_EXT_framebuffer_multisample" ) && GL_CheckForExtension( "GL_EXT_packed_depth_stencil" ) ) ) {
		ri.Printf( PRINT_WARNING, "r_fbo: framebuffer objects are not supported\n" );
		return;
	}
	if ( r_gammamethod->integer != GAMMA_POSTPROCESSING || !tr.gammaPixelShader ) {
		ri.Printf( PRINT_WARNING, "r_fbo: needs the post-process gamma (r_gammamethod 2)\n" );
		return;
	}
	if ( glConfig.stereoEnabled ) {
		ri.Printf( PRINT_WARNING, "r_fbo: not available in stereo\n" );
		return;
	}

#define PFX_PROC( name, ext ) q##name = ( decltype( q##name ) )WIN_GL_GetProcAddress( core ? #name : #name ext )
	PFX_PROC( glGenFramebuffers, "EXT" );
	PFX_PROC( glDeleteFramebuffers, "EXT" );
	PFX_PROC( glBindFramebuffer, "EXT" );
	PFX_PROC( glFramebufferTexture2D, "EXT" );
	PFX_PROC( glFramebufferRenderbuffer, "EXT" );
	PFX_PROC( glCheckFramebufferStatus, "EXT" );
	PFX_PROC( glGenRenderbuffers, "EXT" );
	PFX_PROC( glDeleteRenderbuffers, "EXT" );
	PFX_PROC( glBindRenderbuffer, "EXT" );
	PFX_PROC( glRenderbufferStorage, "EXT" );
	PFX_PROC( glRenderbufferStorageMultisample, "EXT" );
	PFX_PROC( glBlitFramebuffer, "EXT" );
#undef PFX_PROC
	qglClampColorARB = (PFNGLCLAMPCOLORARBPROC)WIN_GL_GetProcAddress( "glClampColorARB" );

	if ( !qglGenFramebuffers || !qglBindFramebuffer || !qglFramebufferTexture2D || !qglFramebufferRenderbuffer ||
		!qglCheckFramebufferStatus || !qglGenRenderbuffers || !qglBindRenderbuffer || !qglRenderbufferStorage ||
		!qglRenderbufferStorageMultisample || !qglBlitFramebuffer || !qglDeleteFramebuffers || !qglDeleteRenderbuffers ) {
		ri.Printf( PRINT_WARNING, "r_fbo: missing framebuffer functions\n" );
		return;
	}
	pfx.available = qtrue;

	pfx.hdr = (qboolean)( r_hdr->integer && ( GL_CheckForExtension( "GL_ARB_texture_float" ) || GL_CheckForExtension( "GL_ARB_half_float_pixel" ) ) );
	if ( r_hdr->integer && !pfx.hdr ) {
		ri.Printf( PRINT_WARNING, "r_hdr: float textures are not supported\n" );
	}
	pfx.bloom = (qboolean)!!r_bloom->integer;

	pfx.tapProgram = R_PostFXProgram( pfxTapFP );
	pfx.copyProgram = R_PostFXProgram( pfxCopyFP );
	pfx.toneMapProgram = R_PostFXProgram( pfxToneMapFP );
	if ( !pfx.tapProgram || !pfx.copyProgram || !pfx.toneMapProgram ) {
		pfx.bloom = qfalse;
		pfx.hdr = qfalse;
	}

	qboolean created = R_CreateTargets();
	if ( !created && pfx.hdr ) {
		ri.Printf( PRINT_WARNING, "r_hdr: can't render to float textures\n" );
		R_DestroyTargets();
		pfx.hdr = qfalse;
		created = R_CreateTargets();
	}
	if ( !created ) {
		ri.Printf( PRINT_WARNING, "r_fbo: couldn't create the render targets\n" );
		R_DestroyTargets();
		qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
		return;
	}

	if ( pfx.hdr && qglClampColorARB ) {
		// keep additive light above 1.0 in the float scene
		qglClampColorARB( GL_CLAMP_FRAGMENT_COLOR_ARB, GL_FALSE );
	}

	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
	pfx.active = qtrue;
	if ( pfx.hdr ) {
		// the glow pass copies the scene into sceneImage and draws it back
		R_UpdateImages();
	}
	ri.Printf( PRINT_ALL, "...rendering offscreen (%ix%i%s%s%s)\n", pfx.width, pfx.height,
		pfx.samples ? va( ", MSAA %ix", pfx.samples ) : "", pfx.hdr ? ", HDR" : "", pfx.bloom ? ", bloom" : "" );
}

void R_ShutdownPostFX( void ) {
	if ( !pfx.available ) {
		return;
	}
	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
	R_DestroyTargets();
	const GLuint programs[] = { pfx.tapProgram, pfx.copyProgram, pfx.toneMapProgram };
	for ( size_t i = 0; i < ARRAY_LEN( programs ); i++ ) {
		if ( programs[i] ) {
			qglDeleteProgramsARB( 1, &programs[i] );
		}
	}
	Com_Memset( &pfx, 0, sizeof( pfx ) );
}

// window resized: recreate the targets at the new size
void R_ResizePostFX( void ) {
	if ( !pfx.active || ( pfx.width == glConfig.vidWidth && pfx.height == glConfig.vidHeight ) ) {
		return;
	}
	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
	R_DestroyTargets();
	if ( !R_CreateTargets() ) {
		ri.Printf( PRINT_WARNING, "r_fbo: couldn't resize the render targets, rendering to the window\n" );
		R_DestroyTargets();
		pfx.active = qfalse;
	}
	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

// format of tr.sceneImage, which the glow pass fills with copies of the scene
GLenum R_PostFXSceneFormat( void ) {
	return pfx.active && pfx.hdr ? GL_RGBA16F : GL_RGBA8;
}

/*
==================
R_PostFXBindScene

Frame start (RB_DrawBuffer): the back buffer of mono rendering is replaced by
the offscreen target. Returns qfalse to draw to the window as usual.
==================
*/
qboolean R_PostFXBindScene( GLenum buffer ) {
	if ( !pfx.active || buffer != GL_BACK ) {
		pfx.drawing = qfalse;
		return qfalse;
	}
	qglBindFramebuffer( GL_FRAMEBUFFER, pfx.samples ? pfx.msaaFbo : pfx.scene.fbo );
	qglDrawBuffer( GL_COLOR_ATTACHMENT0 );
	qglReadBuffer( GL_COLOR_ATTACHMENT0 );
	pfx.drawing = qtrue;
	return qtrue;
}

// the frame hasn't reached the window yet
qboolean R_PostFXPending( void ) {
	return pfx.drawing;
}

// copies the multisampled image into the scene texture
static void R_ResolveScene( int x, int y, int w, int h ) {
	if ( !pfx.samples ) {
		return;
	}
	qglBindFramebuffer( GL_READ_FRAMEBUFFER, pfx.msaaFbo );
	qglBindFramebuffer( GL_DRAW_FRAMEBUFFER, pfx.scene.fbo );
	qglBlitFramebuffer( x, y, x + w, y + h, x, y, x + w, y + h, GL_COLOR_BUFFER_BIT, GL_NEAREST );
}

/*
==================
R_PostFXCopyFrame

glCopyTexSubImage2D of the frame being drawn into the bound rectangle texture
(glow pass). Multisampled framebuffers can't be copied from: the area is
resolved into the scene target first.
==================
*/
void R_PostFXCopyFrame( int width, int height ) {
	if ( pfx.drawing && pfx.samples ) {
		R_ResolveScene( 0, 0, width, height );
		qglBindFramebuffer( GL_READ_FRAMEBUFFER, pfx.scene.fbo );
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, width, height );
		qglBindFramebuffer( GL_FRAMEBUFFER, pfx.msaaFbo );
		return;
	}
	qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, width, height );
}

// draws a source rectangle over the whole current viewport
static void R_DrawQuad( float s0, float t0, float s1, float t1 ) {
	qglBegin( GL_QUADS );
	qglTexCoord2f( s0, t0 );
	qglVertex2f( -1.0f, -1.0f );
	qglTexCoord2f( s0, t1 );
	qglVertex2f( -1.0f, 1.0f );
	qglTexCoord2f( s1, t1 );
	qglVertex2f( 1.0f, 1.0f );
	qglTexCoord2f( s1, t0 );
	qglVertex2f( 1.0f, -1.0f );
	qglEnd();
}

// one 4-tap pass from a rectangle texture into the current target
static void R_TapPass( GLuint source, float s0, float t0, float s1, float t1, float offset, float threshold, float scale ) {
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, source );
	qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, offset, offset, -offset, -offset );
	qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1, threshold, scale, 0.0f, 0.0f );
	R_DrawQuad( s0, t0, s1, t1 );
}

static void R_SetTarget( GLuint fbo, int x, int y, int w, int h ) {
	qglBindFramebuffer( GL_FRAMEBUFFER, fbo );
	qglViewport( x, y, w, h );
	qglScissor( x, y, w, h );
}

// state for the full screen passes, through the gamma vertex program which
// passes clip space positions as they are
static void R_BeginPasses( void ) {
	GL_State( GLS_DEPTHTEST_DISABLE );
	qglDisable( GL_CLIP_PLANE0 );
	GL_Cull( CT_TWO_SIDED );
	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );
	qglEnable( GL_VERTEX_PROGRAM_ARB );
	qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, tr.gammaVertexShader );
	qglEnable( GL_FRAGMENT_PROGRAM_ARB );
}

static void R_EndPasses( void ) {
	qglDisable( GL_FRAGMENT_PROGRAM_ARB );
	qglDisable( GL_VERTEX_PROGRAM_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_ARB );
	qglEnable( GL_TEXTURE_2D );
	qglBindFramebuffer( GL_FRAMEBUFFER, pfx.samples ? pfx.msaaFbo : pfx.scene.fbo );
}

/*
==================
R_BloomChain

Blurs the parts of the view brighter than the threshold into blur[0]:
threshold and halve, halve down the chain, then add each level to the one
above on the way back up. Returns the size used in blur[0].
==================
*/
static void R_BloomChain( int x, int y, int w, int h, float threshold, int *bloomW, int *bloomH ) {
	int levelW[BLOOM_LEVELS], levelH[BLOOM_LEVELS];

	qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, pfx.tapProgram );
	for ( int i = 0; i < BLOOM_LEVELS; i++ ) {
		levelW[i] = MAX( 1, ( i ? levelW[i - 1] : w ) / 2 );
		levelH[i] = MAX( 1, ( i ? levelH[i - 1] : h ) / 2 );

		R_SetTarget( pfx.blur[i].fbo, 0, 0, levelW[i], levelH[i] );
		// one source texel apart, so the bilinear taps cover 4x4 texels
		if ( i == 0 ) {
			R_TapPass( pfx.scene.texture, x, y, x + w, y + h, 1.0f, threshold, 1.0f );
		} else {
			R_TapPass( pfx.blur[i - 1].texture, 0, 0, levelW[i - 1], levelH[i - 1], 1.0f, 0.0f, 1.0f );
		}
	}

	qglEnable( GL_BLEND );
	qglBlendFunc( GL_ONE, GL_ONE );
	for ( int i = BLOOM_LEVELS - 1; i > 0; i-- ) {
		R_SetTarget( pfx.blur[i - 1].fbo, 0, 0, levelW[i - 1], levelH[i - 1] );
		R_TapPass( pfx.blur[i].texture, 0, 0, levelW[i], levelH[i], 0.5f, 0.0f, 1.0f );
	}
	qglDisable( GL_BLEND );

	*bloomW = levelW[0];
	*bloomH = levelH[0];
}

// adds blur[0] to the view, leaving the destination alpha alone
static void R_AddBloom( int x, int y, int w, int h, int bloomW, int bloomH ) {
	R_SetTarget( pfx.samples ? pfx.msaaFbo : pfx.scene.fbo, x, y, w, h );
	qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, pfx.tapProgram );
	qglEnable( GL_BLEND );
	qglBlendFunc( GL_ONE, GL_ONE );
	qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE );
	R_TapPass( pfx.blur[0].texture, 0, 0, bloomW, bloomH, 0.5f, 0.0f,
		Com_Clamp( 0.0f, 4.0f, r_bloomIntensity->value ) / BLOOM_LEVELS );
	qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	qglDisable( GL_BLEND );
}

// LUT input that comes out white: identityLight raised to the gamma (see
// R_SetColorMappings), the base of the bloom threshold and the HDR roll-off
static float R_PostFXWhite( void ) {
	return powf( tr.identityLight, Com_Clamp( 0.5f, 3.0f, r_gamma->value ) );
}

/*
==================
R_PostFXEndView

End of the main 3D view (RB_DrawSurfs), before the HUD is drawn over it:
bloom, and in HDR the tone mapping of the view, which can't be done in place
so it goes through the post target.
==================
*/
void R_PostFXEndView( int x, int y, int w, int h ) {
	// r_bloom 2 blooms the whole frame at present time instead (debugging)
	const qboolean bloom = (qboolean)( pfx.bloom && r_bloom->integer != 2 );
	const float white = R_PostFXWhite();
	int bloomW = 0, bloomH = 0;

	if ( !pfx.drawing || ( !bloom && !pfx.hdr ) || w <= 0 || h <= 0 ) {
		return;
	}

	R_ResolveScene( x, y, w, h );
	R_BeginPasses();
	if ( bloom ) {
		R_BloomChain( x, y, w, h, Com_Clamp( 0.0f, 1.0f, r_bloomThreshold->value ) * white, &bloomW, &bloomH );
	}

	if ( !pfx.hdr ) {
		R_AddBloom( x, y, w, h, bloomW, bloomH );
		R_EndPasses();
		return;
	}

	const float knee = white * 0.8f;
	const float range = white - knee;

	R_SetTarget( pfx.post.fbo, x, y, w, h );
	qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, pfx.toneMapProgram );
	qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, knee, range, 1.4426950f / range,
		Com_Clamp( 0.25f, 4.0f, r_exposure->value ) );
	qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 1,
		bloom ? Com_Clamp( 0.0f, 4.0f, r_bloomIntensity->value ) / BLOOM_LEVELS : 0.0f, 0.0f, 0.0f, 0.0f );
	qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 2, x, y, bloomW / (float)w, bloomH / (float)h );
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, bloom ? pfx.blur[0].texture : pfx.scene.texture );
	GL_SelectTexture( 0 );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, pfx.scene.texture );
	R_DrawQuad( x, y, x + w, y + h );
	GL_SelectTexture( 1 );
	qglDisable( GL_TEXTURE_RECTANGLE_ARB );
	GL_SelectTexture( 0 );

	R_SetTarget( pfx.samples ? pfx.msaaFbo : pfx.scene.fbo, x, y, w, h );
	qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, pfx.copyProgram );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, pfx.post.texture );
	R_DrawQuad( x, y, x + w, y + h );
	R_EndPasses();
}

/*
==================
R_PostFXPresent

End of frame (gamma pass), instead of copying the back buffer: resolves the
scene, goes back to the window and binds the scene texture on unit 0. Returns
the fragment program drawing it through the gamma LUT, or 0 when the frame
wasn't drawn offscreen.
==================
*/
GLuint R_PostFXPresent( void ) {
	if ( !pfx.drawing ) {
		return 0;
	}

	R_ResolveScene( 0, 0, pfx.width, pfx.height );
	if ( pfx.bloom && r_bloom->integer == 2 ) {
		int bloomW, bloomH;

		R_BeginPasses();
		R_BloomChain( 0, 0, pfx.width, pfx.height, 0.0f, &bloomW, &bloomH );
		R_AddBloom( 0, 0, pfx.width, pfx.height, bloomW, bloomH );
		R_EndPasses();
		R_ResolveScene( 0, 0, pfx.width, pfx.height );
	}
	pfx.drawing = qfalse;

	qglBindFramebuffer( GL_FRAMEBUFFER, 0 );
	qglDrawBuffer( GL_BACK );
	qglReadBuffer( GL_BACK );
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );

	GL_SelectTexture( 0 );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, pfx.scene.texture );
	return tr.gammaPixelShader;
}
