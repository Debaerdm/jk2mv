// sv_demo.cpp -- server-side demos (svrecord, svstoprecord, sv_autoRecord)
//
// A server demo is the demo a client would record itself with "record": the
// gamestate, then a server message for each snapshot built for that client,
// in the client demo format of the server's game version (dm_15 / dm_16),
// played with "demo".  The messages are written for the demo and not copied
// from the network: each snapshot is delta compressed against the previous
// one in the file and each reliable command is written once, so packet loss,
// the rate and the client's acknowledgements make no difference, and bots,
// which get no messages, are recorded the same way.  Nothing changes for the
// clients.  The files are written by a thread of their own (sv_demo_writer.cpp).

#include "server.h"

// the entity delta of the network snapshots
#include "sv_snapshot_delta.h"

// up to 42 letters, digits, '_', '-' and '.', like video names
#define	DEMO_NAME_MAX		42

// message number of the gamestate, as on a connection past its first
// messages; the snapshots follow
#define	DEMO_FIRST_MESSAGE	PACKET_BACKUP

// room past the end of a message: one write can add several Huffman codes
// after MSG_WriteBits checked the size.  Such a write leaves the message full
// and is undone, so what is written stays within the MAX_MSGLEN a client reads.
#define	DEMO_MSG_MARGIN		32

// starting a demo (console line, gamestate) takes about a millisecond: only a
// few start in a server frame, so that "svrecord all" or sv_autoRecord on a
// full server doesn't stall one frame.  The others start with the next
// snapshots.
#define	DEMO_STARTS_PER_FRAME	4

typedef struct {
	svDemoFile_t		*file;
	char				path[MAX_OSPATH];	// demos/<name>.dm_<protocol>
	qboolean			automatic;			// started by sv_autoRecord
	qboolean			autoTried;			// sv_autoRecord already took its turn on this map
	qboolean			gamestateWritten;
	qboolean			deltaValid;			// deltaFrame can be delta compressed against
	qboolean			forceFull;			// the next snapshot is not delta compressed
	int					sequence;			// message number of the next message
	int					deltaSequence;		// message number of deltaFrame
	clientSnapshot_t	deltaFrame;			// the last snapshot written
	int					lastCommand;		// last reliable command written
	int					snapshots;
	int					bytes;
	int					startTime;			// svs.time
} svDemo_t;

static svDemo_t	sv_demos[MAX_CLIENTS];
static byte		sv_demoBuffer[MAX_MSGLEN + DEMO_MSG_MARGIN];

static int		sv_demoStartFrame = -1;		// svs.time of the frame sv_demoStarts counts
static int		sv_demoStarts;

/*
=============================================================================

FILE NAMES

=============================================================================
*/

static qboolean SV_DemoNameChar( char c ) {
	return (qboolean)( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
		( c >= '0' && c <= '9' ) || c == '_' || c == '-' );
}

/*
===============
SV_ValidDemoName

A plain file name from the svrecord command: it can't leave demos/
===============
*/
qboolean SV_ValidDemoName( const char *name ) {
	const size_t	len = strlen( name );
	const char		*p;

	if ( !len || len > DEMO_NAME_MAX || name[0] == '-' || name[0] == '.' || strstr( name, ".." ) ) {
		return qfalse;
	}
	for ( p = name; *p; p++ ) {
		if ( !SV_DemoNameChar( *p ) && *p != '.' ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
===============
SV_DemoNamePart

The letters, digits, '-' and '_' of a player or map name for a demo file
name, a run of other characters as one '_', at most size - 1 characters
===============
*/
void SV_DemoNamePart( char *out, int size, const char *in ) {
	char		clean[MAX_STRING_CHARS];
	const char	*p;
	int			len = 0;
	qboolean	gap = qfalse;

	Q_strncpyz( clean, in, sizeof( clean ) );
	Q_CleanStr( clean, MV_USE102COLOR );

	for ( p = clean; *p && len < size - 1; p++ ) {
		if ( !SV_DemoNameChar( *p ) ) {
			gap = qtrue;
			continue;
		}
		if ( gap && len ) {
			out[len++] = '_';
			if ( len == size - 1 ) {
				len--;
				break;
			}
		}
		gap = qfalse;
		out[len++] = *p;
	}
	out[len] = '\0';
}

/*
===============
SV_DemoAutoName

<date>-<time>_<map>_<client number>_<player name>
===============
*/
static void SV_DemoAutoName( client_t *cl, char *out, int size ) {
	qtime_t	t;
	char	map[17], player[16];

	Com_RealTime( &t );
	SV_DemoNamePart( map, sizeof( map ), sv_mapname->string );
	SV_DemoNamePart( player, sizeof( player ), cl->name );

	Com_sprintf( out, size, "%04i%02i%02i-%02i%02i%02i_%s_%i%s%s",
		1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
		map[0] ? map : "map", (int)( cl - svs.clients ), player[0] ? "_" : "", player );
}

/*
=============================================================================

RECORDING

=============================================================================
*/

/*
===============
SV_DemoStart

Opens demos/<base>.dm_<protocol>, or <base>_2, <base>_3... when the name is
taken: a server demo never overwrites a file.  The gamestate is written with
the first snapshot.
===============
*/
static qboolean SV_DemoStart( client_t *cl, const char *base, qboolean automatic ) {
	svDemo_t	*demo = &sv_demos[cl - svs.clients];
	char		path[MAX_OSPATH];
	int			i;

	for ( i = 1; i < 100; i++ ) {
		if ( i == 1 ) {
			Com_sprintf( path, sizeof( path ), "demos/%s.dm_%i", base, (int)MV_GetCurrentProtocol() );
		} else {
			Com_sprintf( path, sizeof( path ), "demos/%s_%i.dm_%i", base, i, (int)MV_GetCurrentProtocol() );
		}
		// another demo may still be writing it
		if ( !FS_FileExists( path ) && !SV_DemoFileInUse( path ) ) {
			break;
		}
	}
	if ( i == 100 ) {
		Com_Printf( "No free file name for the server demo %s\n", base );
		return qfalse;
	}

	demo->file = SV_DemoFileOpen( path );
	if ( !demo->file ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open %s for the server demo\n", path );
		return qfalse;
	}

	Q_strncpyz( demo->path, path, sizeof( demo->path ) );
	demo->automatic = automatic;
	demo->gamestateWritten = qfalse;
	demo->deltaValid = qfalse;
	demo->forceFull = qfalse;
	demo->sequence = DEMO_FIRST_MESSAGE;
	demo->snapshots = 0;
	demo->bytes = 0;
	demo->startTime = svs.time;

	Com_Printf( "Recording client %i (%s" S_COLOR_WHITE ") to %s\n", (int)( cl - svs.clients ), cl->name, path );
	return qtrue;
}

/*
===============
SV_DemoClose

Ends the demo like CL_StopRecord_f; a demo stopped before its first
snapshot had nothing in it and is removed
===============
*/
static void SV_DemoClose( svDemo_t *demo, const char *why ) {
	int		end[2];

	if ( !demo->file ) {
		return;
	}

	if ( demo->gamestateWritten ) {
		// a message length of -1 ends the demo
		end[0] = end[1] = -1;
		SV_DemoFileWrite( demo->file, end, sizeof( end ) );
		SV_DemoFileClose( demo->file, qfalse );
		Com_Printf( "Stopped %s%s: %i snapshots, %i s, %i KB\n", demo->path, why, demo->snapshots,
			( svs.time - demo->startTime ) / 1000, ( demo->bytes + 1023 ) / 1024 );
	} else {
		SV_DemoFileClose( demo->file, qtrue );
		Com_Printf( "Stopped %s%s before its first snapshot\n", demo->path, why );
	}

	demo->file = NULL;
	demo->automatic = qfalse;
	demo->gamestateWritten = qfalse;
}

static void SV_DemoBeginMessage( client_t *cl, msg_t *msg ) {
	MSG_Init( msg, sv_demoBuffer, MAX_MSGLEN );
	// like every server message: the last client command the server got
	MSG_WriteLong( msg, cl->lastClientCommand );
}

// no room left for the svc_EOF that ends the message: MSG_WriteBits doesn't
// write in the last 4 bytes
static qboolean SV_DemoMessageFull( const msg_t *msg ) {
	return (qboolean)( msg->overflowed || msg->maxsize - msg->cursize < 4 );
}

/*
===============
SV_DemoRestoreMessage

Back to a copy of the message taken before the last write, which didn't fit.
The bit writer only clears a byte when it starts it, so the bits the failed
write left after that point in the current byte are cleared here.
===============
*/
static void SV_DemoRestoreMessage( msg_t *msg, const msg_t *before ) {
	*msg = *before;
	if ( msg->bit & 7 ) {
		msg->data[msg->bit >> 3] &= ( 1 << ( msg->bit & 7 ) ) - 1;
	}
}

/*
===============
SV_DemoEndMessage

Writes the message to the demo as the client writes the messages it gets
(CL_WriteDemoMessage): message number, length, then the message.  False once
the file can't be written.
===============
*/
static qboolean SV_DemoEndMessage( svDemo_t *demo, msg_t *msg ) {
	int		header[2];

	MSG_WriteByte( msg, svc_EOF );

	header[0] = LittleLong( demo->sequence );
	header[1] = LittleLong( msg->cursize );
	SV_DemoFileWrite( demo->file, header, sizeof( header ) );
	SV_DemoFileWrite( demo->file, msg->data, msg->cursize );
	if ( SV_DemoFileFailed( demo->file ) ) {
		return qfalse;
	}

	demo->sequence++;
	demo->bytes += (int)sizeof( header ) + msg->cursize;
	return qtrue;
}

/*
===============
SV_DemoWriteGamestate

The gamestate as CL_Record_f writes it, which is what SV_SendClientGameState
sends: the configstrings and baselines of now, and the client's commands so
far count as executed
===============
*/
static qboolean SV_DemoWriteGamestate( client_t *cl, svDemo_t *demo ) {
	msg_t			msg;
	entityState_t	nullstate;
	int				i;

	SV_DemoBeginMessage( cl, &msg );

	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, cl->reliableSequence );

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( sv.configstrings[i][0] ) {
			MSG_WriteByte( &msg, svc_configstring );
			MSG_WriteShort( &msg, i );
			MSG_WriteBigString( &msg, sv.configstrings[i] );
		}
	}

	Com_Memset( &nullstate, 0, sizeof( nullstate ) );
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( sv.svEntities[i].baseline.number ) {
			MSG_WriteByte( &msg, svc_baseline );
			MSG_WriteDeltaEntity( &msg, &nullstate, &sv.svEntities[i].baseline, qtrue );
		}
	}

	MSG_WriteByte( &msg, svc_EOF );

	MSG_WriteLong( &msg, (int)( cl - svs.clients ) );
	MSG_WriteLong( &msg, sv.checksumFeed );

	if ( SV_DemoMessageFull( &msg ) || !SV_DemoEndMessage( demo, &msg ) ) {
		return qfalse;
	}

	demo->lastCommand = cl->reliableSequence;
	demo->gamestateWritten = qtrue;
	return qtrue;
}

/*
===============
SV_DemoWriteSnapshot

The svc_snapshot of SV_WriteSnapshotToClient, delta compressed against the
last snapshot of the demo rather than the last one the client acknowledged
===============
*/
static void SV_DemoWriteSnapshot( client_t *cl, svDemo_t *demo, clientSnapshot_t *frame, msg_t *msg ) {
	clientSnapshot_t	*oldframe = NULL;
	int					lastframe = 0;
	int					snapFlags;

	if ( demo->deltaValid && !demo->forceFull ) {
		// as for the network: not from too far back, and not once the
		// entities of that snapshot rolled off the ring
		lastframe = demo->sequence - demo->deltaSequence;
		if ( lastframe < PACKET_BACKUP - 3 &&
			!SV_SnapshotEntitiesRolledOff( demo->deltaFrame.first_entity, svs.nextSnapshotEntities, svs.numSnapshotEntities ) ) {
			oldframe = &demo->deltaFrame;
		} else {
			lastframe = 0;
		}
	}

	MSG_WriteByte( msg, svc_snapshot );
	MSG_WriteLong( msg, sv.time );
	MSG_WriteByte( msg, lastframe );

	snapFlags = svs.snapFlagServerBit;
	if ( cl->rateDelayed ) {
		snapFlags |= SNAPFLAG_RATE_DELAYED;
	}
	MSG_WriteByte( msg, snapFlags );

	MSG_WriteByte( msg, frame->areabytes );
	MSG_WriteData( msg, frame->areabits, frame->areabytes );

	MSG_WriteDeltaPlayerstate( msg, oldframe ? &oldframe->ps : NULL, &frame->ps );
	SV_EmitPacketEntities( oldframe, frame, msg );
}

static void SV_DemoWriteFailed( svDemo_t *demo ) {
	Com_Printf( S_COLOR_YELLOW "WARNING: couldn't write %s (disk full or not writable?)\n", demo->path );
	SV_DemoClose( demo, " after a write error" );
}

/*
===============
SV_DemoAutoRecordWanted

sv_autoRecord: the demo of a player (1) or of any client (2) starts with its
first snapshot in the game on each map, once: svstoprecord stops it until
the next map
===============
*/
static qboolean SV_DemoAutoRecordWanted( const client_t *cl, const svDemo_t *demo ) {
	if ( sv_autoRecord->integer <= 0 || demo->autoTried ) {
		return qfalse;
	}
	return (qboolean)( cl->netchan.remoteAddress.type != NA_BOT || sv_autoRecord->integer >= 2 );
}

// DEMO_STARTS_PER_FRAME
static qboolean SV_DemoMayStart( void ) {
	if ( svs.time != sv_demoStartFrame ) {
		sv_demoStartFrame = svs.time;
		sv_demoStarts = 0;
	}
	if ( sv_demoStarts >= DEMO_STARTS_PER_FRAME ) {
		return qfalse;
	}
	sv_demoStarts++;
	return qtrue;
}

/*
===============
SV_DemoClientSnapshot

Called after each snapshot built for a client, bots included: writes the
reliable commands queued since the last message and the snapshot
===============
*/
void SV_DemoClientSnapshot( client_t *cl ) {
	svDemo_t			*demo = &sv_demos[cl - svs.clients];
	clientSnapshot_t	*frame;
	msg_t				msg, before;
	int					i, commands;
	char				base[MAX_QPATH];

	// game snapshots only.  The demo takes sv.time even while the network
	// still sends a client the time of the previous map (oldServerTime,
	// which bots keep for good after a map change).
	if ( cl->state != CS_ACTIVE ) {
		return;
	}

	if ( !demo->file ) {
		if ( !SV_DemoAutoRecordWanted( cl, demo ) || !SV_DemoMayStart() ) {
			return;
		}
		demo->autoTried = qtrue;
		SV_DemoAutoName( cl, base, sizeof( base ) );
		if ( !SV_DemoStart( cl, base, qtrue ) ) {
			return;
		}
	} else if ( SV_DemoFileFailed( demo->file ) ) {
		// the writer thread couldn't create or write the file
		SV_DemoWriteFailed( demo );
		return;
	} else if ( !demo->gamestateWritten && !SV_DemoMayStart() ) {
		// opened by svrecord, starts with a later snapshot
		return;
	}

	if ( !demo->gamestateWritten && !SV_DemoWriteGamestate( cl, demo ) ) {
		SV_DemoWriteFailed( demo );
		return;
	}

	SV_DemoBeginMessage( cl, &msg );
	commands = 0;

	i = demo->lastCommand + 1;
	if ( i <= cl->reliableSequence - MAX_RELIABLE_COMMANDS ) {
		// cycled out of the buffer, which SV_AddServerCommand doesn't let happen
		i = cl->reliableSequence - MAX_RELIABLE_COMMANDS + 1;
	}
	for ( ; i <= cl->reliableSequence; i++ ) {
		before = msg;
		MSG_WriteByte( &msg, svc_serverCommand );
		MSG_WriteLong( &msg, i );
		MSG_WriteString( &msg, cl->reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )] );
		if ( !SV_DemoMessageFull( &msg ) ) {
			commands++;
			continue;
		}

		// the commands so far get a message of their own (a command always
		// fits in an empty message)
		SV_DemoRestoreMessage( &msg, &before );
		if ( !commands || !SV_DemoEndMessage( demo, &msg ) ) {
			SV_DemoWriteFailed( demo );
			return;
		}
		SV_DemoBeginMessage( cl, &msg );
		commands = 0;
		i--;
	}
	demo->lastCommand = cl->reliableSequence;

	frame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];

	before = msg;
	SV_DemoWriteSnapshot( cl, demo, frame, &msg );
	if ( SV_DemoMessageFull( &msg ) && commands ) {
		// the snapshot gets a message of its own
		SV_DemoRestoreMessage( &msg, &before );
		if ( !SV_DemoEndMessage( demo, &msg ) ) {
			SV_DemoWriteFailed( demo );
			return;
		}
		SV_DemoBeginMessage( cl, &msg );
		SV_DemoWriteSnapshot( cl, demo, frame, &msg );
	}
	if ( SV_DemoMessageFull( &msg ) ) {
		// too big even alone: the next one deltas from the last one written
		Com_DPrintf( "%s: snapshot too big for the server demo, skipped\n", cl->name );
		return;
	}

	if ( !SV_DemoEndMessage( demo, &msg ) ) {
		SV_DemoWriteFailed( demo );
		return;
	}
	demo->deltaFrame = *frame;
	demo->deltaSequence = demo->sequence - 1;
	demo->deltaValid = qtrue;
	demo->forceFull = qfalse;
	demo->snapshots++;
}

/*
===============
SV_DemoClientRestart

The client entered the world, again after a map_restart: its next network
snapshot is a full one, so is the demo's
===============
*/
void SV_DemoClientRestart( client_t *cl ) {
	sv_demos[cl - svs.clients].forceFull = qtrue;
}

/*
===============
SV_DemoClientGone

The client left or its slot is reused: its demo ends, and sv_autoRecord
will record whoever takes the slot
===============
*/
void SV_DemoClientGone( client_t *cl, const char *why ) {
	svDemo_t	*demo = &sv_demos[cl - svs.clients];

	SV_DemoClose( demo, why );
	demo->autoTried = qfalse;
}

/*
===============
SV_DemoStopAll

Map change or shutdown: every demo ends.  sv_autoRecord starts new ones on
the next map, but not on shutdown, where SV_FinalMessage still builds
snapshots, and which waits for the files to be written.
===============
*/
void SV_DemoStopAll( const char *why, qboolean newMap ) {
	int		i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		SV_DemoClose( &sv_demos[i], why );
		sv_demos[i].autoTried = (qboolean)!newMap;
	}
	sv_demoStartFrame = -1;

	if ( !newMap ) {
		SV_DemoFilesShutdown();
	}
}

/*
=============================================================================

COMMANDS

=============================================================================
*/

// a client slot number from the command line, or NULL with a message
static client_t *SV_DemoClientArg( const char *s ) {
	const char	*p;
	int			num;

	for ( p = s; *p; p++ ) {
		if ( *p < '0' || *p > '9' ) {
			break;
		}
	}
	if ( !*s || *p || p - s > 2 ) {
		Com_Printf( "Bad client number: %s\n", s );
		return NULL;
	}
	num = atoi( s );
	if ( num >= sv_maxclients->integer ) {
		Com_Printf( "Bad client number: %i\n", num );
		return NULL;
	}
	return &svs.clients[num];
}

static void SV_DemoList( void ) {
	int		i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		const svDemo_t *demo = &sv_demos[i];

		if ( demo->file ) {
			Com_Printf( "%2i %s%s: %i s, %i KB\n", i, demo->path, demo->automatic ? " (auto)" : "",
				( svs.time - demo->startTime ) / 1000, ( demo->bytes + 1023 ) / 1024 );
		}
	}
}

/*
==================
SV_Record_f

svrecord <client number | all> [name]
==================
*/
void SV_Record_f( void ) {
	char		name[MAX_QPATH];
	char		base[MAX_QPATH];
	client_t	*cl;
	int			i, count;

	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 2 || Cmd_Argc() > 3 ) {
		Com_Printf( "usage: svrecord <client number | all> [name]\n" );
		SV_DemoList();
		return;
	}

	name[0] = '\0';
	if ( Cmd_Argc() == 3 ) {
		size_t len;

		Q_strncpyz( name, Cmd_Argv( 2 ), sizeof( name ) );
		// "name.dm_16" names the file, not name.dm_16.dm_16
		len = strlen( name );
		if ( len > 6 && ( !Q_stricmp( name + len - 6, ".dm_15" ) || !Q_stricmp( name + len - 6, ".dm_16" ) ) ) {
			name[len - 6] = '\0';
		}
		if ( !SV_ValidDemoName( name ) ) {
			Com_Printf( "Demo names: up to %i letters, digits, '_', '-' and '.'\n", DEMO_NAME_MAX );
			return;
		}
	}

	if ( !Q_stricmp( Cmd_Argv( 1 ), "all" ) ) {
		count = 0;
		for ( i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
			if ( cl->state != CS_ACTIVE || sv_demos[i].file ) {
				continue;
			}
			if ( name[0] ) {
				Com_sprintf( base, sizeof( base ), "%s_%i", name, i );
			} else {
				SV_DemoAutoName( cl, base, sizeof( base ) );
			}
			if ( SV_DemoStart( cl, base, qfalse ) ) {
				count++;
			}
		}
		if ( !count ) {
			Com_Printf( "No client to record.\n" );
		}
		return;
	}

	cl = SV_DemoClientArg( Cmd_Argv( 1 ) );
	if ( !cl ) {
		return;
	}
	i = (int)( cl - svs.clients );
	if ( sv_demos[i].file ) {
		Com_Printf( "Client %i is already recorded to %s\n", i, sv_demos[i].path );
		return;
	}
	if ( cl->state != CS_ACTIVE ) {
		Com_Printf( "Client %i is not in the game.\n", i );
		return;
	}

	if ( name[0] ) {
		Q_strncpyz( base, name, sizeof( base ) );
	} else {
		SV_DemoAutoName( cl, base, sizeof( base ) );
	}
	SV_DemoStart( cl, base, qfalse );
}

/*
==================
SV_StopRecord_f

svstoprecord [client number | all]
==================
*/
void SV_StopRecord_f( void ) {
	client_t	*cl;
	int			i, count;

	if ( Cmd_Argc() > 2 ) {
		Com_Printf( "usage: svstoprecord [client number | all]\n" );
		return;
	}

	// what is stopped here, sv_autoRecord doesn't restart before the next map
	if ( Cmd_Argc() == 1 || !Q_stricmp( Cmd_Argv( 1 ), "all" ) ) {
		count = 0;
		for ( i = 0; i < MAX_CLIENTS; i++ ) {
			if ( sv_demos[i].file ) {
				SV_DemoClose( &sv_demos[i], "" );
				sv_demos[i].autoTried = qtrue;
				count++;
			}
		}
		if ( !count ) {
			Com_Printf( "No server demo is being recorded.\n" );
		}
		return;
	}

	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	cl = SV_DemoClientArg( Cmd_Argv( 1 ) );
	if ( !cl ) {
		return;
	}
	i = (int)( cl - svs.clients );
	if ( !sv_demos[i].file ) {
		Com_Printf( "Client %i is not being recorded.\n", i );
		return;
	}
	SV_DemoClose( &sv_demos[i], "" );
	sv_demos[i].autoTried = qtrue;
}
