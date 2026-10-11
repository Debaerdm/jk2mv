// test_mv_limits.cpp - the limits of the protocol in use (qcommon/mv_limits.h).
// Step 1 of docs/PROTOCOL-1.05.md keeps 32 clients and 1024 entities for
// every protocol, and the configstring layout of the 1.02-1.04 modules. The
// expected values are literals on purpose: raising MAX_CLIENTS or
// MAX_GENTITIES must not move them.

#include <gtest/gtest.h>
#include <climits>

#include "qcommon/qcommon.h"
#include "game/bg_public.h"
#include "mv_limits_stub.h"

namespace {

TEST(MvLimits, EveryProtocolHasTheLegacyRow) {
	const int	protocols[] = { PROTOCOL_UNDEF, PROTOCOL15, PROTOCOL16, -1, 1, 14, 17, 18, 255, INT_MIN, INT_MAX };

	for ( size_t i = 0; i < ARRAY_LEN( protocols ); i++ ) {
		const mvprotocolLimits_t *limits = MV_LimitsForProtocol( protocols[i] );

		EXPECT_EQ( limits->maxClients, 32 ) << protocols[i];
		EXPECT_EQ( limits->gentityBits, 10 ) << protocols[i];
		EXPECT_EQ( limits->maxGEntities, 1024 ) << protocols[i];
	}
}

TEST(MvLimits, EveryRowFitsTheEngine) {
	for ( int p = -2; p <= 300; p++ ) {
		const mvprotocolLimits_t *limits = MV_LimitsForProtocol( p );

		EXPECT_TRUE( MV_LIMITS_ROW_VALID( limits->maxClients, limits->gentityBits, limits->maxGEntities ) ) << p;
	}
	EXPECT_TRUE( MV_LIMITS_ROW_VALID( kSmallLimits.maxClients, kSmallLimits.gentityBits, kSmallLimits.maxGEntities ) );

	EXPECT_FALSE( MV_LIMITS_ROW_VALID( MAX_CLIENTS + 1, 10, 1024 ) );		// past the client arrays
	EXPECT_FALSE( MV_LIMITS_ROW_VALID( 32, GENTITYNUM_BITS + 1, MAX_GENTITIES * 2 ) );	// past the entity arrays
	EXPECT_FALSE( MV_LIMITS_ROW_VALID( 32, 10, 1000 ) );					// not the range of the bits
	EXPECT_FALSE( MV_LIMITS_ROW_VALID( 32, 5, 32 ) );						// no room for the world and none
}

// The 64-client masks of the module ABI (broadcastClients[2], the mind trick
// masks) bound a row whatever the engine's arrays hold. While MAX_CLIENTS is
// 64 or less, the array bound rejects 65 clients first, so the probe moves it
// out of the way: MV_LIMITS_ROW_VALID reads MAX_CLIENTS where it is used.
TEST(MvLimits, RowFitsTheModuleMasks) {
#pragma push_macro("MAX_CLIENTS")
#undef MAX_CLIENTS
#define MAX_CLIENTS 128
	EXPECT_TRUE( MV_LIMITS_ROW_VALID( 64, 10, 1024 ) );
	EXPECT_FALSE( MV_LIMITS_ROW_VALID( 65, 10, 1024 ) );		// past broadcastClients[2]
	EXPECT_FALSE( MV_LIMITS_ROW_VALID( 128, 10, 1024 ) );
#pragma pop_macro("MAX_CLIENTS")
}

TEST(MvLimits, AccessorsFollowTheCurrentRow) {
	EXPECT_EQ( MV_MaxClients(), 32 );
	EXPECT_EQ( MV_MaxGEntities(), 1024 );
	EXPECT_EQ( MV_ENTITYNUM_NONE, 1023 );
	EXPECT_EQ( MV_ENTITYNUM_WORLD, 1022 );

	ScopedLimits small( kSmallLimits );
	EXPECT_EQ( MV_MaxClients(), 16 );
	EXPECT_EQ( MV_MaxGEntities(), 512 );
	EXPECT_EQ( MV_ENTITYNUM_NONE, 511 );
	EXPECT_EQ( MV_ENTITYNUM_WORLD, 510 );
}

// the 1.02-1.04 modules' configstrings: CS_CHARSKINS and the ones after it
// follow MAX_CLIENTS in bg_public.h
TEST(MvLimits, LegacyConfigstringLayout) {
	EXPECT_EQ( CS_PLAYERS, 544 );
	EXPECT_EQ( CS_PLAYERS + MV_MaxClients(), 576 );
	EXPECT_EQ( CS_CHARSKINS, 576 );
	EXPECT_EQ( CS_STRING_PACKAGES, 1024 );
	EXPECT_EQ( CS_MAX, 1054 );
}

} // namespace
