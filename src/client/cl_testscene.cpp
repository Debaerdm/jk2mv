// cl_testscene.cpp -- testscene command: draws a map from a fixed camera
// while disconnected, without the game modules
//
// testscene <map> [x y z [yaw [pitch]]] [dlight | dlights] [spin]
// testscene off
//
// For trying renderer settings on a map and for the renderer smoke test,
// which has no retail assets (so no cgame) but needs world rendering. The
// optional dlight adds a fixed dynamic light in front of the camera; dlights
// adds it after 40 others behind the camera, past the classic limit of 32
// lights (r_dlightPriority). spin turns the camera at 90 degrees per second
// of client time, for time based effects (video motion blur).

#include "client.h"

static struct {
	qboolean	active;
	int			dlights;	// 0, 1: the light in front, 2: plus 40 behind first
	qboolean	spin;
	int			startTime;
	vec3_t		origin;
	vec3_t		angles;
} testScene;

// the renderer was (re)started: the world it had is gone
void CL_TestSceneReset( void ) {
	testScene.active = qfalse;
}

qboolean CL_TestSceneActive( void ) {
	return (qboolean)( testScene.active && cls.state == CA_DISCONNECTED );
}

static void CL_TestScene_f( void ) {
	char	name[MAX_QPATH];
	int		argc = Cmd_Argc();

	if ( argc < 2 ) {
		Com_Printf( "usage: testscene <map> [x y z [yaw [pitch]]] [dlight | dlights] [spin], testscene off\n" );
		return;
	}
	if ( !Q_stricmp( Cmd_Argv( 1 ), "off" ) ) {
		if ( testScene.active ) {
			testScene.active = qfalse;
			Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
		}
		return;
	}
	if ( cls.state != CA_DISCONNECTED ) {
		Com_Printf( "testscene only works while disconnected\n" );
		return;
	}

	Com_sprintf( name, sizeof( name ), "maps/%s.bsp", Cmd_Argv( 1 ) );
	if ( FS_ReadFile( name, NULL ) <= 0 ) {
		Com_Printf( "testscene: can't find %s\n", name );
		return;
	}

	// keywords after the numbers
	testScene.dlights = 0;
	testScene.spin = qfalse;
	while ( argc > 2 ) {
		const char *word = Cmd_Argv( argc - 1 );

		if ( !Q_stricmp( word, "dlight" ) ) {
			testScene.dlights = 1;
		} else if ( !Q_stricmp( word, "dlights" ) ) {
			testScene.dlights = 2;
		} else if ( !Q_stricmp( word, "spin" ) ) {
			testScene.spin = qtrue;
		} else {
			break;
		}
		argc--;
	}
	VectorClear( testScene.origin );
	VectorClear( testScene.angles );
	testScene.origin[2] = 64.0f;
	for ( int i = 0; i < 3 && 2 + i < argc; i++ ) {
		testScene.origin[i] = atof( Cmd_Argv( 2 + i ) );
	}
	if ( argc > 5 ) {
		testScene.angles[YAW] = atof( Cmd_Argv( 5 ) );
	}
	if ( argc > 6 ) {
		testScene.angles[PITCH] = atof( Cmd_Argv( 6 ) );
	}

	// the renderer loads one world per registration: start from a fresh one
	CL_FlushMemory( qfalse );
	re.LoadWorld( name );
	testScene.active = qtrue;
	testScene.startTime = cls.realtime;
	if ( uivm ) {
		VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE );
	}
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
}

/*
==================
CL_DrawTestScene

Instead of the main menu, from SCR_DrawScreenField
==================
*/
void CL_DrawTestScene( void ) {
	refdef_t	rd;

	Com_Memset( &rd, 0, sizeof( rd ) );
	rd.width = cls.glconfig.vidWidth;
	rd.height = cls.glconfig.vidHeight;
	rd.fov_x = 90.0f;
	rd.fov_y = RAD2DEG( 2.0f * atanf( tanf( DEG2RAD( rd.fov_x * 0.5f ) ) * rd.height / rd.width ) );
	VectorCopy( testScene.origin, rd.vieworg );
	vec3_t angles;
	VectorCopy( testScene.angles, angles );
	if ( testScene.spin ) {
		angles[YAW] += ( cls.realtime - testScene.startTime ) * 0.09f;
	}
	AnglesToAxis( angles, rd.viewaxis );
	rd.time = cls.realtime;

	re.ClearScene();
	if ( testScene.dlights == 2 ) {
		// out of sight behind the camera, added first
		for ( int i = 0; i < 40; i++ ) {
			vec3_t	light;

			VectorMA( rd.vieworg, -300.0f - 4.0f * i, rd.viewaxis[0], light );
			re.AddLightToScene( light, 100.0f, 1.0f, 0.2f, 0.2f );
		}
	}
	if ( testScene.dlights ) {
		vec3_t	light;

		VectorMA( rd.vieworg, 192.0f, rd.viewaxis[0], light );
		light[2] = rd.vieworg[2] - 32.0f;
		re.AddLightToScene( light, 200.0f, 0.3f, 0.5f, 1.0f );
	}
	re.RenderScene( &rd );
}

void CL_InitTestScene( void ) {
	Cmd_AddCommand( "testscene", CL_TestScene_f );
}

void CL_ShutdownTestScene( void ) {
	Cmd_RemoveCommand( "testscene" );
	testScene.active = qfalse;
}
