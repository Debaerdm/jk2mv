/*
Preloaded by client_smoke.py: reports the GL calls made with no current GL
context (GLX or EGL), which the GL spec leaves undefined; macOS crashes on
them. Only the calls the renderer makes on its way down are watched.
*/

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLsizei;

typedef void *( *currentContext_t )( void );

// SDL opens libEGL as a local library: look in it too when it's loaded
static currentContext_t CurrentContextCall( const char *name, const char *library ) {
	void	*call = dlsym( RTLD_DEFAULT, name );

	if ( !call ) {
		void	*handle = dlopen( library, RTLD_LAZY | RTLD_NOLOAD );

		if ( handle ) {
			call = dlsym( handle, name );
			dlclose( handle );
		}
	}
	return (currentContext_t)call;
}

static int HasContext( void ) {
	currentContext_t	glxCurrent = CurrentContextCall( "glXGetCurrentContext", "libGLX.so.0" );
	currentContext_t	eglCurrent = CurrentContextCall( "eglGetCurrentContext", "libEGL.so.1" );

	return ( glxCurrent && glxCurrent() ) || ( eglCurrent && eglCurrent() );
}

static void Report( const char *call ) {
	fprintf( stderr, "GL call without a context: %s\n", call );
	fflush( stderr );
}

void glBindTexture( GLenum target, GLuint texture ) {
	static void ( *real )( GLenum, GLuint );

	if ( !HasContext() ) {
		Report( "glBindTexture" );
		return;
	}
	if ( !real ) {
		real = (void ( * )( GLenum, GLuint ))dlsym( RTLD_NEXT, "glBindTexture" );
	}
	real( target, texture );
}

void glDeleteTextures( GLsizei n, const GLuint *textures ) {
	static void ( *real )( GLsizei, const GLuint * );

	if ( !HasContext() ) {
		Report( "glDeleteTextures" );
		return;
	}
	if ( !real ) {
		real = (void ( * )( GLsizei, const GLuint * ))dlsym( RTLD_NEXT, "glDeleteTextures" );
	}
	real( n, textures );
}
