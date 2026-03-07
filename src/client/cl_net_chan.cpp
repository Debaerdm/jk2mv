
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "client.h"

// TTimo: unused, commenting out to make gcc happy
#if 1
typedef struct netchanMessageState_s {
	int readcount;
	int bit;
	qboolean oob;
} netchanMessageState_t;

static void CL_SaveMessageState( const msg_t *msg, netchanMessageState_t *state )
{
	state->readcount = msg->readcount;
	state->bit = msg->bit;
	state->oob = msg->oob;
}

static void CL_RestoreMessageState( msg_t *msg, const netchanMessageState_t *state )
{
	msg->readcount = state->readcount;
	msg->bit = state->bit;
	msg->oob = state->oob;
}

/*
==============
CL_Netchan_Encode

	// first 12 bytes of the data are always:
	long serverId;
	long messageAcknowledge;
	long reliableAcknowledge;

==============
*/
static void CL_Netchan_Encode( msg_t *msg ) {
	int serverId, messageAcknowledge, reliableAcknowledge;
	int i, index;
	netchanMessageState_t state;
	byte key, *string;

	if ( msg->cursize <= CL_ENCODE_START ) {
		return;
	}

	CL_SaveMessageState( msg, &state );

	msg->bit = 0;
	msg->readcount = 0;
	msg->oob = qfalse;

	serverId = MSG_ReadLong(msg);
	messageAcknowledge = MSG_ReadLong(msg);
	reliableAcknowledge = MSG_ReadLong(msg);

	CL_RestoreMessageState( msg, &state );

	string = (byte *)clc.serverCommands[ reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ];
	index = 0;
	//
	key = clc.challenge ^ serverId ^ messageAcknowledge;
	for (i = CL_ENCODE_START; i < msg->cursize; i++) {
		// modify the key with the last received now acknowledged server command
		if (!string[index])
			index = 0;
		if (string[index] > 127 || string[index] == '%') {
			key ^= '.' << (i & 1);
		}
		else {
			key ^= string[index] << (i & 1);
		}
		index++;
		// encode the data with this key
		*(msg->data + i) = (*(msg->data + i)) ^ key;
	}
}

/*
==============
CL_Netchan_Decode

	// first four bytes of the data are always:
	long reliableAcknowledge;

==============
*/
static void CL_Netchan_Decode( msg_t *msg ) {
	int reliableAcknowledge, i, index;
	netchanMessageState_t state;
	byte key, *string;

	CL_SaveMessageState( msg, &state );

	msg->oob = qfalse;

	reliableAcknowledge = MSG_ReadLong(msg);

	CL_RestoreMessageState( msg, &state );

	string = (unsigned char *)clc.reliableCommands[ reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ];
	index = 0;
	// xor the client challenge with the netchan sequence number (need something that changes every message)
	key = clc.challenge ^ LittleLong( *(unsigned *)msg->data );
	for (i = msg->readcount + CL_DECODE_START; i < msg->cursize; i++) {
		// modify the key with the last sent and with this message acknowledged client command
		if (!string[index])
			index = 0;
		if (string[index] > 127 || string[index] == '%') {
			key ^= '.' << (i & 1);
		}
		else {
			key ^= string[index] << (i & 1);
		}
		index++;
		// decode the data with this key
		*(msg->data + i) = *(msg->data + i) ^ key;
	}
}
#endif

/*
=================
CL_Netchan_TransmitNextFragment
=================
*/
void CL_Netchan_TransmitNextFragment( netchan_t *chan ) {
	Netchan_TransmitNextFragment( chan );
}

//byte chksum[65536];

/*
===============
CL_Netchan_Transmit
================
*/
void CL_Netchan_Transmit( netchan_t *chan, msg_t* msg ) {
//	int i;
	MSG_WriteByte( msg, clc_EOF );
//	for(i=CL_ENCODE_START;i<msg->cursize;i++) {
//		chksum[i-CL_ENCODE_START] = msg->data[i];
//	}

//	Huff_Compress( msg, CL_ENCODE_START );
	CL_Netchan_Encode( msg );
	Netchan_Transmit( chan, msg->cursize, msg->data );
}

extern	int oldsize;
int newsize = 0;

/*
=================
CL_Netchan_Process
=================
*/
qboolean CL_Netchan_Process( netchan_t *chan, msg_t *msg ) {
	int ret;
//	int i;
//	static		int newsize = 0;

	ret = Netchan_Process( chan, msg );
	if (!ret)
		return qfalse;
	CL_Netchan_Decode( msg );
//	Huff_Decompress( msg, CL_DECODE_START );
//	for(i=CL_DECODE_START+msg->readcount;i<msg->cursize;i++) {
//		if (msg->data[i] != chksum[i-(CL_DECODE_START+msg->readcount)]) {
//			Com_Error(ERR_DROP,"bad %d v %d", msg->data[i], chksum[i-(CL_DECODE_START+msg->readcount)]);
//		}
//	}
	newsize += msg->cursize;
//	Com_Printf("saved %d to %d (%d%%)\n", (oldsize>>3), newsize, 100-(newsize*100/(oldsize>>3)));
	return qtrue;
}
