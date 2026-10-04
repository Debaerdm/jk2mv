// cl_demotools.cpp -- demo playback tools: transport controls, timeline,
// free camera, camera paths and photo mode.
//
// Everything here only acts while a demo is playing, so none of it can be
// used in a live game.

#include "client.h"
#include "cl_campath.h"
#include <cmath>

extern console_t con;

#define MAX_CAM_KEYS	256
#define CAM_BACKOFF		40.0f		// how far the free camera backs off the recorded eye

cvar_t	*cl_demoTimeline;
cvar_t	*cl_freecamSpeed;

static struct {
	// view of the last full screen world scene drawn by cgame
	qboolean	haveView;
	vec3_t		viewOrigin;
	vec3_t		viewAngles;
	float		viewFov;

	// camera
	qboolean	freecam;
	qboolean	freecamPending;		// the free camera starts from the next game view
	qboolean	detached;			// the free camera left the recorded eye
	qboolean	playing;			// following the camera path
	vec3_t		origin;
	vec3_t		angles;
	float		fov;				// 0 keeps the game's fov
	int64_t		lastFrameUsec;

	camKey_t	keys[MAX_CAM_KEYS];
	int			numKeys;

	// timeline
	int			startServerTime;
	float		pausedTimescale;	// speed to restore after demo_pause

	// photo mode
	qboolean	photo;
	int			photoHideHud;
	float		photoTimescale;
	float		photoPausedTimescale;
	qboolean	photoFreecam;
	qboolean	photoPlaying;
} dt;

/*
==================
CL_DemoToolsReset

From CL_Disconnect, so a demo never starts with the camera, the photo mode
or the timeline of the previous one. The camera keys stay.
==================
*/
void CL_DemoToolsReset( void ) {
	if ( dt.photo ) {
		Cvar_Set( "cl_demoHideHud", va( "%i", dt.photoHideHud ) );
	}
	dt.freecam = dt.freecamPending = dt.detached = dt.playing = dt.photo = qfalse;
	dt.startServerTime = 0;
	dt.haveView = qfalse;
	dt.lastFrameUsec = 0;
	dt.pausedTimescale = 0.0f;
}

static qboolean CL_DemoToolsCheck( void ) {
	if ( !clc.demoplaying ) {
		Com_Printf( "This command only works during demo playback.\n" );
		return qfalse;
	}
	return qtrue;
}

// the demo camera shows its own view: a camera path, or a free camera that
// left the recorded eye (until then it shows the game's own view)
static qboolean CL_DemoCamOwnView( void ) {
	return (qboolean)( clc.demoplaying && ( ( dt.playing && dt.numKeys > 0 ) || ( dt.freecam && dt.detached ) ) );
}

static float CL_Timescale( void ) {
	return com_timescale ? com_timescale->value : 1.0f;
}

static void CL_SetTimescale( float scale ) {
	Cvar_Set( "timescale", va( "%g", scale ) );
}

/*
===============================================================================

TRANSPORT CONTROLS

===============================================================================
*/

static void CL_DemoPause_f( void ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	if ( CL_Timescale() > 0.0f ) {
		dt.pausedTimescale = CL_Timescale();
		CL_SetTimescale( 0.0f );
		Com_Printf( "demo paused\n" );
	} else {
		CL_SetTimescale( dt.pausedTimescale > 0.0f ? dt.pausedTimescale : 1.0f );
		Com_Printf( "demo resumed\n" );
	}
}

static void CL_DemoSpeed_f( void ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: demo_speed <0.05 - 16>, currently %g\n", CL_Timescale() );
		return;
	}
	CL_SetTimescale( Com_Clamp( 0.05f, 16.0f, (float)atof( Cmd_Argv( 1 ) ) ) );
}

static void CL_DemoScaleSpeed( float factor ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	float scale = CL_Timescale() > 0.0f ? CL_Timescale() : dt.pausedTimescale > 0.0f ? dt.pausedTimescale : 1.0f;
	scale = Com_Clamp( 0.0625f, 16.0f, scale * factor );
	CL_SetTimescale( scale );
	Com_Printf( "demo speed %gx\n", scale );
}

static void CL_DemoFaster_f( void ) {
	CL_DemoScaleSpeed( 2.0f );
}

static void CL_DemoSlower_f( void ) {
	CL_DemoScaleSpeed( 0.5f );
}

static void CL_DemoStep_f( void ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	if ( CL_Timescale() > 0.0f ) {
		dt.pausedTimescale = CL_Timescale();
		CL_SetTimescale( 0.0f );
	}
	// advances the paused demo by that much game time on the next frame
	com_demoStepMsec = Cmd_Argc() > 1 ? Com_Clampi( 1, 1000, atoi( Cmd_Argv( 1 ) ) ) : 50;
}

/*
===============================================================================

FREE CAMERA AND CAMERA PATHS

===============================================================================
*/

/*
==================
CL_DemoCamStart

Turns the free camera on. During a camera path it takes over the path's
current view. Otherwise it starts at the recorded eye with the next game
view, and shows that view as it is (view weapon, no body) until it leaves
it: see CL_DemoCamDetach.
==================
*/
static void CL_DemoCamStart( void ) {
	// dt.origin, dt.angles and dt.fov hold a playing path's view
	dt.detached = (qboolean)( dt.playing && dt.numKeys > 0 );
	dt.freecamPending = (qboolean)!dt.detached;
	dt.freecam = qtrue;
	dt.playing = qfalse;
	dt.lastFrameUsec = 0;
	if ( dt.detached ) {
		dt.angles[PITCH] = Com_Clamp( -89.0f, 89.0f, AngleNormalize180( dt.angles[PITCH] ) );
		// mouse look continues from the camera angles
		VectorCopy( dt.angles, cl.viewangles );
	}
}

/*
==================
CL_DemoCamDetach

The free camera leaves the recorded eye, because it moved or turned or the
recorded player did: from now on the scene shows the recorder's body and not
the view weapon. The camera backs off behind the eye first, short of any wall,
so it doesn't see the head from inside.
==================
*/
static void CL_DemoCamDetach( void ) {
	vec3_t	forward, end;
	trace_t	tr;

	dt.detached = qtrue;
	AngleVectors( dt.angles, forward, NULL, NULL );
	VectorMA( dt.origin, -CAM_BACKOFF, forward, end );
	CM_BoxTrace( &tr, dt.origin, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
	// a few units off the wall, past the near clip plane
	VectorMA( dt.origin, -MAX( 0.0f, CAM_BACKOFF * tr.fraction - 4.0f ), forward, dt.origin );
}

static void CL_DemoFreecam_f( void ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	if ( !dt.freecam ) {
		CL_DemoCamStart();
		Com_Printf( "free camera on: move with the movement keys and the mouse\n" );
	} else {
		dt.freecam = qfalse;
		Com_Printf( "free camera off\n" );
	}
}

static void CL_CamAdd_f( void ) {
	camKey_t	key;
	int			n;

	if ( !CL_DemoToolsCheck() ) {
		return;
	}

	key.time = cl.serverTime;
	if ( CL_DemoCamOwnView() ) {
		VectorCopy( dt.origin, key.origin );
		VectorCopy( dt.angles, key.angles );
		key.fov = dt.fov > 0.0f ? dt.fov : dt.viewFov;
	} else if ( dt.haveView ) {
		VectorCopy( dt.viewOrigin, key.origin );
		VectorCopy( dt.viewAngles, key.angles );
		key.fov = dt.viewFov;
	} else {
		Com_Printf( "no view to record yet\n" );
		return;
	}
	key.timescale = CL_Timescale() > 0.0f ? CL_Timescale() : 1.0f;

	n = CamPath_Insert( dt.keys, dt.numKeys, MAX_CAM_KEYS, &key );
	if ( n < 0 ) {
		Com_Printf( "camera path full (%i keys)\n", MAX_CAM_KEYS );
		return;
	}
	dt.numKeys = n;
	Com_Printf( "camera key at %i ms (%i keys)\n", key.time, dt.numKeys );
}

static void CL_CamDel_f( void ) {
	int i;

	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	if ( !dt.numKeys ) {
		Com_Printf( "no camera keys\n" );
		return;
	}
	// the given index, or the key nearest to the current time
	if ( Cmd_Argc() > 1 ) {
		i = atoi( Cmd_Argv( 1 ) );
	} else {
		i = 0;
		for ( int k = 1; k < dt.numKeys; k++ ) {
			if ( abs( dt.keys[k].time - cl.serverTime ) < abs( dt.keys[i].time - cl.serverTime ) ) {
				i = k;
			}
		}
	}
	if ( i < 0 || i >= dt.numKeys ) {
		Com_Printf( "no camera key %i\n", i );
		return;
	}
	memmove( &dt.keys[i], &dt.keys[i + 1], ( dt.numKeys - i - 1 ) * sizeof( dt.keys[0] ) );
	dt.numKeys--;
	Com_Printf( "camera key %i removed (%i keys)\n", i, dt.numKeys );
}

static void CL_CamClear_f( void ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	dt.numKeys = 0;
	dt.playing = qfalse;
	Com_Printf( "camera path cleared\n" );
}

static void CL_CamList_f( void ) {
	for ( int i = 0; i < dt.numKeys; i++ ) {
		const camKey_t *k = &dt.keys[i];
		Com_Printf( "%3i: %8i ms  origin %.0f %.0f %.0f  angles %.1f %.1f %.1f  fov %.1f  speed %g\n", i, k->time,
			k->origin[0], k->origin[1], k->origin[2], k->angles[0], k->angles[1], k->angles[2], k->fov, k->timescale );
	}
	Com_Printf( "%i camera keys\n", dt.numKeys );
}

static void CL_CamPlay_f( void ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	if ( dt.numKeys < 1 && !dt.playing ) {
		Com_Printf( "add camera keys first with cam_add\n" );
		return;
	}
	dt.playing = (qboolean)!dt.playing;
	if ( dt.playing ) {
		dt.freecam = qfalse;
	}
	Com_Printf( "camera path %s\n", dt.playing ? "playing" : "stopped" );
}

static qboolean CL_CamPathName( char *path, int size ) {
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: %s <name>\n", Cmd_Argv( 0 ) );
		return qfalse;
	}
	Com_sprintf( path, size, "demos/%s", Cmd_Argv( 1 ) );
	COM_SanitizeExtension( path, size, ".cam" );
	return qtrue;
}

static void CL_CamSave_f( void ) {
	char			path[MAX_QPATH];
	fileHandle_t	f;

	if ( !CL_DemoToolsCheck() || !CL_CamPathName( path, sizeof( path ) ) ) {
		return;
	}
	f = FS_FOpenFileWrite( path );
	if ( !f ) {
		Com_Printf( "couldn't write %s\n", path );
		return;
	}
	FS_Printf( f, "jk2mvcam 1\n" );
	for ( int i = 0; i < dt.numKeys; i++ ) {
		const camKey_t *k = &dt.keys[i];
		FS_Printf( f, "%i %f %f %f %f %f %f %f %f\n", k->time, k->origin[0], k->origin[1], k->origin[2],
			k->angles[0], k->angles[1], k->angles[2], k->fov, k->timescale );
	}
	FS_FCloseFile( f );
	Com_Printf( "wrote %i camera keys to %s\n", dt.numKeys, path );
}

// one "time x y z pitch yaw roll fov speed" line of a .cam file
static qboolean CL_CamParseKey( const char *line, camKey_t *k ) {
	int	used = 0;

	if ( sscanf( line, "%d %f %f %f %f %f %f %f %f %n", &k->time, &k->origin[0], &k->origin[1], &k->origin[2],
		&k->angles[0], &k->angles[1], &k->angles[2], &k->fov, &k->timescale, &used ) != 9 || line[used] ) {
		return qfalse;
	}
	for ( int i = 0; i < 3; i++ ) {
		if ( !std::isfinite( k->origin[i] ) || !std::isfinite( k->angles[i] ) ) {
			return qfalse;
		}
	}
	if ( !std::isfinite( k->fov ) || !std::isfinite( k->timescale ) ) {
		return qfalse;
	}
	// 0 keeps the game's fov; speeds in the demo_speed range
	k->fov = k->fov <= 0.0f ? 0.0f : Com_Clamp( 1.0f, 179.0f, k->fov );
	k->timescale = Com_Clamp( 0.05f, 16.0f, k->timescale );
	return qtrue;
}

static void CL_CamLoad_f( void ) {
	char		path[MAX_QPATH];
	char		*buffer;
	const char	*p;
	int			n = 0, bad = 0;
	qboolean	header = qtrue;

	if ( !CL_DemoToolsCheck() || !CL_CamPathName( path, sizeof( path ) ) ) {
		return;
	}
	if ( FS_ReadFile( path, (void **)&buffer ) < 0 || !buffer ) {
		Com_Printf( "couldn't read %s\n", path );
		return;
	}

	// line by line: a short line must not borrow numbers from the next one
	for ( p = buffer; *p && n < MAX_CAM_KEYS; ) {
		char		line[256];
		const char	*eol = strchr( p, '\n' );
		size_t		len = eol ? (size_t)( eol - p ) : strlen( p );
		camKey_t	k;

		Q_strncpyz( line, p, (int)MIN( len + 1, sizeof( line ) ) );
		len = strlen( line );
		while ( len && ( line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t' ) ) {
			line[--len] = 0;
		}
		p = eol ? eol + 1 : p + strlen( p );

		if ( header ) {
			if ( strcmp( line, "jk2mvcam 1" ) ) {
				Com_Printf( "%s is not a camera path\n", path );
				FS_FreeFile( buffer );
				return;
			}
			header = qfalse;
			continue;
		}
		if ( !line[0] ) {
			continue;
		}
		if ( !CL_CamParseKey( line, &k ) ) {
			bad++;
			continue;
		}
		n = CamPath_Insert( dt.keys, n, MAX_CAM_KEYS, &k );
		if ( n < 0 ) {
			n = MAX_CAM_KEYS;
			break;
		}
	}
	FS_FreeFile( buffer );

	dt.numKeys = n;
	dt.playing = qfalse;
	Com_Printf( "loaded %i camera keys from %s\n", dt.numKeys, path );
	if ( bad ) {
		Com_Printf( S_COLOR_YELLOW "skipped %i malformed lines\n", bad );
	}
}

/*
==================
CL_PhotoMode_f

Pauses the demo, frees the camera and hides the HUD; again restores all of it
==================
*/
static void CL_PhotoMode_f( void ) {
	if ( !CL_DemoToolsCheck() ) {
		return;
	}
	if ( !dt.photo ) {
		dt.photo = qtrue;
		dt.photoHideHud = cl_demoHideHud->integer;
		dt.photoTimescale = CL_Timescale();
		dt.photoPausedTimescale = dt.pausedTimescale;
		dt.photoFreecam = dt.freecam;
		dt.photoPlaying = dt.playing;
		Cvar_Set( "cl_demoHideHud", "1" );
		if ( CL_Timescale() > 0.0f ) {
			dt.pausedTimescale = CL_Timescale();
			CL_SetTimescale( 0.0f );
		}
		if ( !dt.freecam ) {
			// from the camera path's view when one plays
			CL_DemoCamStart();
		}
		Com_Printf( "photo mode: frame your shot, then screenshot_png. photomode again to leave.\n" );
	} else {
		// back exactly as before: a paused demo stays paused
		dt.photo = qfalse;
		Cvar_Set( "cl_demoHideHud", va( "%i", dt.photoHideHud ) );
		CL_SetTimescale( dt.photoTimescale );
		dt.pausedTimescale = dt.photoPausedTimescale;
		dt.freecam = dt.photoFreecam;
		dt.playing = dt.photoPlaying;
		Com_Printf( "photo mode off\n" );
	}
}

/*
===============================================================================

PER FRAME

===============================================================================
*/

/*
==================
CL_DemoToolsFrame

Called before cgame draws a frame: moves the free camera with the usercmds
the engine builds during playback, or follows the camera path.
==================
*/
void CL_DemoToolsFrame( void ) {
	const int64_t	now = Sys_Microseconds();
	const float		seconds = dt.lastFrameUsec ? MIN( 0.25f, ( now - dt.lastFrameUsec ) / 1000000.0f ) : 0.0f;

	dt.lastFrameUsec = now;

	if ( !clc.demoplaying ) {
		if ( dt.freecam || dt.playing || dt.photo || dt.startServerTime ) {
			CL_DemoToolsReset();
		}
		return;
	}

	if ( !dt.startServerTime && cl.snap.valid ) {
		dt.startServerTime = cl.snap.serverTime;
	}

	if ( dt.playing && dt.numKeys > 0 ) {
		camView_t view;

		if ( CamPath_Evaluate( dt.keys, dt.numKeys, (double)cl.serverTime, &view ) ) {
			VectorCopy( view.origin, dt.origin );
			VectorCopy( view.angles, dt.angles );
			dt.fov = view.fov;
			// speed ramps, unless paused by hand
			const float scale = Com_Clamp( 0.05f, 16.0f, view.timescale );
			if ( CL_Timescale() > 0.0f && fabsf( scale - CL_Timescale() ) > 0.001f ) {
				CL_SetTimescale( scale );
			}
		}
	} else if ( dt.freecam && !dt.freecamPending ) {
		const usercmd_t	*cmd = &cl.cmds[cl.cmdNumber & CMD_MASK];
		vec3_t			forward, right;
		const float		speed = cl_freecamSpeed->value * seconds;

		// no looking past straight up or down: the controls would invert
		cl.viewangles[PITCH] = Com_Clamp( -89.0f, 89.0f, AngleNormalize180( cl.viewangles[PITCH] ) );
		if ( !dt.detached && ( cmd->forwardmove || cmd->rightmove || cmd->upmove ||
			fabsf( AngleDelta( cl.viewangles[PITCH], dt.angles[PITCH] ) ) > 0.01f ||
			fabsf( AngleDelta( cl.viewangles[YAW], dt.angles[YAW] ) ) > 0.01f ) ) {
			VectorCopy( cl.viewangles, dt.angles );
			CL_DemoCamDetach();
		}
		VectorCopy( cl.viewangles, dt.angles );
		AngleVectors( dt.angles, forward, right, NULL );
		VectorMA( dt.origin, speed * cmd->forwardmove / 127.0f, forward, dt.origin );
		VectorMA( dt.origin, speed * cmd->rightmove / 127.0f, right, dt.origin );
		dt.origin[2] += speed * cmd->upmove / 127.0f;
	}
}

/*
==================
CL_DemoFreecamFollowView

With the game's view of a scene: places a starting free camera there, or
detaches it once the recorded player moved or turned away from it.
==================
*/
static void CL_DemoFreecamFollowView( void ) {
	vec3_t angles;

	// the free camera's angles: level, and no looking past straight up or down
	angles[PITCH] = Com_Clamp( -89.0f, 89.0f, AngleNormalize180( dt.viewAngles[PITCH] ) );
	angles[YAW] = dt.viewAngles[YAW];
	angles[ROLL] = 0.0f;

	if ( dt.freecamPending ) {
		dt.freecamPending = qfalse;
		VectorCopy( dt.viewOrigin, dt.origin );
		VectorCopy( angles, dt.angles );
		dt.fov = 0.0f;
		// mouse look continues from the camera angles
		VectorCopy( angles, cl.viewangles );
	} else if ( Distance( dt.viewOrigin, dt.origin ) > 0.5f ||
		fabsf( AngleDelta( angles[PITCH], dt.angles[PITCH] ) ) > 0.1f ||
		fabsf( AngleDelta( angles[YAW], dt.angles[YAW] ) ) > 0.1f ) {
		// the camera stays: the game's view would put the view weapon away
		// from it and keep the body that walks off hidden
		CL_DemoCamDetach();
	}
}

/*
==================
CL_DemoCamView

Called for every scene cgame renders. Remembers the game's view and, while the
demo camera shows its own view, returns the scene seen from the camera instead.
==================
*/
qboolean CL_DemoCamView( const refdef_t *fd, refdef_t *out ) {
	// only full screen world scenes, not HUD models or picture in picture
	const qboolean mainView = (qboolean)( !( fd->rdflags & RDF_NOWORLDMODEL ) &&
		fd->width * fd->height * 2 >= cls.glconfig.winWidth * cls.glconfig.winHeight );

	if ( clc.demoplaying && mainView ) {
		// also under the camera: it is where the free camera starts
		VectorCopy( fd->vieworg, dt.viewOrigin );
		vectoangles( fd->viewaxis[0], dt.viewAngles );
		// roll from the left vector
		dt.viewAngles[ROLL] = RAD2DEG( atan2f( fd->viewaxis[1][2], fd->viewaxis[2][2] ) );
		dt.viewFov = fd->fov_x;
		dt.haveView = qtrue;

		if ( dt.freecam && !dt.detached ) {
			CL_DemoFreecamFollowView();
		}
	}

	if ( !clc.demoplaying || !mainView || !CL_DemoCamOwnView() ) {
		// the game's own view, a free camera's first one included
		if ( fd->rdflags & RDF_FREECAM ) {
			// reserved for the engine
			*out = *fd;
			out->rdflags &= ~RDF_FREECAM;
			return qtrue;
		}
		return qfalse;
	}

	*out = *fd;
	VectorCopy( dt.origin, out->vieworg );
	AnglesToAxis( dt.angles, out->viewaxis );
	if ( dt.fov > 1.0f && dt.fov < 179.0f ) {
		const float x = out->width / tanf( DEG2RAD( dt.fov ) * 0.5f );

		out->fov_x = dt.fov;
		out->fov_y = RAD2DEG( atan2f( (float)out->height, x ) ) * 2.0f;
	}
	// show the areas behind closed doors too, the recorder's own body, and
	// not the view weapon
	Com_Memset( out->areamask, 0, sizeof( out->areamask ) );
	out->rdflags |= RDF_FREECAM;
	return qtrue;
}

// camera position for sound and effects while the demo camera shows its own view
qboolean CL_DemoCamOrigin( vec3_t origin, vec3_t axis[3] ) {
	if ( !CL_DemoCamOwnView() ) {
		return qfalse;
	}
	VectorCopy( dt.origin, origin );
	AnglesToAxis( dt.angles, axis );
	return qtrue;
}

/*
==================
CL_DrawDemoTimeline

cl_demoTimeline 1: speed, elapsed time and a progress bar at the bottom
==================
*/
void CL_DrawDemoTimeline( void ) {
	static const vec4_t	backdrop = { 0.0f, 0.0f, 0.0f, 0.6f };
	static const vec4_t	fill = { 0.95f, 0.75f, 0.2f, 0.9f };
	char				text[96];

	// not in photo mode shots or with the HUD hidden
	if ( !clc.demoplaying || !cl_demoTimeline->integer || cls.state != CA_ACTIVE || dt.photo || CL_DemoHideHud() ) {
		return;
	}

	const float	scale = CL_Timescale();
	const int	elapsed = dt.startServerTime ? MAX( 0, cl.snap.serverTime - dt.startServerTime ) / 1000 : 0;
	const float	progress = clc.demoLength > 0 ? Com_Clamp( 0.0f, 1.0f, FS_FTell( clc.demofile ) / (float)clc.demoLength ) : 0.0f;

	Com_sprintf( text, sizeof( text ), "%s %5.2fx  %02i:%02i  %3i%%%s%s", scale > 0.0f ? ">" : "||", scale,
		elapsed / 60, elapsed % 60, (int)( progress * 100.0f ),
		dt.playing ? "  CAM PATH" : dt.freecam ? "  FREECAM" : "", dt.photo ? "  PHOTO" : "" );

	const int	barWidth = cls.glconfig.vidWidth / 2;
	const int	x = ( cls.glconfig.vidWidth - barWidth ) / 2;
	const int	y = cls.glconfig.vidHeight - con.charHeight * 3;

	re.SetColor( backdrop );
	re.DrawStretchPic( x - con.charWidth, y - con.charHeight / 2, barWidth + 2 * con.charWidth, con.charHeight * 5 / 2,
		0, 0, 0, 0, cls.whiteShader, cls.xadjust, cls.yadjust );
	re.SetColor( fill );
	re.DrawStretchPic( x, y + con.charHeight + con.charHeight / 4, barWidth * progress, con.charHeight / 2,
		0, 0, 0, 0, cls.whiteShader, cls.xadjust, cls.yadjust );
	re.SetColor( nullptr );
	SCR_DrawSmallStringExt( x, y, text, g_color_table[ColorIndex(COLOR_WHITE)], qtrue );
}

void CL_InitDemoTools( void ) {
	cl_demoTimeline = Cvar_Get( "cl_demoTimeline", "0", CVAR_ARCHIVE | CVAR_GLOBAL );
	cl_freecamSpeed = Cvar_Get( "cl_freecamSpeed", "400", CVAR_ARCHIVE | CVAR_GLOBAL );

	Cmd_AddCommand( "demo_pause", CL_DemoPause_f );
	Cmd_AddCommand( "demo_speed", CL_DemoSpeed_f );
	Cmd_AddCommand( "demo_faster", CL_DemoFaster_f );
	Cmd_AddCommand( "demo_slower", CL_DemoSlower_f );
	Cmd_AddCommand( "demo_step", CL_DemoStep_f );
	Cmd_AddCommand( "demo_freecam", CL_DemoFreecam_f );
	Cmd_AddCommand( "cam_add", CL_CamAdd_f );
	Cmd_AddCommand( "cam_del", CL_CamDel_f );
	Cmd_AddCommand( "cam_clear", CL_CamClear_f );
	Cmd_AddCommand( "cam_list", CL_CamList_f );
	Cmd_AddCommand( "cam_play", CL_CamPlay_f );
	Cmd_AddCommand( "cam_save", CL_CamSave_f );
	Cmd_AddCommand( "cam_load", CL_CamLoad_f );
	Cmd_AddCommand( "photomode", CL_PhotoMode_f );
}

void CL_ShutdownDemoTools( void ) {
	static const char * const commands[] = {
		"demo_pause", "demo_speed", "demo_faster", "demo_slower", "demo_step", "demo_freecam",
		"cam_add", "cam_del", "cam_clear", "cam_list", "cam_play", "cam_save", "cam_load", "photomode",
	};

	for ( size_t i = 0; i < ARRAY_LEN( commands ); i++ ) {
		Cmd_RemoveCommand( commands[i] );
	}
}
