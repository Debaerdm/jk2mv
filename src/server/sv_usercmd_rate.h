// sv_usercmd_rate.h -- per-client usercmd accounting and the sv_maxUsercmdRate cap
//
// Free of engine dependencies, so tests/unit/test_sv_usercmd_rate.cpp links
// sv_usercmd_rate.cpp as it is. SV_UserMove, SV_ClientThink and SV_Frame drive
// it, the 'clientstats' command shows it.

#ifndef SV_USERCMD_RATE_H
#define SV_USERCMD_RATE_H

#include <stdint.h>

// A client sends one usercmd per frame, 125 to 333 per second at the usual
// com_maxfps, and the jump physics depend on that frame rate, so a lower
// sv_maxUsercmdRate is raised to this
#define USERCMD_RATE_MIN		500

// A client may run this fraction of a second's worth of usercmds in a row
// before the cap applies, so network jitter and hitches never trip it
#define USERCMD_BURST_DIVISOR	2

typedef struct {
	int64_t		credit;			// usercmds the client may run now, in thousandths
	int			lastTime;		// msec time of the last refill
	int			started;		// the credit has been filled once
	int			seenTime;		// serverTime of the newest usercmd received
} usercmdBucket_t;

typedef struct {
	int			packets;		// packets received from the client
	int			cmds;			// usercmds run through GAME_CLIENT_THINK
	int			dropped;		// usercmds dropped by sv_maxUsercmdRate
	int			thinkUsec;		// microseconds spent in GAME_CLIENT_THINK
} usercmdCounts_t;

typedef struct {
	usercmdCounts_t	current;		// the second being counted
	usercmdCounts_t	last;			// the last full second, scaled to one second
	usercmdCounts_t	peak;			// highest values of 'last' since the client connected
	int64_t			totalDropped;	// usercmds dropped since the client connected
} usercmdStats_t;

// usercmds per second for a sv_maxUsercmdRate value, 0 for no cap
int SV_UsercmdRateCap( int cvarValue );

// usercmds a client may run in a row before the cap applies
int SV_UsercmdBurst( int rate );

// picks the usercmds of a client packet to run, see sv_usercmd_rate.cpp
int SV_UsercmdSchedule( usercmdBucket_t *bucket, int rate, int now,
	const int *serverTimes, int count, int lastServerTime, int *run, int *dropped );

// closes the current one-second window, which lasted elapsedMsec
void SV_UsercmdStatsRoll( usercmdStats_t *stats, int elapsedMsec );

#endif // SV_USERCMD_RATE_H
