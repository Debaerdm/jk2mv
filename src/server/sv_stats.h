// sv_stats.h -- server frame statistics for serverstats and sv_statsLog
//
// Pure bookkeeping with no engine dependency, so the unit tests can link it.
// SV_Frame times its stages with Sys_Microseconds and records one frame each
// time it runs the game (sv_main_stats.h); the summaries give the average,
// percentiles and maximum of every stage over the latest frames.

#ifndef SV_STATS_H
#define SV_STATS_H

#include <stdint.h>

// stages of a server frame, timed in microseconds, in the order they run
enum {
	SVSTAT_PACKETS,		// incoming packets, handled between frames
	SVSTAT_PINGS,		// SV_CalcPings
	SVSTAT_BOTAI,		// SV_BotFrame
	SVSTAT_GAME,		// the GAME_RUN_FRAME calls
	SVSTAT_SNAPBUILD,	// building the snapshots, bots included
	SVSTAT_SNAPSEND,	// encoding and sending them
	SVSTAT_OTHER,		// the rest of SV_Frame: timeouts, configstrings, heartbeats
	SVSTAT_NUM_TIMED,

	SVSTAT_TOTAL = SVSTAT_NUM_TIMED,	// summaries only: the sum of the stages
	SVSTAT_NUM_STAGES
};

// how a snapshot was encoded
enum {
	SVSTAT_SNAP_DELTA,		// against a snapshot the client has
	SVSTAT_SNAP_FULL,		// whole: the client asked for it or is connecting
	SVSTAT_SNAP_FALLBACK	// whole because the client's delta base is gone
};

#define SVSTAT_HIBERNATING	1	// frame flag: paced at sv_hibernateFps (SVStats_SetPacing)

// One server frame: an SV_Frame call that ran the game, with the packets
// and the SV_Frame calls that came since the previous one without running it.
typedef struct {
	uint32_t	endMsec;				// clock at its end, in milliseconds (wraps)
	uint32_t	usec[SVSTAT_NUM_TIMED];
	uint32_t	snapshotBytes;			// size of the snapshots sent, without the rest of their messages
	uint32_t	fullSnapshotBytes;		// the part of it in full snapshots
	uint32_t	snapshotEntities;		// entities in the snapshots sent
	uint16_t	gameFrames;				// GAME_RUN_FRAME calls
	uint16_t	frameMsec;				// game frame length, 1000 / sv_fps: the budget
	uint16_t	packets;
	uint16_t	snapshots;				// snapshots sent to clients
	uint16_t	fullSnapshots;			// of which not delta compressed
	uint16_t	fallbackSnapshots;		// of which full because the delta base was gone
	uint16_t	flags;					// SVSTAT_HIBERNATING
} svStatsFrame_t;

// what happened since the last recorded frame
typedef struct {
	uint64_t	usec[SVSTAT_NUM_TIMED];
	uint64_t	snapshotBytes;
	uint64_t	fullSnapshotBytes;
	uint64_t	snapshotEntities;
	uint32_t	packets;
	uint32_t	snapshots;
	uint32_t	fullSnapshots;
	uint32_t	fallbackSnapshots;
	uint32_t	flags;					// of the latest pacing, SVStats_SetPacing
} svStatsPending_t;

#define SVSTATS_HISTORY		8192	// frames kept: 409 s at sv_fps 20, 204 s at 40

typedef struct {
	svStatsFrame_t	frames[SVSTATS_HISTORY];
	int				head;			// where the next frame goes
	int				count;			// frames kept
	uint32_t		recorded;		// frames recorded so far (wraps)
} svStatsHistory_t;

typedef struct {
	uint32_t	avg, p50, p95, p99, max;	// microseconds
} svStatsStage_t;

typedef struct {
	int				frames;
	uint32_t		spanMsec;			// wall time the frames cover
	svStatsStage_t	stage[SVSTAT_NUM_STAGES];
	uint64_t		busyUsec;			// sum of the frame totals
	int				gameFrames;
	int				catchupFrames;		// frames that ran several game frames, hibernation aside
	int				maxGameFrames;		// the most in one frame, hibernation aside
	int				overBudget;			// frames whose total took longer than a game frame, hibernation aside
	int				hibernating;		// frames paced at sv_hibernateFps
	uint64_t		packets;
	uint64_t		snapshots;
	uint64_t		fullSnapshots;
	uint64_t		fallbackSnapshots;
	uint64_t		snapshotBytes;
	uint64_t		fullSnapshotBytes;
	uint64_t		snapshotEntities;
} svStatsSummary_t;

void		SVStats_Clear( svStatsHistory_t *h );

// counting between two recorded frames
void		SVStats_AddPacket( svStatsPending_t *p, int64_t usec );
void		SVStats_AddSnapshot( svStatsPending_t *p, int type, int bytes, int entities );

// SV_FrameMsec says how it paces the next server frame, before the wait and
// the packets: SVSTAT_HIBERNATING when at sv_hibernateFps, 0 at sv_fps. The
// frame gets the flags of the latest pacing, whatever the hibernation state
// at its end: a client that connects during a hibernation wait ends it, and
// the frame still runs the game frames of a whole hibernation period. A
// listen server is not paced by SV_FrameMsec, so its frames get 0.
void		SVStats_SetPacing( svStatsPending_t *p, int flags );

// Turns the pending counts into a frame, saturating what does not fit, adds
// it to the history in place of the oldest frame when full, and clears them,
// pacing included.
void		SVStats_Record( svStatsHistory_t *h, svStatsPending_t *p, uint32_t endMsec,
						int gameFrames, int frameMsec );

// The number of latest frames that ended less than windowMsec before nowMsec.
int			SVStats_FramesWithin( const svStatsHistory_t *h, uint32_t nowMsec, uint32_t windowMsec );

// The number of frames recorded since h->recorded was `recorded`, as far as
// the history goes back.
int			SVStats_FramesSince( const svStatsHistory_t *h, uint32_t recorded );

// Summarizes the latest numFrames frames. scratch holds SVSTATS_HISTORY values.
void		SVStats_Summarize( const svStatsHistory_t *h, int numFrames, uint32_t *scratch, svStatsSummary_t *out );

// Nearest rank: the smallest of count increasing values with at least percent
// of them at or below it. 0 without values.
uint32_t	SVStats_Percentile( const uint32_t *sorted, int count, int percent );

const char	*SVStats_StageName( int stage );

// The sv_statsLog schedule, a line per second on average: whether a line is
// due at nowMsec, in which case *nextMsec moves to the next one, a second
// later or, after a hitch of a second or more, a second after nowMsec.
// Across the clock wrap too.
bool		SVStats_LogDue( uint32_t *nextMsec, uint32_t nowMsec );

// The sv_statsLog CSV: a header, then one line per summary. Both end with a
// newline; the buffer must hold SVSTATS_CSV_SIZE characters.
#define SVSTATS_CSV_SIZE	1024
void		SVStats_CSVHeader( char *buf, int size );
void		SVStats_CSVLine( char *buf, int size, uint32_t unixTime, int fps, int humans, int bots,
						const svStatsSummary_t *s );

#endif // SV_STATS_H
