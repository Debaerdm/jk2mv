// cl_presets.cpp -- one-command visual presets: preset classic|enhanced|ultra|competitive|movie,
// and cl_preset, the preset the current settings match, for the setup menus

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
	{ "r_dlightPriority", "0" },
	{ "r_fbo", "0" },
	{ "r_hdr", "0" },
	{ "r_bloom", "0" },
	{ "cl_autolodscale", "1" },
	{ "r_swapInterval", "0" },
	{ "r_maxFrameLatency", "0" },
	{ "cl_fovAspectFix", "0" },
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
	{ "r_DynamicGlowWidth", "0" },					// automatic size, sharp at any resolution: a width
													// of 0 is enough, so the height keeps its default
													// and the menu's Glow Quality (width only) matches
	{ "r_subdivisions", "2" },						// smoother curved surfaces
	{ "r_lodCurveError", "1000" },					// keep them detailed farther away
	{ "r_dlightMode", "1" },						// round, smooth dynamic lights
	{ "r_dlightPriority", "1" },					// past 32 lights, the ones that matter for the view
	{ "cl_fovAspectFix", "1" },						// wide screens see more, not less
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
	{ "r_dlightPriority", "1" },					// the lights near the player in big fights
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

static const preset_t presetEnhanced = { "enhanced", "anisotropic 16x, MSAA 4x, glow, per-pixel dynamic lights, widescreen fov, sharper and smoother surfaces", NULL, presetEnhancedCvars };

static const preset_t presets[] = {
	{ "classic", "the original look (every preset cvar at its default)", NULL, NULL },
	presetEnhanced,
	{ "ultra", "enhanced with MSAA 8x, HDR, bloom and full model detail (CPU heavy in big fights)", &presetEnhanced, presetUltraCvars },
	{ "competitive", "the original look without glow and vsync, lowest input lag, nearby lights first in big fights", NULL, presetCompetitiveCvars },
	{ "movie", "enhanced with HDR, bloom, 60 fps video capture with motion blur", &presetEnhanced, presetMovieCvars },
};

static const char * const presetNames[] = { "classic", "enhanced", "ultra", "competitive", "movie" };

static cvar_t	*cl_preset;				// read only: the preset the settings match, or "custom"
static int		presetChangeCount = -1;	// cvar modification counts summed by CL_PresetFrame
static qboolean	presetRestart;			// a preset changed latched cvars

#define PRESET_PROTECTED	( CVAR_CHEAT | CVAR_ROM | CVAR_INIT )

// numbers compare by value ("1" and "1.000000"), the rest without case
static qboolean CL_PresetValueIs( const char *current, const char *value ) {
	char	*end1, *end2;
	double	a = strtod( current, &end1 );
	double	b = strtod( value, &end2 );

	if ( end1 != current && !*end1 && end2 != value && !*end2 ) {
		return (qboolean)( a == b );
	}
	return (qboolean)!Q_stricmp( current, value );
}

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
	cvar_t			*cv = Cvar_FindVar( name );
	const cvar_t	*copy;
	const char		*current;
	qboolean		restart = qfalse;

	if ( !cv ) {
		// user created, so the registration still takes the engine default
		// as the reset value (Cvar_Set would make the preset value the default)
		Com_Printf( "  %s: %s\n", name, value );
		Cvar_Get( name, value, CVAR_USER_CREATED );
		return qfalse;
	}
	if ( cv->flags & PRESET_PROTECTED ) {
		Com_Printf( "  %s is protected, skipped\n", name );
		return qfalse;
	}

	current = cv->latchedString ? cv->latchedString : cv->string;
	if ( !CL_PresetValueIs( current, value ) ) {
		Com_Printf( "  %s: %s -> %s\n", name, current, value );
		Cvar_Set( name, value );
		restart = (qboolean)( ( cv->flags & CVAR_LATCH ) != 0 );
	}

	// The setup menus edit copies of some video cvars (ui_r_picmip...),
	// taken when their video page opens and written back by its Apply
	// button: keep them in step, even if the cvar had the value already, so
	// the page shows the preset and a later Apply keeps it.
	copy = Cvar_FindVar( va( "ui_%s", name ) );
	if ( copy && !CL_PresetValueIs( copy->string, value ) ) {
		Cvar_Set( copy->name, value );
	}

	return restart;
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

// what a preset sets presetDefaults[i] to: its value, else the default
static const char *CL_PresetTarget( const preset_t *preset, size_t i ) {
	const char *value = CL_PresetValue( preset, presetDefaults[i].name );

	if ( !value ) {
		// the registered default when there is one
		const cvar_t *cv = Cvar_FindVar( presetDefaults[i].name );

		value = cv && cv->resetString && !( cv->flags & CVAR_USER_CREATED ) ? cv->resetString : presetDefaults[i].value;
	}
	return value;
}

/*
==================
CL_PresetUpdate

Sets cl_preset to the preset whose values the cvars have (the pending value
of a latched one), or "custom". Protected cvars, which presets skip, don't
count. At most one preset matches: any two differ by a cvar.
==================
*/
static void CL_PresetUpdate( void ) {
	const char *match = "custom";

	for ( size_t p = 0; p < ARRAY_LEN( presets ); p++ ) {
		size_t i;

		for ( i = 0; i < ARRAY_LEN( presetDefaults ); i++ ) {
			const cvar_t	*cv = Cvar_FindVar( presetDefaults[i].name );
			const char		*current = presetDefaults[i].value;

			if ( cv ) {
				if ( cv->flags & PRESET_PROTECTED ) {
					continue;
				}
				current = cv->latchedString ? cv->latchedString : cv->string;
			}
			if ( !CL_PresetValueIs( current, CL_PresetTarget( &presets[p], i ) ) ) {
				break;
			}
		}
		if ( i == ARRAY_LEN( presetDefaults ) ) {
			match = presets[p].name;
			break;
		}
	}

	if ( strcmp( cl_preset->string, match ) ) {
		Cvar_Set( cl_preset->name, match );
	}
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
		CL_PresetUpdate();
		Com_Printf( "the current settings: %s\n", cl_preset->string );
		return;
	}

	Com_Printf( "preset %s:\n", preset->name );
	for ( size_t i = 0; i < ARRAY_LEN( presetDefaults ); i++ ) {
		restart = (qboolean)( CL_PresetSet( presetDefaults[i].name, CL_PresetTarget( preset, i ) ) | restart );
	}
	CL_PresetUpdate();

	// a renderer that isn't started yet (autoexec.cfg) starts with the values
	if ( restart && cls.rendererStarted ) {
		// once, before this frame is drawn (CL_PresetFrame), unless a
		// vid_restart comes first: in a script or bind, what follows a wait
		// runs with the preset applied, a chain of presets restarts once,
		// and a latched cvar set right after the preset is applied with it.
		// The setup menus run a preset together with their own video
		// changes, whose vid_restart applies both.
		Com_Printf( "restarting the renderer to apply the preset\n" );
		presetRestart = qtrue;
	}
}

static void CL_CompletePresetName( char *args, int argNum ) {
	if ( argNum == 2 ) {
		Field_CompleteList( presetNames, (int)ARRAY_LEN( presetNames ) );
	}
}

// any change of a preset cvar (or of cl_preset) raises this sum
static int CL_PresetChangeCount( void ) {
	int count = cl_preset->modificationCount;

	for ( size_t i = 0; i < ARRAY_LEN( presetDefaults ); i++ ) {
		const cvar_t *cv = Cvar_FindVar( presetDefaults[i].name );

		if ( cv ) {
			count += cv->modificationCount;
		}
	}
	return count;
}

/*
==================
CL_PresetFrame

From CL_Frame, after the commands of the frame: the vid_restart a preset
asked for, and cl_preset kept up to date whenever a preset cvar changes, from
the console, a menu or a config.
==================
*/
void CL_PresetFrame( void ) {
	if ( presetRestart ) {
		presetRestart = qfalse;
		Cbuf_ExecuteText( EXEC_NOW, "vid_restart\n" );
	}

	if ( CL_PresetChangeCount() != presetChangeCount ) {
		CL_PresetUpdate();
		presetChangeCount = CL_PresetChangeCount();
	}
}

// from vid_restart, which applies the latched cvars a preset changed
void CL_PresetRestarted( void ) {
	presetRestart = qfalse;
}

/*
==================
CL_PresetUIStarted

From CL_InitUI. The setup menus keep the preset picked on their video page in
ui_presetPending until their Apply button runs it, and the patched video
warning popup, which the menus of other mods may use too, runs it as well. A
new UI module starts without a pending preset.
==================
*/
void CL_PresetUIStarted( void ) {
	if ( Cvar_FindVar( "ui_presetPending" ) ) {
		Cvar_Set( "ui_presetPending", "" );
	}
}

// from CL_InitKeyCommands, before autoexec.cfg runs, and never removed
void CL_InitPresets( void ) {
	cl_preset = Cvar_Get( "cl_preset", "", CVAR_ROM | CVAR_VM_NOWRITE );
	Cmd_AddCommand( "preset", CL_Preset_f );
	Cmd_SetCommandCompletionFunc( "preset", CL_CompletePresetName );
}
