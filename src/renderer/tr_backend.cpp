#include "tr_local.h"

#ifndef DEDICATED
#include "glext.h"

#if !defined __TR_WORLDEFFECTS_H
	#include "tr_WorldEffects.h"
#endif
#endif

backEndData_t	*backEndData;
backEndState_t	backEnd;

// Whether we are currently rendering only glowing objects or not.
bool g_bRenderGlowingObjects = false;

// Whether the current hardware supports dynamic glows/flares.
bool g_bDynamicGlowSupported = false;

#ifndef DEDICATED

static void RB_DrawGlowOverlay( qboolean drawScene );
static void RB_BlurGlowTexture( qboolean offscreen );
#define MAX_GLOW_PASSES		16		// with the automatic glow size
const void *RB_GammaCorrection( const void *data );

/*
** GL_Bind
*/
void GL_Bind( image_t *image ) {
	int texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		image = tr.defaultImage;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		image = tr.dlightImage;
	}

	texnum = image->texnum;

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		image->frameUsed = tr.frameCount;
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
		backEnd.pc.c_binds++;
	}
}

/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit == 0 )
	{
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE0_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE0_ARB )\n" );
	}
	else if ( unit == 1 )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE1_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE1_ARB )\n" );
	} else {
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}

	glState.currenttmu = unit;
}


/*
** GL_BindMultitexture
*/
void GL_BindMultitexture( image_t *image0, GLuint env0, image_t *image1, GLuint env1 ) {
	int		texnum0, texnum1;

	texnum0 = image0->texnum;
	texnum1 = image1->texnum;

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum0 = texnum1 = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[1] != texnum1 ) {
		GL_SelectTexture( 1 );
		image1->frameUsed = tr.frameCount;
		glState.currenttextures[1] = texnum1;
		qglBindTexture( GL_TEXTURE_2D, texnum1 );
		backEnd.pc.c_binds++;
	}
	if ( glState.currenttextures[0] != texnum0 ) {
		GL_SelectTexture( 0 );
		image0->frameUsed = tr.frameCount;
		glState.currenttextures[0] = texnum0;
		qglBindTexture( GL_TEXTURE_2D, texnum0 );
		backEnd.pc.c_binds++;
	}
}


/*
** GL_Cull
*/
void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;
	if (backEnd.projection2D){	//don't care, we're in 2d when it's always disabled
		return;
	}

	if ( cullType == CT_TWO_SIDED )
	{
		qglDisable( GL_CULL_FACE );
	}
	else
	{
		qglEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_FRONT );
			}
			else
			{
				qglCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_BACK );
			}
			else
			{
				qglCullFace( GL_FRONT );
			}
		}
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv( int env )
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;


	switch ( env )
	{
	case GL_MODULATE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case GL_REPLACE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case GL_DECAL:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
		break;
	case GL_ADD:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed", env );
		break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned int stateBits )
{
	unsigned int diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}
	backEnd.pc.c_stateChanges++;

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	// coverage depends on the blend too, so a blend change of an alpha
	// tested stage goes through here again
	if ( ( diff & GLS_ATEST_BITS ) ||
		( ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) && ( stateBits & GLS_ATEST_BITS ) ) )
	{
		// With MSAA, alpha to coverage antialiases the cut-out edges of
		// foliage, fences and grates: the opaque "alpha >= 0.5" stages. It
		// would apply the alpha twice on blended stages, and can't express
		// the other thresholds, which keep the alpha test. The stages then
		// bind their images with sharpened alpha (R_CreateCoverageImage).
		const qboolean alphaToCoverage = (qboolean)( r_ext_alphaToCoverage->integer && tr.msaaSamples > 1 &&
			!( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) );
		qboolean coverage = qfalse;

		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			if ( alphaToCoverage ) {
				qglDisable( GL_ALPHA_TEST );
				coverage = qtrue;
			} else {
				qglEnable( GL_ALPHA_TEST );
				qglAlphaFunc( GL_GEQUAL, 0.5f );
			}
			break;
		case GLS_ATEST_GE_C0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.75f );
			break;
		default:
			assert( 0 );
			break;
		}

		if ( coverage != glState.alphaToCoverage ) {
			if ( coverage ) {
				qglEnable( GL_SAMPLE_ALPHA_TO_COVERAGE );
			} else {
				qglDisable( GL_SAMPLE_ALPHA_TO_COVERAGE );
			}
		}
		glState.alphaToCoverage = coverage;
	}

	glState.glStateBits = stateBits;
}



/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}


void SetViewportAndScissor( void ) {
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void) {
	int clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	if (!com_developer->integer && r_shadows->integer == 2)
	{
		Cvar_Set("cg_shadows", "1");
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_measureOverdraw->integer || r_shadows->integer == 2 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	if ( r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && !g_bRenderGlowingObjects )
	{
		clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef DEBUG
		qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
		qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
#endif
	}

	if ( /*tr.refdef.rdflags & RDF_AUTOMAP ||*/ (!( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && r_DynamicGlow->integer && !g_bRenderGlowingObjects ) )
	{
		if (tr.world && tr.world->globalFog != -1)
		{ //this is because of a bug in multiple scenes I think, it needs to clear for the second scene but it doesn't normally.
			const fog_t		*fog = &tr.world->fogs[tr.world->globalFog];

			clearBits |= GL_COLOR_BUFFER_BIT;
			qglClearColor(fog->parms.color[0],  fog->parms.color[1], fog->parms.color[2], 1.0f );
		}
	}

	// If this pass is to just render the glowing objects, don't clear the depth buffer since
	// we're sharing it with the main scene (since the main scene has already been rendered). -AReis
	if ( g_bRenderGlowingObjects )
	{
		clearBits &= ~GL_DEPTH_BUFFER_BIT;
	}

	qglClear( clearBits );

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.ori.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.ori.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.ori.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.ori.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	} else {
		qglDisable (GL_CLIP_PLANE0);
	}
}


#define	MAC_EVENT_PUMP_MSEC		5

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				fogNum, oldFogNum;
	int				entityNum, oldEntityNum;
	int				dlighted, oldDlighted;
	int				depthRange, oldDepthRange;
	int				i;
	drawSurf_t		*drawSurf;
	unsigned int	oldSort;
	double			originalTime;
	bool			didShadowPass = false;

	if (g_bRenderGlowingObjects)
	{ //only shadow on initial passes
		didShadowPass = true;
	}

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldDlighted = qfalse;
	oldSort = (unsigned int)-1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}

		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
		// If we're rendering glowing objects, but this shader has no stages with glow, skip it!
		if ( g_bRenderGlowingObjects && !shader->hasGlow )
		{
			shader = oldShader;
			entityNum = oldEntityNum;
			fogNum = oldFogNum;
			dlighted = oldDlighted;
			continue;
		}
		oldSort = drawSurf->sort;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted
			|| ( entityNum != oldEntityNum && (assert(shader), !shader->entityMergable) ) ) {
			if (oldShader != NULL) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );

			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = 0;

			if ( entityNum != ENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				if (backEnd.currentEntity->intShaderTime) {
					backEnd.refdef.floatTime = originalTime - 0.001 * backEnd.currentEntity->e.shaderTime.i;
				} else {
					// precision loss on high server time
					backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime.f;
				}
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

				// set up the dynamic lighting if needed
				if ( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
				}

				if ( backEnd.currentEntity->e.renderfx & RF_NODEPTH ) {
					// No depth at all, very rare but some things for seeing through walls
					depthRange = 2;
				}
				else if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = 1;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
			}

			qglLoadMatrixf( backEnd.ori.modelMatrix );

			//
			// change depthrange if needed
			//
			if ( oldDepthRange != depthRange ) {
				switch ( depthRange ) {
					default:
					case 0:
						qglDepthRange (0, 1);
						break;

					case 1:
						qglDepthRange (0, .3);
						break;

					case 2:
						qglDepthRange (0, 0);
						break;
				}

				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}

#if 0
	RB_DrawSun();
#endif

	if (!didShadowPass)
	{
		// darken down any stencil shadows
		RB_ShadowFinish();
		didShadowPass = true;
	}

	// darken down any stencil shadows
	RB_ShadowFinish();

	// add light flares on lights that aren't obscured

	// rww - 9-13-01 [1-26-01-sof2]
	//	RB_RenderFlares();
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void	RB_SetGL2D (void) {
	backEnd.projection2D = qtrue;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho (0, 640, 480, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_CULL_FACE );
	qglDisable( GL_CLIP_PLANE0 );

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001;
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty)
{
	int			start, end;

	if ( !tr.registered ) {
		return;
	}
	R_SyncRenderThread();

	// we definately want to sync every frame for the cinematics
	qglFinish();

	start = end = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	if ( (cols&(cols-1)) || (rows&(rows-1)) )
	{
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

	qglBegin (GL_QUADS);
	qglTexCoord2f ( 0.5f / cols,  0.5f / rows );
	qglVertex2f (x, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols ,  0.5f / rows );
	qglVertex2f (x+w, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( 0.5f / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x, y+h);
	qglEnd ();
}

void RE_UploadCinematic (int cols, int rows, const byte *data, int client, qboolean dirty) {

	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}
}


/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	tess.vertexColorsui[ numVerts ] =
		tess.vertexColorsui[ numVerts + 1 ] =
		tess.vertexColorsui[ numVerts + 2 ] =
		tess.vertexColorsui[ numVerts + 3 ] = backEnd.color2Dui;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[0][ numVerts ][0] = cmd->s1;
	tess.texCoords[0][ numVerts ][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[0][ numVerts + 1 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 1 ][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[0][ numVerts + 2 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 2 ][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[0][ numVerts + 3 ][0] = cmd->s1;
	tess.texCoords[0][ numVerts + 3 ][1] = cmd->t2;

	return (const void *)(cmd + 1);
}

/*
=============
RB_TransformPic
=============
*/
const void *RB_TransformPic ( const void *data ) {
	const transformPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;

	cmd = (const transformPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	tess.vertexColorsui[ numVerts ] =
		tess.vertexColorsui[ numVerts + 1 ] =
		tess.vertexColorsui[ numVerts + 2 ] =
		tess.vertexColorsui[ numVerts + 3 ] = backEnd.color2Dui;

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0;

	tess.texCoords[0][ numVerts ][0] = cmd->s1;
	tess.texCoords[0][ numVerts ][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->m[0][0];
	tess.xyz[ numVerts + 1 ][1] = cmd->y + cmd->m[1][0];
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[0][ numVerts + 1 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 1 ][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->m[0][0] + cmd->m[0][1];
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->m[1][0] + cmd->m[1][1];
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[0][ numVerts + 2 ][0] = cmd->s2;
	tess.texCoords[0][ numVerts + 2 ][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x + cmd->m[0][1];
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->m[1][1];
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[0][ numVerts + 3 ][0] = cmd->s1;
	tess.texCoords[0][ numVerts + 3 ][1] = cmd->t2;

	return (const void *)(cmd + 1);
}

/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	if ( !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) && g_bDynamicGlowSupported && r_DynamicGlow->integer )
	{
		R_GPUTimerBegin( GPU_GLOW );

		if ( R_PostFXGlowBegin() ) {
			// r_fbo: the glowing objects go to their own target, which shares
			// the scene depth, so the scene stays as it is: no copies, and
			// the blur passes go from one small target to the other
			g_bRenderGlowingObjects = true;
			RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
			g_bRenderGlowingObjects = false;
			R_PostFXGlowObjectsDone();

			const int oldViewWidth = backEnd.viewParms.viewportWidth;
			const int oldViewHeight = backEnd.viewParms.viewportHeight;
			backEnd.viewParms.viewportWidth = tr.glowWidth;
			backEnd.viewParms.viewportHeight = tr.glowHeight;
			RB_BlurGlowTexture( qtrue );
			backEnd.viewParms.viewportWidth = oldViewWidth;
			backEnd.viewParms.viewportHeight = oldViewHeight;

			R_PostFXGlowEnd();
			SetViewportAndScissor();
			if ( r_DynamicGlow->integer == 2 ) {
				// debugging: the glow alone
				qglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
				qglClear( GL_COLOR_BUFFER_BIT );
			}
			RB_DrawGlowOverlay( qfalse );
		} else {
			// Copy the normal scene to texture.
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage );
			R_PostFXCopyFrame( glConfig.vidWidth, glConfig.vidHeight );
			qglDisable( GL_TEXTURE_RECTANGLE_ARB );
			qglEnable( GL_TEXTURE_2D );

			// Just clear colors, but leave the depth buffer intact so we can 'share' it.
			qglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			qglClear( GL_COLOR_BUFFER_BIT );

			// Render the glowing objects.
			g_bRenderGlowingObjects = true;
			RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
			g_bRenderGlowingObjects = false;

			// GL commands run in order, so the copy below already sees the glow
			// draws; the finish only stalled the CPU. r_DynamicGlowFinish brings it
			// back for drivers that might need it (old texture rectangle hack)
			if ( r_DynamicGlowFinish->integer ) {
				qglFinish();
			}

			// Copy the glow scene to texture.
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.screenGlow );
			R_PostFXCopyFrame( glConfig.vidWidth, glConfig.vidHeight );
			qglDisable( GL_TEXTURE_RECTANGLE_ARB );
			qglEnable( GL_TEXTURE_2D );

			// Resize the viewport to the blur texture size.
			const int oldViewWidth = backEnd.viewParms.viewportWidth;
			const int oldViewHeight = backEnd.viewParms.viewportHeight;
			backEnd.viewParms.viewportWidth = tr.glowWidth;
			backEnd.viewParms.viewportHeight = tr.glowHeight;
			SetViewportAndScissor();

			// Blur the scene.
			RB_BlurGlowTexture( qfalse );

			// Copy the finished glow scene back to texture.
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.blurImage );
			R_PostFXCopyFrame( backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
			qglDisable( GL_TEXTURE_RECTANGLE_ARB );
			qglEnable( GL_TEXTURE_2D );

			// Set the viewport back to normal.
			backEnd.viewParms.viewportWidth = oldViewWidth;
			backEnd.viewParms.viewportHeight = oldViewHeight;
			SetViewportAndScissor();
			qglClear( GL_COLOR_BUFFER_BIT );

			// Draw the glow additively over the screen.
			RB_DrawGlowOverlay( qtrue );

		}

		R_GPUTimerEnd( GPU_GLOW );
	}

	// r_bloom, r_hdr: on the main view, after the glow so it blooms too
	if ( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && !backEnd.viewParms.isPortal ) {
		R_GPUTimerBegin( GPU_POST );
		R_PostFXEndView( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		R_GPUTimerEnd( GPU_POST );
		SetViewportAndScissor();
	}

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	R_GPUTimerFrameBegin();

	// r_fbo: draw into the offscreen target instead of the back buffer
	if ( !R_PostFXBindScene( (GLenum)cmd->buffer ) ) {
		qglDrawBuffer( cmd->buffer );
		qglReadBuffer( cmd->buffer );
	}

	// clear screen for debugging
	if (tr.world && tr.world->globalFog != -1)
	{
		unsigned	i = tr.world->fogs[tr.world->globalFog].colorInt;

		qglClearColor( ( (byte *)&i )[0] / 255.0, ( (byte *)&i )[1] / 255.0, ( (byte *)&i )[2] / 255.0,  1.0 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
	else if ( r_clear->integer ) {
		int i = r_clear->integer;
		if (i == 42) {
			i = Q_irand(0,8);
		}
		switch (i)
		{
		default:
			qglClearColor( 1, 0, 0.5, 1 );
			break;
		case 1:
			qglClearColor( 1.0, 0.0, 0.0, 1.0); //red
			break;
		case 2:
			qglClearColor( 0.0, 1.0, 0.0, 1.0); //green
			break;
		case 3:
			qglClearColor( 1.0, 1.0, 0.0, 1.0); //yellow
			break;
		case 4:
			qglClearColor( 0.0, 0.0, 1.0, 1.0); //blue
			break;
		case 5:
			qglClearColor( 0.0, 1.0, 1.0, 1.0); //cyan
			break;
		case 6:
			qglClearColor( 1.0, 0.0, 1.0, 1.0); //magenta
			break;
		case 7:
			qglClearColor( 1.0, 1.0, 1.0, 1.0); //white
			break;
		case 8:
			qglClearColor( 0.0, 0.0, 0.0, 1.0); //black
			break;
		}
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	image_t	*image;
	float	x, y, w, h;
	int		start, end;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();


	int i=0;
					 R_Images_StartIteration();
	while ( (image = R_Images_GetNextIteration()) != NULL)
	{
		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );
		qglBegin (GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
		i++;
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );

}


/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// texture swapping test
	if (r_showImages->integer) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	// r_fbo frame that skipped the gamma pass: show it anyway
	if ( R_PostFXPending() ) {
		gammaCorrectionCommand_t present;

		RB_GammaCorrection( &present );
	}

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if ( r_measureOverdraw->integer ) {
		int i;
		int sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = (unsigned char *)ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory( stencilReadback );
	}

	backEnd.projection2D = qfalse;

	// frames with a 3D view already applied r_finish in RB_BeginDrawingView;
	// menu and loading frames used to finish unconditionally
	if ( !glState.finishCalled && r_finish->integer ) {
		qglFinish();
	}

	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	R_GPUTimerFrameEnd();
    WIN_Present(&glWindow);
	R_LimitFrameLatency();

	return (const void *)(cmd + 1);
}

/*
==================
RB_WorldEffects
==================
*/
const void *RB_WorldEffects( const void *data )
{
	const worldEffectsCommand_t	*cmd;

	cmd = (const worldEffectsCommand_t *)data;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	RB_RenderWorldEffects();

	return (const void *)(cmd + 1);
}

/*
==================
RB_GammaCorrection
==================
*/
const void *RB_GammaCorrection( const void *data )
{
	const gammaCorrectionCommand_t	*cmd;

	cmd = (const gammaCorrectionCommand_t *)data;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	RB_SetGL2D();
	R_GPUTimerBegin( GPU_POST );

	// r_fbo: the frame is already in a texture, R_PostFXPresent binds it
	const GLuint offscreenProgram = R_PostFXPresent();

	qglEnable(GL_VERTEX_PROGRAM_ARB);
	qglBindProgramARB(GL_VERTEX_PROGRAM_ARB, tr.gammaVertexShader);
	qglEnable(GL_FRAGMENT_PROGRAM_ARB);
	qglBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, offscreenProgram ? offscreenProgram : tr.gammaPixelShader);

	if (!offscreenProgram) {
		// sceneImage is allocated at the screen size (R_BindGlowImages), so
		// update it in place instead of re-specifying the texture every frame
		GL_SelectTexture(0);
		qglEnable(GL_TEXTURE_RECTANGLE_ARB);
		qglBindTexture(GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage);
		qglCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight);
		qglDisable(GL_TEXTURE_RECTANGLE_ARB);
	}

	GL_SelectTexture(1);
	qglEnable(GL_TEXTURE_3D);

	qglClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	qglClear(GL_COLOR_BUFFER_BIT);

	// r_colorGradeSplit: ungraded left half, graded right half
	const float splitX = tr.gammaLUTSplit ? 0.0f : -1.0f;
	const float splitS = tr.gammaLUTSplit ? glConfig.vidWidth * 0.5f : 0.0f;

	if (tr.gammaLUTSplit) {
		qglBindTexture(GL_TEXTURE_3D, tr.gammaLUTClassicImage);
		qglBegin(GL_QUADS);
		qglTexCoord2f(0.0f, 0.0f);
		qglVertex2f(-1.0f, -1.0f);
		qglTexCoord2f(0.0f, (float)glConfig.vidHeight);
		qglVertex2f(-1.0f, 1.0f);
		qglTexCoord2f(splitS, (float)glConfig.vidHeight);
		qglVertex2f(0.0f, 1.0f);
		qglTexCoord2f(splitS, 0.0f);
		qglVertex2f(0.0f, -1.0f);
		qglEnd();
	}

	qglBindTexture(GL_TEXTURE_3D, tr.gammaLUTImage);
	qglBegin(GL_QUADS);
	qglTexCoord2f(splitS, 0.0f);
	qglVertex2f(splitX, -1.0f);

	qglTexCoord2f(splitS, (float)glConfig.vidHeight);
	qglVertex2f(splitX, 1.0f);

	qglTexCoord2f((float)glConfig.vidWidth, (float)glConfig.vidHeight);
	qglVertex2f(1.0f, 1.0f);

	qglTexCoord2f((float)glConfig.vidWidth, 0.0f);
	qglVertex2f(1.0f, -1.0f);
	qglEnd();

	qglDisable(GL_VERTEX_PROGRAM_ARB);
	qglDisable(GL_FRAGMENT_PROGRAM_ARB);

	qglDisable(GL_TEXTURE_3D);
	GL_SelectTexture(0);

	if (tr.gammaLUTSplit || tr.gradeSplit) {
		// divider line between the two halves (with r_fbo the grade was
		// applied to the right half of the 3D view)
		qglScissor(glConfig.vidWidth / 2 - 1, 0, 2, glConfig.vidHeight);
		qglClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		qglClear(GL_COLOR_BUFFER_BIT);
		qglScissor(0, 0, glConfig.vidWidth, glConfig.vidHeight);
	}

	// video motion blur: add the finished frame to the average
	if (cmd->blendSubframes > 1) {
		R_PostFXBlendFrame(cmd->blendSubframe, cmd->blendSubframes);
	}

	R_GPUTimerEnd( GPU_POST );
	return (const void *)(cmd + 1);
}

/*
==================
RB_ReadPixels
==================
*/
const void *RB_ReadPixels( const void *data )
{
	const readPixelsCommand_t	*cmd;
	int		memcount;

	cmd = (const readPixelsCommand_t *)data;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	memcount = PAD(glConfig.vidWidth * 3, cmd->padding) * glConfig.vidHeight;

	if ( cmd->bufSize < memcount ) {
		ri.Error( ERR_DROP, "RB_ReadPixels: buffer too small\n" );
	}

	qglPixelStorei(GL_PACK_ALIGNMENT, cmd->padding);
	qglReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight,
		cmd->format, GL_UNSIGNED_BYTE, cmd->buffer);

	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called syncronously if running without
smp extensions, or asyncronously by another thread.
====================
*/
void RB_ExecuteRenderCommands( const void *data ) {
	int64_t		t1, t2;

	t1 = Sys_Microseconds();

	while ( 1 ) {
		data = PADP( data, sizeof( void * ) );

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_TRANSFORM_PIC:
			data = RB_TransformPic( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_WORLD_EFFECTS:
			data = RB_WorldEffects( data );
			break;
		case RC_GAMMA_CORRECTION:
			data = RB_GammaCorrection( data );
			break;
		case RC_READ_PIXELS:
			data = RB_ReadPixels( data );
			break;
		case RC_END_OF_LIST:
			// stop rendering
			t2 = Sys_Microseconds();
			backEnd.pc.usec = (int)( t2 - t1 );
			return;
		default:
			ri.Error(ERR_DROP, "Unknown render command");
		}
	}
}

// What Pixel Shader type is currently active (regcoms or fragment programs).
GLuint g_uiCurrentPixelShaderType = 0x0;

// Begin using a Pixel Shader.
void BeginPixelShader( GLuint uiType, GLuint uiID )
{
	switch ( uiType )
	{
		// Using Register Combiners, so call the Display List that stores it.
		case GL_REGISTER_COMBINERS_NV:
		{
			// Just in case...
			if ( !qglCombinerParameterfvNV )
				return;

			// Call the list with the regcom in it.
			qglEnable( GL_REGISTER_COMBINERS_NV );
			qglCallList( uiID );

			g_uiCurrentPixelShaderType = GL_REGISTER_COMBINERS_NV;
		}
		return;

		// Using Fragment Programs, so call the program.
		case GL_FRAGMENT_PROGRAM_ARB:
		{
			// Just in case...
			if ( !qglGenProgramsARB )
				return;

			qglEnable( GL_FRAGMENT_PROGRAM_ARB );
			qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, uiID );

			g_uiCurrentPixelShaderType = GL_FRAGMENT_PROGRAM_ARB;
		}
		return;
	}
}

// Stop using a Pixel Shader and return states to normal.
void EndPixelShader()
{
	if ( g_uiCurrentPixelShaderType == 0x0 )
		return;

	qglDisable( g_uiCurrentPixelShaderType );
}

// Hack variable for deciding which kind of texture rectangle thing to do (for some
// reason it acts different on radeon! It's against the spec!).
extern bool g_bTextureRectangleHack;

static void RB_BlurGlowTexture( qboolean offscreen )
{
	qglDisable (GL_CLIP_PLANE0);
	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_DEPTH_TEST );

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	/////////////////////////////////////////////////////////
	// Setup vertex and pixel programs.
	/////////////////////////////////////////////////////////

	// The automatic glow size (glowRadiusScale above 1) widens the blur with
	// more passes, not wider ones: offsets near whole texels make the
	// bilinear taps comb-like and stripe thin glows. Each pass sums the taps
	// with weight r_DynamicGlowIntensity in all, so that gain is spread over
	// the passes to keep the glow as bright.
	// (below 1, at low resolutions, narrower offsets are fine)
	const float fOffsetScale = MIN( tr.glowRadiusScale, 1.0f );
	const int basePasses = r_DynamicGlowPasses->integer;
	const float fDelta = r_DynamicGlowDelta->value * fOffsetScale;
	int iBlurPasses = basePasses;
	float fIntensity = r_DynamicGlowIntensity->value;

	if ( tr.glowRadiusScale > 1.0f && basePasses > 0 ) {
		float target = 0.0f, spread = 0.0f;

		// the variance of the blur grows with the sum of the squared offsets
		for ( int i = 0; i < basePasses; i++ ) {
			target += ( 0.1f + i * fDelta ) * ( 0.1f + i * fDelta );
		}
		target *= tr.glowRadiusScale * tr.glowRadiusScale;
		for ( iBlurPasses = 0; spread < target && iBlurPasses < MAX_GLOW_PASSES; iBlurPasses++ ) {
			spread += ( 0.1f + iBlurPasses * fDelta ) * ( 0.1f + iBlurPasses * fDelta );
		}
		iBlurPasses = MAX( iBlurPasses, basePasses );
		fIntensity = powf( fIntensity, basePasses / (float)iBlurPasses );
	}

	// NOTE: The 0.25 is because we're blending 4 textures (so = 1.0) and we want a relatively normalized pixel
	// intensity distribution, but this won't happen anyways if intensity is higher than 1.0.
	const float fBlurDistribution = fIntensity * 0.25f;
	const float fBlurWeight[4] = { fBlurDistribution, fBlurDistribution, fBlurDistribution, 1.0f };

	// Enable and set the Vertex Program.
	qglEnable( GL_VERTEX_PROGRAM_ARB );
	qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, tr.glowVShader );

	// Apply Pixel Shaders.
	if ( qglCombinerParameterfvNV )
	{
		BeginPixelShader( GL_REGISTER_COMBINERS_NV, tr.glowPShader );

		// Pass the blur weight to the regcom.
		qglCombinerParameterfvNV( GL_CONSTANT_COLOR0_NV, (float*)&fBlurWeight );
	}
	else if ( qglProgramEnvParameter4fARB )
	{
		BeginPixelShader( GL_FRAGMENT_PROGRAM_ARB, tr.glowPShader );

		// Pass the blur weight to the Fragment Program.
		qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, fBlurWeight[0], fBlurWeight[1], fBlurWeight[2], fBlurWeight[3] );
	}

	/////////////////////////////////////////////////////////
	// Set the blur texture to the 4 texture stages.
	/////////////////////////////////////////////////////////

	// How much to offset each texel by.
	float fTexelWidthOffset = 0.1f * fOffsetScale, fTexelHeightOffset = 0.1f * fOffsetScale;

	GLuint uiTex = tr.screenGlow;

	qglActiveTextureARB( GL_TEXTURE3_ARB );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );

	qglActiveTextureARB( GL_TEXTURE2_ARB );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );

	qglActiveTextureARB( GL_TEXTURE1_ARB );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );

	qglActiveTextureARB(GL_TEXTURE0_ARB );
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );

	/////////////////////////////////////////////////////////
	// Draw the blur passes (each pass blurs it more, increasing the blur radius ).
	/////////////////////////////////////////////////////////

	//int iTexWidth = backEnd.viewParms.viewportWidth, iTexHeight = backEnd.viewParms.viewportHeight;
	int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight;

	for ( int iNumBlurPasses = 0; iNumBlurPasses < iBlurPasses; iNumBlurPasses++ )
	{
		if ( offscreen ) {
			// r_fbo: read the previous pass (the glow objects at first),
			// draw into the other blur target
			if ( iNumBlurPasses == 1 && !g_bTextureRectangleHack ) {
				iTexWidth = backEnd.viewParms.viewportWidth;
				iTexHeight = backEnd.viewParms.viewportHeight;
			}
			for ( int unit = 3; unit >= 0; unit-- ) {
				qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
				qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			}
			uiTex = R_PostFXGlowBlurTarget( iNumBlurPasses, iBlurPasses );
		}

		// Load the Texel Offsets into the Vertex Program.
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 0, -fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 1, -fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 2, fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 3, fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f );

		// After first pass put the tex coords to the viewport size.
		if ( iNumBlurPasses == 1 && !offscreen )
		{
			// OK, very weird, but dependent on which texture rectangle extension we're using, the
			// texture either needs to be always texure correct or view correct...
			if ( !g_bTextureRectangleHack )
			{
				iTexWidth = backEnd.viewParms.viewportWidth;
				iTexHeight = backEnd.viewParms.viewportHeight;
			}

			uiTex = tr.blurImage;
			qglActiveTextureARB( GL_TEXTURE3_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			qglActiveTextureARB( GL_TEXTURE2_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			qglActiveTextureARB( GL_TEXTURE1_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			qglActiveTextureARB(GL_TEXTURE0_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_ARB );
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );

			// Copy the current image over.
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, uiTex );
			R_PostFXCopyFrame( backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		}

		// Draw the fullscreen quad.
		backEnd.pc.c_drawCalls++;
		qglBegin( GL_QUADS );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, iTexHeight );
			qglVertex2f( 0, 0 );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, 0 );
			qglVertex2f( 0, backEnd.viewParms.viewportHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, iTexWidth, 0 );
			qglVertex2f( backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, iTexWidth, iTexHeight );
			qglVertex2f( backEnd.viewParms.viewportWidth, 0 );
		qglEnd();

		if ( !offscreen ) {
			qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.blurImage );
			R_PostFXCopyFrame( backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		}

		// Increase the texel offsets.
		// NOTE: This is possibly the most important input to the effect. Even by using an exponential function I've been able to
		// make it look better (at a much higher cost of course). This is cheap though and still looks pretty great. In the future
		// I might want to use an actual gaussian equation to correctly calculate the pixel coefficients and attenuates, texel
		// offsets, gaussian amplitude and radius...
		fTexelWidthOffset += fDelta;
		fTexelHeightOffset += fDelta;
	}

	// Disable multi-texturing.
	qglActiveTextureARB( GL_TEXTURE3_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_ARB );

	qglActiveTextureARB( GL_TEXTURE2_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_ARB );

	qglActiveTextureARB( GL_TEXTURE1_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_ARB );

	qglActiveTextureARB(GL_TEXTURE0_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_ARB );
	qglEnable( GL_TEXTURE_2D );

	qglDisable( GL_VERTEX_PROGRAM_ARB );
	EndPixelShader();

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglDisable( GL_BLEND );
	qglEnable( GL_DEPTH_TEST );

	glState.currenttmu = 0;	//this matches the last one we activated
}

// Draw the glow blur over the screen additively.
static void RB_DrawGlowOverlay( qboolean drawScene )
{
	qglDisable (GL_CLIP_PLANE0);
	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_DEPTH_TEST );

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, glConfig.vidWidth, glConfig.vidHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_TEXTURE_RECTANGLE_ARB );

	// For debug purposes. With r_fbo the scene was never covered.
	if ( drawScene && r_DynamicGlow->integer != 2 )
	{
		// Render the normal scene texture.
		qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage );
		qglBegin(GL_QUADS);
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			qglTexCoord2f( 0, glConfig.vidHeight );
			qglVertex2f( 0, 0 );

			qglTexCoord2f( 0, 0 );
			qglVertex2f( 0, glConfig.vidHeight );

			qglTexCoord2f( glConfig.vidWidth, 0 );
			qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

			qglTexCoord2f( glConfig.vidWidth, glConfig.vidHeight );
			qglVertex2f( glConfig.vidWidth, 0 );
		qglEnd();
	}

	// One and Inverse Src Color give a very soft addition, while one one is a bit stronger. With one one we can
	// use additive blending through multitexture though.
	if ( r_DynamicGlowSoft->integer )
	{
		qglBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
	}
	else
	{
		qglBlendFunc( GL_ONE, GL_ONE );
	}
	qglEnable( GL_BLEND );

	// Now additively render the glow texture.
	qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.blurImage );
	qglBegin(GL_QUADS);
		qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		qglTexCoord2f( 0, tr.glowHeight );
		qglVertex2f( 0, 0 );

		qglTexCoord2f( 0, 0 );
		qglVertex2f( 0, glConfig.vidHeight );

		qglTexCoord2f( tr.glowWidth, 0 );
		qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

		qglTexCoord2f( tr.glowWidth, tr.glowHeight );
		qglVertex2f( glConfig.vidWidth, 0 );
	qglEnd();

	qglDisable( GL_TEXTURE_RECTANGLE_ARB );
	qglEnable( GL_TEXTURE_2D );
	qglBlendFunc( GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR );
	qglDisable( GL_BLEND );

	// NOTE: Multi-texture wasn't that much faster (we're obviously not bottlenecked by transform pipeline),
	// and besides, soft glow looks better anyways.
/*	else
	{
		int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight;
		if ( GL_TEXTURE_RECTANGLE_ARB == GL_TEXTURE_RECTANGLE_NV )
		{
			iTexWidth = tr.glowWidth;
			iTexHeight = tr.glowHeight;
		}

		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_ARB );
		qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.screenGlow );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_ARB );
		qglBindTexture( GL_TEXTURE_RECTANGLE_ARB, tr.sceneImage );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

		qglBegin(GL_QUADS);
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, iTexHeight );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, glConfig.vidHeight );
			qglVertex2f( 0, 0 );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, 0 );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, 0 );
			qglVertex2f( 0, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, 0 );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, 0 );
			qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, iTexHeight );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, glConfig.vidHeight );
			qglVertex2f( glConfig.vidWidth, 0 );
		qglEnd();

		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglDisable( GL_TEXTURE_RECTANGLE_ARB );

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglDisable( GL_TEXTURE_RECTANGLE_ARB );
		qglEnable( GL_TEXTURE_2D );
	}*/

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglEnable( GL_DEPTH_TEST );
}
#endif //!DEDICATED
