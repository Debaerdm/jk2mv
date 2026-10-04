// test_sv_usercmd_rate.cpp - usercmd accounting and the sv_maxUsercmdRate cap
//
// Links the real engine source (src/server/sv_usercmd_rate.cpp), which has no
// engine dependency. The client model sends usercmds the way
// CL_CreateNewCommands and CL_WritePacket do.

#include <gtest/gtest.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "server/sv_usercmd_rate.h"

namespace {

const int START_TIME = 10000;

// the loop of SV_UserMove before sv_maxUsercmdRate, as the reference:
// SV_ClientThink stores each usercmd it runs as the last one
std::vector<int> LegacyRun( const std::vector<int> &serverTimes, int &lastServerTime ) {
	std::vector<int> ran;
	const int count = (int)serverTimes.size();

	for ( int i = 0; i < count; i++ ) {
		if ( serverTimes[i] > serverTimes[count - 1] ) {
			continue;
		}
		if ( serverTimes[i] <= lastServerTime ) {
			continue;
		}
		ran.push_back( i );
		lastServerTime = serverTimes[i];
	}
	return ran;
}

// one client as SV_UserMove sees it, with the cap
struct ServerModel {
	usercmdBucket_t		bucket;
	int					rate;
	int					lastServerTime;
	long long			dropped;
	std::vector<int>	ranTimes;

	ServerModel( int rate_, int lastServerTime_ ) : rate( rate_ ), lastServerTime( lastServerTime_ ), dropped( 0 ) {
		memset( &bucket, 0, sizeof( bucket ) );
	}

	// returns the usercmds run, as indices into the packet
	std::vector<int> Receive( const std::vector<int> &packet, int now ) {
		int					run[32];
		int					packetDropped = 0;
		std::vector<int>	ran;
		const int			numRun = SV_UsercmdSchedule( &bucket, rate, now, packet.data(), (int)packet.size(),
								lastServerTime, run, &packetDropped );

		dropped += packetDropped;
		for ( int i = 0; i < numRun; i++ ) {
			ran.push_back( run[i] );
			ranTimes.push_back( packet[run[i]] );
			lastServerTime = packet[run[i]];
		}
		return ran;
	}
};

// a client running at a fixed frame rate: one usercmd per frame, and a packet
// every packetMsec (0: every frame, as on a LAN) with the usercmds made since
// the packet 1 + packetdup packets back, 32 at most
struct ClientModel {
	int					frameMsec;
	int					packetMsec;
	int					packetdup;
	int					time;
	int					lastPacketTime;
	std::vector<int>	cmds;
	std::vector<size_t>	sentAt;

	ClientModel( int frameMsec_, int packetMsec_, int packetdup_ ) :
		frameMsec( frameMsec_ ), packetMsec( packetMsec_ ), packetdup( packetdup_ ),
		time( START_TIME ), lastPacketTime( START_TIME ) {
	}

	// runs a frame; returns true with a packet to send
	bool Frame( std::vector<int> &packet ) {
		time += frameMsec;
		cmds.push_back( time );

		if ( packetMsec && time - lastPacketTime < packetMsec ) {
			return false;
		}

		const size_t back = 1 + packetdup;
		size_t from = sentAt.size() >= back ? sentAt[sentAt.size() - back] : 0;
		if ( cmds.size() - from > 32 ) {
			from = cmds.size() - 32;
		}
		packet.assign( cmds.begin() + from, cmds.end() );
		sentAt.push_back( cmds.size() );
		lastPacketTime = time;
		return true;
	}
};

struct Result {
	long long	made;		// usercmds the client made and sent
	long long	ran;		// usercmds the server ran
	long long	dropped;	// SV_UsercmdSchedule's count
	long long	lost;		// usercmds neither run nor seen by the server
	int			maxLag;		// most msec the last usercmd run trailed a packet's newest
	int			maxGap;		// longest msec between two usercmds run
};

// the server's clock is the client's, plus a stall of stallMsec every
// stallEvery msec during which packets queue up and then arrive together
Result Simulate( int rate, int frameMsec, int packetMsec, int packetdup, int durationMsec,
		int stallEvery = 0, int stallMsec = 0 ) {
	ClientModel							client( frameMsec, packetMsec, packetdup );
	ServerModel							server( rate, START_TIME );
	std::vector< std::vector<int> >		held;
	std::vector<int>					packet;
	Result								result;

	memset( &result, 0, sizeof( result ) );

	while ( client.time < START_TIME + durationMsec || !held.empty() ) {
		if ( !client.Frame( packet ) ) {
			continue;
		}

		const int now = client.time;
		const bool stalled = stallEvery && ( now - START_TIME ) % stallEvery < stallMsec;

		held.push_back( packet );
		if ( stalled ) {
			continue;
		}

		for ( size_t i = 0; i < held.size(); i++ ) {
			server.Receive( held[i], now );
			const int lag = held[i].back() - server.lastServerTime;
			if ( lag > result.maxLag ) {
				result.maxLag = lag;
			}
		}
		held.clear();
	}

	// the usercmds made after the last packet were never sent
	result.made = client.sentAt.empty() ? 0 : (long long)client.sentAt.back();
	result.ran = (long long)server.ranTimes.size();
	result.dropped = server.dropped;

	int previous = START_TIME;
	for ( size_t i = 0; i < server.ranTimes.size(); i++ ) {
		EXPECT_GT( server.ranTimes[i], previous ) << "usercmds must run in serverTime order";
		if ( server.ranTimes[i] - previous > result.maxGap ) {
			result.maxGap = server.ranTimes[i] - previous;
		}
		previous = server.ranTimes[i];
	}
	result.lost = result.made - result.ran - result.dropped;
	return result;
}

std::vector<int> RandomPacket( int base ) {
	std::vector<int> packet;
	const int count = 1 + rand() % 32;

	for ( int i = 0; i < count; i++ ) {
		switch ( rand() % 4 ) {
		case 0:		// out of order or repeated
			packet.push_back( base + rand() % 20 - 10 );
			break;
		default:
			base += rand() % 4;
			packet.push_back( base );
			break;
		}
	}
	return packet;
}

} // namespace

// ============================================================================
// The cvar value
// ============================================================================

TEST(SvUsercmdRate, CapFromCvar) {
	EXPECT_EQ( SV_UsercmdRateCap( 0 ), 0 );
	EXPECT_EQ( SV_UsercmdRateCap( -5 ), 0 );
	EXPECT_EQ( SV_UsercmdRateCap( 1 ), USERCMD_RATE_MIN );
	EXPECT_EQ( SV_UsercmdRateCap( 333 ), USERCMD_RATE_MIN );
	EXPECT_EQ( SV_UsercmdRateCap( 499 ), 500 );
	EXPECT_EQ( SV_UsercmdRateCap( 500 ), 500 );
	EXPECT_EQ( SV_UsercmdRateCap( 1000 ), 1000 );
	EXPECT_EQ( SV_UsercmdBurst( 500 ), 250 );
	EXPECT_EQ( SV_UsercmdBurst( 1000 ), 500 );
}

// ============================================================================
// Same usercmds as before when the cap does not bite
// ============================================================================

TEST(SvUsercmdRate, NoCapRunsWhatSvUserMoveRan) {
	srand( 1234 );
	for ( int trial = 0; trial < 200; trial++ ) {
		ServerModel	server( 0, START_TIME );
		int			legacyLast = START_TIME;
		int			base = START_TIME;

		for ( int p = 0; p < 50; p++ ) {
			const std::vector<int> packet = RandomPacket( base );
			base = packet.back() > base ? packet.back() : base;

			const std::vector<int> expected = LegacyRun( packet, legacyLast );
			const std::vector<int> ran = server.Receive( packet, START_TIME + p * 8 );
			ASSERT_EQ( ran, expected );
		}
		EXPECT_EQ( server.dropped, 0 );
	}
}

TEST(SvUsercmdRate, AmpleCreditRunsWhatSvUserMoveRan) {
	srand( 4321 );
	for ( int trial = 0; trial < 200; trial++ ) {
		ServerModel	server( 1000000, START_TIME );
		int			legacyLast = START_TIME;
		int			base = START_TIME;

		for ( int p = 0; p < 50; p++ ) {
			const std::vector<int> packet = RandomPacket( base );
			base = packet.back() > base ? packet.back() : base;

			const std::vector<int> expected = LegacyRun( packet, legacyLast );
			const std::vector<int> ran = server.Receive( packet, START_TIME + p * 8 );
			ASSERT_EQ( ran, expected );
		}
		EXPECT_EQ( server.dropped, 0 );
	}
}

// ============================================================================
// Legitimate clients at the lowest cap
// ============================================================================

TEST(SvUsercmdRate, LegitimateClientsAreNeverDropped) {
	struct Case { int frameMsec, packetMsec, packetdup; };
	const Case cases[] = {
		{ 8, 0, 1 },		// com_maxfps 125 on a LAN: a packet per frame
		{ 8, 16, 1 },		// com_maxfps 125, cl_maxpackets 60
		{ 4, 0, 1 },		// com_maxfps 250
		{ 3, 0, 1 },		// com_maxfps 333
		{ 3, 33, 1 },		// com_maxfps 333, cl_maxpackets 30
		{ 3, 66, 5 },		// com_maxfps 333, cl_maxpackets 15, cl_packetdup 5
		{ 3, 10, 0 },		// com_maxfps 333, cl_maxpackets 100, no duplicates
	};

	for ( size_t i = 0; i < sizeof( cases ) / sizeof( cases[0] ); i++ ) {
		const Case &c = cases[i];
		const Result r = Simulate( SV_UsercmdRateCap( 1 ), c.frameMsec, c.packetMsec, c.packetdup, 60000 );

		EXPECT_EQ( r.dropped, 0 ) << "case " << i;
		EXPECT_EQ( r.ran, r.made ) << "case " << i;
		EXPECT_EQ( r.maxGap, c.frameMsec ) << "case " << i;
	}
}

TEST(SvUsercmdRate, NetworkStallsDoNotTripTheCap) {
	// 333 fps, everything held for 600 ms every 5 seconds, then delivered at once
	const Result r = Simulate( SV_UsercmdRateCap( 1 ), 3, 0, 1, 60000, 5000, 600 );

	EXPECT_EQ( r.dropped, 0 );
	EXPECT_EQ( r.ran, r.made );
}

// ============================================================================
// The cap
// ============================================================================

TEST(SvUsercmdRate, CapsAThousandFpsClient) {
	// 1 ms frames and a packet every frame: 1000 usercmds per second
	const int		rate = 500;
	const int		seconds = 10;
	const Result	r = Simulate( rate, 1, 0, 1, seconds * 1000 );

	EXPECT_LE( r.ran, (long long)rate * seconds + SV_UsercmdBurst( rate ) + 1 );
	EXPECT_GE( r.ran, (long long)rate * seconds - 1 );
	// every usercmd is either run or counted as dropped
	EXPECT_EQ( r.lost, 0 );
	EXPECT_EQ( r.ran + r.dropped, r.made );
	// the newest usercmds run, so the server never trails the client
	EXPECT_LE( r.maxLag, 2 );
	EXPECT_LE( r.maxGap, 2 );
}

TEST(SvUsercmdRate, RunsTheNewestOfAPacket) {
	ServerModel	server( 500, START_TIME );
	int			last = START_TIME;

	// use up the whole burst first
	for ( int p = 0; p < 8; p++ ) {
		std::vector<int> burst;
		for ( int i = 0; i < 32; i++ ) {
			burst.push_back( ++last );
		}
		server.Receive( burst, START_TIME );
	}
	EXPECT_EQ( (int)server.ranTimes.size(), 250 );

	// 10 ms later the client may run 5 more
	std::vector<int> next;
	for ( int i = 0; i < 12; i++ ) {
		next.push_back( last + 1 + i );
	}
	const std::vector<int> ran = server.Receive( next, START_TIME + 10 );
	const int expected[] = { 7, 8, 9, 10, 11 };
	ASSERT_EQ( ran, std::vector<int>( expected, expected + 5 ) );
	EXPECT_EQ( server.lastServerTime, next.back() );
}

TEST(SvUsercmdRate, FloodWithForgedTimesIsCapped) {
	// 32 new usercmds per packet, ten packets per msec, with serverTime
	// running far ahead of the clock
	const int	rate = 1000;
	const int	seconds = 5;
	ServerModel	server( rate, START_TIME );
	int			serverTime = START_TIME;
	long long	made = 0;

	for ( int now = 0; now < seconds * 1000; now++ ) {
		for ( int p = 0; p < 10; p++ ) {
			std::vector<int> packet;
			for ( int i = 0; i < 32; i++ ) {
				packet.push_back( ++serverTime );
			}
			made += 32;
			server.Receive( packet, START_TIME + now );
		}
	}

	EXPECT_LE( (long long)server.ranTimes.size(), (long long)rate * seconds + SV_UsercmdBurst( rate ) );
	EXPECT_GE( (long long)server.ranTimes.size(), (long long)rate * ( seconds - 1 ) );
	EXPECT_EQ( (long long)server.ranTimes.size() + server.dropped, made );
}

TEST(SvUsercmdRate, DroppedCountsEachUsercmdOnce) {
	// duplicated and lost packets: each usercmd the server saw and did not
	// run counts once, even when it came again or ran from a later packet
	srand( 99 );
	ClientModel			client( 1, 0, 2 );
	ServerModel			server( 500, START_TIME );
	std::set<int>		seen;
	std::vector<int>	packet;
	int					now = START_TIME;

	while ( client.time < START_TIME + 20000 ) {
		if ( !client.Frame( packet ) ) {
			continue;
		}
		if ( rand() % 5 == 0 ) {
			continue;	// lost
		}
		now = client.time;
		seen.insert( packet.begin(), packet.end() );
		server.Receive( packet, now );
	}

	EXPECT_GT( server.dropped, 0 );
	EXPECT_EQ( (long long)seen.size(), (long long)server.ranTimes.size() + server.dropped );
}

TEST(SvUsercmdRate, CreditStaysRightAcrossTheClockWrap) {
	usercmdBucket_t	bucket;
	int				serverTimes[32];
	int				run[32];
	int				dropped;

	memset( &bucket, 0, sizeof( bucket ) );
	for ( int i = 0; i < 32; i++ ) {
		serverTimes[i] = i + 1;
	}

	// take the whole burst just before the wrap
	int taken = 0;
	for ( int p = 0; p < 10; p++ ) {
		taken += SV_UsercmdSchedule( &bucket, 500, INT_MAX - 15, serverTimes, 32, 0, run, &dropped );
	}
	EXPECT_EQ( taken, 250 );

	// 32 ms later, on the other side of the wrap: 16 usercmds of credit
	const int now = (int)( (unsigned)INT_MAX + 17u );
	EXPECT_EQ( SV_UsercmdSchedule( &bucket, 500, now, serverTimes, 32, 0, run, &dropped ), 16 );

	// a clock going back refills it instead of blocking the client
	const int before = (int)( (unsigned)now - 5000u );
	EXPECT_EQ( SV_UsercmdSchedule( &bucket, 500, before, serverTimes, 32, 0, run, &dropped ), 32 );
}

// ============================================================================
// Statistics
// ============================================================================

TEST(SvUsercmdRate, StatsRollScalesToOneSecond) {
	usercmdStats_t stats;
	memset( &stats, 0, sizeof( stats ) );

	stats.current.packets = 150;
	stats.current.cmds = 375;
	stats.current.dropped = 25;
	stats.current.thinkUsec = 2500;
	SV_UsercmdStatsRoll( &stats, 1250 );

	EXPECT_EQ( stats.last.packets, 120 );
	EXPECT_EQ( stats.last.cmds, 300 );
	EXPECT_EQ( stats.last.dropped, 20 );
	EXPECT_EQ( stats.last.thinkUsec, 2000 );
	EXPECT_EQ( stats.peak.cmds, 300 );
	EXPECT_EQ( stats.totalDropped, 25 );
	EXPECT_EQ( stats.current.cmds, 0 );

	// a quieter second keeps the peaks; usercmds counted as dropped that ran
	// in the next second take the count below zero, shown as none
	stats.current.cmds = 100;
	stats.current.dropped = -5;
	SV_UsercmdStatsRoll( &stats, 1000 );

	EXPECT_EQ( stats.last.cmds, 100 );
	EXPECT_EQ( stats.last.dropped, 0 );
	EXPECT_EQ( stats.peak.cmds, 300 );
	EXPECT_EQ( stats.peak.dropped, 20 );
	EXPECT_EQ( stats.totalDropped, 20 );
}
