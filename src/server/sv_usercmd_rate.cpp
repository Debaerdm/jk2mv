// sv_usercmd_rate.cpp -- per-client usercmd accounting and the sv_maxUsercmdRate cap

#include "sv_usercmd_rate.h"

#include <string.h>

/*
==================
SV_UsercmdRateCap

Usercmds per second for a sv_maxUsercmdRate value, 0 for no cap
==================
*/
int SV_UsercmdRateCap( int cvarValue ) {
	if ( cvarValue <= 0 ) {
		return 0;
	}
	if ( cvarValue < USERCMD_RATE_MIN ) {
		return USERCMD_RATE_MIN;
	}
	return cvarValue;
}

/*
==================
SV_UsercmdBurst

Usercmds a client may run in a row before the cap applies
==================
*/
int SV_UsercmdBurst( int rate ) {
	return rate / USERCMD_BURST_DIVISOR;
}

/*
==================
SV_UsercmdTake

Adds the credit earned since the last packet, then takes up to 'wanted'
usercmds from it. Returns how many it took.
==================
*/
static int SV_UsercmdTake( usercmdBucket_t *bucket, int rate, int now, int wanted ) {
	const int64_t	capacity = (int64_t)SV_UsercmdBurst( rate ) * 1000;
	// unsigned, so it stays right when the msec clock wraps after 24 days
	const int		elapsed = (int)( (unsigned)now - (unsigned)bucket->lastTime );
	int64_t			allowed;

	if ( !bucket->started || elapsed < 0 || elapsed > 1000 ) {
		// first packet, clock jump, or long enough to fill it anyway
		bucket->credit = capacity;
		bucket->started = 1;
	} else {
		bucket->credit += (int64_t)elapsed * rate;
		if ( bucket->credit > capacity ) {
			bucket->credit = capacity;
		}
	}
	bucket->lastTime = now;

	allowed = bucket->credit / 1000;
	if ( allowed > wanted ) {
		allowed = wanted;
	}
	bucket->credit -= allowed * 1000;

	return (int)allowed;
}

/*
==================
SV_UsercmdSchedule

Picks the usercmds of a client packet to run, in order, with a cap of 'rate'
usercmds per second (0: no cap) at msec time 'now'. serverTimes holds the
packet's usercmds and lastServerTime the last one run. As SV_UserMove has
always done, a usercmd runs when it is newer than the last one run and not
newer than the packet's last one. When the cap allows fewer, the oldest are
dropped and the newest run: the game moves the player from the last usercmd
it ran to the next one in one go, so the player keeps up with the client and
loses no time, only the inputs of the dropped usercmds.

Returns how many usercmds to run, with their indices in 'run', which has room
for 'count'. *dropped gets the usercmds received for the first time minus the
usercmds run: a client sends each usercmd in 1 + cl_packetdup packets and one
dropped from a packet can still run from the next, so a usercmd counts once,
when it is first dropped, and is taken back if it runs later.
==================
*/
int SV_UsercmdSchedule( usercmdBucket_t *bucket, int rate, int now,
		const int *serverTimes, int count, int lastServerTime, int *run, int *dropped ) {
	int		numNew = 0;
	int		fresh = 0;
	int		allowed;
	int		i;

	// nothing up to the last usercmd run is new, whatever came before
	if ( bucket->seenTime < lastServerTime ) {
		bucket->seenTime = lastServerTime;
	}

	for ( i = 0; i < count; i++ ) {
		if ( serverTimes[i] > serverTimes[count - 1] ) {
			continue;
		}
		if ( serverTimes[i] <= lastServerTime ) {
			continue;
		}
		lastServerTime = serverTimes[i];
		run[numNew++] = i;

		if ( serverTimes[i] > bucket->seenTime ) {
			fresh++;
		}
	}

	if ( numNew > 0 && serverTimes[run[numNew - 1]] > bucket->seenTime ) {
		bucket->seenTime = serverTimes[run[numNew - 1]];
	}

	allowed = rate > 0 ? SV_UsercmdTake( bucket, rate, now, numNew ) : numNew;
	if ( allowed < numNew ) {
		memmove( run, run + numNew - allowed, allowed * sizeof( *run ) );
	}

	*dropped = fresh - allowed;
	return allowed;
}

/*
==================
SV_UsercmdStatsScale

A count over elapsedMsec, per second
==================
*/
static int SV_UsercmdStatsScale( int count, int elapsedMsec ) {
	return (int)( ( (int64_t)count * 1000 + elapsedMsec / 2 ) / elapsedMsec );
}

static int SV_UsercmdStatsMax( int a, int b ) {
	return a > b ? a : b;
}

/*
==================
SV_UsercmdStatsRoll

Closes the window being counted, which lasted elapsedMsec
==================
*/
void SV_UsercmdStatsRoll( usercmdStats_t *stats, int elapsedMsec ) {
	usercmdCounts_t	*cur = &stats->current;
	usercmdCounts_t	*last = &stats->last;
	usercmdCounts_t	*peak = &stats->peak;

	if ( elapsedMsec < 1 ) {
		elapsedMsec = 1;
	}

	last->packets = SV_UsercmdStatsScale( cur->packets, elapsedMsec );
	last->cmds = SV_UsercmdStatsScale( cur->cmds, elapsedMsec );
	// usercmds dropped at the end of a window can run in the next one,
	// which then counts them back
	last->dropped = cur->dropped > 0 ? SV_UsercmdStatsScale( cur->dropped, elapsedMsec ) : 0;
	last->thinkUsec = SV_UsercmdStatsScale( cur->thinkUsec, elapsedMsec );

	peak->packets = SV_UsercmdStatsMax( peak->packets, last->packets );
	peak->cmds = SV_UsercmdStatsMax( peak->cmds, last->cmds );
	peak->dropped = SV_UsercmdStatsMax( peak->dropped, last->dropped );
	peak->thinkUsec = SV_UsercmdStatsMax( peak->thinkUsec, last->thinkUsec );

	stats->totalDropped += cur->dropped;

	memset( cur, 0, sizeof( *cur ) );
}
