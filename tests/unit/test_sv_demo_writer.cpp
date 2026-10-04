// test_sv_demo_writer.cpp - the files of the server demos (sv_demo_writer.cpp)
//
// Links the real writer: its thread creates, writes, closes and deletes real
// files in a folder of the current directory (one for each test), which
// stands for fs_homepath.

#include <gtest/gtest.h>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "server/server.h"

// ===========================================================================
// engine stubs
// ===========================================================================

void QDECL Com_Printf( const char *fmt, ... ) {
	va_list	ap;

	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
}

void QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	char	buf[4096];
	va_list	ap;

	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	throw std::runtime_error( buf );
}

static std::string	homePath;		// a folder of the current directory

const char *Cvar_VariableString( const char *name ) {
	static char	value[MAX_OSPATH];

	Q_strncpyz( value, strcmp( name, "fs_homepath" ) ? "" : homePath.c_str(), sizeof( value ) );
	return value;
}

// <base>/base/<qpath> with the separators of the system
char *FS_BuildOSPath( const char *base, const char *game, const char *qpath ) {
	static char	path[MAX_OSPATH];

	Com_sprintf( path, sizeof( path ), "%s/%s/%s", base, game && game[0] ? game : "base", qpath );
	for ( char *p = path; *p; p++ ) {
		if ( *p == '/' ) {
			*p = PATH_SEP;
		}
	}
	return path;
}

qboolean Sys_Mkdir( const char *path ) {
#ifdef _WIN32
	return (qboolean)( _mkdir( path ) == 0 || errno == EEXIST );
#else
	return (qboolean)( mkdir( path, 0750 ) == 0 || errno == EEXIST );
#endif
}

int Sys_Milliseconds( bool ) {
	return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch() ).count();
}

// ===========================================================================
// helpers
// ===========================================================================

static std::string OSPath( const std::string &qpath ) {
	return FS_BuildOSPath( homePath.c_str(), "", qpath.c_str() );
}

static bool Exists( const std::string &qpath ) {
	FILE	*f = fopen( OSPath( qpath ).c_str(), "rb" );

	if ( f ) {
		fclose( f );
	}
	return f != NULL;
}

static std::vector<unsigned char> ReadAll( const std::string &qpath ) {
	std::vector<unsigned char>	data;
	FILE						*f = fopen( OSPath( qpath ).c_str(), "rb" );
	unsigned char				buf[4096];
	size_t						n;

	if ( !f ) {
		ADD_FAILURE() << "no file " << qpath;
		return data;
	}
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 ) {
		data.insert( data.end(), buf, buf + n );
	}
	fclose( f );
	return data;
}

static std::vector<unsigned char> Bytes( size_t len, unsigned seed ) {
	std::vector<unsigned char>	data( len );

	for ( size_t i = 0; i < len; i++ ) {
		seed = seed * 1103515245u + 12345u;
		data[i] = (unsigned char)( seed >> 16 );
	}
	return data;
}

static void RemoveTree() {
	const char	*files[] = { "demos/a.dm_16", "demos/b.dm_16", "demos/c.dm_16", "demos/gone.dm_16",
		"demos/again.dm_16", "demos", "" };

	for ( int i = 0; files[i][0]; i++ ) {
		remove( OSPath( files[i] ).c_str() );
#ifdef _WIN32
		_rmdir( OSPath( files[i] ).c_str() );
#else
		rmdir( OSPath( files[i] ).c_str() );
#endif
	}
#ifdef _WIN32
	_rmdir( ( homePath + "\\base" ).c_str() );
	_rmdir( homePath.c_str() );
#else
	rmdir( ( homePath + "/base" ).c_str() );
	rmdir( homePath.c_str() );
#endif
}

class SvDemoWriter : public ::testing::Test {
protected:
	virtual void SetUp() {
		// a folder for each test: ctest runs them side by side
		homePath = std::string( "svdemo_writer_" ) + ::testing::UnitTest::GetInstance()->current_test_info()->name();
		RemoveTree();
	}
	virtual void TearDown() {
		SV_DemoFilesShutdown();
		RemoveTree();
	}
};

// ===========================================================================
// tests
// ===========================================================================

TEST_F( SvDemoWriter, WritesEveryByteInOrder ) {
	const char					*names[3] = { "demos/a.dm_16", "demos/b.dm_16", "demos/c.dm_16" };
	svDemoFile_t				*files[3];
	std::vector<unsigned char>	sent[3];

	for ( int i = 0; i < 3; i++ ) {
		files[i] = SV_DemoFileOpen( names[i] );
		ASSERT_TRUE( files[i] != NULL );
	}
	// interleaved writes of 1 byte to 40 KB, past the blocks the files hand over
	for ( int round = 0; round < 60; round++ ) {
		for ( int i = 0; i < 3; i++ ) {
			std::vector<unsigned char> chunk = Bytes( 1 + ( round * 7919 + i * 104729 ) % 40000, round * 3 + i );
			SV_DemoFileWrite( files[i], &chunk[0], (int)chunk.size() );
			sent[i].insert( sent[i].end(), chunk.begin(), chunk.end() );
		}
	}
	for ( int i = 0; i < 3; i++ ) {
		EXPECT_TRUE( SV_DemoFileInUse( names[i] ) );
		SV_DemoFileClose( files[i], qfalse );
	}
	SV_DemoFilesShutdown();

	for ( int i = 0; i < 3; i++ ) {
		EXPECT_FALSE( SV_DemoFileInUse( names[i] ) );
		std::vector<unsigned char> got = ReadAll( names[i] );
		ASSERT_EQ( got.size(), sent[i].size() ) << names[i];
		EXPECT_TRUE( got == sent[i] ) << names[i];
	}
}

TEST_F( SvDemoWriter, DiscardDeletesTheFile ) {
	svDemoFile_t	*file = SV_DemoFileOpen( "demos/gone.dm_16" );
	unsigned char	byte = 7;

	ASSERT_TRUE( file != NULL );
	SV_DemoFileWrite( file, &byte, 1 );
	SV_DemoFileClose( file, qtrue );
	SV_DemoFilesShutdown();
	EXPECT_FALSE( Exists( "demos/gone.dm_16" ) );
	EXPECT_FALSE( SV_DemoFileInUse( "demos/gone.dm_16" ) );
}

TEST_F( SvDemoWriter, InUseUntilTheThreadClosedIt ) {
	svDemoFile_t	*file = SV_DemoFileOpen( "demos/a.dm_16" );

	ASSERT_TRUE( file != NULL );
	EXPECT_TRUE( SV_DemoFileInUse( "demos/a.dm_16" ) );
	EXPECT_TRUE( SV_DemoFileInUse( "DEMOS/A.DM_16" ) );
	EXPECT_FALSE( SV_DemoFileInUse( "demos/b.dm_16" ) );
	SV_DemoFileClose( file, qfalse );
	SV_DemoFilesShutdown();
	EXPECT_FALSE( SV_DemoFileInUse( "demos/a.dm_16" ) );
	EXPECT_TRUE( Exists( "demos/a.dm_16" ) );
	EXPECT_TRUE( ReadAll( "demos/a.dm_16" ).empty() );
}

TEST_F( SvDemoWriter, AFileThatCantBeCreatedFails ) {
	// a file where the folder should be
	Sys_Mkdir( homePath.c_str() );
	Sys_Mkdir( ( homePath + PATH_SEP + "base" ).c_str() );
	FILE *blocker = fopen( OSPath( "demos" ).c_str(), "wb" );
	ASSERT_TRUE( blocker != NULL );
	fclose( blocker );

	svDemoFile_t	*file = SV_DemoFileOpen( "demos/a.dm_16" );
	unsigned char	byte = 7;

	ASSERT_TRUE( file != NULL );
	SV_DemoFileWrite( file, &byte, 1 );
	SV_DemoFilesShutdown();		// waits for the thread
	EXPECT_TRUE( SV_DemoFileFailed( file ) );
	SV_DemoFileClose( file, qfalse );
	SV_DemoFilesShutdown();
	EXPECT_FALSE( SV_DemoFileInUse( "demos/a.dm_16" ) );
}

TEST_F( SvDemoWriter, RefusesAHomePathThatGoesUp ) {
	const std::string	home = homePath;

	homePath = home + "/../elsewhere";
	EXPECT_TRUE( SV_DemoFileOpen( "demos/a.dm_16" ) == NULL );
	EXPECT_FALSE( SV_DemoFileInUse( "demos/a.dm_16" ) );
	homePath = home;
}

TEST_F( SvDemoWriter, StartsAgainAfterAShutdown ) {
	std::vector<unsigned char>	one = Bytes( 50000, 1 ), two = Bytes( 300, 2 );
	svDemoFile_t				*file;

	file = SV_DemoFileOpen( "demos/a.dm_16" );
	SV_DemoFileWrite( file, &one[0], (int)one.size() );
	SV_DemoFileClose( file, qfalse );
	SV_DemoFilesShutdown();

	file = SV_DemoFileOpen( "demos/again.dm_16" );
	SV_DemoFileWrite( file, &two[0], (int)two.size() );
	SV_DemoFileClose( file, qfalse );
	SV_DemoFilesShutdown();

	EXPECT_TRUE( ReadAll( "demos/a.dm_16" ) == one );
	EXPECT_TRUE( ReadAll( "demos/again.dm_16" ) == two );
}
