// cl_presets.cpp -- one-command visual presets: preset classic|enhanced|ultra|competitive|movie

#include "client.h"

typedef struct {
	const char	*name;
	const char	*value;
} presetCvar_t;

typedef struct preset_s {
	const char				*name;
	const char				*description;
	const struct preset_s	*base;		// applied first
	const presetCvar_t		*cvars;		// NULL terminated, NULL for classic
} preset_t;

// Every cvar a preset may change: "preset classic" resets exactly these.
// Never touched: com_maxfps (tied to jump physics), snaps, rate, cl_timeNudge.
static const char * const presetCvarNames[] = {
	"r_picmip",
	"r_textureMode",
	"r_ext_texture_filter_anisotropic",
	"r_ext_multisample",
	"r_ext_alphaToCoverage",
	"r_DynamicGlow",
	"r_DynamicGlowWidth",
	"r_DynamicGlowHeight",
	"r_subdivisions",
	"r_lodCurveError",
	"r_fbo",
	"r_hdr",
	"r_bloom",
	"cl_autolodscale",
	"r_swapInterval",
	"cl_aviFrameRate",
	"cl_aviMotionJpegQuality",
};

static const presetCvar_t presetEnhancedCvars[] = {
	{ "r_picmip", "0" },
	{ "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR" },
	{ "r_ext_texture_filter_anisotropic", "16" },	// clamped to the hardware maximum
	{ "r_ext_multisample", "4" },
	{ "r_ext_alphaToCoverage", "1" },
	{ "r_DynamicGlow", "1" },
	{ "r_DynamicGlowWidth", "0" },					// automatic size, sharp at any resolution
	{ "r_DynamicGlowHeight", "0" },
	{ "r_subdivisions", "2" },						// smoother curved surfaces
	{ "r_lodCurveError", "1000" },					// keep them detailed farther away
	{ NULL, NULL }
};

// offscreen rendering with HDR and bloom: sabers, blasters and lights
// bloom and stay colored instead of clipping to white
static const presetCvar_t presetUltraCvars[] = {
	{ "r_ext_multisample", "8" },
	{ "cl_autolodscale", "0" },						// full model detail with many players
	{ "r_fbo", "1" },
	{ "r_hdr", "1" },
	{ "r_bloom", "1" },
	{ NULL, NULL }
};

static const presetCvar_t presetCompetitiveCvars[] = {
	{ "r_DynamicGlow", "0" },
	{ "r_swapInterval", "0" },
	{ NULL, NULL }
};

static const presetCvar_t presetMovieCvars[] = {
	{ "cl_aviFrameRate", "60" },
	{ "cl_aviMotionJpegQuality", "95" },
	{ "r_fbo", "1" },
	{ "r_hdr", "1" },
	{ "r_bloom", "1" },
	{ NULL, NULL }
};

static const preset_t presetEnhanced = { "enhanced", "anisotropic 16x, MSAA 4x, glow, sharper and smoother surfaces", NULL, presetEnhancedCvars };

static const preset_t presets[] = {
	{ "classic", "the original look (resets every preset cvar)", NULL, NULL },
	presetEnhanced,
	{ "ultra", "enhanced with MSAA 8x, HDR, bloom and full model detail (CPU heavy in big fights)", &presetEnhanced, presetUltraCvars },
	{ "competitive", "no glow, no vsync", NULL, presetCompetitiveCvars },
	{ "movie", "enhanced with HDR, bloom, 60 fps and high quality video capture", &presetEnhanced, presetMovieCvars },
};

static const char * const presetNames[] = { "classic", "enhanced", "ultra", "competitive", "movie" };

/*
==================
CL_PresetSet

Sets a cvar for a preset and returns qtrue if it needs a vid_restart.
Protected cvars (cheat, read only, init) are left alone.
==================
*/
static qboolean CL_PresetSet( const char *name, const char *value ) {
	cvar_t		*cv = Cvar_FindVar( name );
	const char	*current;

	if ( !cv ) {
		return qfalse;
	}
	if ( cv->flags & ( CVAR_CHEAT | CVAR_ROM | CVAR_INIT ) ) {
		Com_Printf( "  %s is protected, skipped\n", name );
		return qfalse;
	}

	current = cv->latchedString ? cv->latchedString : cv->string;
	if ( !Q_stricmp( current, value ) ) {
		return qfalse;
	}

	Com_Printf( "  %s: %s -> %s\n", name, current, value );
	Cvar_Set( name, value );

	return (qboolean)( ( cv->flags & CVAR_LATCH ) != 0 );
}

static qboolean CL_PresetApply( const preset_t *preset ) {
	qboolean	restart = qfalse;

	if ( preset->base ) {
		restart = (qboolean)( CL_PresetApply( preset->base ) | restart );
	}
	for ( const presetCvar_t *pc = preset->cvars; pc && pc->name; pc++ ) {
		restart = (qboolean)( CL_PresetSet( pc->name, pc->value ) | restart );
	}
	return restart;
}

/*
==================
CL_Preset_f
==================
*/
static void CL_Preset_f( void ) {
	const preset_t	*preset = NULL;
	qboolean		restart = qfalse;

	if ( Cmd_Argc() == 2 ) {
		for ( size_t i = 0; i < ARRAY_LEN( presets ); i++ ) {
			if ( !Q_stricmp( Cmd_Argv( 1 ), presets[i].name ) ) {
				preset = &presets[i];
				break;
			}
		}
	}

	if ( !preset ) {
		Com_Printf( "usage: preset <name>\n" );
		for ( size_t i = 0; i < ARRAY_LEN( presets ); i++ ) {
			Com_Printf( "  " S_COLOR_YELLOW "%-12s" S_COLOR_WHITE " %s\n", presets[i].name, presets[i].description );
		}
		return;
	}

	Com_Printf( "preset %s:\n", preset->name );
	if ( !preset->cvars ) {
		// classic: back to the engine defaults of everything a preset touches
		for ( size_t i = 0; i < ARRAY_LEN( presetCvarNames ); i++ ) {
			cvar_t *cv = Cvar_FindVar( presetCvarNames[i] );

			if ( cv && cv->resetString ) {
				restart = (qboolean)( CL_PresetSet( presetCvarNames[i], cv->resetString ) | restart );
			}
		}
	} else {
		restart = CL_PresetApply( preset );
	}

	if ( restart ) {
		Com_Printf( "restarting the renderer to apply the preset\n" );
		Cbuf_AddText( "vid_restart\n" );
	}
}

static void CL_CompletePresetName( char *args, int argNum ) {
	if ( argNum == 2 ) {
		Field_CompleteList( presetNames, (int)ARRAY_LEN( presetNames ) );
	}
}

void CL_InitPresets( void ) {
	Cmd_AddCommand( "preset", CL_Preset_f );
	Cmd_SetCommandCompletionFunc( "preset", CL_CompletePresetName );
}

void CL_ShutdownPresets( void ) {
	Cmd_RemoveCommand( "preset" );
}
