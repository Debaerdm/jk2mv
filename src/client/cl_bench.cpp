// cl_bench.cpp -- benchmark command: timedemo runs with per frame times,
// percentiles and CSV output
//
// benchmark <demo> [runs=5] [warmup=1] [tag]
//
// Plays the demo warmup + runs times in timedemo mode without the usual
// 1000 fps cap, records the time of every frame and prints, per run and
// pooled over the measured runs, the average fps, frame time percentiles and
// the 1% low. benchmarks/<demo>_<tag>.csv gets every frame time and
// benchmarks/<demo>_<tag>.txt the summary with the system details.

#include "client.h"
#include <algorithm>
#include <string>
#include <vector>

#define BENCH_START_TIMEOUT	600		// frames to wait for a run to start

static struct {
	qboolean			active;
	char				demo[MAX_QPATH];
	char				tag[64];
	int					runs;
	int					warmup;
	int					current;		// run being played, warm-up included
	qboolean			pendingStart;
	int					waitFrames;		// frames since the run was requested
	qboolean			started;		// the run's demo is playing
	char				timedemo[16];	// restored at the end
	int64_t				lastFrameUsec;
	std::vector<int>	frames;			// current run, microseconds
	std::vector<int>	pooled;			// measured runs
	std::vector<int>	runOf;			// run of each pooled frame, for the CSV
	std::vector<double>	runFps;
	std::vector<std::string> runLines;
} bench;

typedef struct {
	int		frames;
	double	seconds;
	double	avgFps;
	double	p50, p90, p99, p999, max;	// frame times in ms
	double	low1, low01;				// average fps of the slowest 1% and 0.1% of frames
} benchStats_t;

static void CL_BenchStats( const std::vector<int> &frameUsec, benchStats_t *st ) {
	std::vector<int>	sorted( frameUsec );
	double				sum = 0.0;

	Com_Memset( st, 0, sizeof( *st ) );
	if ( sorted.empty() ) {
		return;
	}
	std::sort( sorted.begin(), sorted.end() );
	for ( size_t i = 0; i < sorted.size(); i++ ) {
		sum += sorted[i];
	}

	const size_t n = sorted.size();
	st->frames = (int)n;
	st->seconds = sum / 1e6;
	st->avgFps = st->seconds > 0.0 ? n / st->seconds : 0.0;
	st->p50 = sorted[n * 50 / 100] / 1000.0;
	st->p90 = sorted[n * 90 / 100] / 1000.0;
	st->p99 = sorted[n * 99 / 100] / 1000.0;
	st->p999 = sorted[n * 999 / 1000] / 1000.0;
	st->max = sorted[n - 1] / 1000.0;

	// CapFrameX style lows: average frame rate of the slowest frames
	const size_t n1 = std::max<size_t>( 1, n / 100 ), n01 = std::max<size_t>( 1, n / 1000 );
	double slow = 0.0;
	for ( size_t i = 0; i < n1; i++ ) {
		slow += sorted[n - 1 - i];
		if ( i + 1 == n01 ) {
			st->low01 = slow > 0.0 ? n01 / ( slow / 1e6 ) : 0.0;
		}
	}
	st->low1 = slow > 0.0 ? n1 / ( slow / 1e6 ) : 0.0;
}

static void CL_BenchFinish( void ) {
	benchStats_t	st;
	char			base[MAX_QPATH], line[256];
	fileHandle_t	f;
	double			mean = 0.0, var = 0.0, median;

	CL_BenchStats( bench.pooled, &st );

	std::vector<double> fps( bench.runFps );
	std::sort( fps.begin(), fps.end() );
	median = fps.empty() ? 0.0 : fps.size() % 2 ? fps[fps.size() / 2] : ( fps[fps.size() / 2 - 1] + fps[fps.size() / 2] ) / 2.0;
	for ( size_t i = 0; i < fps.size(); i++ ) {
		mean += fps[i] / fps.size();
	}
	for ( size_t i = 0; i < fps.size(); i++ ) {
		var += ( fps[i] - mean ) * ( fps[i] - mean ) / fps.size();
	}

	Com_sprintf( line, sizeof( line ), "%s: %i runs, median %.1f fps, run-to-run spread %.1f%% (CV)",
		bench.demo, (int)fps.size(), median, mean > 0.0 ? 100.0 * sqrt( var ) / mean : 0.0 );
	bench.runLines.push_back( line );
	Com_sprintf( line, sizeof( line ), "pooled %i frames: p50 %.2f p90 %.2f p99 %.2f p99.9 %.2f max %.2f ms, 1%% low %.1f fps, 0.1%% low %.1f fps",
		st.frames, st.p50, st.p90, st.p99, st.p999, st.max, st.low1, st.low01 );
	bench.runLines.push_back( line );

	Com_Printf( "\n" S_COLOR_YELLOW "benchmark results\n" );
	for ( size_t i = 0; i < bench.runLines.size(); i++ ) {
		Com_Printf( "%s\n", bench.runLines[i].c_str() );
	}

	// file names: no path separators from the demo name
	Com_sprintf( base, sizeof( base ), "benchmarks/%s_%s", bench.demo, bench.tag );
	for ( char *p = base + strlen( "benchmarks/" ); *p; p++ ) {
		if ( *p == '/' || *p == '\\' || *p == ':' ) {
			*p = '_';
		}
	}

	f = FS_FOpenFileWrite( va( "%s.csv", base ) );
	if ( f ) {
		FS_Printf( f, "run,frame,usec\n" );
		for ( size_t i = 0, frame = 0; i < bench.pooled.size(); i++, frame++ ) {
			if ( i > 0 && bench.runOf[i] != bench.runOf[i - 1] ) {
				frame = 0;
			}
			FS_Printf( f, "%i,%i,%i\n", bench.runOf[i], (int)frame, bench.pooled[i] );
		}
		FS_FCloseFile( f );
	}

	f = FS_FOpenFileWrite( va( "%s.txt", base ) );
	if ( f ) {
		for ( size_t i = 0; i < bench.runLines.size(); i++ ) {
			FS_Printf( f, "%s\n", bench.runLines[i].c_str() );
		}
		FS_Printf( f, "\nGL_VENDOR: %s\nGL_RENDERER: %s\nGL_VERSION: %s\nresolution: %ix%i\n",
			cls.glconfig.vendor_string, cls.glconfig.renderer_string, cls.glconfig.version_string,
			cls.glconfig.vidWidth, cls.glconfig.vidHeight );
		static const char * const cvars[] = {
			"version", "r_picmip", "r_textureMode", "r_ext_texture_filter_anisotropic", "r_ext_multisample",
			"r_DynamicGlow", "r_DynamicGlowWidth", "r_DynamicGlowHeight", "r_gammamethod", "r_swapInterval",
			"com_maxfps", "cl_autolodscale",
		};
		for ( size_t i = 0; i < ARRAY_LEN( cvars ); i++ ) {
			FS_Printf( f, "%s: %s\n", cvars[i], Cvar_VariableString( cvars[i] ) );
		}
		FS_FCloseFile( f );
		Com_Printf( "wrote %s.csv and %s.txt\n", base, base );
	}
}

static void CL_BenchStop( void ) {
	Cvar_Set( "timedemo", bench.timedemo );
	bench.active = qfalse;
	com_benchmarkActive = 0;
	bench.frames.clear();
	bench.pooled.clear();
	bench.runOf.clear();
	bench.runFps.clear();
	bench.runLines.clear();
}

static void CL_Benchmark_f( void ) {
	if ( Cmd_Argc() == 2 && !Q_stricmp( Cmd_Argv( 1 ), "stop" ) ) {
		if ( bench.active ) {
			CL_BenchStop();
			Com_Printf( "benchmark stopped\n" );
		}
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: benchmark <demo> [runs=5] [warmup=1] [tag], benchmark stop\n" );
		return;
	}
	if ( bench.active ) {
		Com_Printf( "a benchmark is already running, benchmark stop to abort it\n" );
		return;
	}

	Q_strncpyz( bench.demo, Cmd_Argv( 1 ), sizeof( bench.demo ) );
	bench.runs = Cmd_Argc() > 2 ? Com_Clampi( 1, 50, atoi( Cmd_Argv( 2 ) ) ) : 5;
	bench.warmup = Cmd_Argc() > 3 ? Com_Clampi( 0, 10, atoi( Cmd_Argv( 3 ) ) ) : 1;
	Q_strncpyz( bench.tag, Cmd_Argc() > 4 ? Cmd_Argv( 4 ) : "run", sizeof( bench.tag ) );
	Q_strncpyz( bench.timedemo, Cvar_VariableString( "timedemo" ), sizeof( bench.timedemo ) );

	bench.active = qtrue;
	bench.current = 0;
	bench.pendingStart = qtrue;
	com_benchmarkActive = 1;
	Cvar_Set( "timedemo", "1" );
	Com_Printf( "benchmark: %s, %i runs after %i warm-up\n", bench.demo, bench.runs, bench.warmup );
}

/*
==================
CL_BenchmarkFrame

Every client frame: starts the next run and records frame times
==================
*/
void CL_BenchmarkFrame( void ) {
	if ( !bench.active ) {
		return;
	}

	if ( bench.pendingStart ) {
		if ( clc.demoplaying || cls.state > CA_DISCONNECTED ) {
			return;		// the previous run is still shutting down
		}
		bench.pendingStart = qfalse;
		bench.started = qfalse;
		bench.waitFrames = 0;
		bench.lastFrameUsec = 0;
		bench.frames.clear();
		bench.frames.reserve( 8192 );
		Cbuf_AddText( va( "demo %s\n", bench.demo ) );
		return;
	}

	if ( clc.demoplaying && cls.state == CA_ACTIVE ) {
		const int64_t now = Sys_Microseconds();

		bench.started = qtrue;
		if ( bench.lastFrameUsec ) {
			bench.frames.push_back( (int)( now - bench.lastFrameUsec ) );
		}
		bench.lastFrameUsec = now;
	} else if ( !bench.started && ++bench.waitFrames > BENCH_START_TIMEOUT ) {
		Com_Printf( S_COLOR_RED "benchmark: demo %s did not start\n", bench.demo );
		CL_BenchStop();
	} else if ( bench.started && !clc.demoplaying ) {
		Com_Printf( S_COLOR_RED "benchmark: run interrupted\n" );
		CL_BenchStop();
	}
}

/*
==================
CL_BenchmarkDemoCompleted

When a timedemo ends: closes the run and queues the next one
==================
*/
void CL_BenchmarkDemoCompleted( void ) {
	char line[256];

	if ( !bench.active ) {
		return;
	}

	if ( bench.current >= bench.warmup ) {
		const int	run = bench.current - bench.warmup + 1;
		benchStats_t st;

		CL_BenchStats( bench.frames, &st );
		bench.runFps.push_back( st.avgFps );
		for ( size_t i = 0; i < bench.frames.size(); i++ ) {
			bench.pooled.push_back( bench.frames[i] );
			bench.runOf.push_back( run );
		}
		Com_sprintf( line, sizeof( line ), "run %i: %i frames, %.2f s, %.1f fps, p50 %.2f ms, p99 %.2f ms, 1%% low %.1f fps",
			run, st.frames, st.seconds, st.avgFps, st.p50, st.p99, st.low1 );
		bench.runLines.push_back( line );
		Com_Printf( "%s\n", line );
	} else {
		Com_Printf( "benchmark: warm-up %i done\n", bench.current + 1 );
	}

	bench.current++;
	bench.started = qfalse;
	if ( bench.current < bench.warmup + bench.runs ) {
		bench.pendingStart = qtrue;
	} else {
		CL_BenchFinish();
		CL_BenchStop();
	}
}

void CL_InitBenchmark( void ) {
	Cmd_AddCommand( "benchmark", CL_Benchmark_f );
	Cmd_SetCommandCompletionFunc( "benchmark", CL_CompleteDemoName );
}

void CL_ShutdownBenchmark( void ) {
	Cmd_RemoveCommand( "benchmark" );
	if ( bench.active ) {
		CL_BenchStop();
	}
}
