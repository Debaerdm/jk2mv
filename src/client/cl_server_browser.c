#include "client.h"
#include "cl_server_browser.h"

/*
===================
CL_InitServerInfo
===================
*/
void CL_InitServerInfo(serverInfo_t *server, serverAddress_t *address) {
	server->adr.type = NA_IP;
	server->adr.ip[0] = address->ip[0];
	server->adr.ip[1] = address->ip[1];
	server->adr.ip[2] = address->ip[2];
	server->adr.ip[3] = address->ip[3];
	server->adr.port = address->port;
	server->clients = 0;
	server->bots = 0;
	server->hostName[0] = '\0';
	server->mapName[0] = '\0';
	server->maxClients = 0;
	server->maxPing = 0;
	server->minPing = 0;
	server->ping = -1;
	server->game[0] = '\0';
	server->gameType = 0;
	server->netType = 0;
	server->needPassword = qfalse;
	server->trueJedi = 0;
	server->weaponDisable = 0;
	server->forceDisable = 0;
	server->gameVersion = VERSION_UNDEF;
}

/*
===================
IsAlreadyInGlobalServerList
===================
*/
serverInfo_t *IsAlreadyInGlobalServerList(serverAddress_t *addr) {
	int j;

	for (j = 0; j < cls.numglobalservers && j < MAX_GLOBAL_SERVERS; j++) {
		if (cls.globalServers[j].adr.ip[0] == addr->ip[0] &&
			cls.globalServers[j].adr.ip[1] == addr->ip[1] &&
			cls.globalServers[j].adr.ip[2] == addr->ip[2] &&
			cls.globalServers[j].adr.ip[3] == addr->ip[3] &&
			cls.globalServers[j].adr.port == addr->port) {
			return &cls.globalServers[j];
		}
	}

	return NULL;
}

static void CL_SetServerInfo(serverInfo_t *server, const char *info, int ping) {
	if (server) {
		if (info) {
			Q_strncpyz(server->hostName, Info_ValueForKey(info, "hostname"),
				MAX_NAME_LENGTH);
			Q_strncpyz(server->mapName, Info_ValueForKey(info, "mapname"),
				MAX_NAME_LENGTH);
			server->maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
			Q_strncpyz(server->game, Info_ValueForKey(info, "game"),
				MAX_NAME_LENGTH);
			server->gameType = atoi(Info_ValueForKey(info, "gametype"));
			server->netType = atoi(Info_ValueForKey(info, "nettype"));
			server->minPing = atoi(Info_ValueForKey(info, "minping"));
			server->maxPing = atoi(Info_ValueForKey(info, "maxping"));
			server->needPassword = (qboolean) !!atoi(Info_ValueForKey(info, "needpass"));
			server->trueJedi = atoi(Info_ValueForKey(info, "truejedi"));
			server->weaponDisable = atoi(Info_ValueForKey(info, "wdisable"));
			server->forceDisable = atoi(Info_ValueForKey(info, "fdisable"));
			server->protocol = atoi(Info_ValueForKey(info, "protocol"));
		}
		server->ping = ping;
	}
}

static void CL_SetServerInfoByAddress(netadr_t from, const char *info, int ping) {
	int i;

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.localServers[i].adr)) {
			CL_SetServerInfo(&cls.localServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.mplayerServers[i].adr)) {
			CL_SetServerInfo(&cls.mplayerServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.globalServers[i].adr)) {
			CL_SetServerInfo(&cls.globalServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, cls.favoriteServers[i].adr)) {
			CL_SetServerInfo(&cls.favoriteServers[i], info, ping);
		}
	}
}

/*
===================
CL_ServersResponsePacket
===================
*/
void CL_ServersResponsePacket(netadr_t from, msg_t *msg) {
	int i, count, max, total;
	serverAddress_t addresses[MAX_SERVERSPERPACKET];
	int numservers;
	byte *buffptr;
	byte *buffend;

	Com_Printf("CL_ServersResponsePacket\n");

	if (cls.numglobalservers == -1) {
		cls.numglobalservers = 0;
		cls.numGlobalServerAddresses = 0;
	}

	if (cls.nummplayerservers == -1) {
		cls.nummplayerservers = 0;
	}

	// parse through server response string
	numservers = 0;
	buffptr = msg->data;
	buffend = buffptr + msg->cursize;
	while (buffptr + 1 < buffend) {
		// advance to initial token
		do {
			if (*buffptr++ == '\\')
				break;
		} while (buffptr < buffend);

		if (buffptr >= buffend - 6) {
			break;
		}

		// parse out ip
		addresses[numservers].ip[0] = *buffptr++;
		addresses[numservers].ip[1] = *buffptr++;
		addresses[numservers].ip[2] = *buffptr++;
		addresses[numservers].ip[3] = *buffptr++;

		// parse out port
		addresses[numservers].port = (unsigned short)((*buffptr++) << 8);
		addresses[numservers].port += (unsigned short)(*buffptr++);
		addresses[numservers].port = BigShort(addresses[numservers].port);

		// syntax check
		if (*buffptr != '\\') {
			break;
		}

		Com_DPrintf("server: %d ip: %d.%d.%d.%d:%d\n", numservers,
			addresses[numservers].ip[0], addresses[numservers].ip[1],
			addresses[numservers].ip[2], addresses[numservers].ip[3],
			addresses[numservers].port);

		numservers++;
		if (numservers >= MAX_SERVERSPERPACKET) {
			break;
		}

		// parse out EOT
		if (buffptr[1] == 'E' && buffptr[2] == 'O' && buffptr[3] == 'T') {
			break;
		}
	}

	if (cls.masterNum == 0) {
		count = cls.numglobalservers;
		max = MAX_GLOBAL_SERVERS;
	} else {
		count = cls.nummplayerservers;
		max = MAX_OTHER_SERVERS;
	}

	for (i = 0; i < numservers && count < max; i++) {
		serverInfo_t *server;

		// multimaster
		server = IsAlreadyInGlobalServerList(&addresses[i]);
		if (cls.masterNum != 0 || !server) {
			server = (cls.masterNum == 0) ? &cls.globalServers[count]
									  : &cls.mplayerServers[count];
			CL_InitServerInfo(server, &addresses[i]);
			count++;
		}
	}

	// if getting the global list
	if (cls.masterNum == 0) {
		if (cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS) {
			// if we couldn't store the servers in the main list anymore
			for (; i < numservers && count >= max; i++) {
				serverAddress_t *addr;

				// multimaster
				if (cls.masterNum == 0 &&
					IsAlreadyInGlobalServerList(&addresses[i])) {
					continue;
				}

				// just store the addresses in an additional list
				addr = &cls.globalServerAddresses[cls.numGlobalServerAddresses++];
				addr->ip[0] = addresses[i].ip[0];
				addr->ip[1] = addresses[i].ip[1];
				addr->ip[2] = addresses[i].ip[2];
				addr->ip[3] = addresses[i].ip[3];
				addr->port = addresses[i].port;
			}
		}
	}

	if (cls.masterNum == 0) {
		cls.numglobalservers = count;
		total = count + cls.numGlobalServerAddresses;
	} else {
		cls.nummplayerservers = count;
		total = count;
	}

	Com_Printf("%d servers parsed (total %d)\n", numservers, total);
}
