// sys_spawn.cpp -- start helper programs without a shell (see sys_spawn.h)

#include "sys_spawn.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <windows.h>
#include <string>

struct sysPipe_s {
	HANDLE	process;
	HANDLE	input;
};

// quotes an argument the way the Microsoft C runtime parses command lines
static void AppendArgument( std::string &cmd, const char *arg ) {
	if ( !cmd.empty() ) {
		cmd += ' ';
	}
	if ( *arg && !strpbrk( arg, " \t\n\v\"" ) ) {
		cmd += arg;
		return;
	}

	cmd += '"';
	for ( const char *p = arg; ; p++ ) {
		size_t backslashes = 0;

		while ( *p == '\\' ) {
			p++;
			backslashes++;
		}
		if ( !*p ) {
			// double them so they don't escape the closing quote
			cmd.append( backslashes * 2, '\\' );
			break;
		}
		if ( *p == '"' ) {
			cmd.append( backslashes * 2 + 1, '\\' );
		} else {
			cmd.append( backslashes, '\\' );
		}
		cmd += *p;
	}
	cmd += '"';
}

static HANDLE Spawn( const char * const *argv, HANDLE input ) {
	std::string			cmd;
	SECURITY_ATTRIBUTES	sa = { sizeof( sa ), NULL, TRUE };
	STARTUPINFOA		si;
	PROCESS_INFORMATION	pi;
	HANDLE				nul = CreateFileA( "NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
							&sa, OPEN_EXISTING, 0, NULL );

	for ( int i = 0; argv[i]; i++ ) {
		AppendArgument( cmd, argv[i] );
	}

	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = input ? input : nul;
	si.hStdOutput = nul;
	si.hStdError = nul;

	std::string	writable( cmd );
	const BOOL	ok = CreateProcessA( NULL, &writable[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi );

	if ( nul != INVALID_HANDLE_VALUE ) {
		CloseHandle( nul );
	}
	if ( !ok ) {
		return NULL;
	}
	CloseHandle( pi.hThread );
	return pi.hProcess;
}

static int Wait( HANDLE process ) {
	DWORD code = (DWORD)-1;

	WaitForSingleObject( process, INFINITE );
	if ( !GetExitCodeProcess( process, &code ) ) {
		code = (DWORD)-1;
	}
	CloseHandle( process );
	return (int)code;
}

sysPipe_t *Sys_SpawnPipe( const char * const *argv ) {
	SECURITY_ATTRIBUTES	sa = { sizeof( sa ), NULL, TRUE };
	HANDLE				readEnd, writeEnd;

	if ( !CreatePipe( &readEnd, &writeEnd, &sa, 0 ) ) {
		return NULL;
	}
	// only the child's end is inherited
	SetHandleInformation( writeEnd, HANDLE_FLAG_INHERIT, 0 );

	HANDLE process = Spawn( argv, readEnd );
	CloseHandle( readEnd );
	if ( !process ) {
		CloseHandle( writeEnd );
		return NULL;
	}

	sysPipe_t *pipe = (sysPipe_t *)malloc( sizeof( sysPipe_t ) );
	pipe->process = process;
	pipe->input = writeEnd;
	return pipe;
}

bool Sys_PipeWrite( sysPipe_t *pipe, const void *data, size_t length ) {
	const char *p = (const char *)data;

	while ( length > 0 ) {
		DWORD written = 0;

		if ( !WriteFile( pipe->input, p, (DWORD)( length > 0x10000000 ? 0x10000000 : length ), &written, NULL ) || !written ) {
			return false;
		}
		p += written;
		length -= written;
	}
	return true;
}

int Sys_PipeClose( sysPipe_t *pipe ) {
	CloseHandle( pipe->input );
	const int code = Wait( pipe->process );
	free( pipe );
	return code;
}

int Sys_RunProcess( const char * const *argv ) {
	HANDLE process = Spawn( argv, NULL );

	return process ? Wait( process ) : -1;
}

#else // POSIX

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

struct sysPipe_s {
	pid_t	pid;
	int		input;
};

static bool Spawn( const char * const *argv, const int pipeFds[2], pid_t *pid ) {
	posix_spawn_file_actions_t actions;

	posix_spawn_file_actions_init( &actions );
	if ( pipeFds ) {
		posix_spawn_file_actions_adddup2( &actions, pipeFds[0], STDIN_FILENO );
		posix_spawn_file_actions_addclose( &actions, pipeFds[0] );
		posix_spawn_file_actions_addclose( &actions, pipeFds[1] );
	} else {
		posix_spawn_file_actions_addopen( &actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0 );
	}
	posix_spawn_file_actions_addopen( &actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0 );
	posix_spawn_file_actions_addopen( &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0 );

	const int err = posix_spawnp( pid, argv[0], &actions, NULL, (char * const *)argv, environ );
	posix_spawn_file_actions_destroy( &actions );
	return err == 0;
}

static int Wait( pid_t pid ) {
	int status;

	while ( waitpid( pid, &status, 0 ) < 0 ) {
		if ( errno != EINTR ) {
			return -1;
		}
	}
	// a failed exec in the child reports 127
	return WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
}

sysPipe_t *Sys_SpawnPipe( const char * const *argv ) {
	int		fds[2];
	pid_t	pid;

	// a write to a program that exited must fail, not kill the game
	signal( SIGPIPE, SIG_IGN );

	if ( pipe( fds ) < 0 ) {
		return NULL;
	}
	fcntl( fds[1], F_SETFD, FD_CLOEXEC );

	if ( !Spawn( argv, fds, &pid ) ) {
		close( fds[0] );
		close( fds[1] );
		return NULL;
	}
	close( fds[0] );

	sysPipe_t *p = (sysPipe_t *)malloc( sizeof( sysPipe_t ) );
	p->pid = pid;
	p->input = fds[1];
	return p;
}

bool Sys_PipeWrite( sysPipe_t *p, const void *data, size_t length ) {
	const char *d = (const char *)data;

	while ( length > 0 ) {
		const ssize_t written = write( p->input, d, length );

		if ( written < 0 ) {
			if ( errno == EINTR ) {
				continue;
			}
			return false;
		}
		d += written;
		length -= (size_t)written;
	}
	return true;
}

int Sys_PipeClose( sysPipe_t *p ) {
	close( p->input );
	const int code = Wait( p->pid );
	free( p );
	return code;
}

int Sys_RunProcess( const char * const *argv ) {
	pid_t pid;

	if ( !Spawn( argv, NULL, &pid ) ) {
		return -1;
	}
	return Wait( pid );
}

#endif
