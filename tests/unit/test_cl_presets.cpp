// test_cl_presets.cpp - visual presets, cl_preset and the setup menu copies
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

// Cvar_Set is a forced set: no latching, a pending value is dropped
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

class Presets : public ::testing::Test {
protected:
	void SetUp() override {
		cvars.clear();
		epoch += 10000;
		executed.clear();
		presetCommand = nullptr;
		cls.rendererStarted = qtrue;
		CL_PresetRestarted();
		CL_InitPresets();
		// the registrations of the renderer and the client
		Register( "r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST" );
		Register( "r_ext_texture_filter_anisotropic", "2" );
		Register( "r_ext_multisample", "0", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "r_ext_alphaToCoverage", "0" );
		Register( "r_DynamicGlow", "0" );
		Register( "r_DynamicGlowWidth", "320", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "r_DynamicGlowHeight", "240", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "r_subdivisions", "4", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "r_lodCurveError", "250" );
		Register( "r_dlightMode", "0" );
		Register( "r_fbo", "0", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "r_hdr", "0", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "r_bloom", "0", CVAR_ARCHIVE | CVAR_LATCH );
		Register( "cl_autolodscale", "1" );
		Register( "r_swapInterval", "0" );
		Register( "r_maxFrameLatency", "0" );
		Register( "cl_fovAspectFix", "0" );
		Register( "cl_aviFrameRate", "30" );
		Register( "cl_aviMotionJpegQuality", "90" );
		Register( "cl_aviMotionBlur", "0" );
		CL_PresetFrame();
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

TEST_F(Presets, UnregisteredCvarsKeepTheirEngineDefault) {
	cvars.erase( cvars.begin() + 1 );	// r_picmip, not registered yet (autoexec.cfg)
	ASSERT_EQ( Cvar_FindVar( "r_picmip" ), nullptr );
	Preset( "enhanced" );
	ASSERT_NE( Cvar_FindVar( "r_picmip" ), nullptr );
	EXPECT_NE( Cvar_FindVar( "r_picmip" )->flags & CVAR_USER_CREATED, 0 );
	Register( "r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH );
	EXPECT_STREQ( Value( "r_picmip" ), "0" );
	EXPECT_STREQ( Cvar_FindVar( "r_picmip" )->resetString, "1" );
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

TEST_F(Presets, ANewMenuModuleStartsWithoutAPendingPreset) {
	CL_PresetUIStarted();
	EXPECT_EQ( Cvar_FindVar( "ui_presetPending" ), nullptr );
	Cvar_Set( "ui_presetPending", "ultra" );
	CL_PresetUIStarted();
	EXPECT_STREQ( Value( "ui_presetPending" ), "" );
}

TEST_F(Presets, UnknownNamesChangeNothing) {
	Preset( "nonsense" );
	Preset( nullptr );
	EXPECT_STREQ( Value( "r_picmip" ), "1" );
	EXPECT_STREQ( Value( "cl_preset" ), "classic" );
}

} // namespace
