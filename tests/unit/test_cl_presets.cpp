// test_cl_presets.cpp - visual presets, cl_preset and what the setup menus use
//
// Links the real src/client/cl_presets.cpp with small fakes of the engine
// functions it calls: a cvar table with the engine's set semantics
// (src/qcommon/cvar.cpp), the command arguments and the command buffer.

#include <gtest/gtest.h>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "client/client.h"

clientStatic_t cls;

namespace {

struct FakeCvar {
	cvar_t		cv;
	std::string	name, string, reset, latched;
};

std::vector<std::unique_ptr<FakeCvar>> cvars;
int		epoch;			// keeps modification counts apart between tests
xcommand_t	presetCommand;
std::vector<std::string> args;
std::vector<std::string> executed;

int Compare( const char *a, const char *b ) {
	for ( ;; a++, b++ ) {
		int ca = tolower( (unsigned char)*a ), cb = tolower( (unsigned char)*b );
		if ( ca != cb ) {
			return ca < cb ? -1 : 1;
		}
		if ( !ca ) {
			return 0;
		}
	}
}

void Sync( FakeCvar &f ) {
	f.cv.name = f.name.c_str();
	f.cv.string = f.string.c_str();
	f.cv.resetString = f.reset.c_str();
	f.cv.latchedString = f.latched.empty() ? nullptr : f.latched.c_str();
	f.cv.value = (float)atof( f.cv.string );
	f.cv.integer = atoi( f.cv.string );
}

FakeCvar *Find( const char *name ) {
	for ( auto &f : cvars ) {
		if ( !Compare( f->name.c_str(), name ) ) {
			return f.get();
		}
	}
	return nullptr;
}

FakeCvar *Create( const char *name, const char *value, int flags ) {
	std::unique_ptr<FakeCvar> f( new FakeCvar() );
	f->name = name;
	f->string = value;
	f->reset = value;
	f->cv.flags = flags;
	f->cv.modificationCount = 1 + epoch;
	Sync( *f );
	cvars.push_back( std::move( f ) );
	return cvars.back().get();
}

// what the renderer or the client registers, and what the UI copies
void Register( const char *name, const char *value, int flags = CVAR_ARCHIVE ) {
	Cvar_Get( name, value, flags );
}

// a console set of a latched cvar: pending until the next vid_restart
void SetLatched( const char *name, const char *value ) {
	FakeCvar *f = Find( name );
	ASSERT_NE( f, nullptr );
	f->latched = value;
	f->cv.modificationCount++;
	Sync( *f );
}

// what a menu script does (setcvar, a multi item): a forced set
void MenuSet( const char *name, const char *value ) {
	Cvar_Set( name, value );
}

const char *Value( const char *name ) {
	const FakeCvar *f = Find( name );
	return f ? f->cv.string : "(none)";
}

void Preset( const char *name ) {
	args.clear();
	args.push_back( "preset" );
	if ( name ) {
		args.push_back( name );
	}
	ASSERT_NE( presetCommand, nullptr );
	presetCommand();
}

int Restarts() {
	int n = 0;
	for ( const std::string &s : executed ) {
		n += s == "vid_restart\n";
	}
	return n;
}

} // namespace

// ---- the engine functions cl_presets.cpp uses

cvar_t *Cvar_FindVar( const char *name ) {
	FakeCvar *f = Find( name );
	return f ? &f->cv : nullptr;
}

cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	FakeCvar *f = Find( name );
	if ( !f ) {
		return &Create( name, value, flags )->cv;
	}
	// like the engine: a registration takes over a user created cvar, and
	// its value becomes the reset value
	if ( ( f->cv.flags & CVAR_USER_CREATED ) && !( flags & CVAR_USER_CREATED ) ) {
		f->cv.flags &= ~CVAR_USER_CREATED;
		if ( value[0] ) {
			f->reset = value;
		}
	}
	f->cv.flags |= flags;
	Sync( *f );
	return &f->cv;
}

// Cvar_Set is a forced set: no latching, a pending value is dropped. Like
// a set from a VM, it creates a missing cvar as a registered one.
void Cvar_Set( const char *name, const char *value ) {
	FakeCvar *f = Find( name );
	if ( !f ) {
		Create( name, value, 0 );
		return;
	}
	if ( !f->latched.empty() ) {
		f->latched.clear();
		f->cv.modificationCount++;
	}
	if ( f->string != value ) {
		f->string = value;
		f->cv.modificationCount++;
	}
	Sync( *f );
}

void Cmd_AddCommand( const char *name, xcommand_t function ) {
	if ( !Compare( name, "preset" ) ) {
		presetCommand = function;
	}
}

void Cmd_SetCommandCompletionFunc( const char *, completionFunc_t ) {
}

int Cmd_Argc( void ) {
	return (int)args.size();
}

char *Cmd_Argv( int arg ) {
	static char empty[1];
	return arg < (int)args.size() ? (char *)args[arg].c_str() : empty;
}

void Cbuf_ExecuteText( cbufExec_t, const char *text ) {
	executed.push_back( text );
}

void Field_CompleteList( const char * const *, int ) {
}

void QDECL Com_Printf( const char *, ... ) {
}

char * QDECL va( const char *format, ... ) {
	static char buffers[4][1024];
	static int index;
	char *buf = buffers[index++ & 3];
	va_list ap;
	va_start( ap, format );
	vsnprintf( buf, sizeof( buffers[0] ), format, ap );
	va_end( ap );
	return buf;
}

int Q_stricmp( const char *s1, const char *s2 ) {
	return Compare( s1, s2 );
}

// ---- tests

namespace {

const char * const presetNames[] = { "classic", "enhanced", "ultra", "competitive", "movie" };

// the registrations of the renderer and the client
const struct {
	const char	*name;
	const char	*value;
	int			flags;
} registered[] = {
	{ "r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE },
	{ "r_ext_texture_filter_anisotropic", "2", CVAR_ARCHIVE },
	{ "r_ext_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_ext_alphaToCoverage", "0", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_DynamicGlow", "0", CVAR_ARCHIVE },
	{ "r_DynamicGlowWidth", "320", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_DynamicGlowHeight", "240", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_subdivisions", "4", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_lodCurveError", "250", CVAR_ARCHIVE },
	{ "r_dlightMode", "0", CVAR_ARCHIVE },
	{ "r_dlightPriority", "0", CVAR_ARCHIVE },
	{ "r_fbo", "0", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_hdr", "0", CVAR_ARCHIVE | CVAR_LATCH },
	{ "r_bloom", "0", CVAR_ARCHIVE | CVAR_LATCH },
	{ "cl_autolodscale", "1", CVAR_ARCHIVE },
	{ "r_swapInterval", "0", CVAR_ARCHIVE },
	{ "r_maxFrameLatency", "0", CVAR_ARCHIVE },
	{ "cl_fovAspectFix", "0", CVAR_ARCHIVE },
	{ "cl_aviFrameRate", "30", CVAR_ARCHIVE },
	{ "cl_aviMotionJpegQuality", "90", CVAR_ARCHIVE },
	{ "cl_aviMotionBlur", "0", CVAR_ARCHIVE },
};

class Presets : public ::testing::Test {
protected:
	void SetUp() override {
		Start( nullptr );
		CL_PresetFrame();
	}

	// a fresh client: the presets, then every registration but skip's
	void Start( const char *skip ) {
		cvars.clear();
		epoch += 10000;
		executed.clear();
		presetCommand = nullptr;
		cls.rendererStarted = qtrue;
		CL_PresetRestarted();
		CL_InitPresets();
		for ( const auto &r : registered ) {
			if ( !skip || Compare( r.name, skip ) ) {
				Register( r.name, r.value, r.flags );
			}
		}
	}
};

TEST_F(Presets, FreshSettingsAreClassic) {
	EXPECT_STREQ( Value( "cl_preset" ), "classic" );
	EXPECT_NE( Cvar_FindVar( "cl_preset" )->flags & CVAR_ROM, 0 );
	EXPECT_NE( Cvar_FindVar( "cl_preset" )->flags & CVAR_VM_NOWRITE, 0 );
}

// cl_preset shows the first preset that matches, so each preset showing its
// own name also proves that no two presets have the same values
TEST_F(Presets, EachPresetIsRecognizedAsItself) {
	for ( const char *name : presetNames ) {
		Preset( name );
		CL_PresetFrame();
		EXPECT_STREQ( Value( "cl_preset" ), name );
	}
}

TEST_F(Presets, ClassicRestoresEveryDefault) {
	Preset( "ultra" );
	Preset( "classic" );
	for ( const auto &f : cvars ) {
		if ( f->name != "cl_preset" ) {
			EXPECT_EQ( f->string, f->reset ) << f->name;
		}
	}
	EXPECT_STREQ( Value( "cl_preset" ), "classic" );
}

TEST_F(Presets, EnhancedSetsAnAutomaticGlowSizeWithTheWidthOnly) {
	Preset( "enhanced" );
	EXPECT_STREQ( Value( "r_DynamicGlowWidth" ), "0" );
	EXPECT_STREQ( Value( "r_DynamicGlowHeight" ), "240" );
}

TEST_F(Presets, AnotherValueMakesTheSettingsCustom) {
	Preset( "enhanced" );
	Cvar_Set( "r_picmip", "1" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "custom" );
	Cvar_Set( "r_picmip", "0" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "enhanced" );
}

TEST_F(Presets, NumbersCompareByValue) {
	Preset( "enhanced" );
	Cvar_Set( "r_picmip", "0.000000" );	// as menus write some values
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "enhanced" );
}

TEST_F(Presets, APendingLatchedValueCounts) {
	SetLatched( "r_picmip", "0" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "custom" );
}

TEST_F(Presets, ProtectedCvarsAreSkippedAndDontCount) {
	Cvar_FindVar( "r_lodCurveError" )->flags |= CVAR_CHEAT;
	Preset( "enhanced" );
	EXPECT_STREQ( Value( "r_lodCurveError" ), "250" );
	EXPECT_STREQ( Value( "cl_preset" ), "enhanced" );
}

TEST_F(Presets, OneRestartAtTheNextFrame) {
	Preset( "enhanced" );
	EXPECT_EQ( Restarts(), 0 );
	CL_PresetFrame();
	EXPECT_EQ( Restarts(), 1 );
	CL_PresetFrame();
	EXPECT_EQ( Restarts(), 1 );
}

// the setup menus run the preset together with their own vid_restart
TEST_F(Presets, AVidRestartFirstReplacesThePresetsOwn) {
	Preset( "enhanced" );
	CL_PresetRestarted();
	CL_PresetFrame();
	EXPECT_EQ( Restarts(), 0 );
}

TEST_F(Presets, NoRestartBeforeTheRendererStarts) {
	cls.rendererStarted = qfalse;
	Preset( "enhanced" );
	CL_PresetFrame();
	EXPECT_EQ( Restarts(), 0 );
	EXPECT_STREQ( Value( "r_picmip" ), "0" );
}

TEST_F(Presets, NoRestartWithoutLatchedChanges) {
	Preset( "competitive" );	// only r_maxFrameLatency differs from classic
	CL_PresetFrame();
	EXPECT_EQ( Restarts(), 0 );
	EXPECT_STREQ( Value( "r_maxFrameLatency" ), "1" );
}

// a preset in autoexec.cfg runs before the renderer registers its cvars
TEST_F(Presets, UnregisteredCvarsKeepTheirEngineDefault) {
	Start( "r_picmip" );
	Preset( "enhanced" );
	ASSERT_NE( Cvar_FindVar( "r_picmip" ), nullptr );
	EXPECT_NE( Cvar_FindVar( "r_picmip" )->flags & CVAR_USER_CREATED, 0 );
	Register( "r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH );
	EXPECT_STREQ( Value( "r_picmip" ), "0" );
	EXPECT_STREQ( Cvar_FindVar( "r_picmip" )->resetString, "1" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "enhanced" );
}

TEST_F(Presets, ACvarRegisteredLaterCounts) {
	Start( "cl_fovAspectFix" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "classic" );	// its default assumed
	Register( "cl_fovAspectFix", "0", CVAR_ARCHIVE );
	Cvar_Set( "cl_fovAspectFix", "1" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "custom" );
}

// the video page edits copies (ui_r_picmip...) and its Apply writes them back
TEST_F(Presets, TheMenuCopiesFollowThePreset) {
	Register( "ui_r_picmip", "1", CVAR_ROM | CVAR_INTERNAL );
	Register( "ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ROM | CVAR_INTERNAL );
	Preset( "enhanced" );
	EXPECT_STREQ( Value( "ui_r_picmip" ), "0" );
	EXPECT_STREQ( Value( "ui_r_texturemode" ), "GL_LINEAR_MIPMAP_LINEAR" );
	EXPECT_EQ( Cvar_FindVar( "ui_r_subdivisions" ), nullptr );	// no copy, none created
}

TEST_F(Presets, TheMenuCopiesFollowEvenWhenTheCvarHadTheValue) {
	Preset( "enhanced" );
	Register( "ui_r_picmip", "2", CVAR_ROM | CVAR_INTERNAL );	// an unapplied page edit
	Preset( "enhanced" );
	EXPECT_STREQ( Value( "ui_r_picmip" ), "0" );
}

// the latched options of the advanced video page have copies too, which the
// engine keeps: the ui modules don't know them
TEST_F(Presets, TheAdvancedPageCopiesStartWithTheUI) {
	Cvar_Set( "r_fbo", "1" );
	CL_PresetUIStarted();
	EXPECT_STREQ( Value( "ui_r_fbo" ), "1" );
	EXPECT_STREQ( Value( "ui_r_hdr" ), "0" );
	EXPECT_STREQ( Value( "ui_r_bloom" ), "0" );
	EXPECT_STREQ( Value( "ui_r_DynamicGlowWidth" ), "320" );
	EXPECT_STREQ( Value( "ui_r_DynamicGlowHeight" ), "240" );
	// like the copies of the ui modules
	EXPECT_NE( Cvar_FindVar( "ui_r_fbo" )->flags & CVAR_ROM, 0 );
	EXPECT_NE( Cvar_FindVar( "ui_r_fbo" )->flags & CVAR_INTERNAL, 0 );
	EXPECT_EQ( Cvar_FindVar( "ui_r_fbo" )->flags & CVAR_ARCHIVE, 0 );
}

TEST_F(Presets, TheCopiesFollowTheirCvarWhileNothingWaitsToBeApplied) {
	CL_PresetUIStarted();
	Cvar_Set( "r_fbo", "1" );			// the console, a config...
	SetLatched( "r_bloom", "1" );		// pending until the next vid_restart
	CL_PresetFrame();
	EXPECT_STREQ( Value( "ui_r_fbo" ), "1" );
	EXPECT_STREQ( Value( "ui_r_bloom" ), "1" );
	MenuSet( "ui_r_modified", "0" );	// a video page opened
	Cvar_Set( "r_fbo", "0" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "ui_r_fbo" ), "0" );
}

// Post-Processing turned on, then the unapplied changes popup: Discard
TEST_F(Presets, DiscardDropsAChangeOfTheAdvancedPage) {
	CL_PresetUIStarted();
	MenuSet( "ui_r_fbo", "1" );
	MenuSet( "ui_r_modified", "1" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "ui_r_fbo" ), "1" );	// kept for APPLY CHANGES
	EXPECT_STREQ( Value( "r_fbo" ), "0" );
	MenuSet( "ui_r_modified", "0" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "ui_r_fbo" ), "0" );
	EXPECT_STREQ( Value( "r_fbo" ), "0" );
	EXPECT_STREQ( Value( "cl_preset" ), "classic" );
}

// ...and Apply: the menu writes the copy back (setcvartocvar) first
TEST_F(Presets, ApplyKeepsAChangeOfTheAdvancedPage) {
	CL_PresetUIStarted();
	MenuSet( "ui_r_fbo", "1" );
	MenuSet( "ui_r_modified", "1" );
	CL_PresetFrame();
	MenuSet( "r_fbo", Value( "ui_r_fbo" ) );
	MenuSet( "ui_r_modified", "0" );	// updatevideosetup
	CL_PresetFrame();
	EXPECT_STREQ( Value( "r_fbo" ), "1" );
	EXPECT_STREQ( Value( "ui_r_fbo" ), "1" );
	EXPECT_STREQ( Value( "cl_preset" ), "custom" );
}

TEST_F(Presets, ANewUIModuleStartsWithoutPendingChanges) {
	CL_PresetUIStarted();
	MenuSet( "ui_r_fbo", "1" );
	MenuSet( "ui_r_modified", "1" );
	MenuSet( "ui_mvPresetPending", "ultra" );
	CL_PresetUIStarted();
	EXPECT_STREQ( Value( "ui_r_fbo" ), "0" );
	EXPECT_STREQ( Value( "ui_mvPresetPending" ), "" );
}

TEST_F(Presets, ACopyCreatedFromTheConsoleIsTakenOver) {
	Cvar_Get( "ui_r_hdr", "1", CVAR_USER_CREATED );		// set ui_r_hdr 1
	CL_PresetUIStarted();
	EXPECT_STREQ( Value( "ui_r_hdr" ), "0" );
	EXPECT_EQ( Cvar_FindVar( "ui_r_hdr" )->flags & CVAR_USER_CREATED, 0 );
	EXPECT_NE( Cvar_FindVar( "ui_r_hdr" )->flags & CVAR_ROM, 0 );
}

TEST_F(Presets, APresetSetsTheAdvancedPageCopiesToo) {
	CL_PresetUIStarted();
	MenuSet( "ui_r_modified", "1" );	// other changes wait on a video page
	Preset( "ultra" );
	EXPECT_STREQ( Value( "ui_r_fbo" ), "1" );
	EXPECT_STREQ( Value( "ui_r_bloom" ), "1" );
	EXPECT_STREQ( Value( "ui_r_DynamicGlowWidth" ), "0" );
}

// APPLY CHANGES runs "preset pending" before its vid_restart
TEST_F(Presets, PresetPendingRunsThePresetPickedInTheMenu) {
	MenuSet( "ui_mvPresetPending", "ultra" );
	Preset( "pending" );
	EXPECT_STREQ( Value( "ui_mvPresetPending" ), "" );
	EXPECT_STREQ( Value( "r_fbo" ), "1" );
	EXPECT_STREQ( Value( "cl_preset" ), "ultra" );
	CL_PresetRestarted();		// the menu's vid_restart
	CL_PresetFrame();
	EXPECT_EQ( Restarts(), 0 );
}

TEST_F(Presets, PresetPendingWithoutAPickChangesNothing) {
	Preset( "pending" );					// no menu yet
	EXPECT_EQ( Cvar_FindVar( "ui_mvPresetPending" ), nullptr );
	for ( const char *none : { "", "none", "custom", "pending" } ) {
		MenuSet( "ui_mvPresetPending", none );
		Preset( "pending" );
		EXPECT_STREQ( Value( "ui_mvPresetPending" ), "" );
	}
	CL_PresetFrame();
	EXPECT_EQ( Restarts(), 0 );
	for ( const auto &r : registered ) {
		EXPECT_STREQ( Value( r.name ), r.value ) << r.name;
	}
	EXPECT_STREQ( Value( "cl_preset" ), "classic" );
}

// the preset row of the video page shows ui_mvPreset
TEST_F(Presets, ThePresetRowShowsTheSettingsUnlessAPresetIsPicked) {
	MenuSet( "ui_mvPreset", "classic" );			// the video page opens
	MenuSet( "ui_mvPresetPending", "none" );
	Preset( "enhanced" );						// from the console
	CL_PresetFrame();
	EXPECT_STREQ( Value( "ui_mvPreset" ), "enhanced" );
	MenuSet( "ui_mvPreset", "ultra" );			// picked on the row
	MenuSet( "ui_mvPresetPending", "ultra" );
	Cvar_Set( "r_picmip", "1" );
	CL_PresetFrame();
	EXPECT_STREQ( Value( "cl_preset" ), "custom" );
	EXPECT_STREQ( Value( "ui_mvPreset" ), "ultra" );
	MenuSet( "ui_mvPresetPending", "none" );		// Discard
	CL_PresetFrame();
	EXPECT_STREQ( Value( "ui_mvPreset" ), "custom" );
}

TEST_F(Presets, UnknownNamesChangeNothing) {
	Preset( "nonsense" );
	Preset( nullptr );
	EXPECT_STREQ( Value( "r_picmip" ), "1" );
	EXPECT_STREQ( Value( "cl_preset" ), "classic" );
}

} // namespace
