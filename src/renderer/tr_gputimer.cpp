// tr_gputimer.cpp -- GPU time of the frame and of some passes (r_gpuTimers)
//
// GL timestamps (ARB_timer_query) at the start and end of each frame and
// around the dynamic glow and the post-process passes. Results are read four
// frames later, when the GPU is done with them, so nothing waits for it.
// The client shows them in the perf overlay and records them in benchmarks.

#include "tr_local.h"

cvar_t	*r_gpuTimers;

#define GPU_FRAMES		4		// frames in flight
#define GPU_MAX_PAIRS	8		// timed occurrences of a section in a frame

static PFNGLGENQUERIESPROC				qglGenQueries;
static PFNGLDELETEQUERIESPROC			qglDeleteQueries;
static PFNGLQUERYCOUNTERPROC			qglQueryCounter;
static PFNGLGETQUERYOBJECTIVPROC		qglGetQueryObjectiv;
static PFNGLGETQUERYOBJECTUI64VPROC		qglGetQueryObjectui64v;

typedef struct {
	GLuint		frame[2];
	GLuint		section[GPU_SECTIONS][GPU_MAX_PAIRS * 2];
	int			pairs[GPU_SECTIONS];
	int			open[GPU_SECTIONS];		// section started, not ended
	qboolean	started, ended;
} gpuFrame_t;

static struct {
	qboolean	available;
	gpuFrame_t	frames[GPU_FRAMES];
	int			current;
	int			frameUsec;				// latest finished frame, -1 if none
	int			sectionUsec[GPU_SECTIONS];
} gpu;

void R_InitGPUTimers( void ) {
	int major = 1, minor = 0;

	r_gpuTimers = ri.Cvar_Get( "r_gpuTimers", "0", CVAR_ARCHIVE | CVAR_GLOBAL );
	Com_Memset( &gpu, 0, sizeof( gpu ) );
	gpu.frameUsec = -1;

	sscanf( glConfig.version_string, "%d.%d", &major, &minor );
	if ( !GL_CheckForExtension( "GL_ARB_timer_query" ) && ( major < 3 || ( major == 3 && minor < 3 ) ) ) {
		return;
	}
	qglGenQueries = (PFNGLGENQUERIESPROC)WIN_GL_GetProcAddress( "glGenQueries" );
	qglDeleteQueries = (PFNGLDELETEQUERIESPROC)WIN_GL_GetProcAddress( "glDeleteQueries" );
	qglQueryCounter = (PFNGLQUERYCOUNTERPROC)WIN_GL_GetProcAddress( "glQueryCounter" );
	qglGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC)WIN_GL_GetProcAddress( "glGetQueryObjectiv" );
	qglGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC)WIN_GL_GetProcAddress( "glGetQueryObjectui64v" );
	if ( !qglGenQueries || !qglDeleteQueries || !qglQueryCounter || !qglGetQueryObjectiv || !qglGetQueryObjectui64v ) {
		return;
	}

	for ( int i = 0; i < GPU_FRAMES; i++ ) {
		qglGenQueries( 2, gpu.frames[i].frame );
		qglGenQueries( GPU_SECTIONS * GPU_MAX_PAIRS * 2, gpu.frames[i].section[0] );
	}
	gpu.available = qtrue;
}

void R_ShutdownGPUTimers( void ) {
	if ( gpu.available ) {
		for ( int i = 0; i < GPU_FRAMES; i++ ) {
			qglDeleteQueries( 2, gpu.frames[i].frame );
			qglDeleteQueries( GPU_SECTIONS * GPU_MAX_PAIRS * 2, gpu.frames[i].section[0] );
		}
	}
	Com_Memset( &gpu, 0, sizeof( gpu ) );
	gpu.frameUsec = -1;
}

static qboolean R_GPUTimersOn( void ) {
	return (qboolean)( gpu.available && r_gpuTimers->integer );
}

static GLuint64 R_QueryTime( GLuint query ) {
	GLuint64 t = 0;

	qglGetQueryObjectui64v( query, GL_QUERY_RESULT, &t );
	return t;
}

// results of a frame the GPU has finished, read when its slot comes back
static void R_ReadGPUFrame( gpuFrame_t *f ) {
	GLint ready = 0;

	if ( !f->ended ) {
		return;
	}
	qglGetQueryObjectiv( f->frame[1], GL_QUERY_RESULT_AVAILABLE, &ready );
	if ( !ready ) {
		return;		// still busy after GPU_FRAMES frames: drop it rather than wait
	}
	gpu.frameUsec = (int)( ( R_QueryTime( f->frame[1] ) - R_QueryTime( f->frame[0] ) ) / 1000 );
	for ( int s = 0; s < GPU_SECTIONS; s++ ) {
		GLuint64 total = 0;

		for ( int p = 0; p < f->pairs[s]; p++ ) {
			total += R_QueryTime( f->section[s][p * 2 + 1] ) - R_QueryTime( f->section[s][p * 2] );
		}
		gpu.sectionUsec[s] = (int)( total / 1000 );
	}
}

// frame start (RB_DrawBuffer); a second call in the frame (stereo) is ignored
void R_GPUTimerFrameBegin( void ) {
	gpuFrame_t *f = &gpu.frames[gpu.current];

	if ( !R_GPUTimersOn() || ( f->started && !f->ended ) ) {
		return;
	}
	gpu.current = ( gpu.current + 1 ) % GPU_FRAMES;
	f = &gpu.frames[gpu.current];
	R_ReadGPUFrame( f );

	f->started = qtrue;
	f->ended = qfalse;
	Com_Memset( f->pairs, 0, sizeof( f->pairs ) );
	Com_Memset( f->open, 0, sizeof( f->open ) );
	qglQueryCounter( f->frame[0], GL_TIMESTAMP );
}

// before the buffer swap
void R_GPUTimerFrameEnd( void ) {
	gpuFrame_t *f = &gpu.frames[gpu.current];

	if ( !gpu.available || !f->started || f->ended ) {
		return;
	}
	qglQueryCounter( f->frame[1], GL_TIMESTAMP );
	f->ended = qtrue;
}

void R_GPUTimerBegin( gpuSection_t section ) {
	gpuFrame_t *f = &gpu.frames[gpu.current];

	if ( !gpu.available || !f->started || f->ended || f->open[section] || f->pairs[section] >= GPU_MAX_PAIRS ) {
		return;
	}
	qglQueryCounter( f->section[section][f->pairs[section] * 2], GL_TIMESTAMP );
	f->open[section] = 1;
}

void R_GPUTimerEnd( gpuSection_t section ) {
	gpuFrame_t *f = &gpu.frames[gpu.current];

	if ( !gpu.available || !f->open[section] ) {
		return;
	}
	qglQueryCounter( f->section[section][f->pairs[section] * 2 + 1], GL_TIMESTAMP );
	f->open[section] = 0;
	f->pairs[section]++;
}

/*
==================
RE_GetGPUTimes

GPU time of the latest finished frame in microseconds, with the glow and the
post-process passes (gamma, r_fbo effects) if the pointers are not NULL; -1
when r_gpuTimers is off or unsupported
==================
*/
int RE_GetGPUTimes( int *glowUsec, int *postUsec ) {
	if ( !R_GPUTimersOn() ) {
		return -1;
	}
	if ( glowUsec ) {
		*glowUsec = gpu.sectionUsec[GPU_GLOW];
	}
	if ( postUsec ) {
		*postUsec = gpu.sectionUsec[GPU_POST];
	}
	return gpu.frameUsec;
}
