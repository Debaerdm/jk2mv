// cl_presets.cpp -- one-command visual presets: preset classic|enhanced|ultra|competitive|movie

#include "client.h"

typedef struct {
	const char	*name;
	const char	*value;
} presetCvar_t;

typedef struct preset_s {
	const char				*name;
	const char				*description;
	const struct preset_s	*base;		// its values apply where this one has none
	const presetCvar_t		*cvars;		// NULL terminated, NULL for classic
} preset_t;

// Every cvar a preset may change. Presets are absolute: each one sets all of
// these, the ones it doesn't list back to their engine default, so switching
// presets never keeps leftovers of the previous one. The defaults here are
// only used when the preset runs before the cvar is registered (autoexec.cfg).
// Never touched: com_maxfps (tied to jump physics), snaps, rate, cl_timeNudge.
static const presetCvar_t presetDefaults[] = {
	{ "r_picmip", "1" },
	{ "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST" },
	{ "r_ext_texture_filter_anisotropic", "2" },
	{ "r_ext_multisample", "0" },
	{ "r_ext_alphaToCoverage", "0" },
	{ "r_DynamicGlow", "0" },
	{ "r_DynamicGlowWidth", "320" },
	{ "r_DynamicGlowHeight", "240" },
	{ "r_subdivisions", "4" },
	{ "r_lodCurveError", "250" },
	{ "r_dlightMode", "0" },
	{ "r_fbo", "0" },
	{ "r_hdr", "0" },
	{ "r_bloom", "0" },
	{ "cl_autolodscale", "1" },
	{ "r_swapInterval", "0" },
	{ "r_maxFrameLatency", "0" },
	{ "cl_aviFrameRate", "30" },
	{ "cl_aviMotionJpegQuality", "90" },
	{ "cl_aviMotionBlur", "0" },
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
	{ "r_dlightMode", "1" },						// round, smooth dynamic lights
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
	{ "r_maxFrameLatency", "1" },					// no frames queued ahead of the GPU
	{ NULL, NULL }
};

static const presetCvar_t presetMovieCvars[] = {
	{ "cl_aviFrameRate", "60" },
	{ "cl_aviMotionJpegQuality", "95" },
	{ "cl_aviMotionBlur", "4" },					// 4 frames blended per video frame
	{ "r_fbo", "1" },
	{ "r_hdr", "1" },
	{ "r_bloom", "1" },
	{ NULL, NULL }
};

static const preset_t presetEnhanced = { "enhanced", "anisotropic 16x, MSAA 4x, glow, per-pixel dynamic lights, sharper and smoother surfaces", NULL, presetEnhancedCvars };

static const preset_t presets[] = {
	{ "classic", "the original look (every preset cvar at its default)", NULL, NULL },
	presetEnhanced,
	{ "ultra", "enhanced with MSAA 8x, HDR, bloom and full model detail (CPU heavy in big fights)", &presetEnhanced, presetUltraCvars },
	{ "competitive", "the original look without glow and vsync, lowest input lag", NULL, presetCompetitiveCvars },
	{ "movie", "enhanced with HDR, bloom, 60 fps video capture with motion blur", &presetEnhanced, presetMovieCvars },
};

static const char * const presetNames[] = { "classic", "enhanced", "ultra", "competitive", "movie" };

/*
==================
CL_PresetSet

Sets a cvar for a preset and returns qtrue if it needs a vid_restart.
Protected cvars (cheat, read only, init) are left alone. A cvar that isn't
registered yet (preset in autoexec.cfg) is created with the value, which
the registration keeps.
==================
*/
static qboolean CL_PresetSet( const char *name, const char *value ) {
	cvar_t		*cv = Cvar_FindVar( name );
	const char	*current;

	if ( !cv ) {
		// user created, so the registration still takes the engine default
		// as the reset value (Cvar_Set would make the preset value the default)
		Com_Printf( "  %s: %s\n", name, value );
		Cvar_Get( name, value, CVAR_USER_CREATED );
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

// the preset's own value, else its base's, else NULL for the default
static const char *CL_PresetValue( const preset_t *preset, const char *name ) {
	for ( ; preset; preset = preset->base ) {
		for ( const presetCvar_t *pc = preset->cvars; pc && pc->name; pc++ ) {
			if ( !Q_stricmp( pc->name, name ) ) {
				return pc->value;
			}
		}
	}
	return NULL;
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
	for ( size_t i = 0; i < ARRAY_LEN( presetDefaults ); i++ ) {
		const char *value = CL_PresetValue( preset, presetDefaults[i].name );

		if ( !value ) {
			// the registered default when there is one
			const cvar_t *cv = Cvar_FindVar( presetDefaults[i].name );

			value = cv && cv->resetString && !( cv->flags & CVAR_USER_CREATED ) ? cv->resetString : presetDefaults[i].value;
		}
		restart = (qboolean)( CL_PresetSet( presetDefaults[i].name, value ) | restart );
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

// from CL_InitKeyCommands, before autoexec.cfg runs, and never removed
void CL_InitPresets( void ) {
	Cmd_AddCommand( "preset", CL_Preset_f );
	Cmd_SetCommandCompletionFunc( "preset", CL_CompletePresetName );
}
