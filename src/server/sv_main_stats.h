// sv_main_stats.h -- Server frame statistics: serverstats and sv_statsLog
//
// SV_Frame times its stages, Com_RunAndTimeServerPacket the packets handled
// between frames and SV_SendClientMessages the snapshot building; SV_FrameMsec
// notes whether it paced the frame at sv_hibernateFps. The frames that ran
// the game go into a history (sv_stats.cpp). Always on, as it only costs two
// clock reads per packet and per snapshot built, and about ten per frame.

#ifndef SV_MAIN_STATS_H
#define SV_MAIN_STATS_H

// the sv_statsLog file, in fs_homepath: no cvar or command chooses it
#define SVSTATS_LOG_FILE	"svstats.csv"

static uint32_t	svStatsScratch[SVSTATS_HISTORY];	// percentile sorting

/*
==================
SV_StatsLap

Adds the time since `since` to a stage and returns the current time
==================
*/
int64_t SV_StatsLap( int stage, int64_t since ) {
	const int64_t now = Sys_Microseconds();

	if ( now > since ) {
		svStats.pending.usec[stage] += (uint64_t)( now - since );
	}
	return now;
}

/*
==================
SV_StatsPacket

Com_RunAndTimeServerPacket reports each packet the server handled
==================
*/
void SV_StatsPacket( int64_t usec ) {
	SVStats_AddPacket( &svStats.pending, usec );
}

/*
==================
SV_StatsCountClients
==================
*/
static void SV_StatsCountClients( int *humans, int *bots ) {
	int i;

	*humans = 0;
	*bots = 0;
	if ( !svs.clients ) {
		return;
	}
	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		const client_t *cl = &svs.clients[i];

		if ( cl->state < CS_CONNECTED ) {
			continue;
		}
		if ( cl->netchan.remoteAddress.type == NA_BOT ) {
			(*bots)++;
		} else {
			(*humans)++;
		}
	}
}

/*
==================
SV_StatsCloseLog
==================
*/
static void SV_StatsCloseLog( void ) {
	if ( svStats.logFile ) {
		FS_FCloseFile( svStats.logFile );
		svStats.logFile = 0;
	}
}

/*
==================
SV_StatsLog

Appends a line to the sv_statsLog file every second, summing up the frames
since the previous line. Returns qtrue when it wrote something.
==================
*/
static qboolean SV_StatsLog( uint32_t nowMsec ) {
	svStatsSummary_t	s;
	char				line[SVSTATS_CSV_SIZE];
	int					humans, bots;

	if ( !svStats.logFile ) {
		svStats.logFile = FS_SV_FOpenFileAppend( SVSTATS_LOG_FILE );
		if ( !svStats.logFile ) {
			Com_Printf( "sv_statsLog: can't write to %s, turning it off\n", SVSTATS_LOG_FILE );
			Cvar_Set( "sv_statsLog", "0" );
			return qfalse;
		}
		if ( FS_filelength( svStats.logFile ) == 0 ) {
			SVStats_CSVHeader( line, sizeof( line ) );
			FS_Write( line, (int)strlen( line ), svStats.logFile );
			FS_Flush( svStats.logFile );
		}
		Com_Printf( "sv_statsLog: a line per second in %s\n", SVSTATS_LOG_FILE );
		svStats.logNextMsec = nowMsec + 1000;
		svStats.logRecorded = svStats.history.recorded;
		return qtrue;
	}

	if ( !SVStats_LogDue( &svStats.logNextMsec, nowMsec ) ) {
		return qfalse;
	}

	SVStats_Summarize( &svStats.history, SVStats_FramesSince( &svStats.history, svStats.logRecorded ),
		svStatsScratch, &s );
	svStats.logRecorded = svStats.history.recorded;
	SV_StatsCountClients( &humans, &bots );
	SVStats_CSVLine( line, sizeof( line ), (uint32_t)Com_RealTime( NULL ), sv_fps->integer, humans, bots, &s );
	FS_Write( line, (int)strlen( line ), svStats.logFile );
	FS_Flush( svStats.logFile );
	return qtrue;
}

/*
==================
SV_StatsEndFrame

Records the frame if it ran the game, flagged as SV_FrameMsec paced it;
otherwise its times wait for the next one that does. Then the log.
==================
*/
static void SV_StatsEndFrame( int64_t now, int gameFrames, int frameMsec ) {
	const uint32_t nowMsec = (uint32_t)( now / 1000 );

	if ( gameFrames > 0 ) {
		SVStats_Record( &svStats.history, &svStats.pending, nowMsec, gameFrames, frameMsec );
	}

	if ( sv_statsLog->integer ) {
		// the log is server work too: it goes in the next frame
		if ( SV_StatsLog( nowMsec ) ) {
			SV_StatsLap( SVSTAT_OTHER, now );
		}
	} else {
		SV_StatsCloseLog();
	}
}

/*
==================
SV_StatsShutdown

The statistics belong to one run of the server
==================
*/
void SV_StatsShutdown( void ) {
	SV_StatsCloseLog();
	SVStats_Clear( &svStats.history );
	Com_Memset( &svStats.pending, 0, sizeof( svStats.pending ) );
}

/*
==================
SV_ServerStats_f

serverstats [seconds]: what the server frames of the last seconds cost
==================
*/
void SV_ServerStats_f( void ) {
	svStatsSummary_t	s;
	int					seconds = 60;
	int					humans, bots, i;

	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	if ( Cmd_Argc() > 2 || ( Cmd_Argc() == 2 && ( seconds = atoi( Cmd_Argv( 1 ) ) ) < 1 ) ) {
		Com_Printf( "usage: serverstats [seconds]   (default 60)\n" );
		return;
	}
	if ( seconds > 86400 ) {
		seconds = 86400;
	}

	const uint32_t nowMsec = (uint32_t)( Sys_Microseconds() / 1000 );

	SVStats_Summarize( &svStats.history, SVStats_FramesWithin( &svStats.history, nowMsec, (uint32_t)seconds * 1000 ),
		svStatsScratch, &s );
	if ( !s.frames ) {
		Com_Printf( "No server frame in the last %i seconds.\n", seconds );
		return;
	}
	SV_StatsCountClients( &humans, &bots );

	const int		fps = sv_fps->integer > 0 ? sv_fps->integer : 1;
	const double	budgetUsec = 1000000.0 / fps;
	const double	span = s.spanMsec > 0 ? s.spanMsec / 1000.0 : 0.001;

	Com_Printf( "Server frames, last %.1f s: %i frames at sv_fps %i (%.1f ms budget), %i humans, %i bots\n",
		s.spanMsec / 1000.0, s.frames, fps, budgetUsec / 1000.0, humans, bots );
	Com_Printf( "stage          avg      p50      p95      p99      max  (ms)\n" );
	for ( i = 0; i < SVSTAT_NUM_STAGES; i++ ) {
		const svStatsStage_t *st = &s.stage[i];

		Com_Printf( "%-9s %8.3f %8.3f %8.3f %8.3f %8.3f\n", SVStats_StageName( i ),
			st->avg / 1000.0, st->p50 / 1000.0, st->p95 / 1000.0, st->p99 / 1000.0, st->max / 1000.0 );
	}
	// no percent signs: rcon clients print them as dots
	Com_Printf( "load %.1f percent, p99 frame at %.0f percent of the budget, %i frames over budget\n",
		s.busyUsec / ( span * 10000.0 ), s.stage[SVSTAT_TOTAL].p99 * 100.0 / budgetUsec, s.overBudget );
	Com_Printf( "catch-up frames (several game frames at once): %i, most game frames in one: %i\n",
		s.catchupFrames, s.maxGameFrames );
	if ( s.hibernating ) {
		Com_Printf( "frames while hibernating: %i\n", s.hibernating );
	}
	Com_Printf( "packets in: %.0f (%.1f/s)\n", (double)s.packets, s.packets / span );
	Com_Printf( "snapshots out: %.0f (%.1f/s, %.1f KB/s), %.1f entities each\n", (double)s.snapshots,
		s.snapshots / span, s.snapshotBytes / ( span * 1024.0 ),
		s.snapshots ? (double)s.snapshotEntities / s.snapshots : 0.0 );
	Com_Printf( "full snapshots: %.0f (%.0f after a lost delta base), %.1f KB\n", (double)s.fullSnapshots,
		(double)s.fallbackSnapshots, s.fullSnapshotBytes / 1024.0 );
}

#endif // SV_MAIN_STATS_H
