// sv_netchan_transmit.h -- Network channel transmission
// Extracted from sv_net_chan.cpp as part of aggressive server refactoring

#ifndef SV_NETCHAN_TRANSMIT_H
#define SV_NETCHAN_TRANSMIT_H

/*
=================
SV_Netchan_TransmitNextFragment
=================
*/
void SV_Netchan_TransmitNextFragment( netchan_t *chan ) {
	Netchan_TransmitNextFragment( chan );
}

/*
===============
SV_Netchan_Transmit
================
*/
void SV_Netchan_Transmit( client_t *client, msg_t *msg) {
	MSG_WriteByte( msg, svc_EOF );
	SV_Netchan_Encode( client, msg );
	Netchan_Transmit( &client->netchan, msg->cursize, msg->data );
}

#endif // SV_NETCHAN_TRANSMIT_H
