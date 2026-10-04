// test_sys_spawn.cpp - helper process spawning used by video_mp4
//
// Links the real engine source (src/sys/sys_spawn.cpp). POSIX only: the
// tests use standard Unix tools.

#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <algorithm>
#include "sys/sys_spawn.h"

namespace {

std::string TempPath( const char *name ) {
	const char *dir = getenv( "TMPDIR" );
	return std::string( dir ? dir : "/tmp" ) + "/jk2mv_spawn_" + name;
}

std::string ReadFile( const std::string &path ) {
	std::string data;
	FILE *f = fopen( path.c_str(), "rb" );
	if ( f ) {
		char buf[4096];
		size_t n;
		while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 ) {
			data.append( buf, n );
		}
		fclose( f );
	}
	return data;
}

} // namespace

TEST(SysSpawn, RunProcessReturnsTheExitCode) {
	const char *ok[] = { "true", nullptr };
	const char *fail[] = { "false", nullptr };
	EXPECT_EQ( Sys_RunProcess( ok ), 0 );
	EXPECT_EQ( Sys_RunProcess( fail ), 1 );
}

TEST(SysSpawn, MissingProgramFails) {
	const char *argv[] = { "jk2mv-no-such-program", nullptr };
	const int code = Sys_RunProcess( argv );
	// -1 when posix_spawnp reports it, 127 from the child on older C libraries
	EXPECT_TRUE( code == -1 || code == 127 ) << code;
}

TEST(SysSpawn, ArgumentsArePassedVerbatimWithoutAShell) {
	// spaces, quotes and shell syntax must reach the program untouched
	const char *argv[] = { "sh", "-c", "test \"$1\" = 'a b\"c; $(x) *'", "sh", "a b\"c; $(x) *", nullptr };
	EXPECT_EQ( Sys_RunProcess( argv ), 0 );
}

TEST(SysSpawn, PipeDeliversEveryByte) {
	const std::string path = TempPath( "pipe.bin" );
	const char *argv[] = { "tee", path.c_str(), nullptr };
	sysPipe_t *pipe = Sys_SpawnPipe( argv );
	ASSERT_NE( pipe, nullptr );

	std::string expected;
	for ( int i = 0; i < 256 * 1024; i++ ) {
		expected += (char)( i * 7 );
	}
	// several writes, larger than a pipe buffer
	for ( size_t ofs = 0; ofs < expected.size(); ofs += 50000 ) {
		const size_t n = std::min<size_t>( 50000, expected.size() - ofs );
		ASSERT_TRUE( Sys_PipeWrite( pipe, expected.data() + ofs, n ) );
	}
	EXPECT_EQ( Sys_PipeClose( pipe ), 0 );
	EXPECT_EQ( ReadFile( path ), expected );
	remove( path.c_str() );
}

TEST(SysSpawn, WritingToAnExitedProgramFailsWithoutKillingUs) {
	const char *argv[] = { "true", nullptr };
	sysPipe_t *pipe = Sys_SpawnPipe( argv );
	ASSERT_NE( pipe, nullptr );

	std::string block( 65536, 'x' );
	bool failed = false;
	for ( int i = 0; i < 100 && !failed; i++ ) {
		failed = !Sys_PipeWrite( pipe, block.data(), block.size() );
	}
	EXPECT_TRUE( failed );
	EXPECT_EQ( Sys_PipeClose( pipe ), 0 );
}
