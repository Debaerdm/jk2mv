// test_sv_init_clients.cpp - the bound of sv_maxclients (SV_BoundMaxClients,
// SV_Startup and SV_ChangeMaxClients in src/server/sv_init_clients.h)
//
// Includes the real engine header. SV_SpawnServer calls SV_Startup or
// SV_ChangeMaxClients before it chooses the version, so the protocol row in
// force there is still the previous map's, or PROTOCOL_UNDEF's on a fresh
// server. Their cap is the engine's capacity, MAX_CLIENTS: the probes below
// run them under a smaller row (16 clients) and check that it does not lower
// the cvar (docs/PROTOCOL-1.05.md, step 1, and step 2 item 3).

#include <gtest/gtest.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <stdexcept>
#include <string>

#include "server/server.h"
#include "mv_limits_stub.h"

// ===========================================================================
// engine state and stubs
// ===========================================================================

serverStatic_t	svs;
server_t		sv;
cvar_t			*sv_maxclients;
cvar_t			*sv_snapshotEntityBudget;
cvar_t			*com_dedicated;

namespace {

struct TestCvar {
	cvar_t		cvar;
	std::string	string;
	std::string	latched;	// Cvar_SetLatched's value, taken by the next Cvar_Get
	bool		hasLatched;
};

std::map<std::string, TestCvar>	g_cvars;	// std::map keeps the addresses
int								g_clientsBytes;	// the size of the last TAG_CLIENTS Z_Malloc

void SetValue( TestCvar &tc, const char *value ) {
	tc.string = value;
	tc.cvar.string = tc.string.c_str();
	tc.cvar.value = (float)atof( value );
	tc.cvar.integer = atoi( value );
}

TestCvar &FindCvar( const char *name, const char *value ) {
	std::map<std::string, TestCvar>::iterator it = g_cvars.find( name );
	if ( it == g_cvars.end() ) {
		it = g_cvars.insert( std::make_pair( std::string( name ), TestCvar() ) ).first;
		memset( &it->second.cvar, 0, sizeof( it->second.cvar ) );
		it->second.cvar.name = it->first.c_str();
		it->second.hasLatched = false;
		SetValue( it->second, value );
	}
	return it->second;
}

} // namespace

// like the engine's: an existing cvar takes its latched value now
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	TestCvar &tc = FindCvar( name, value );
	if ( tc.hasLatched ) {
		tc.hasLatched = false;
		SetValue( tc, tc.latched.c_str() );
		tc.cvar.modified = qtrue;
		tc.cvar.modificationCount++;
	}
	return &tc.cvar;
}

// like the engine's Cvar_SetLatched on the CVAR_LATCH sv_maxclients of a
// running server: the value waits for the next Cvar_Get
void Cvar_SetLatched( const char *name, const char *value ) {
	TestCvar &tc = FindCvar( name, value );
	tc.latched = value;
	tc.hasLatched = true;
	tc.cvar.modified = qtrue;
	tc.cvar.modificationCount++;
}

void Cvar_Set( const char *name, const char *value ) {
	TestCvar &tc = FindCvar( name, value );
	SetValue( tc, value );
	tc.cvar.modified = qtrue;
	tc.cvar.modificationCount++;
}

char * QDECL va( const char *format, ... ) {
	static char	buffers[4][1024];
	static int	index;
	char		*buf = buffers[index++ & 3];
	va_list		ap;

	va_start( ap, format );
	vsnprintf( buf, sizeof( buffers[0] ), format, ap );
	va_end( ap );
	return buf;
}

void QDECL Com_Printf( const char *fmt, ... ) {
}

void QDECL Com_DPrintf( const char *fmt, ... ) {
}

void QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	char	text[1024];
	va_list	ap;

	va_start( ap, fmt );
	vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	throw std::runtime_error( text );
}

void *Z_Malloc( int size, memtag_t tag, qboolean zero ) {
	if ( tag == TAG_CLIENTS ) {
		g_clientsBytes = size;
	}
	return calloc( 1, size );
}

void Z_Free( void *ptr ) {
	free( ptr );
}

void *Hunk_AllocateTempMemory( int size ) {
	return malloc( size );
}

void Hunk_FreeTempMemory( void *buf ) {
	free( buf );
}

sharedEntity_t *SV_GentityNum( int num ) {
	throw std::runtime_error( "SV_GentityNum: no game module in this test" );
}

#include "server/sv_init_clients.h"

namespace {

class SvBoundMaxClients : public ::testing::Test {
protected:
	void SetUp() override {
		g_cvars.clear();
		g_clientsBytes = 0;
		memset( &svs, 0, sizeof( svs ) );
		sv_maxclients = Cvar_Get( "sv_maxclients", "8", 0 );
		com_dedicated = Cvar_Get( "dedicated", "1", 0 );
	}
	void TearDown() override {
		Z_Free( svs.clients );
		svs.clients = NULL;
	}
	static void SetMaxClients( int n ) {
		Cvar_Set( "sv_maxclients", va( "%i", n ) );
		sv_maxclients->modified = qfalse;
	}
};

TEST_F(SvBoundMaxClients, CapIsTheEngineCapacity) {
	SetMaxClients( MAX_CLIENTS + 1 );
	SV_BoundMaxClients( 1 );
	EXPECT_EQ( MAX_CLIENTS, sv_maxclients->integer );
	EXPECT_EQ( 32, sv_maxclients->integer );	// step 1 keeps the legacy value

	SetMaxClients( MAX_CLIENTS );
	SV_BoundMaxClients( 1 );
	EXPECT_EQ( MAX_CLIENTS, sv_maxclients->integer );
	EXPECT_FALSE( sv_maxclients->modified );
}

// the highest connected slot wins over a lower cvar
TEST_F(SvBoundMaxClients, ConnectedSlotsWin) {
	SetMaxClients( 4 );
	SV_BoundMaxClients( 10 );
	EXPECT_EQ( 10, sv_maxclients->integer );

	SetMaxClients( 0 );
	SV_BoundMaxClients( 1 );
	EXPECT_EQ( 1, sv_maxclients->integer );
}

// Probe: the row in force before the version is chosen is not the map's. A
// cap taken from it would cut the slots that the map's protocol allows.
TEST_F(SvBoundMaxClients, StaleRowDoesNotLowerTheCvar) {
	ScopedLimits small( kSmallLimits );

	SetMaxClients( MAX_CLIENTS );
	SV_BoundMaxClients( 1 );
	EXPECT_EQ( MAX_CLIENTS, sv_maxclients->integer );
	EXPECT_FALSE( sv_maxclients->modified );

	SetMaxClients( MAX_CLIENTS + 1 );
	SV_BoundMaxClients( 1 );
	EXPECT_EQ( MAX_CLIENTS, sv_maxclients->integer );
}

// Probe: a fresh server (SV_Startup) keeps sv_maxclients and allocates its
// slots whatever the row before the version choice says
TEST_F(SvBoundMaxClients, StartupAllocatesTheAskedSlots) {
	ScopedLimits small( kSmallLimits );

	SetMaxClients( MAX_CLIENTS );
	SV_Startup();
	EXPECT_EQ( MAX_CLIENTS, sv_maxclients->integer );
	ASSERT_TRUE( svs.clients != NULL );
	EXPECT_EQ( (int)sizeof( client_t ) * MAX_CLIENTS, g_clientsBytes );	// every loop goes up to sv_maxclients
	EXPECT_TRUE( svs.initialized );
}

// Probe: a running server raising sv_maxclients under a smaller row gets the
// new slots, its connected clients kept in place
TEST_F(SvBoundMaxClients, ChangeMaxClientsGrowsUnderAStaleRow) {
	ScopedLimits small( kSmallLimits );

	SetMaxClients( kSmallLimits.maxClients );
	SV_Startup();
	svs.clients[3].state = CS_ACTIVE;
	svs.clients[3].ping = 42;

	// a higher sv_maxclients on the console, then the next map
	Cvar_SetLatched( "sv_maxclients", va( "%i", MAX_CLIENTS ) );
	ASSERT_TRUE( sv_maxclients->modified );
	SV_ChangeMaxClients();
	// fatal: with a lower cvar, svs.clients kept its 16 slots and the last
	// check below would read past them
	ASSERT_EQ( MAX_CLIENTS, sv_maxclients->integer );
	ASSERT_EQ( (int)sizeof( client_t ) * MAX_CLIENTS, g_clientsBytes );	// reallocated for the new slots
	EXPECT_EQ( CS_ACTIVE, svs.clients[3].state );
	EXPECT_EQ( 42, svs.clients[3].ping );
	EXPECT_EQ( CS_FREE, svs.clients[MAX_CLIENTS - 1].state );
}

} // namespace
