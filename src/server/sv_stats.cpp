// sv_stats.cpp -- server frame statistics for serverstats and sv_statsLog
//
// No engine dependency: see sv_stats.h.

#include "sv_stats.h"

#include <stdio.h>
#include <string.h>
#include <algorithm>

static const char *svStatsStageNames[SVSTAT_NUM_STAGES] = {
	"packets",
	"pings",
	"botai",
	"game",
	"snapbuild",
	"snapsend",
	"other",
	"total"
};

static uint32_t SVStats_Sat32( uint64_t v ) {
	return v > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)v;
}

static uint16_t SVStats_Sat16( uint64_t v ) {
	return v > 0xFFFFu ? (uint16_t)0xFFFFu : (uint16_t)v;
}

static uint16_t SVStats_Sat16( int v ) {
	return v < 0 ? (uint16_t)0 : SVStats_Sat16( (uint64_t)v );
}

static uint64_t SVStats_FrameTotal( const svStatsFrame_t *f ) {
	uint64_t total = 0;

	for ( int i = 0; i < SVSTAT_NUM_TIMED; i++ ) {
		total += f->usec[i];
	}
	return total;
}

// the frame recorded `back` frames before the latest one
static const svStatsFrame_t *SVStats_Frame( const svStatsHistory_t *h, int back ) {
	return &h->frames[( h->head - 1 - back + 2 * SVSTATS_HISTORY ) % SVSTATS_HISTORY];
}

void SVStats_Clear( svStatsHistory_t *h ) {
	h->head = 0;
	h->count = 0;
	h->recorded = 0;
}

void SVStats_AddPacket( svStatsPending_t *p, int64_t usec ) {
	if ( usec > 0 ) {
		p->usec[SVSTAT_PACKETS] += (uint64_t)usec;
	}
	p->packets++;
}

void SVStats_AddSnapshot( svStatsPending_t *p, int type, int bytes, int entities ) {
	if ( bytes < 0 ) {
		bytes = 0;
	}
	if ( entities < 0 ) {
		entities = 0;
	}
	p->snapshots++;
	p->snapshotBytes += (uint64_t)bytes;
	p->snapshotEntities += (uint64_t)entities;
	if ( type != SVSTAT_SNAP_DELTA ) {
		p->fullSnapshots++;
		p->fullSnapshotBytes += (uint64_t)bytes;
		if ( type == SVSTAT_SNAP_FALLBACK ) {
			p->fallbackSnapshots++;
		}
	}
}

void SVStats_SetPacing( svStatsPending_t *p, int flags ) {
	p->flags = (uint32_t)flags;
}

void SVStats_Record( svStatsHistory_t *h, svStatsPending_t *p, uint32_t endMsec,
					int gameFrames, int frameMsec ) {
	svStatsFrame_t *f = &h->frames[h->head];

	f->endMsec = endMsec;
	for ( int i = 0; i < SVSTAT_NUM_TIMED; i++ ) {
		f->usec[i] = SVStats_Sat32( p->usec[i] );
	}
	f->snapshotBytes = SVStats_Sat32( p->snapshotBytes );
	f->fullSnapshotBytes = SVStats_Sat32( p->fullSnapshotBytes );
	f->snapshotEntities = SVStats_Sat32( p->snapshotEntities );
	f->gameFrames = SVStats_Sat16( gameFrames );
	f->frameMsec = SVStats_Sat16( frameMsec );
	f->packets = SVStats_Sat16( (uint64_t)p->packets );
	f->snapshots = SVStats_Sat16( (uint64_t)p->snapshots );
	f->fullSnapshots = SVStats_Sat16( (uint64_t)p->fullSnapshots );
	f->fallbackSnapshots = SVStats_Sat16( (uint64_t)p->fallbackSnapshots );
	f->flags = (uint16_t)p->flags;

	h->head = ( h->head + 1 ) % SVSTATS_HISTORY;
	if ( h->count < SVSTATS_HISTORY ) {
		h->count++;
	}
	h->recorded++;

	memset( p, 0, sizeof( *p ) );
}

int SVStats_FramesWithin( const svStatsHistory_t *h, uint32_t nowMsec, uint32_t windowMsec ) {
	int n;

	for ( n = 0; n < h->count; n++ ) {
		// signed, so a frame stamped after nowMsec still counts as recent
		const int32_t age = (int32_t)( nowMsec - SVStats_Frame( h, n )->endMsec );

		if ( age >= 0 && (uint32_t)age >= windowMsec ) {
			break;
		}
	}
	return n;
}

int SVStats_FramesSince( const svStatsHistory_t *h, uint32_t recorded ) {
	const uint32_t n = h->recorded - recorded;

	return n > (uint32_t)h->count ? h->count : (int)n;
}

bool SVStats_LogDue( uint32_t *nextMsec, uint32_t nowMsec ) {
	if ( (int32_t)( nowMsec - *nextMsec ) < 0 ) {
		return false;
	}
	*nextMsec += 1000;
	if ( (int32_t)( nowMsec - *nextMsec ) >= 0 ) {
		*nextMsec = nowMsec + 1000;
	}
	return true;
}

uint32_t SVStats_Percentile( const uint32_t *sorted, int count, int percent ) {
	if ( count <= 0 ) {
		return 0;
	}
	const int64_t rank = ( (int64_t)count * percent + 99 ) / 100;

	if ( rank < 1 ) {
		return sorted[0];
	}
	if ( rank > count ) {
		return sorted[count - 1];
	}
	return sorted[rank - 1];
}

void SVStats_Summarize( const svStatsHistory_t *h, int numFrames, uint32_t *scratch, svStatsSummary_t *out ) {
	memset( out, 0, sizeof( *out ) );
	if ( numFrames > h->count ) {
		numFrames = h->count;
	}
	if ( numFrames <= 0 ) {
		return;
	}
	out->frames = numFrames;

	for ( int i = 0; i < numFrames; i++ ) {
		const svStatsFrame_t	*f = SVStats_Frame( h, i );
		const uint64_t			total = SVStats_FrameTotal( f );

		out->busyUsec += total;
		out->gameFrames += f->gameFrames;
		// a frame paced at sv_hibernateFps runs several game frames on
		// purpose, with that much more time for them
		if ( f->flags & SVSTAT_HIBERNATING ) {
			out->hibernating++;
		} else {
			if ( f->gameFrames > 1 ) {
				out->catchupFrames++;
			}
			if ( f->gameFrames > out->maxGameFrames ) {
				out->maxGameFrames = f->gameFrames;
			}
			if ( total > (uint64_t)f->frameMsec * 1000 ) {
				out->overBudget++;
			}
		}
		out->packets += f->packets;
		out->snapshots += f->snapshots;
		out->fullSnapshots += f->fullSnapshots;
		out->fallbackSnapshots += f->fallbackSnapshots;
		out->snapshotBytes += f->snapshotBytes;
		out->fullSnapshotBytes += f->fullSnapshotBytes;
		out->snapshotEntities += f->snapshotEntities;
	}

	for ( int s = 0; s < SVSTAT_NUM_STAGES; s++ ) {
		svStatsStage_t	*st = &out->stage[s];
		uint64_t		sum = 0;

		for ( int i = 0; i < numFrames; i++ ) {
			const svStatsFrame_t *f = SVStats_Frame( h, i );

			scratch[i] = s == SVSTAT_TOTAL ? SVStats_Sat32( SVStats_FrameTotal( f ) ) : f->usec[s];
			sum += scratch[i];
		}
		std::sort( scratch, scratch + numFrames );
		st->avg = (uint32_t)( ( sum + numFrames / 2 ) / numFrames );
		st->p50 = SVStats_Percentile( scratch, numFrames, 50 );
		st->p95 = SVStats_Percentile( scratch, numFrames, 95 );
		st->p99 = SVStats_Percentile( scratch, numFrames, 99 );
		st->max = scratch[numFrames - 1];
	}

	// from the end of the frame before the oldest one to the end of the
	// latest; without that frame, the oldest one took about its game time
	const svStatsFrame_t	*oldest = SVStats_Frame( h, numFrames - 1 );
	uint32_t				start;

	if ( numFrames < h->count ) {
		start = SVStats_Frame( h, numFrames )->endMsec;
	} else {
		start = oldest->endMsec - (uint32_t)oldest->frameMsec * oldest->gameFrames;
	}
	out->spanMsec = SVStats_Frame( h, 0 )->endMsec - start;
}

const char *SVStats_StageName( int stage ) {
	if ( stage < 0 || stage >= SVSTAT_NUM_STAGES ) {
		return "";
	}
	return svStatsStageNames[stage];
}

// appends ",text" (or just text at the start of the line) when it fits
static int SVStats_CSVAppend( char *buf, int size, int len, const char *text ) {
	const int n = (int)strlen( text ) + ( len ? 1 : 0 );

	if ( len + n + 2 > size ) {
		return len;	// keep room for the newline; SVSTATS_CSV_SIZE always has it
	}
	if ( len ) {
		buf[len++] = ',';
	}
	strcpy( buf + len, text );
	return len + (int)strlen( text );
}

static int SVStats_CSVAppendValue( char *buf, int size, int len, uint64_t value ) {
	char text[16];

	sprintf( text, "%u", (unsigned)SVStats_Sat32( value ) );
	return SVStats_CSVAppend( buf, size, len, text );
}

static void SVStats_CSVEnd( char *buf, int len ) {
	buf[len] = '\n';
	buf[len + 1] = '\0';
}

void SVStats_CSVHeader( char *buf, int size ) {
	static const char *columns =
		"time,msec,sv_fps,humans,bots,frames,game_frames,catchup_frames,max_game_frames,"
		"over_budget,hibernating,packets,snapshots,full_snapshots,fallback_snapshots,"
		"snapshot_bytes,full_snapshot_bytes,snapshot_entities";
	char	name[32];
	int		len = 0;

	if ( size < 2 ) {
		return;
	}
	buf[0] = '\0';
	len = SVStats_CSVAppend( buf, size, len, columns );
	for ( int s = 0; s < SVSTAT_NUM_STAGES; s++ ) {
		sprintf( name, "%s_avg_us", svStatsStageNames[s] );
		len = SVStats_CSVAppend( buf, size, len, name );
		sprintf( name, "%s_max_us", svStatsStageNames[s] );
		len = SVStats_CSVAppend( buf, size, len, name );
	}
	SVStats_CSVEnd( buf, len );
}

void SVStats_CSVLine( char *buf, int size, uint32_t unixTime, int fps, int humans, int bots,
					const svStatsSummary_t *s ) {
	int len = 0;

	if ( size < 2 ) {
		return;
	}
	buf[0] = '\0';
	len = SVStats_CSVAppendValue( buf, size, len, unixTime );
	len = SVStats_CSVAppendValue( buf, size, len, s->spanMsec );
	len = SVStats_CSVAppendValue( buf, size, len, fps > 0 ? (uint64_t)fps : 0 );
	len = SVStats_CSVAppendValue( buf, size, len, humans > 0 ? (uint64_t)humans : 0 );
	len = SVStats_CSVAppendValue( buf, size, len, bots > 0 ? (uint64_t)bots : 0 );
	len = SVStats_CSVAppendValue( buf, size, len, (uint64_t)s->frames );
	len = SVStats_CSVAppendValue( buf, size, len, (uint64_t)s->gameFrames );
	len = SVStats_CSVAppendValue( buf, size, len, (uint64_t)s->catchupFrames );
	len = SVStats_CSVAppendValue( buf, size, len, (uint64_t)s->maxGameFrames );
	len = SVStats_CSVAppendValue( buf, size, len, (uint64_t)s->overBudget );
	len = SVStats_CSVAppendValue( buf, size, len, (uint64_t)s->hibernating );
	len = SVStats_CSVAppendValue( buf, size, len, s->packets );
	len = SVStats_CSVAppendValue( buf, size, len, s->snapshots );
	len = SVStats_CSVAppendValue( buf, size, len, s->fullSnapshots );
	len = SVStats_CSVAppendValue( buf, size, len, s->fallbackSnapshots );
	len = SVStats_CSVAppendValue( buf, size, len, s->snapshotBytes );
	len = SVStats_CSVAppendValue( buf, size, len, s->fullSnapshotBytes );
	len = SVStats_CSVAppendValue( buf, size, len, s->snapshotEntities );
	for ( int i = 0; i < SVSTAT_NUM_STAGES; i++ ) {
		len = SVStats_CSVAppendValue( buf, size, len, s->stage[i].avg );
		len = SVStats_CSVAppendValue( buf, size, len, s->stage[i].max );
	}
	SVStats_CSVEnd( buf, len );
}
