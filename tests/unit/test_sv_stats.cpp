// test_sv_stats.cpp - server frame statistics (serverstats, sv_statsLog)
//
// Links the real engine source (src/server/sv_stats.cpp), which has no
// engine dependency.

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "server/sv_stats.h"

namespace {

class SVStatsTest : public ::testing::Test {
protected:
	void SetUp() override {
		history.reset( new svStatsHistory_t() );
		SVStats_Clear( history.get() );
		memset( &pending, 0, sizeof( pending ) );
		scratch.assign( SVSTATS_HISTORY, 0 );
	}

	// records a frame whose stages took the given microseconds
	void Record( uint32_t endMsec, uint32_t packets, uint32_t pings, uint32_t botai, uint32_t game,
				uint32_t build, uint32_t send, uint32_t other, int gameFrames = 1, int frameMsec = 50, int flags = 0 ) {
		const uint32_t usec[SVSTAT_NUM_TIMED] = { packets, pings, botai, game, build, send, other };

		for ( int i = 0; i < SVSTAT_NUM_TIMED; i++ ) {
			pending.usec[i] += usec[i];
		}
		SVStats_Record( history.get(), &pending, endMsec, gameFrames, frameMsec, flags );
	}

	// a frame where only the game ran
	void RecordGame( uint32_t endMsec, uint32_t game, int gameFrames = 1, int frameMsec = 50, int flags = 0 ) {
		Record( endMsec, 0, 0, 0, game, 0, 0, 0, gameFrames, frameMsec, flags );
	}

	svStatsSummary_t SummarizeLatest( int numFrames ) {
		svStatsSummary_t s;
		SVStats_Summarize( history.get(), numFrames, scratch.data(), &s );
		return s;
	}

	std::unique_ptr<svStatsHistory_t>	history;
	svStatsPending_t					pending;
	std::vector<uint32_t>				scratch;
};

int CountChar( const char *s, char c ) {
	int n = 0;
	for ( ; *s; s++ ) {
		n += *s == c;
	}
	return n;
}

std::vector<std::string> SplitCSV( const char *line ) {
	std::vector<std::string>	fields;
	std::string					field;

	for ( const char *p = line; *p && *p != '\n'; p++ ) {
		if ( *p == ',' ) {
			fields.push_back( field );
			field.clear();
		} else {
			field += *p;
		}
	}
	fields.push_back( field );
	return fields;
}

} // namespace

TEST(SVStatsPercentile, NearestRank) {
	std::vector<uint32_t> v;
	for ( uint32_t i = 1; i <= 100; i++ ) {
		v.push_back( i );
	}
	EXPECT_EQ( SVStats_Percentile( v.data(), 100, 50 ), 50u );
	EXPECT_EQ( SVStats_Percentile( v.data(), 100, 95 ), 95u );
	EXPECT_EQ( SVStats_Percentile( v.data(), 100, 99 ), 99u );
	EXPECT_EQ( SVStats_Percentile( v.data(), 100, 100 ), 100u );
	EXPECT_EQ( SVStats_Percentile( v.data(), 100, 0 ), 1u );

	// ten values: the 95th and 99th percentiles are the largest
	EXPECT_EQ( SVStats_Percentile( v.data(), 10, 50 ), 5u );
	EXPECT_EQ( SVStats_Percentile( v.data(), 10, 95 ), 10u );
	EXPECT_EQ( SVStats_Percentile( v.data(), 10, 99 ), 10u );

	// rank rounds up: 0.95 * 21 = 19.95 -> the 20th value
	EXPECT_EQ( SVStats_Percentile( v.data(), 21, 95 ), 20u );
}

TEST(SVStatsPercentile, SameRankAsTheBenchmark) {
	// cl_bench.cpp: rank = ( n * num + den - 1 ) / den, value at rank - 1
	std::vector<uint32_t> v;
	for ( uint32_t i = 0; i < 1000; i++ ) {
		v.push_back( i * 3 );
	}
	for ( int n = 1; n <= 1000; n += 37 ) {
		for ( int p = 1; p <= 100; p++ ) {
			const int rank = ( n * p + 99 ) / 100;
			EXPECT_EQ( SVStats_Percentile( v.data(), n, p ), v[rank - 1] ) << "n " << n << " p " << p;
		}
	}
}

TEST(SVStatsPercentile, EmptyAndSingle) {
	const uint32_t one = 1234;
	EXPECT_EQ( SVStats_Percentile( nullptr, 0, 99 ), 0u );
	EXPECT_EQ( SVStats_Percentile( &one, 1, 1 ), 1234u );
	EXPECT_EQ( SVStats_Percentile( &one, 1, 50 ), 1234u );
	EXPECT_EQ( SVStats_Percentile( &one, 1, 99 ), 1234u );
}

TEST_F(SVStatsTest, EmptyHistorySummarizesToZero) {
	const svStatsSummary_t s = SummarizeLatest( 100 );
	EXPECT_EQ( s.frames, 0 );
	EXPECT_EQ( s.spanMsec, 0u );
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].max, 0u );
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 1000, 60000 ), 0 );
	EXPECT_EQ( SVStats_FramesSince( history.get(), 0 ), 0 );
}

TEST_F(SVStatsTest, StagesAverageAndMaximum) {
	Record( 1050, 10, 1, 300, 1000, 200, 50, 5 );
	Record( 1100, 20, 2, 400, 2000, 300, 60, 6 );
	Record( 1150, 30, 3, 500, 3001, 400, 70, 7 );

	const svStatsSummary_t s = SummarizeLatest( 3 );
	ASSERT_EQ( s.frames, 3 );
	EXPECT_EQ( s.stage[SVSTAT_PACKETS].avg, 20u );
	EXPECT_EQ( s.stage[SVSTAT_PACKETS].max, 30u );
	EXPECT_EQ( s.stage[SVSTAT_PINGS].avg, 2u );
	EXPECT_EQ( s.stage[SVSTAT_BOTAI].avg, 400u );
	EXPECT_EQ( s.stage[SVSTAT_GAME].avg, 2000u );	// 6001 / 3 rounded
	EXPECT_EQ( s.stage[SVSTAT_GAME].p50, 2000u );
	EXPECT_EQ( s.stage[SVSTAT_GAME].max, 3001u );
	EXPECT_EQ( s.stage[SVSTAT_SNAPBUILD].max, 400u );
	EXPECT_EQ( s.stage[SVSTAT_SNAPSEND].avg, 60u );
	EXPECT_EQ( s.stage[SVSTAT_OTHER].max, 7u );

	// the total is the sum of the stages of each frame
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].max, 30u + 3 + 500 + 3001 + 400 + 70 + 7 );
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].p50, 20u + 2 + 400 + 2000 + 300 + 60 + 6 );
	EXPECT_EQ( s.busyUsec, 1566u + 2788 + 4011 );
}

TEST_F(SVStatsTest, PercentilesOfTheTotalArePerFrame) {
	// 100 frames: 98 quick ones, then two where different stages spike
	for ( int i = 0; i < 98; i++ ) {
		Record( 1000 + i * 50, 0, 0, 100, 900, 0, 0, 0 );
	}
	Record( 6000, 0, 0, 5000, 900, 0, 0, 0 );
	Record( 6050, 0, 0, 100, 7000, 0, 0, 0 );

	const svStatsSummary_t s = SummarizeLatest( 100 );
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].p50, 1000u );
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].p95, 1000u );
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].p99, 5900u );
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].max, 7100u );
	EXPECT_EQ( s.stage[SVSTAT_BOTAI].max, 5000u );
	EXPECT_EQ( s.stage[SVSTAT_GAME].max, 7000u );
}

TEST_F(SVStatsTest, CatchupFramesLeaveHibernationAside) {
	RecordGame( 1000, 100, 1 );
	RecordGame( 1100, 100, 2 );
	RecordGame( 1150, 100, 1 );
	RecordGame( 1400, 100, 5, 50, SVSTAT_HIBERNATING );
	RecordGame( 1550, 100, 3 );

	const svStatsSummary_t s = SummarizeLatest( 5 );
	EXPECT_EQ( s.gameFrames, 12 );
	EXPECT_EQ( s.catchupFrames, 2 );
	EXPECT_EQ( s.maxGameFrames, 5 );
	EXPECT_EQ( s.hibernating, 1 );
}

TEST_F(SVStatsTest, OverBudgetMeansLongerThanTheGameFrame) {
	RecordGame( 1000, 50000 );				// exactly 50 ms: on time
	RecordGame( 1050, 50001 );				// over
	RecordGame( 1075, 25001, 1, 25 );		// over at sv_fps 40
	Record( 1100, 30000, 0, 0, 20000, 0, 0, 1 );	// packets count too
	RecordGame( 1350, 60000, 5, 50, SVSTAT_HIBERNATING );	// 5 game frames in 250 ms: on time

	const svStatsSummary_t s = SummarizeLatest( 5 );
	EXPECT_EQ( s.overBudget, 3 );
	EXPECT_EQ( s.hibernating, 1 );
}

TEST_F(SVStatsTest, RecordSaturatesAndClearsPending) {
	pending.usec[SVSTAT_GAME] = 5000000000ull;
	pending.packets = 70000;
	pending.snapshots = 3;
	pending.snapshotBytes = 6000000000ull;
	SVStats_Record( history.get(), &pending, 1000, -3, 70000, 0 );

	const svStatsFrame_t &f = history->frames[0];
	EXPECT_EQ( f.usec[SVSTAT_GAME], 0xFFFFFFFFu );
	EXPECT_EQ( f.packets, 0xFFFFu );
	EXPECT_EQ( f.snapshots, 3u );
	EXPECT_EQ( f.snapshotBytes, 0xFFFFFFFFu );
	EXPECT_EQ( f.gameFrames, 0u );
	EXPECT_EQ( f.frameMsec, 0xFFFFu );

	const svStatsPending_t zero = {};
	EXPECT_EQ( memcmp( &pending, &zero, sizeof( zero ) ), 0 );
	EXPECT_EQ( history->count, 1 );
	EXPECT_EQ( history->recorded, 1u );
}

TEST_F(SVStatsTest, TotalSaturatesInsteadOfWrapping) {
	Record( 1000, 0, 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 0 );
	const svStatsSummary_t s = SummarizeLatest( 1 );
	EXPECT_EQ( s.stage[SVSTAT_TOTAL].max, 0xFFFFFFFFu );
	EXPECT_EQ( s.busyUsec, 2ull * 0xFFFFFFFFu );
}

TEST_F(SVStatsTest, HistoryKeepsTheLatestFrames) {
	const int extra = 100;
	for ( int i = 0; i < SVSTATS_HISTORY + extra; i++ ) {
		RecordGame( 1000 + i * 50, (uint32_t)i );
	}
	EXPECT_EQ( history->count, SVSTATS_HISTORY );
	EXPECT_EQ( history->recorded, (uint32_t)( SVSTATS_HISTORY + extra ) );

	const svStatsSummary_t all = SummarizeLatest( SVSTATS_HISTORY + 1000 );
	EXPECT_EQ( all.frames, SVSTATS_HISTORY );
	EXPECT_EQ( all.stage[SVSTAT_GAME].max, (uint32_t)( SVSTATS_HISTORY + extra - 1 ) );

	const svStatsSummary_t latest = SummarizeLatest( 10 );
	EXPECT_EQ( latest.stage[SVSTAT_GAME].max, (uint32_t)( SVSTATS_HISTORY + extra - 1 ) );
	EXPECT_EQ( latest.stage[SVSTAT_GAME].avg, (uint32_t)( SVSTATS_HISTORY + extra - 1 ) - 4 );	// mean of the last 10

	// the oldest frame kept is number `extra`
	std::vector<uint32_t> game;
	for ( int i = 0; i < SVSTATS_HISTORY; i++ ) {
		game.push_back( history->frames[i].usec[SVSTAT_GAME] );
	}
	EXPECT_EQ( *std::min_element( game.begin(), game.end() ), (uint32_t)extra );
}

TEST_F(SVStatsTest, FramesWithinATimeWindow) {
	// a frame every 50 ms from 1000 to 5950
	for ( int i = 0; i < 100; i++ ) {
		RecordGame( 1000 + i * 50, 10 );
	}
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 5950, 1000 ), 20 );		// ages 0 to 950
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 5960, 1000 ), 20 );		// ages 10 to 960
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 5951, 1 ), 0 );
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 5950, 1 ), 1 );
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 5950, 60000 ), 100 );
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 5950, 0 ), 0 );
	// a frame stamped after the query time still counts as recent
	EXPECT_EQ( SVStats_FramesWithin( history.get(), 5940, 100 ), 3 );
}

TEST_F(SVStatsTest, FramesWithinAcrossTheClockWrap) {
	// the millisecond clock wraps from 0xFFFFFFFF to 0
	const uint32_t start = 0xFFFFFFFFu - 120;
	for ( int i = 0; i < 6; i++ ) {
		RecordGame( start + i * 50, 10 );		// the last three are past the wrap
	}
	const uint32_t now = start + 250;			// small, after the wrap
	EXPECT_LT( now, start );
	EXPECT_EQ( SVStats_FramesWithin( history.get(), now, 120 ), 3 );
	EXPECT_EQ( SVStats_FramesWithin( history.get(), now, 1000 ), 6 );

	const svStatsSummary_t s = SummarizeLatest( 4 );
	EXPECT_EQ( s.spanMsec, 200u );
}

TEST_F(SVStatsTest, FramesSinceACount) {
	const uint32_t before = history->recorded;
	for ( int i = 0; i < 7; i++ ) {
		RecordGame( 1000 + i * 50, 10 );
	}
	EXPECT_EQ( SVStats_FramesSince( history.get(), before ), 7 );
	EXPECT_EQ( SVStats_FramesSince( history.get(), history->recorded ), 0 );
	// never more than the history holds
	EXPECT_EQ( SVStats_FramesSince( history.get(), before - 1000 ), 7 );

	// the frame counter wraps
	history->recorded = 2;
	EXPECT_EQ( SVStats_FramesSince( history.get(), 0xFFFFFFFEu ), 4 );
}

TEST_F(SVStatsTest, SpanStartsAtTheFrameBefore) {
	for ( int i = 0; i < 10; i++ ) {
		RecordGame( 1000 + i * 50, 10 );
	}
	// the frame before the oldest one is known
	EXPECT_EQ( SummarizeLatest( 5 ).spanMsec, 250u );
	EXPECT_EQ( SummarizeLatest( 1 ).spanMsec, 50u );
	// the oldest frame of the history counts for its game time
	EXPECT_EQ( SummarizeLatest( 10 ).spanMsec, 9u * 50 + 50 );
}

TEST_F(SVStatsTest, SpanOfAnOldestFrameThatCaughtUp) {
	RecordGame( 4000, 10, 60, 50 );		// a map load: 3 s of game in one frame
	RecordGame( 4050, 10, 1, 50 );
	EXPECT_EQ( SummarizeLatest( 2 ).spanMsec, 50u + 3000 );
}

TEST_F(SVStatsTest, PacketsAndSnapshotsAreCounted) {
	SVStats_AddPacket( &pending, 120 );
	SVStats_AddPacket( &pending, 80 );
	SVStats_AddPacket( &pending, -5 );			// a clock going back adds no time
	SVStats_AddSnapshot( &pending, SVSTAT_SNAP_DELTA, 300, 40 );
	SVStats_AddSnapshot( &pending, SVSTAT_SNAP_FULL, 2000, 60 );
	SVStats_AddSnapshot( &pending, SVSTAT_SNAP_FALLBACK, 2500, 70 );
	SVStats_Record( history.get(), &pending, 1000, 1, 50, 0 );

	SVStats_AddSnapshot( &pending, SVSTAT_SNAP_DELTA, 200, 30 );
	SVStats_AddPacket( &pending, 50 );
	SVStats_Record( history.get(), &pending, 1050, 1, 50, 0 );

	const svStatsSummary_t s = SummarizeLatest( 2 );
	EXPECT_EQ( s.packets, 4u );
	EXPECT_EQ( s.stage[SVSTAT_PACKETS].max, 200u );
	EXPECT_EQ( s.stage[SVSTAT_PACKETS].avg, 125u );
	EXPECT_EQ( s.snapshots, 4u );
	EXPECT_EQ( s.fullSnapshots, 2u );
	EXPECT_EQ( s.fallbackSnapshots, 1u );
	EXPECT_EQ( s.snapshotBytes, 5000u );
	EXPECT_EQ( s.fullSnapshotBytes, 4500u );
	EXPECT_EQ( s.snapshotEntities, 200u );
}

TEST(SVStatsNames, StageNames) {
	EXPECT_STREQ( SVStats_StageName( SVSTAT_PACKETS ), "packets" );
	EXPECT_STREQ( SVStats_StageName( SVSTAT_GAME ), "game" );
	EXPECT_STREQ( SVStats_StageName( SVSTAT_TOTAL ), "total" );
	EXPECT_STREQ( SVStats_StageName( SVSTAT_NUM_STAGES ), "" );
	EXPECT_STREQ( SVStats_StageName( -1 ), "" );
}

TEST_F(SVStatsTest, CSVLineMatchesTheHeader) {
	char header[SVSTATS_CSV_SIZE], line[SVSTATS_CSV_SIZE];

	Record( 1000, 10, 1, 300, 1000, 200, 50, 5, 1, 25 );
	Record( 1025, 30, 3, 500, 3000, 400, 70, 7, 2, 25 );
	SVStats_AddSnapshot( &pending, SVSTAT_SNAP_FALLBACK, 2500, 70 );
	SVStats_AddPacket( &pending, 40 );
	Record( 1050, 0, 1, 300, 1000, 200, 50, 5, 1, 25 );
	const svStatsSummary_t s = SummarizeLatest( 2 );

	SVStats_CSVHeader( header, sizeof( header ) );
	SVStats_CSVLine( line, sizeof( line ), 1700000000u, 40, 1, 31, &s );

	ASSERT_GT( strlen( header ), 0u );
	EXPECT_EQ( header[strlen( header ) - 1], '\n' );
	EXPECT_EQ( line[strlen( line ) - 1], '\n' );
	EXPECT_EQ( CountChar( header, ',' ), CountChar( line, ',' ) );
	EXPECT_EQ( CountChar( header, '\n' ), 1 );
	EXPECT_EQ( CountChar( line, '\n' ), 1 );

	const std::vector<std::string> names = SplitCSV( header );
	const std::vector<std::string> values = SplitCSV( line );
	ASSERT_EQ( names.size(), values.size() );
	EXPECT_EQ( names.size(), 18u + 2 * SVSTAT_NUM_STAGES );

	// every value is a plain integer
	for ( size_t i = 0; i < values.size(); i++ ) {
		ASSERT_FALSE( values[i].empty() ) << names[i];
		EXPECT_EQ( values[i].find_first_not_of( "0123456789" ), std::string::npos ) << names[i];
	}

	auto value = [&]( const char *name ) -> long {
		for ( size_t i = 0; i < names.size(); i++ ) {
			if ( names[i] == name ) {
				return strtol( values[i].c_str(), nullptr, 10 );
			}
		}
		ADD_FAILURE() << "no column " << name;
		return -1;
	};
	EXPECT_EQ( value( "time" ), 1700000000 );
	EXPECT_EQ( value( "msec" ), 50 );
	EXPECT_EQ( value( "sv_fps" ), 40 );
	EXPECT_EQ( value( "humans" ), 1 );
	EXPECT_EQ( value( "bots" ), 31 );
	EXPECT_EQ( value( "frames" ), 2 );
	EXPECT_EQ( value( "game_frames" ), 3 );
	EXPECT_EQ( value( "catchup_frames" ), 1 );
	EXPECT_EQ( value( "max_game_frames" ), 2 );
	EXPECT_EQ( value( "over_budget" ), 0 );
	EXPECT_EQ( value( "packets" ), 1 );
	EXPECT_EQ( value( "snapshots" ), 1 );
	EXPECT_EQ( value( "full_snapshots" ), 1 );
	EXPECT_EQ( value( "fallback_snapshots" ), 1 );
	EXPECT_EQ( value( "snapshot_bytes" ), 2500 );
	EXPECT_EQ( value( "snapshot_entities" ), 70 );
	EXPECT_EQ( value( "game_avg_us" ), 2000 );
	EXPECT_EQ( value( "game_max_us" ), 3000 );
	EXPECT_EQ( value( "packets_max_us" ), 40 );
	EXPECT_EQ( value( "total_max_us" ), 30 + 3 + 500 + 3000 + 400 + 70 + 7 );
}

TEST(SVStatsCSV, SmallBufferStaysTerminated) {
	char buf[16];
	svStatsSummary_t s = {};

	memset( buf, 'x', sizeof( buf ) );
	SVStats_CSVHeader( buf, sizeof( buf ) );
	EXPECT_LT( strlen( buf ), sizeof( buf ) );
	memset( buf, 'x', sizeof( buf ) );
	SVStats_CSVLine( buf, sizeof( buf ), 1, 20, 0, 0, &s );
	EXPECT_LT( strlen( buf ), sizeof( buf ) );
	EXPECT_EQ( buf[strlen( buf ) - 1], '\n' );
}
