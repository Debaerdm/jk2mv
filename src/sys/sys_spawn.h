// sys_spawn.h -- start helper programs (video encoding with ffmpeg)
//
// No shell is involved: argv[0] is looked up in the PATH (and, on Windows, in
// the game's directory) and every argument is passed as is. The program's
// output goes to logPath when given (truncated first), else it is discarded.
// On POSIX the program gets only its standard handles and default signal
// handling. No engine dependency, so it can be unit tested.

#ifndef SYS_SPAWN_H
#define SYS_SPAWN_H

#include <stddef.h>

typedef struct sysPipe_s sysPipe_t;

// Starts argv[0] with a pipe to its standard input; argv ends with NULL.
// Returns NULL if it can't be started.
sysPipe_t	*Sys_SpawnPipe( const char * const *argv, const char *logPath = NULL );

// Returns false once the program is gone (closed its input or exited).
bool		Sys_PipeWrite( sysPipe_t *pipe, const void *data, size_t length );

// Closes the pipe, waits for the program and returns its exit code, or -1.
int			Sys_PipeClose( sysPipe_t *pipe );

// Runs argv[0] with no input, waits and returns its exit code, or -1 if it
// couldn't be started.
int			Sys_RunProcess( const char * const *argv, const char *logPath = NULL );

#endif // SYS_SPAWN_H
