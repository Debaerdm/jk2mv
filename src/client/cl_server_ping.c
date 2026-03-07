#include "client.h"
#include "cl_server_browser.h"

/*
==================
CL_GetPing
==================
*/
void CL_GetPing(int n, char *buf, int buflen, int *pingtime) {
	const char *str;
	int time;
	int maxPing;

	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
		// empty slot
		buf[0] = '\0';
		*pingtime = 0;
		return;
	}

	str = NET_AdrToString(cl_pinglist[n].adr);
	Q_strncpyz(buf, str, buflen);

	time = cl_pinglist[n].time;
	if (!time) {
		// check for timeout
		time = cls.realtime - cl_pinglist[n].start;
		maxPing = Cvar_VariableIntegerValue("cl_maxPing");
		if (maxPing < 100) {
			maxPing = 100;
		}
		if (time < maxPing) {
			// not timed out yet
			time = 0;
		}
	}

	// Update server info from ping response
	extern void CL_SetServerInfoByAddress(netadr_t from, const char *info, int ping);
	CL_SetServerInfoByAddress(cl_pinglist[n].adr, cl_pinglist[n].info,
		cl_pinglist[n].time);

	*pingtime = time;
}

/*
==================
CL_UpdateServerInfo
==================
*/
void CL_UpdateServerInfo(int n) {
	if (!cl_pinglist[n].adr.port) {
		return;
	}

	extern void CL_SetServerInfoByAddress(netadr_t from, const char *info, int ping);
	CL_SetServerInfoByAddress(cl_pinglist[n].adr, cl_pinglist[n].info,
		cl_pinglist[n].time);
}

/*
==================
CL_GetPingInfo
==================
*/
void CL_GetPingInfo(int n, char *buf, int buflen) {
	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
		// empty slot
		if (buflen)
			buf[0] = '\0';
		return;
	}

	Q_strncpyz(buf, cl_pinglist[n].info, buflen);
}

/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing(int n) {
	if (n < 0 || n >= MAX_PINGREQUESTS)
		return;

	cl_pinglist[n].adr.port = 0;
}

/*
==================
CL_GetPingQueueCount
==================
*/
int CL_GetPingQueueCount(void) {
	int i;
	int count;
	ping_t *pingptr;

	count = 0;
	pingptr = cl_pinglist;

	for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
		if (pingptr->adr.port) {
			count++;
		}
	}

	return (count);
}

/*
==================
CL_GetFreePing
==================
*/
ping_t *CL_GetFreePing(void) {
	ping_t *pingptr;
	ping_t *best;
	int oldest;
	int i;
	int time;

	pingptr = cl_pinglist;
	for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
		// find free ping slot
		if (pingptr->adr.port) {
			if (!pingptr->time) {
				if (cls.realtime - pingptr->start < 500) {
					// still waiting for response
					continue;
				}
			} else if (pingptr->time < 500) {
				// results have not been queried
				continue;
			}
		}

		// clear it
		pingptr->adr.port = 0;
		return (pingptr);
	}

	// use oldest entry
	pingptr = cl_pinglist;
	best = cl_pinglist;
	oldest = INT_MIN;
	for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
		// scan for oldest
		time = cls.realtime - pingptr->start;
		if (time > oldest) {
			oldest = time;
			best = pingptr;
		}
	}

	return (best);
}

/*
==================
CL_Ping_f
==================
*/
void CL_Ping_f(void) {
	netadr_t to;
	ping_t *pingptr;
	char *server;

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: ping [server]\n");
		return;
	}

	Com_Memset(&to, 0, sizeof(netadr_t));

	server = Cmd_Argv(1);

	if (!NET_StringToAdr(server, &to)) {
		return;
	}

	pingptr = CL_GetFreePing();

	memcpy(&pingptr->adr, &to, sizeof(netadr_t));
	pingptr->start = cls.realtime;
	pingptr->time = 0;

	extern void CL_SetServerInfoByAddress(netadr_t from, const char *info, int ping);
	CL_SetServerInfoByAddress(pingptr->adr, NULL, 0);

	NET_OutOfBandPrint(NS_CLIENT, to, "getinfo xxx");
}

/*
==================
CL_UpdateVisiblePings_f
==================
*/
qboolean CL_UpdateVisiblePings_f(int source) {
	int slots, i, max_req;
	char buff[MAX_STRING_CHARS];
	int pingTime;
	int max;
	qboolean status = qfalse;

	if (source < 0 || source > AS_FAVORITES) {
		return qfalse;
	}

	cls.pingUpdateSource = source;

	// some providers will block a large amount of UDP packets
	if (mv_slowrefresh->integer) {
		max_req = mv_slowrefresh->integer;
	} else {
		max_req = MAX_PINGREQUESTS;
	}

	slots = CL_GetPingQueueCount();
	if (slots < max_req) {
		serverInfo_t *server = NULL;

		max = (source == AS_GLOBAL) ? MAX_GLOBAL_SERVERS : MAX_OTHER_SERVERS;
		switch (source) {
		case AS_LOCAL:
			server = &cls.localServers[0];
			max = cls.numlocalservers;
			break;
		case AS_MPLAYER:
			server = &cls.mplayerServers[0];
			max = cls.nummplayerservers;
			break;
		case AS_GLOBAL:
			server = &cls.globalServers[0];
			max = cls.numglobalservers;
			break;
		case AS_FAVORITES:
			server = &cls.favoriteServers[0];
			max = cls.numfavoriteservers;
			break;
		}
		for (i = 0; i < max; i++) {
			if (server[i].visible) {
				if (server[i].ping == -1) {
					int j;

					if (slots >= max_req) {
						break;
					}
					for (j = 0; j < max_req; j++) {
						if (!cl_pinglist[j].adr.port) {
							continue;
						}
						if (NET_CompareAdr(cl_pinglist[j].adr, server[i].adr)) {
							// already on the list
							break;
						}
					}
					if (j >= max_req) {
						status = qtrue;
						for (j = 0; j < max_req; j++) {
							if (!cl_pinglist[j].adr.port) {
								break;
							}
						}

						memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
						cl_pinglist[j].start = cls.realtime;
						cl_pinglist[j].time = 0;

						NET_OutOfBandPrint(NS_CLIENT, cl_pinglist[j].adr, "getinfo");

						serverStatus_t *serverStatus = CL_GetServerStatus(cl_pinglist[j].adr);
						serverStatus->address = cl_pinglist[j].adr;
						serverStatus->print = qfalse;
						serverStatus->pending = qtrue;
						serverStatus->retrieved = qfalse;
						serverStatus->startTime = Com_Milliseconds();
						serverStatus->time = 0;
						NET_OutOfBandPrint(NS_CLIENT, cl_pinglist[j].adr, "getstatus");

						slots++;
					}
				}
				// if the server has a ping higher than cl_maxPing or
				// the ping packet got lost
				else if (server[i].ping == 0) {
					// if we are updating global servers
					if (source == AS_GLOBAL) {
						//
						if (cls.numGlobalServerAddresses > 0) {
							// overwrite this server with one from the additional global
							// servers
							cls.numGlobalServerAddresses--;
							extern void CL_InitServerInfo(serverInfo_t * server,
								serverAddress_t * address);
							CL_InitServerInfo(
								&server[i],
								&cls.globalServerAddresses[cls.numGlobalServerAddresses]);
							// NOTE: the server[i].visible flag stays untouched
						}
					}
				}
			}
		}
	}

	if (slots) {
		status = qtrue;
	}
	for (i = 0; i < max_req; i++) {
		if (!cl_pinglist[i].adr.port) {
			continue;
		}
		CL_GetPing(i, buff, max_req, &pingTime);
		if (pingTime != 0) {
			CL_ClearPing(i);
			status = qtrue;
		}
	}

	return status;
}
