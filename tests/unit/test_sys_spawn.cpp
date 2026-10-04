// test_sys_spawn.cpp - helper process spawning used by video_mp4
//
// Links the real engine source (src/sys/sys_spawn.cpp). On POSIX the tests use
// standard Unix tools. On Windows they start test_sys_spawn_helper, built next
// to this program, which reports what it got; test_sys_spawn_winxp runs them
// again without the handle list, as on Windows XP (SYS_SPAWN_NO_HANDLE_LIST).

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
#include "sys/sys_spawn.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string TempPath( const char *name ) {
#ifdef _WIN32
	// with the process id: test_sys_spawn_winxp may run at the same time
	char		dir[MAX_PATH + 1];
	const DWORD	length = GetTempPathA( sizeof( dir ), dir );

	return std::string( dir, length < sizeof( dir ) ? length : 0 ) + "jk2mv_spawn_" +
		std::to_string( (unsigned long)GetCurrentProcessId() ) + "_" + name;
#else
	const char *dir = getenv( "TMPDIR" );
	return std::string( dir ? dir : "/tmp" ) + "/jk2mv_spawn_" + name;
#endif
}

std::string FileContents( const std::string &path ) {
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

#ifdef _WIN32

// both programs register their tests with ctest, under different names
#ifdef SYS_SPAWN_NO_HANDLE_LIST
#define SPAWN_SUITE SysSpawnWinXP
#else
#define SPAWN_SUITE SysSpawn
#endif

namespace {

// test_sys_spawn_helper.exe, built next to this program
std::string Helper() {
	char		path[1024];
	const DWORD	length = GetModuleFileNameA( NULL, path, sizeof( path ) );
	const std::string self( path, length < sizeof( path ) ? length : 0 );

	return self.substr( 0, self.find_last_of( "\\/" ) + 1 ) + "test_sys_spawn_helper.exe";
}

// the arguments the helper wrote, each ended by a '\0'
std::vector<std::string> SplitArgs( const std::string &text ) {
	std::vector<std::string> args;
	size_t start = 0, end;

	while ( ( end = text.find( '\0', start ) ) != std::string::npos ) {
		args.push_back( text.substr( start, end - start ) );
		start = end + 1;
	}
	return args;
}

// the helper must get exactly these arguments, as CommandLineToArgvW (ffmpeg's
// parser) and the C runtime read its command line
void ExpectArgumentsArrive( const std::string &program, const std::vector<std::string> &args ) {
	const std::string log = TempPath( "args.log" );

	for ( const char *mode : { "args", "crtargs" } ) {
		std::vector<std::string> expected;
		std::vector<const char *> argv;

		expected.push_back( program );
		expected.push_back( mode );
		expected.insert( expected.end(), args.begin(), args.end() );
		for ( const std::string &arg : expected ) {
			argv.push_back( arg.c_str() );
		}
		argv.push_back( nullptr );

		ASSERT_EQ( Sys_RunProcess( argv.data(), log.c_str() ), 0 ) << mode;
		const std::vector<std::string> got = SplitArgs( FileContents( log ) );
		ASSERT_EQ( got.size(), expected.size() ) << mode;
		for ( size_t i = 0; i < got.size(); i++ ) {
			EXPECT_EQ( got[i], expected[i] ) << mode << ", argument " << i;
		}
	}
	remove( log.c_str() );
}

// starts the program the way Windows XP does, with every inheritable handle;
// the arguments only get quotes around them
int RunInheritingEverything( const char * const *argv ) {
	std::string			cmd;
	STARTUPINFOA		si = { sizeof( si ) };
	PROCESS_INFORMATION	pi;
	DWORD				code = (DWORD)-1;

	for ( int i = 0; argv[i]; i++ ) {
		cmd += std::string( i ? " \"" : "\"" ) + argv[i] + "\"";
	}
	if ( !CreateProcessA( NULL, &cmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi ) ) {
		return -1;
	}
	WaitForSingleObject( pi.hProcess, INFINITE );
	GetExitCodeProcess( pi.hProcess, &code );
	CloseHandle( pi.hThread );
	CloseHandle( pi.hProcess );
	return (int)code;
}

} // namespace

TEST(SPAWN_SUITE, RunProcessReturnsTheExitCode) {
	const std::string helper = Helper();
	// 259 is STILL_ACTIVE, which a process that is still running also reports
	const char *codes[] = { "0", "1", "42", "259" };

	for ( const char *code : codes ) {
		const char *argv[] = { helper.c_str(), "exit", code, nullptr };
		EXPECT_EQ( Sys_RunProcess( argv ), atoi( code ) ) << code;
	}
}

TEST(SPAWN_SUITE, MissingProgramFails) {
	const char *argv[] = { "jk2mv-no-such-program", nullptr };
	EXPECT_EQ( Sys_RunProcess( argv ), -1 );
	EXPECT_EQ( Sys_SpawnPipe( argv ), nullptr );
}

TEST(SPAWN_SUITE, ProgramIsFoundInTheGamesDirectory) {
	// no directory and no .exe, like "ffmpeg"
	const char *argv[] = { "test_sys_spawn_helper", "exit", "7", nullptr };
	EXPECT_EQ( Sys_RunProcess( argv ), 7 );
}

TEST(SPAWN_SUITE, ArgumentsArePassedVerbatimWithoutAShell) {
	// quotes, backslashes (doubled only before a quote) and cmd.exe syntax
	const char *args[] = {
		"plain", "with space", "tab\there", "new\nline", "", "\"", "\"\"", "quote\"inside", "\"quoted\"",
		"trailing\\", "trailing space\\", "trailing\\\\", "two space\\\\", "back\\slash", "back\\\\slash",
		"back\\\"quote", "back\\\\\"quote", "C:\\Program Files\\JK2\\videos\\a b.mp4", "\\\\server\\share\\",
		"%PATH%", "^&|<>()!", " both ends ", "-vf", "crop=trunc(iw/2)*2:trunc(ih/2)*2,scale=out_range=tv", "",
	};

	ExpectArgumentsArrive( Helper(), std::vector<std::string>( std::begin( args ), std::end( args ) ) );
}

TEST(SPAWN_SUITE, EveryShortMixOfQuotesBackslashesAndBlanksArrives) {
	// all 780 strings of 1 to 4 of these characters, in one command line
	const char					alphabet[] = { 'a', ' ', '\t', '"', '\\' };
	std::vector<std::string>	args, shorter( 1 );

	for ( int length = 1; length <= 4; length++ ) {
		std::vector<std::string> longer;
		for ( const std::string &s : shorter ) {
			for ( char c : alphabet ) {
				longer.push_back( s + c );
			}
		}
		args.insert( args.end(), longer.begin(), longer.end() );
		shorter.swap( longer );
	}
	ASSERT_EQ( args.size(), 780u );
	ExpectArgumentsArrive( Helper(), args );
}

TEST(SPAWN_SUITE, ProgramPathWithSpaces) {
	// like a game in C:\Program Files (x86)
	const std::string dir = TempPath( "dir with spaces" );
	const std::string program = dir + "\\test_sys_spawn_helper.exe";

	CreateDirectoryA( dir.c_str(), NULL );
	ASSERT_TRUE( CopyFileA( Helper().c_str(), program.c_str(), FALSE ) );
	ExpectArgumentsArrive( program, std::vector<std::string>( 1, "a b" ) );
	DeleteFileA( program.c_str() );
	RemoveDirectoryA( dir.c_str() );
}

TEST(SPAWN_SUITE, PipeDeliversEveryByte) {
	const std::string helper = Helper();
	const std::string path = TempPath( "pipe.bin" );
	const char *argv[] = { helper.c_str(), "tee", path.c_str(), nullptr };
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
	// tee only sees the end of its input if no other program holds the pipe
	EXPECT_EQ( Sys_PipeClose( pipe ), 0 );
	const std::string got = FileContents( path );
	EXPECT_EQ( got.size(), expected.size() );
	EXPECT_TRUE( got == expected );
	remove( path.c_str() );
}

TEST(SPAWN_SUITE, WritingToAnExitedProgramFails) {
	const std::string helper = Helper();
	const char *argv[] = { helper.c_str(), "exit", "0", nullptr };
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

TEST(SPAWN_SUITE, RunProcessGivesAnEmptyInput) {
	const std::string helper = Helper();
	const std::string path = TempPath( "input.bin" );
	const char *argv[] = { helper.c_str(), "tee", path.c_str(), nullptr };

	// tee fails if its input is not a valid handle
	EXPECT_EQ( Sys_RunProcess( argv ), 0 );
	EXPECT_EQ( FileContents( path ), "" );
	remove( path.c_str() );
}

TEST(SPAWN_SUITE, OutputGoesToTheLog) {
	const std::string helper = Helper();
	const std::string log = TempPath( "out.log" );
	const char *argv[] = { helper.c_str(), "outerr", nullptr };

	for ( int piped = 0; piped < 2; piped++ ) {
		// the log is replaced, not appended to
		FILE *f = fopen( log.c_str(), "wb" );
		ASSERT_NE( f, nullptr );
		fputs( "stale", f );
		fclose( f );

		if ( piped ) {
			sysPipe_t *pipe = Sys_SpawnPipe( argv, log.c_str() );
			ASSERT_NE( pipe, nullptr );
			EXPECT_EQ( Sys_PipeClose( pipe ), 0 );
		} else {
			EXPECT_EQ( Sys_RunProcess( argv, log.c_str() ), 0 );
		}
		const std::string text = FileContents( log );
		EXPECT_NE( text.find( "out" ), std::string::npos ) << piped << ": " << text;
		EXPECT_NE( text.find( "err" ), std::string::npos ) << piped << ": " << text;
		EXPECT_EQ( text.find( "stale" ), std::string::npos ) << piped << ": " << text;
	}
	remove( log.c_str() );
}

TEST(SPAWN_SUITE, HandlesTheProgramInherits) {
	// a file the game has open, inheritable like its Winsock socket and the
	// files of the C runtime
	SECURITY_ATTRIBUTES			sa = { sizeof( sa ), NULL, TRUE };
	const std::string			path = TempPath( "open.tmp" );
	const HANDLE				file = CreateFileA( path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, &sa, CREATE_ALWAYS,
									FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL );
	BY_HANDLE_FILE_INFORMATION	info;
	char						handle[32], volume[16], high[16], low[16];

	ASSERT_NE( file, INVALID_HANDLE_VALUE );
	ASSERT_TRUE( GetFileInformationByHandle( file, &info ) );
	snprintf( handle, sizeof( handle ), "%lx", (unsigned long)(ULONG_PTR)file );
	snprintf( volume, sizeof( volume ), "%lx", info.dwVolumeSerialNumber );
	snprintf( high, sizeof( high ), "%lx", info.nFileIndexHigh );
	snprintf( low, sizeof( low ), "%lx", info.nFileIndexLow );

	const std::string helper = Helper();
	const std::string log = TempPath( "inherit.log" );
	const char *argv[] = { helper.c_str(), "inherited", handle, volume, high, low, nullptr };

	// the helper does find the file when it gets it
	EXPECT_EQ( RunInheritingEverything( argv ), 1 );
#ifdef SYS_SPAWN_NO_HANDLE_LIST
	// Windows XP: every inheritable handle, as before
	EXPECT_EQ( Sys_RunProcess( argv ), 1 );
#else
	// only the standard handles: NUL alone, NUL and a log, a pipe and a log
	EXPECT_EQ( Sys_RunProcess( argv ), 0 );
	EXPECT_EQ( Sys_RunProcess( argv, log.c_str() ), 0 );
	sysPipe_t *pipe = Sys_SpawnPipe( argv, log.c_str() );
	ASSERT_NE( pipe, nullptr );
	EXPECT_EQ( Sys_PipeClose( pipe ), 0 );
#endif
	CloseHandle( file );
	remove( log.c_str() );
}

#else // POSIX

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
	EXPECT_EQ( FileContents( path ), expected );
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

TEST(SysSpawn, OutputGoesToTheLog) {
	const std::string log = TempPath( "out.log" );
	const char *argv[] = { "sh", "-c", "echo out; echo err 1>&2", nullptr };
	EXPECT_EQ( Sys_RunProcess( argv, log.c_str() ), 0 );
	const std::string text = FileContents( log );
	EXPECT_NE( text.find( "out" ), std::string::npos ) << text;
	EXPECT_NE( text.find( "err" ), std::string::npos ) << text;
	remove( log.c_str() );
}

TEST(SysSpawn, ChildGetsOnlyTheStandardFiles) {
#if !defined( __GLIBC__ ) || __GLIBC__ < 2 || ( __GLIBC__ == 2 && __GLIBC_MINOR__ < 34 ) || !defined( __linux__ )
	GTEST_SKIP() << "needs posix_spawn_file_actions_addclosefrom_np and /proc";
#endif
	// a file the game has open must not leak into the program
	FILE *open = tmpfile();
	ASSERT_NE( open, nullptr );
	const std::string log = TempPath( "fds.log" );
	const char *argv[] = { "sh", "-c", "ls /proc/self/fd | wc -l", nullptr };
	ASSERT_EQ( Sys_RunProcess( argv, log.c_str() ), 0 );
	// 0, 1, 2 and the directory ls itself reads
	EXPECT_LE( atoi( FileContents( log ).c_str() ), 4 ) << FileContents( log );
	fclose( open );
	remove( log.c_str() );
}

#endif
