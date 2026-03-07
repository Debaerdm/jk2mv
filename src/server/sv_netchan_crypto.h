// sv_netchan_crypto.h -- Network channel encryption (XOR cipher)
// Extracted from sv_net_chan.cpp as part of aggressive server refactoring

#ifndef SV_NETCHAN_CRYPTO_H
#define SV_NETCHAN_CRYPTO_H

// TTimo: unused, commenting out to make gcc happy
#if 1
/*
==============
SV_Netchan_Encode

	// first four bytes of the data are always:
	long reliableAcknowledge;

==============
*/
static void SV_Netchan_Encode( client_t *client, msg_t *msg ) {
	int reliableAcknowledge, i, index;
	byte key, *string;
	int	srdc, sbit;
	qboolean soob;

	if ( msg->cursize < SV_ENCODE_START ) {
		return;
	}

		srdc = msg->readcount;
		sbit = msg->bit;
		soob = msg->oob;

		msg->bit = 0;
		msg->readcount = 0;
		msg->oob = qfalse;

	reliableAcknowledge = MSG_ReadLong(msg);

		msg->oob = soob;
		msg->bit = sbit;
		msg->readcount = srdc;

	string = (byte *)client->lastClientCommandString;
	index = 0;
	// xor the client challenge with the netchan sequence number
	key = client->challenge ^ client->netchan.outgoingSequence;
	for (i = SV_ENCODE_START; i < msg->cursize; i++) {
		// modify the key with the last received and with this message acknowledged client command
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
		*(msg->data + i) = *(msg->data + i) ^ key;
	}
}

/*
==============
SV_Netchan_Decode

	// first 12 bytes of the data are always:
	long serverId;
	long messageAcknowledge;
	long reliableAcknowledge;

==============
*/
static void SV_Netchan_Decode( client_t *client, msg_t *msg ) {
	int serverId, messageAcknowledge, reliableAcknowledge;
	int i, index, srdc, sbit;
	byte key, *string;
	qboolean soob;

		srdc = msg->readcount;
		sbit = msg->bit;
		soob = msg->oob;

		msg->oob = qfalse;

		serverId = MSG_ReadLong(msg);
	messageAcknowledge = MSG_ReadLong(msg);
	reliableAcknowledge = MSG_ReadLong(msg);

		msg->oob = soob;
		msg->bit = sbit;
		msg->readcount = srdc;

	string = (byte *)client->reliableCommands[ reliableAcknowledge & (MAX_RELIABLE_COMMANDS-1) ];
	index = 0;
	//
	key = client->challenge ^ serverId ^ messageAcknowledge;
	for (i = msg->readcount + SV_DECODE_START; i < msg->cursize; i++) {
		// modify the key with the last sent and acknowledged server command
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

#endif // SV_NETCHAN_CRYPTO_H
