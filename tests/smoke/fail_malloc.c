/*
Preloaded by client_smoke.py: malloc fails for the sizes between
FAIL_MALLOC_MIN and FAIL_MALLOC_MAX bytes, like a 32 bit process that can't
find room for a capture buffer as big as the frame. glibc only.
*/

#include <errno.h>
#include <stdlib.h>

extern void *__libc_malloc( size_t size );

static size_t	failMin, failMax;
static int		initialized;

static size_t EnvSize( const char *name ) {
	const char	*value = getenv( name );

	return value ? (size_t)strtoull( value, NULL, 10 ) : 0;
}

void *malloc( size_t size ) {
	if ( !initialized ) {
		initialized = 1;
		failMin = EnvSize( "FAIL_MALLOC_MIN" );
		failMax = EnvSize( "FAIL_MALLOC_MAX" );
	}
	if ( failMin && size >= failMin && size <= failMax ) {
		errno = ENOMEM;
		return NULL;
	}
	return __libc_malloc( size );
}
