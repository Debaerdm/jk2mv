// test_sv_demo.cpp - server-side demos (svrecord, svstoprecord, sv_autoRecord)
//
// Links the real engine sources: src/server/sv_demo.cpp, which uses the
// entity delta of the network snapshots, and the message code (msg.cpp,
// huffman.cpp).  Demos are recorded through the engine functions into
// in-memory files that stand for sv_demo_writer.cpp (test_sv_demo_writer)
// and read back the way the client parses them (CL_ReadDemoMessage,
// CL_ParseServerMessage), with the engine's MSG_Read* functions.

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "server/server.h"

// ===========================================================================
// engine state and stubs
// ===========================================================================

serverStatic_t	svs;
server_t		sv;

static cvar_t	maxclientsCvar, mapnameCvar, autoRecordCvar, runningCvar, shownetCvar, debugMessageCvar;
cvar_t	*sv_maxclients = &maxclientsCvar;
cvar_t	*sv_mapname = &mapnameCvar;
cvar_t	*sv_autoRecord = &autoRecordCvar;
cvar_t	*com_sv_running = &runningCvar;
cvar_t	*cl_shownet = &shownetCvar;
cvar_t	*com_debugMessage = &debugMessageCvar;

static std::string	consoleLog;

void QDECL Com_Printf( const char *fmt, ... ) {
	char	buf[4096];
	va_list	ap;

	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	consoleLog += buf;
}

void QDECL Com_DPrintf( const char *fmt, ... ) {
	char	buf[4096];
	va_list	ap;

	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	consoleLog += buf;
}

void QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	char	buf[4096];
	va_list	ap;

	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	throw std::runtime_error( buf );
}

static mvversion_t	gameVersion = VERSION_1_04;

mvversion_t MV_GetCurrentGameversion() {
	return gameVersion;
}

mvprotocol_t MV_GetCurrentProtocol() {
	return gameVersion == VERSION_1_04 ? PROTOCOL16 : PROTOCOL15;
}

int Com_RealTime( qtime_t *t ) {
	if ( t ) {
		memset( t, 0, sizeof( *t ) );
		t->tm_year = 126;
		t->tm_mon = 9;
		t->tm_mday = 4;
		t->tm_hour = 17;
		t->tm_min = 15;
		t->tm_sec = 30;
	}
	return 0;
}

// the Huffman tables are built, not read from or saved to a cache file
unsigned Com_BlockChecksum( const void *, int ) {
	return 0;
}

int FS_SV_FOpenFileRead( const char *, fileHandle_t *fp, module_t ) {
	*fp = 0;
	return -1;
}

fileHandle_t FS_SV_FOpenFileWrite( const char *, module_t ) {
	return 0;
}

int FS_Read( void *, int, fileHandle_t, module_t ) {
	return 0;
}

// for the Huffman cache of msg.cpp, which FS_SV_FOpenFileWrite turns off
int FS_Write( const void *, int, fileHandle_t, module_t ) {
	throw std::runtime_error( "FS_Write" );
}

void FS_FCloseFile( fileHandle_t, module_t ) {
	throw std::runtime_error( "FS_FCloseFile" );
}

// --- in-memory demos/ -------------------------------------------------------
//
// The files of sv_demo_writer.cpp, which test_sv_demo_writer tests, written at
// once.

struct OpenFile {
	std::string					name;
	std::vector<unsigned char>	data;
};

static std::map<int, OpenFile>							openFiles;
static std::map<std::string, std::vector<unsigned char> >	disk;		// closed files
static std::set<std::string>								removed;
static int		nextHandle = 1;
static bool		failOpens;		// as the thread fails to create a file, later
static bool		failWrites;
static int		writerShutdowns;

struct svDemoFile_s {
	int		handle;		// in openFiles
	bool	failed;
};

svDemoFile_t *SV_DemoFileOpen( const char *qpath ) {
	OpenFile		f;
	svDemoFile_t	*file = new svDemoFile_t;

	f.name = qpath;
	openFiles[nextHandle] = f;
	file->handle = nextHandle++;
	file->failed = failOpens;
	return file;
}

void SV_DemoFileWrite( svDemoFile_t *file, const void *data, int len ) {
	if ( !openFiles.count( file->handle ) ) {
		throw std::runtime_error( "write to a closed demo file" );
	}
	if ( failWrites ) {
		file->failed = true;
		return;
	}
	const unsigned char *p = (const unsigned char *)data;
	openFiles[file->handle].data.insert( openFiles[file->handle].data.end(), p, p + len );
}

qboolean SV_DemoFileFailed( const svDemoFile_t *file ) {
	return file->failed ? qtrue : qfalse;
}

void SV_DemoFileClose( svDemoFile_t *file, qboolean discard ) {
	if ( !openFiles.count( file->handle ) ) {
		throw std::runtime_error( "demo file closed twice" );
	}
	if ( discard ) {
		removed.insert( openFiles[file->handle].name );
	} else {
		disk[openFiles[file->handle].name] = openFiles[file->handle].data;
	}
	openFiles.erase( file->handle );
	delete file;
}

qboolean SV_DemoFileInUse( const char *qpath ) {
	for ( std::map<int, OpenFile>::const_iterator it = openFiles.begin(); it != openFiles.end(); ++it ) {
		if ( it->second.name == qpath ) {
			return qtrue;
		}
	}
	return qfalse;
}

void SV_DemoFilesShutdown( void ) {
	writerShutdowns++;
}

// what is on the disk: the open demos are for SV_DemoFileInUse
qboolean FS_FileExists( const char *file ) {
	return disk.count( file ) ? qtrue : qfalse;
}

// --- command line ------------------------------------------------------------

static std::vector<std::string>	args;

int Cmd_Argc( void ) {
	return (int)args.size();
}

char *Cmd_Argv( int arg ) {
	static char	empty[1];

	if ( arg < 0 || arg >= (int)args.size() ) {
		return empty;
	}
	return &args[arg][0];
}

static void Command( const char *line ) {
	char	buf[1024];

	args.clear();
	Q_strncpyz( buf, line, sizeof( buf ) );
	for ( char *tok = strtok( buf, " " ); tok; tok = strtok( NULL, " " ) ) {
		args.push_back( tok );
	}
	if ( args[0] == "svrecord" ) {
		SV_Record_f();
	} else {
		SV_StopRecord_f();
	}
}

// ===========================================================================
// a small server
// ===========================================================================

static client_t						clientSlots[MAX_CLIENTS];
static std::vector<entityState_t>	ring;

static entityState_t Ent( int number, float x, float y = 0, int event = 0 ) {
	entityState_t	s;

	memset( &s, 0, sizeof( s ) );
	s.number = number;
	s.eType = ET_GENERAL;
	s.modelindex = 3;
	s.pos.trType = TR_STATIONARY;
	s.pos.trBase[0] = x;
	s.pos.trBase[1] = y;
	s.event = event;
	return s;
}

static void ResetServer() {
	SV_DemoStopAll( "", qtrue );

	memset( &svs, 0, sizeof( svs ) );
	memset( &sv, 0, sizeof( sv ) );
	memset( clientSlots, 0, sizeof( clientSlots ) );
	svs.clients = clientSlots;

	ring.assign( 4096, Ent( 0, 0 ) );
	svs.snapshotEntities = &ring[0];
	svs.numSnapshotEntities = (int)ring.size();
	svs.nextSnapshotEntities = 0;
	svs.time = 20000;
	sv.time = 20000;
	sv.checksumFeed = 0x5eed;

	for ( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		sv.configstrings[i] = "";
	}
	sv.configstrings[CS_SERVERINFO] = "\\mapname\\ffa_test\\sv_hostname\\unit test";
	sv.configstrings[CS_SYSTEMINFO] = "\\sv_serverid\\4242";
	sv.configstrings[CS_MODELS + 1] = "models/map_objects/test.md3";

	sv.svEntities[40].baseline = Ent( 40, 64 );
	sv.svEntities[41].baseline = Ent( 41, 128 );
	sv.svEntities[300].baseline = Ent( 300, -512, 256 );

	maxclientsCvar.integer = 8;
	mapnameCvar.string = (char *)"ffa_test";
	autoRecordCvar.integer = 0;
	runningCvar.integer = 1;
	gameVersion = VERSION_1_04;

	openFiles.clear();
	disk.clear();
	removed.clear();
	failOpens = false;
	failWrites = false;
	writerShutdowns = 0;
	consoleLog.clear();
}

static client_t *Join( int num, const char *name, bool bot ) {
	client_t	*cl = &svs.clients[num];

	cl->state = CS_ACTIVE;
	Q_strncpyz( cl->name, name, sizeof( cl->name ) );
	cl->netchan.remoteAddress.type = bot ? NA_BOT : NA_IP;
	cl->netchan.outgoingSequence = 700 + num;
	cl->reliableSequence = 20;
	cl->reliableAcknowledge = 18;
	cl->lastClientCommand = 7;
	for ( int i = 1; i <= cl->reliableSequence; i++ ) {
		Com_sprintf( cl->reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )], MAX_STRING_CHARS, "print \"before %i\n\"", i );
	}
	return cl;
}

static void AddCommand( client_t *cl, const std::string &cmd ) {
	cl->reliableSequence++;
	Q_strncpyz( cl->reliableCommands[cl->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 )], cmd.c_str(), MAX_STRING_CHARS );
}

// a print of len printable characters that Huffman doesn't shrink much
// (no '%', which MSG_ReadString turns into '.')
static std::string BigPrint( int len, unsigned seed ) {
	std::string	s = "print \"";

	for ( int i = 0; i < len; i++ ) {
		seed = seed * 1103515245u + 12345u;
		char c = (char)( '&' + ( seed >> 16 ) % 86 );
		s += c == '\\' ? '/' : c;
	}
	return s + "\n\"";
}

// what a snapshot sent: the playerstate fields and entities the test sets
struct Sent {
	int							serverTime;
	int							commandTime;
	float						origin[3];
	int							health;
	std::vector<entityState_t>	ents;
	int							lastCommand;	// reliable commands queued up to here
};

// builds a snapshot for the client as SV_BuildClientSnapshot does, then
// calls the demo hook; a human's message goes out (outgoingSequence moves
// on), a bot's frame stays in the same slot
static Sent Snapshot( client_t *cl, int dt, const std::vector<entityState_t> &ents ) {
	clientSnapshot_t	*frame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];
	Sent				sent;

	sv.time += dt;
	svs.time += dt;

	memset( frame, 0, sizeof( *frame ) );
	frame->ps.commandTime = sv.time - 8;
	frame->ps.clientNum = (int)( cl - svs.clients );
	frame->ps.origin[0] = (float)( sv.time % 1000 );
	frame->ps.origin[1] = -40.0f;
	frame->ps.origin[2] = 24.25f;
	frame->ps.stats[STAT_HEALTH] = 100 - ( sv.time / 100 ) % 50;
	frame->areabytes = 2;
	frame->areabits[0] = 0xfe;
	frame->areabits[1] = 0x7f;
	frame->first_entity = svs.nextSnapshotEntities;
	frame->num_entities = (int)ents.size();
	for ( size_t i = 0; i < ents.size(); i++ ) {
		svs.snapshotEntities[svs.nextSnapshotEntities++ % svs.numSnapshotEntities] = ents[i];
	}

	sent.serverTime = sv.time;
	sent.commandTime = frame->ps.commandTime;
	memcpy( sent.origin, frame->ps.origin, sizeof( sent.origin ) );
	sent.health = frame->ps.stats[STAT_HEALTH];
	sent.ents = ents;
	sent.lastCommand = cl->reliableSequence;

	SV_DemoClientSnapshot( cl );

	if ( cl->netchan.remoteAddress.type != NA_BOT ) {
		cl->netchan.outgoingSequence++;
	}
	return sent;
}

// ===========================================================================
// reading a demo back like the client
// ===========================================================================

struct Snap {
	int								messageNum;
	int								serverTime;
	int								deltaNum;
	int								snapFlags;
	int								commandSequence;	// commands received before it
	playerState_t					ps;
	std::map<int, entityState_t>	ents;
};

struct Demo {
	bool								terminated;
	int									serverCommandSequence;
	int									clientNum;
	int									checksumFeed;
	std::map<int, std::string>			configstrings;
	std::map<int, entityState_t>		baselines;
	std::vector<int>					sequences;		// of every message
	std::vector<int>					sizes;
	std::vector<std::pair<int, std::string> >	commands;
	std::vector<Snap>					snaps;
};

static int ReadInt( const std::vector<unsigned char> &f, size_t at ) {
	return (int)( f[at] | ( f[at + 1] << 8 ) | ( f[at + 2] << 16 ) | ( (unsigned)f[at + 3] << 24 ) );
}

static void ParseSnapshot( Demo &demo, msg_t *m, int messageNum ) {
	Snap			s;
	const Snap		*old = NULL;
	unsigned char	areamask[MAX_MAP_AREA_BYTES];

	memset( &s.ps, 0, sizeof( s.ps ) );
	s.messageNum = messageNum;
	s.commandSequence = demo.commands.empty() ? demo.serverCommandSequence : demo.commands.back().first;
	s.serverTime = MSG_ReadLong( m );
	int delta = MSG_ReadByte( m );
	s.deltaNum = delta ? messageNum - delta : -1;
	s.snapFlags = MSG_ReadByte( m );

	if ( s.deltaNum > 0 ) {
		for ( size_t i = 0; i < demo.snaps.size(); i++ ) {
			if ( demo.snaps[i].messageNum == s.deltaNum ) {
				old = &demo.snaps[i];
			}
		}
		if ( !old ) {
			throw std::runtime_error( "delta from a snapshot the demo doesn't have" );
		}
		if ( delta >= PACKET_BACKUP ) {
			throw std::runtime_error( "delta from too far back" );
		}
	}

	int len = MSG_ReadByte( m );
	if ( len > (int)sizeof( areamask ) ) {
		throw std::runtime_error( "areamask too long" );
	}
	MSG_ReadData( m, areamask, len );

	MSG_ReadDeltaPlayerstate( m, old ? const_cast<playerState_t *>( &old->ps ) : NULL, &s.ps );

	// CL_ParsePacketEntities
	std::map<int, entityState_t>::const_iterator oldIt;
	if ( old ) {
		oldIt = old->ents.begin();
	}
	while ( 1 ) {
		int newnum = MSG_ReadBits( m, GENTITYNUM_BITS );
		if ( newnum == MAX_GENTITIES - 1 ) {
			break;
		}
		if ( m->readcount > m->cursize ) {
			throw std::runtime_error( "end of message in the entities" );
		}
		while ( old && oldIt != old->ents.end() && oldIt->first < newnum ) {
			s.ents[oldIt->first] = oldIt->second;
			++oldIt;
		}
		entityState_t from, to;
		memset( &to, 0, sizeof( to ) );
		if ( old && oldIt != old->ents.end() && oldIt->first == newnum ) {
			from = oldIt->second;
			++oldIt;
		} else if ( demo.baselines.count( newnum ) ) {
			from = demo.baselines[newnum];
		} else {
			memset( &from, 0, sizeof( from ) );
		}
		MSG_ReadDeltaEntity( m, &from, &to, newnum );
		if ( to.number != MAX_GENTITIES - 1 ) {
			s.ents[newnum] = to;
		}
	}
	while ( old && oldIt != old->ents.end() ) {
		s.ents[oldIt->first] = oldIt->second;
		++oldIt;
	}

	demo.snaps.push_back( s );
}

static void ParseGamestate( Demo &demo, msg_t *m ) {
	entityState_t	nullstate;

	memset( &nullstate, 0, sizeof( nullstate ) );
	demo.serverCommandSequence = MSG_ReadLong( m );
	while ( 1 ) {
		int cmd = MSG_ReadByte( m );
		if ( cmd == svc_EOF ) {
			break;
		}
		if ( cmd == svc_configstring ) {
			int i = MSG_ReadShort( m );
			demo.configstrings[i] = MSG_ReadBigString( m );
		} else if ( cmd == svc_baseline ) {
			int num = MSG_ReadBits( m, GENTITYNUM_BITS );
			entityState_t es;
			memset( &es, 0, sizeof( es ) );
			MSG_ReadDeltaEntity( m, &nullstate, &es, num );
			demo.baselines[num] = es;
		} else {
			throw std::runtime_error( "bad command byte in the gamestate" );
		}
	}
	demo.clientNum = MSG_ReadLong( m );
	demo.checksumFeed = MSG_ReadLong( m );
}

static Demo ReadDemo( const std::vector<unsigned char> &f ) {
	Demo	demo;
	size_t	at = 0;

	demo.terminated = false;
	demo.serverCommandSequence = -1;
	while ( at + 8 <= f.size() ) {
		int seq = ReadInt( f, at );
		int len = ReadInt( f, at + 4 );
		at += 8;
		if ( len == -1 ) {
			demo.terminated = true;
			break;
		}
		if ( len < 0 || len > MAX_MSGLEN || at + len > f.size() ) {
			throw std::runtime_error( "bad message length" );
		}
		demo.sequences.push_back( seq );
		demo.sizes.push_back( len );

		static byte	buf[MAX_MSGLEN];
		msg_t		m;
		MSG_Init( &m, buf, sizeof( buf ) );
		memcpy( buf, &f[at], len );
		m.cursize = len;
		at += len;

		MSG_Bitstream( &m );
		MSG_ReadLong( &m );		// reliable acknowledge
		while ( 1 ) {
			if ( m.readcount > m.cursize ) {
				throw std::runtime_error( "read past end of server message" );
			}
			int cmd = MSG_ReadByte( &m );
			if ( cmd == svc_EOF ) {
				break;
			}
			if ( cmd == svc_serverCommand ) {
				int cseq = MSG_ReadLong( &m );
				std::string s = MSG_ReadString( &m );
				int last = demo.commands.empty() ? demo.serverCommandSequence : demo.commands.back().first;
				if ( cseq > last ) {
					demo.commands.push_back( std::make_pair( cseq, s ) );
				} else {
					throw std::runtime_error( "a command came twice" );
				}
			} else if ( cmd == svc_gamestate ) {
				if ( demo.serverCommandSequence != -1 ) {
					throw std::runtime_error( "second gamestate" );
				}
				ParseGamestate( demo, &m );
			} else if ( cmd == svc_snapshot ) {
				if ( demo.serverCommandSequence == -1 ) {
					throw std::runtime_error( "snapshot before the gamestate" );
				}
				ParseSnapshot( demo, &m, seq );
			} else {
				throw std::runtime_error( "illegible server message" );
			}
		}
	}
	return demo;
}

static const std::vector<unsigned char> &File( const std::string &name ) {
	static const std::vector<unsigned char>	none;

	if ( !disk.count( name ) ) {
		ADD_FAILURE() << "no file " << name;
		return none;
	}
	return disk[name];
}

static bool IsOpen( const std::string &name ) {
	for ( std::map<int, OpenFile>::const_iterator it = openFiles.begin(); it != openFiles.end(); ++it ) {
		if ( it->second.name == name ) {
			return true;
		}
	}
	return false;
}

static void ExpectSnapshot( const Snap &got, const Sent &sent ) {
	EXPECT_EQ( got.serverTime, sent.serverTime );
	EXPECT_EQ( got.ps.commandTime, sent.commandTime );
	EXPECT_EQ( got.ps.origin[0], sent.origin[0] );
	EXPECT_EQ( got.ps.origin[1], sent.origin[1] );
	EXPECT_EQ( got.ps.origin[2], sent.origin[2] );
	EXPECT_EQ( got.ps.stats[STAT_HEALTH], sent.health );
	EXPECT_EQ( got.commandSequence, sent.lastCommand );
	ASSERT_EQ( got.ents.size(), sent.ents.size() );
	for ( size_t i = 0; i < sent.ents.size(); i++ ) {
		const entityState_t &e = sent.ents[i];
		ASSERT_TRUE( got.ents.count( e.number ) ) << "entity " << e.number;
		const entityState_t &g = got.ents.find( e.number )->second;
		EXPECT_EQ( g.eType, e.eType );
		EXPECT_EQ( g.modelindex, e.modelindex );
		EXPECT_EQ( g.pos.trBase[0], e.pos.trBase[0] );
		EXPECT_EQ( g.pos.trBase[1], e.pos.trBase[1] );
		EXPECT_EQ( g.pos.trBase[2], e.pos.trBase[2] );
		EXPECT_EQ( g.event, e.event );
	}
}

class SvDemo : public ::testing::Test {
protected:
	virtual void SetUp() {
		ResetServer();
	}
	virtual void TearDown() {
		SV_DemoStopAll( "", qtrue );
	}
};

static const char *AUTO_NAME_0 = "demos/20261004-171530_ffa_test_0_Padawan.dm_16";

// ===========================================================================
// file names
// ===========================================================================

TEST_F( SvDemo, NamesFromTheCommandLineStayInDemos ) {
	EXPECT_TRUE( SV_ValidDemoName( "match1" ) );
	EXPECT_TRUE( SV_ValidDemoName( "final.round-2_b" ) );
	EXPECT_TRUE( SV_ValidDemoName( std::string( 42, 'a' ).c_str() ) );

	EXPECT_FALSE( SV_ValidDemoName( "" ) );
	EXPECT_FALSE( SV_ValidDemoName( std::string( 43, 'a' ).c_str() ) );
	EXPECT_FALSE( SV_ValidDemoName( "../server" ) );
	EXPECT_FALSE( SV_ValidDemoName( "a..b" ) );
	EXPECT_FALSE( SV_ValidDemoName( "sub/name" ) );
	EXPECT_FALSE( SV_ValidDemoName( "sub\\name" ) );
	EXPECT_FALSE( SV_ValidDemoName( "c:name" ) );
	EXPECT_FALSE( SV_ValidDemoName( ".hidden" ) );
	EXPECT_FALSE( SV_ValidDemoName( "-option" ) );
	EXPECT_FALSE( SV_ValidDemoName( "with space" ) );
	EXPECT_FALSE( SV_ValidDemoName( "caf\xe9" ) );
}

TEST_F( SvDemo, PlayerAndMapNamesBecomeFileNameParts ) {
	char	out[16];

	SV_DemoNamePart( out, sizeof( out ), "^1Pad^7awan" );
	EXPECT_STREQ( out, "Padawan" );
	SV_DemoNamePart( out, sizeof( out ), "  Dark  Jedi!! " );
	EXPECT_STREQ( out, "Dark_Jedi" );
	SV_DemoNamePart( out, sizeof( out ), "mymaps/ffa_x" );
	EXPECT_STREQ( out, "mymaps_ffa_x" );
	SV_DemoNamePart( out, sizeof( out ), "!!!" );
	EXPECT_STREQ( out, "" );
	// cut at 15 characters, never on a separator
	SV_DemoNamePart( out, sizeof( out ), "abcdefghijklmn opq" );
	EXPECT_STREQ( out, "abcdefghijklmn" );
	SV_DemoNamePart( out, sizeof( out ), "abcdefghijklmnopqrstuvwxyz" );
	EXPECT_STREQ( out, "abcdefghijklmno" );
}

TEST_F( SvDemo, AutomaticNameHasDateMapClientAndPlayer ) {
	Join( 0, "^1Pad^7awan", false );
	Command( "svrecord 0" );
	EXPECT_TRUE( IsOpen( AUTO_NAME_0 ) ) << consoleLog;
	EXPECT_NE( consoleLog.find( "Recording client 0" ), std::string::npos );
}

TEST_F( SvDemo, GivenNamesNeverOverwriteAFile ) {
	Join( 0, "Padawan", false );
	Join( 1, "Kyle", true );
	disk["demos/duel.dm_16"] = std::vector<unsigned char>( 10, 1 );

	Command( "svrecord 0 duel" );
	EXPECT_TRUE( IsOpen( "demos/duel_2.dm_16" ) );
	// in use by the demo of client 0
	Command( "svrecord 1 duel.dm_16" );
	EXPECT_TRUE( IsOpen( "demos/duel_3.dm_16" ) );
	EXPECT_EQ( disk["demos/duel.dm_16"].size(), 10u );
}

TEST_F( SvDemo, AllRecordsEveryClientInTheGame ) {
	Join( 0, "Padawan", false );
	Join( 2, "Kyle", true );
	Join( 3, "Luke", true );
	svs.clients[4].state = CS_CONNECTED;	// still loading
	Command( "svrecord 3 luke" );

	Command( "svrecord all match" );
	EXPECT_TRUE( IsOpen( "demos/match_0.dm_16" ) );
	EXPECT_TRUE( IsOpen( "demos/match_2.dm_16" ) );
	EXPECT_TRUE( IsOpen( "demos/luke.dm_16" ) );
	EXPECT_FALSE( IsOpen( "demos/match_3.dm_16" ) );
	EXPECT_FALSE( IsOpen( "demos/match_4.dm_16" ) );
	EXPECT_EQ( openFiles.size(), 3u );
}

TEST_F( SvDemo, RefusesBadArguments ) {
	Join( 0, "Padawan", false );
	svs.clients[1].state = CS_CONNECTED;

	const char *bad[] = {
		"svrecord 0 ../../evil", "svrecord 0 a/b", "svrecord 0 .x", "svrecord 8", "svrecord 31",
		"svrecord -1", "svrecord 0x", "svrecord 1", "svrecord 5", "svrecord", "svrecord 0 a b",
	};
	for ( size_t i = 0; i < sizeof( bad ) / sizeof( bad[0] ); i++ ) {
		Command( bad[i] );
		EXPECT_TRUE( openFiles.empty() ) << bad[i];
	}

	Command( "svrecord 0" );
	Command( "svrecord 0 again" );
	EXPECT_EQ( openFiles.size(), 1u );
	EXPECT_NE( consoleLog.find( "already recorded" ), std::string::npos );

	runningCvar.integer = 0;
	Command( "svrecord all" );
	EXPECT_EQ( openFiles.size(), 1u );
}

TEST_F( SvDemo, GameVersion102And103WriteDm15 ) {
	gameVersion = VERSION_1_02;
	Join( 0, "Padawan", false );
	Command( "svrecord 0 old" );
	EXPECT_TRUE( IsOpen( "demos/old.dm_15" ) );
}

// ===========================================================================
// the stream
// ===========================================================================

TEST_F( SvDemo, RecordsAPlayerAsTheClientWould ) {
	client_t			*cl = Join( 0, "Padawan", false );
	std::vector<Sent>	sent;

	Command( "svrecord 0 p" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 64 ), Ent( 41, 130 ) } ) );
	AddCommand( cl, "print \"hello\n\"" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 65 ), Ent( 41, 130 ), Ent( 300, -500, 250, 3 ) } ) );
	AddCommand( cl, "cs 33 \"models/x.md3\"" );
	AddCommand( cl, "chat \"Kyle: hi\"" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 66 ), Ent( 300, -500, 250 ) } ) );		// 41 removed
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 66 ), Ent( 300, -500, 250 ) } ) );		// unchanged
	Command( "svstoprecord 0" );

	Demo demo = ReadDemo( File( "demos/p.dm_16" ) );
	EXPECT_TRUE( demo.terminated );

	// the gamestate: what the server has now, the commands so far count as executed
	EXPECT_EQ( demo.serverCommandSequence, 20 );
	EXPECT_EQ( demo.clientNum, 0 );
	EXPECT_EQ( demo.checksumFeed, 0x5eed );
	EXPECT_EQ( demo.configstrings.size(), 3u );
	EXPECT_EQ( demo.configstrings[CS_SYSTEMINFO], sv.configstrings[CS_SYSTEMINFO] );
	EXPECT_EQ( demo.configstrings[CS_MODELS + 1], sv.configstrings[CS_MODELS + 1] );
	ASSERT_EQ( demo.baselines.size(), 3u );
	EXPECT_EQ( demo.baselines[300].pos.trBase[0], -512.0f );

	// message numbers follow each other, each snapshot deltas from the previous one
	ASSERT_EQ( demo.sequences.size(), 5u );
	for ( size_t i = 1; i < demo.sequences.size(); i++ ) {
		EXPECT_EQ( demo.sequences[i], demo.sequences[0] + (int)i );
	}
	ASSERT_EQ( demo.snaps.size(), sent.size() );
	EXPECT_EQ( demo.snaps[0].deltaNum, -1 );
	for ( size_t i = 0; i < sent.size(); i++ ) {
		if ( i ) {
			EXPECT_EQ( demo.snaps[i].deltaNum, demo.snaps[i - 1].messageNum );
		}
		ExpectSnapshot( demo.snaps[i], sent[i] );
	}

	// each new command once, in order, before the snapshot that follows it
	ASSERT_EQ( demo.commands.size(), 3u );
	EXPECT_EQ( demo.commands[0], std::make_pair( 21, std::string( "print \"hello\n\"" ) ) );
	EXPECT_EQ( demo.commands[1].first, 22 );
	EXPECT_EQ( demo.commands[2].second, "chat \"Kyle: hi\"" );
}

TEST_F( SvDemo, RecordsBots ) {
	client_t			*bot = Join( 1, "Kyle", true );
	std::vector<Sent>	sent;

	Command( "svrecord 1 bot" );
	for ( int i = 0; i < 40; i++ ) {
		if ( i % 7 == 3 ) {
			AddCommand( bot, va( "print \"%i\n\"", i ) );
		}
		std::vector<entityState_t> ents;
		ents.push_back( Ent( 40, 64.0f + i ) );
		if ( i % 3 ) {
			ents.push_back( Ent( 41 + i % 5, 10.0f * i, 0, i & 3 ) );
		}
		sent.push_back( Snapshot( bot, 50, ents ) );
	}
	Command( "svstoprecord" );

	Demo demo = ReadDemo( File( "demos/bot.dm_16" ) );
	EXPECT_TRUE( demo.terminated );
	EXPECT_EQ( demo.clientNum, 1 );
	ASSERT_EQ( demo.snaps.size(), sent.size() );
	for ( size_t i = 0; i < sent.size(); i++ ) {
		EXPECT_EQ( demo.snaps[i].deltaNum, i ? demo.snaps[i - 1].messageNum : -1 );
		ExpectSnapshot( demo.snaps[i], sent[i] );
	}
	EXPECT_EQ( demo.commands.size(), 6u );
}

TEST_F( SvDemo, CommandsThatDontFitGoInMessagesOfTheirOwn ) {
	client_t			*cl = Join( 0, "Padawan", false );
	std::vector<Sent>	sent;

	Command( "svrecord 0 big" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 1 ) } ) );
	// 40 KB of commands, less than MAX_RELIABLE_COMMANDS of them
	for ( int i = 0; i < 40; i++ ) {
		AddCommand( cl, BigPrint( 1000, i ) );
	}
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 2 ) } ) );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 3 ) } ) );
	Command( "svstoprecord" );

	Demo demo = ReadDemo( File( "demos/big.dm_16" ) );
	ASSERT_EQ( demo.snaps.size(), 3u );
	for ( size_t i = 0; i < demo.sizes.size(); i++ ) {
		EXPECT_LE( demo.sizes[i], MAX_MSGLEN );
	}
	// the command messages were filled up to the limit
	EXPECT_GT( *std::max_element( demo.sizes.begin(), demo.sizes.end() ), MAX_MSGLEN - 1500 );
	ASSERT_EQ( demo.commands.size(), 40u );
	for ( int i = 0; i < 40; i++ ) {
		EXPECT_EQ( demo.commands[i].first, 21 + i );
		EXPECT_EQ( demo.commands[i].second, BigPrint( 1000, i ) );
	}
	for ( size_t i = 0; i < sent.size(); i++ ) {
		ExpectSnapshot( demo.snaps[i], sent[i] );
	}
	// the second snapshot deltas across the command messages
	EXPECT_EQ( demo.snaps[1].deltaNum, demo.snaps[0].messageNum );
	EXPECT_GT( demo.snaps[1].messageNum - demo.snaps[0].messageNum, 2 );
}

static entityState_t BusyEnt( int number, int seed ) {
	entityState_t	e = Ent( number, 0.37f * seed + number, 1.13f * number - seed );

	e.pos.trBase[2] = 0.71f * seed;
	e.pos.trDelta[0] = 3.3f * number;
	e.pos.trDelta[1] = -1.7f * seed;
	e.pos.trDelta[2] = 0.9f * number * seed;
	e.apos.trBase[0] = 0.11f * seed;
	e.apos.trBase[1] = 0.29f * number;
	e.origin[0] = 1.01f * number;
	e.origin[1] = 2.03f * seed;
	e.eFlags = 0x1234567 ^ number;
	e.pos.trTime = 1000003 * seed;
	return e;
}

TEST_F( SvDemo, ASnapshotThatCantFitIsSkipped ) {
	client_t			*cl = Join( 0, "Padawan", false );
	std::vector<Sent>	sent;

	Command( "svrecord 0 huge" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 1 ) } ) );
	AddCommand( cl, "print \"one\n\"" );

	// kilobytes of commands and a snapshot that only fits alone: the commands
	// get a message, the snapshot deltas across it
	std::vector<entityState_t> many;
	for ( int i = 0; i < 150; i++ ) {
		many.push_back( BusyEnt( 100 + i, 1 ) );
	}
	for ( int i = 0; i < 8; i++ ) {
		AddCommand( cl, BigPrint( 900, 100 + i ) );
	}
	sent.push_back( Snapshot( cl, 50, many ) );

	// far more than a message, even alone
	std::vector<entityState_t> tooMany;
	for ( int i = 0; i < 1000; i++ ) {
		tooMany.push_back( BusyEnt( 1 + i, 7 ) );
	}
	Snapshot( cl, 50, tooMany );

	// the next one deltas from the last snapshot written
	sent.push_back( Snapshot( cl, 50, many ) );
	Command( "svstoprecord" );

	Demo demo = ReadDemo( File( "demos/huge.dm_16" ) );
	ASSERT_EQ( demo.snaps.size(), 3u );
	for ( size_t i = 0; i < demo.sizes.size(); i++ ) {
		EXPECT_LE( demo.sizes[i], MAX_MSGLEN );
	}
	for ( size_t i = 0; i < sent.size(); i++ ) {
		ExpectSnapshot( demo.snaps[i], sent[i] );
	}
	EXPECT_EQ( demo.snaps[1].deltaNum, demo.snaps[0].messageNum );
	EXPECT_GT( demo.snaps[1].messageNum, demo.snaps[0].messageNum + 1 );
	EXPECT_EQ( demo.snaps[2].deltaNum, demo.snaps[1].messageNum );
	EXPECT_EQ( demo.snaps[2].messageNum, demo.snaps[1].messageNum + 1 );
	EXPECT_EQ( demo.commands.size(), 9u );
	EXPECT_NE( consoleLog.find( "too big for the server demo" ), std::string::npos );
}

TEST_F( SvDemo, FullSnapshotOnceTheDeltaEntitiesRolledOff ) {
	client_t			*cl = Join( 0, "Padawan", false );
	std::vector<Sent>	sent;

	Command( "svrecord 0 ring" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 1 ), Ent( 41, 2 ) } ) );
	svs.nextSnapshotEntities += svs.numSnapshotEntities;	// other clients filled the ring
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 1 ), Ent( 41, 3 ) } ) );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 1 ), Ent( 41, 4 ) } ) );
	Command( "svstoprecord" );

	Demo demo = ReadDemo( File( "demos/ring.dm_16" ) );
	ASSERT_EQ( demo.snaps.size(), 3u );
	EXPECT_EQ( demo.snaps[1].deltaNum, -1 );
	EXPECT_EQ( demo.snaps[2].deltaNum, demo.snaps[1].messageNum );
	for ( size_t i = 0; i < sent.size(); i++ ) {
		ExpectSnapshot( demo.snaps[i], sent[i] );
	}
}

TEST_F( SvDemo, FullSnapshotAfterAMapRestart ) {
	client_t			*cl = Join( 0, "Padawan", false );
	std::vector<Sent>	sent;

	Command( "svrecord 0 restart" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 1 ) } ) );
	// as SV_MapRestart_f
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;
	AddCommand( cl, "map_restart\n" );
	SV_DemoClientRestart( cl );
	sent.push_back( Snapshot( cl, 400, { Ent( 40, 1 ) } ) );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 2 ) } ) );
	Command( "svstoprecord" );

	Demo demo = ReadDemo( File( "demos/restart.dm_16" ) );
	ASSERT_EQ( demo.snaps.size(), 3u );
	EXPECT_EQ( demo.snaps[0].snapFlags & SNAPFLAG_SERVERCOUNT, 0 );
	EXPECT_EQ( demo.snaps[1].snapFlags & SNAPFLAG_SERVERCOUNT, SNAPFLAG_SERVERCOUNT );
	EXPECT_EQ( demo.snaps[1].deltaNum, -1 );
	EXPECT_EQ( demo.snaps[2].deltaNum, demo.snaps[1].messageNum );
	ASSERT_EQ( demo.commands.size(), 1u );
	EXPECT_EQ( demo.commands[0].second, "map_restart\n" );
	for ( size_t i = 0; i < sent.size(); i++ ) {
		ExpectSnapshot( demo.snaps[i], sent[i] );
	}
}

TEST_F( SvDemo, Dm15StreamUsesThe102Encoding ) {
	gameVersion = VERSION_1_02;
	client_t			*cl = Join( 0, "Padawan", false );
	std::vector<Sent>	sent;

	Command( "svrecord 0 v102" );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 1 ), Ent( 300, 7, 8 ) } ) );
	sent.push_back( Snapshot( cl, 50, { Ent( 40, 9 ) } ) );
	Command( "svstoprecord" );

	Demo demo = ReadDemo( File( "demos/v102.dm_15" ) );
	ASSERT_EQ( demo.snaps.size(), 2u );
	for ( size_t i = 0; i < sent.size(); i++ ) {
		ExpectSnapshot( demo.snaps[i], sent[i] );
	}
}

// ===========================================================================
// many demos at once
// ===========================================================================

// one server frame: a snapshot for each client in the game
static void Frame( int dt ) {
	for ( int i = 0; i < sv_maxclients->integer; i++ ) {
		if ( svs.clients[i].state == CS_ACTIVE ) {
			Snapshot( &svs.clients[i], dt, { Ent( 40, (float)( sv.time % 1000 ) ) } );
			dt = 0;
		}
	}
}

static int StartedDemos() {
	int	started = 0;

	for ( std::map<int, OpenFile>::const_iterator it = openFiles.begin(); it != openFiles.end(); ++it ) {
		started += !it->second.data.empty();
	}
	return started;
}

TEST_F( SvDemo, AFewDemosStartInEachServerFrame ) {
	for ( int i = 0; i < 8; i++ ) {
		Join( i, va( "Player%i", i ), i >= 2 );
	}

	// every file at once, so that rcon gets the replies
	Command( "svrecord all many" );
	ASSERT_EQ( openFiles.size(), 8u );

	// the gamestates of 4 of them in a frame
	Frame( 50 );
	EXPECT_EQ( StartedDemos(), 4 );
	Frame( 50 );
	EXPECT_EQ( StartedDemos(), 8 );
	Frame( 50 );
	Command( "svstoprecord" );

	for ( int i = 0; i < 8; i++ ) {
		Demo demo = ReadDemo( File( va( "demos/many_%i.dm_16", i ) ) );
		EXPECT_TRUE( demo.terminated );
		ASSERT_EQ( demo.snaps.size(), i < 4 ? 3u : 2u ) << i;
		EXPECT_EQ( demo.snaps[0].deltaNum, -1 );
		EXPECT_EQ( demo.snaps[0].serverTime, i < 4 ? 20050 : 20100 );
		EXPECT_EQ( demo.clientNum, i );
	}
}

TEST_F( SvDemo, AutoRecordStartsAFewDemosInEachServerFrame ) {
	for ( int i = 0; i < 8; i++ ) {
		Join( i, va( "Bot%i", i ), true );
	}
	autoRecordCvar.integer = 2;

	Frame( 50 );
	EXPECT_EQ( openFiles.size(), 4u );
	EXPECT_EQ( StartedDemos(), 4 );
	Frame( 50 );
	EXPECT_EQ( openFiles.size(), 8u );
	EXPECT_EQ( StartedDemos(), 8 );

	// a new map starts over at once
	SV_DemoStopAll( " (map change)", qtrue );
	EXPECT_TRUE( openFiles.empty() );
	Frame( 0 );
	EXPECT_EQ( openFiles.size(), 4u );
}

// ===========================================================================
// when demos end
// ===========================================================================

TEST_F( SvDemo, EndsWhenTheClientLeavesOrTheMapChanges ) {
	client_t	*a = Join( 0, "Padawan", false );
	client_t	*b = Join( 1, "Kyle", true );

	Command( "svrecord all" );
	Snapshot( a, 50, { Ent( 40, 1 ) } );
	Snapshot( b, 0, { Ent( 40, 1 ) } );

	SV_DemoClientGone( a, " (disconnected)" );
	EXPECT_TRUE( ReadDemo( File( AUTO_NAME_0 ) ).terminated );
	EXPECT_EQ( openFiles.size(), 1u );
	EXPECT_NE( consoleLog.find( "(disconnected): 1 snapshots" ), std::string::npos );

	SV_DemoStopAll( " (map change)", qtrue );
	EXPECT_TRUE( openFiles.empty() );
	EXPECT_TRUE( ReadDemo( File( "demos/20261004-171530_ffa_test_1_Kyle.dm_16" ) ).terminated );

	// only a shutdown waits for the writer thread
	EXPECT_EQ( writerShutdowns, 0 );
	SV_DemoStopAll( " (server shutdown)", qfalse );
	EXPECT_EQ( writerShutdowns, 1 );
}

TEST_F( SvDemo, AFileTheWriterCouldntCreateStopsTheDemo ) {
	client_t	*cl = Join( 0, "Padawan", false );

	failOpens = true;
	Command( "svrecord 0 nowhere" );
	EXPECT_EQ( openFiles.size(), 1u );
	Snapshot( cl, 50, { Ent( 40, 1 ) } );
	EXPECT_TRUE( openFiles.empty() );
	EXPECT_TRUE( removed.count( "demos/nowhere.dm_16" ) );
	EXPECT_NE( consoleLog.find( "couldn't write demos/nowhere.dm_16" ), std::string::npos );

	// sv_autoRecord takes its turn once: it fails the same way (here at its
	// first write) and doesn't try again on this map
	autoRecordCvar.integer = 1;
	Snapshot( cl, 50, { Ent( 40, 1 ) } );
	EXPECT_TRUE( openFiles.empty() );
	EXPECT_TRUE( removed.count( AUTO_NAME_0 ) );
	Snapshot( cl, 50, { Ent( 40, 1 ) } );
	EXPECT_TRUE( openFiles.empty() );
	EXPECT_EQ( removed.size(), 2u );
}

TEST_F( SvDemo, ADemoWithoutSnapshotsIsRemoved ) {
	Join( 0, "Padawan", false );
	Command( "svrecord 0 empty" );
	Command( "svstoprecord 0" );
	EXPECT_TRUE( openFiles.empty() );
	EXPECT_FALSE( disk.count( "demos/empty.dm_16" ) );
	EXPECT_TRUE( removed.count( "demos/empty.dm_16" ) );
}

TEST_F( SvDemo, OnlyGameSnapshotsAreRecorded ) {
	client_t	*cl = Join( 0, "Padawan", false );

	Command( "svrecord 0 z" );
	cl->state = CS_ZOMBIE;
	Snapshot( cl, 50, { Ent( 40, 1 ) } );
	EXPECT_TRUE( openFiles.begin()->second.data.empty() );
}

TEST_F( SvDemo, AWriteErrorStopsTheDemo ) {
	client_t	*cl = Join( 0, "Padawan", false );

	Command( "svrecord 0 full" );
	Snapshot( cl, 50, { Ent( 40, 1 ) } );
	failWrites = true;
	Snapshot( cl, 50, { Ent( 40, 2 ) } );
	EXPECT_TRUE( openFiles.empty() );
	EXPECT_NE( consoleLog.find( "couldn't write demos/full.dm_16" ), std::string::npos );
	failWrites = false;
	Snapshot( cl, 50, { Ent( 40, 3 ) } );
	EXPECT_TRUE( openFiles.empty() );
}

TEST_F( SvDemo, AutoRecordStartsOncePerMap ) {
	client_t	*human = Join( 0, "Padawan", false );
	client_t	*bot = Join( 1, "Kyle", true );

	// off
	Snapshot( human, 50, { Ent( 40, 1 ) } );
	EXPECT_TRUE( openFiles.empty() );

	// players only
	autoRecordCvar.integer = 1;
	Snapshot( human, 50, { Ent( 40, 1 ) } );
	Snapshot( bot, 0, { Ent( 40, 1 ) } );
	EXPECT_TRUE( IsOpen( AUTO_NAME_0 ) );
	EXPECT_EQ( openFiles.size(), 1u );

	// bots too
	autoRecordCvar.integer = 2;
	Snapshot( bot, 0, { Ent( 40, 1 ) } );
	EXPECT_EQ( openFiles.size(), 2u );

	// what svstoprecord stopped stays stopped on this map
	Command( "svstoprecord 0" );
	Snapshot( human, 50, { Ent( 40, 1 ) } );
	EXPECT_EQ( openFiles.size(), 1u );

	// a new map starts new demos
	SV_DemoStopAll( " (map change)", qtrue );
	Snapshot( human, 50, { Ent( 40, 1 ) } );
	Snapshot( bot, 0, { Ent( 40, 1 ) } );
	EXPECT_EQ( openFiles.size(), 2u );

	// not the final snapshots of a shutdown
	SV_DemoStopAll( " (server shutdown)", qfalse );
	Snapshot( human, 50, { Ent( 40, 1 ) } );
	EXPECT_TRUE( openFiles.empty() );

	// whoever takes a freed slot is recorded
	SV_DemoStopAll( " (map change)", qtrue );
	Snapshot( human, 50, { Ent( 40, 1 ) } );
	SV_DemoClientGone( human, " (disconnected)" );
	Join( 0, "Newcomer", false );
	Snapshot( human, 50, { Ent( 40, 1 ) } );
	EXPECT_TRUE( IsOpen( "demos/20261004-171530_ffa_test_0_Newcomer.dm_16" ) );
}
