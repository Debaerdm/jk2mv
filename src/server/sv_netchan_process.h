// sv_netchan_process.h -- Network channel packet reception
// Extracted from sv_net_chan.cpp as part of aggressive server refactoring

#ifndef SV_NETCHAN_PROCESS_H
#define SV_NETCHAN_PROCESS_H

/*
=================
Netchan_SV_Process
=================
*/
qboolean SV_Netchan_Process( client_t *client, msg_t *msg ) {
	int ret;
	ret = Netchan_Process( &client->netchan, msg );
	if (!ret)
		return qfalse;
	SV_Netchan_Decode( client, msg );
	return qtrue;
}

#endif // SV_NETCHAN_PROCESS_H
