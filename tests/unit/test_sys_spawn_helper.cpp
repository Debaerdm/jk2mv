// test_sys_spawn_helper.cpp - the program test_sys_spawn starts on Windows
//
//   exit <code>     exits with <code>
//   args ...        writes its arguments, each ended by a '\0', to its standard
//                   output, as CommandLineToArgvW splits its command line (the
//                   way ffmpeg reads it)
//   crtargs ...     the same with the C runtime's argv
//   tee <file>      copies its standard input to <file>
//   outerr          writes "out" to its standard output and "err" to its
//                   standard error
//   inherited <handle> <volume> <index high> <index low>
//                   exits with 1 if <handle> is open on the file with this
//                   volume serial number and file index, else 0 (all in hex)
//
// Other failures exit with 100 and up.

#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool WriteOut( const void *data, DWORD length ) {
	DWORD written = 0;

	return WriteFile( GetStdHandle( STD_OUTPUT_HANDLE ), data, length, &written, NULL ) && written == length;
}

static int PrintCommandLine( void ) {
	int		count = 0;
	LPWSTR	*args = CommandLineToArgvW( GetCommandLineW(), &count );
	int		code = 0;

	if ( !args ) {
		return 101;
	}
	for ( int i = 0; i < count && !code; i++ ) {
		// back to the code page CreateProcessA converted them from, '\0' included
		char		arg[8192];
		const int	length = WideCharToMultiByte( CP_ACP, 0, args[i], -1, arg, sizeof( arg ), NULL, NULL );

		if ( length <= 0 || !WriteOut( arg, (DWORD)length ) ) {
			code = 102;
		}
	}
	LocalFree( args );
	return code;
}

static int PrintCrtArgs( int argc, char **argv ) {
	for ( int i = 0; i < argc; i++ ) {
		if ( !WriteOut( argv[i], (DWORD)strlen( argv[i] ) + 1 ) ) {
			return 102;
		}
	}
	return 0;
}

static int Tee( const char *path ) {
	const HANDLE	input = GetStdHandle( STD_INPUT_HANDLE );
	const HANDLE	file = CreateFileA( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	static char		buffer[65536];
	int				code = 0;

	if ( file == INVALID_HANDLE_VALUE ) {
		return 103;
	}
	for ( ;; ) {
		DWORD length = 0, written = 0;

		if ( !ReadFile( input, buffer, sizeof( buffer ), &length, NULL ) ) {
			// how a pipe ends; anything else means a bad input handle
			if ( GetLastError() != ERROR_BROKEN_PIPE ) {
				code = 104;
			}
			break;
		}
		if ( !length ) {
			break;	// how NUL ends
		}
		if ( !WriteFile( file, buffer, length, &written, NULL ) || written != length ) {
			code = 105;
			break;
		}
	}
	CloseHandle( file );
	return code;
}

static int Inherited( char **argv ) {
	const HANDLE				handle = (HANDLE)(ULONG_PTR)strtoul( argv[0], NULL, 16 );
	BY_HANDLE_FILE_INFORMATION	info;

	if ( !GetFileInformationByHandle( handle, &info ) ) {
		return 0;
	}
	return info.dwVolumeSerialNumber == strtoul( argv[1], NULL, 16 ) && info.nFileIndexHigh == strtoul( argv[2], NULL, 16 )
		&& info.nFileIndexLow == strtoul( argv[3], NULL, 16 );
}

int main( int argc, char **argv ) {
	if ( argc >= 3 && !strcmp( argv[1], "exit" ) ) {
		return atoi( argv[2] );
	}
	if ( argc >= 2 && !strcmp( argv[1], "args" ) ) {
		return PrintCommandLine();
	}
	if ( argc >= 2 && !strcmp( argv[1], "crtargs" ) ) {
		return PrintCrtArgs( argc, argv );
	}
	if ( argc >= 3 && !strcmp( argv[1], "tee" ) ) {
		return Tee( argv[2] );
	}
	if ( argc >= 2 && !strcmp( argv[1], "outerr" ) ) {
		fputs( "out\n", stdout );
		fflush( stdout );
		fputs( "err\n", stderr );
		return 0;
	}
	if ( argc >= 6 && !strcmp( argv[1], "inherited" ) ) {
		return Inherited( argv + 2 );
	}
	return 100;
}
