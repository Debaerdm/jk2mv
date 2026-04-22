#include "client.h"
#include "cl_server_browser.h"

/*
===================
CL_InitServerInfo
===================
*/
void CL_InitServerInfo(serverInfo_t *server, serverAddress_t *address) {
	Com_Memset(server, 0, sizeof(*server));
	server->adr.type = NA_IP;
	server->adr.ip[0] = address->ip[0];
	server->adr.ip[1] = address->ip[1];
	server->adr.ip[2] = address->ip[2];
	server->adr.ip[3] = address->ip[3];
	server->adr.port = address->port;
	server->ping = -1;
	server->gameVersion = VERSION_UNDEF;
}

/*
===================
IsAlreadyInGlobalServerList
===================
*/
serverInfo_t *IsAlreadyInGlobalServerList(serverAddress_t *addr) {
	int j;
	int max;

	max = cls.numglobalservers;
	if (max > MAX_GLOBAL_SERVERS) {
		max = MAX_GLOBAL_SERVERS;
	}

	for (j = 0; j < max; j++) {
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

static void CL_ApplyServerInfoPair(serverInfo_t *server, const char *key, const char *value) {
	if (!Q_stricmp(key, "hostname")) {
		Q_strncpyz(server->hostName, value, MAX_NAME_LENGTH);
	} else if (!Q_stricmp(key, "mapname")) {
		Q_strncpyz(server->mapName, value, MAX_NAME_LENGTH);
	} else if (!Q_stricmp(key, "sv_maxclients")) {
		server->maxClients = atoi(value);
	} else if (!Q_stricmp(key, "game")) {
		Q_strncpyz(server->game, value, MAX_NAME_LENGTH);
	} else if (!Q_stricmp(key, "gametype")) {
		server->gameType = atoi(value);
	} else if (!Q_stricmp(key, "nettype")) {
		server->netType = atoi(value);
	} else if (!Q_stricmp(key, "minping")) {
		server->minPing = atoi(value);
	} else if (!Q_stricmp(key, "maxping")) {
		server->maxPing = atoi(value);
	} else if (!Q_stricmp(key, "needpass")) {
		server->needPassword = (qboolean) !!atoi(value);
	} else if (!Q_stricmp(key, "truejedi")) {
		server->trueJedi = atoi(value);
	} else if (!Q_stricmp(key, "wdisable")) {
		server->weaponDisable = atoi(value);
	} else if (!Q_stricmp(key, "fdisable")) {
		server->forceDisable = atoi(value);
	} else if (!Q_stricmp(key, "protocol")) {
		server->protocol = atoi(value);
	}
}

static void CL_ParseServerInfoString(serverInfo_t *server, const char *info) {
	char key[BIG_INFO_KEY];
	char value[BIG_INFO_VALUE];
	const char *s = info;
	int keyLen;
	int valueLen;

	while (*s) {
		if (*s == '\\') {
			s++;
		}

		keyLen = 0;
		while (*s && *s != '\\') {
			if (keyLen < (int)sizeof(key) - 1) {
				key[keyLen++] = *s;
			}
			s++;
		}
		key[keyLen] = '\0';

		if (*s == '\\') {
			s++;
		}

		valueLen = 0;
		while (*s && *s != '\\') {
			if (valueLen < (int)sizeof(value) - 1) {
				value[valueLen++] = *s;
			}
			s++;
		}
		value[valueLen] = '\0';

		if (!key[0]) {
			break;
		}

		CL_ApplyServerInfoPair(server, key, value);
	}
}

static void CL_SetServerInfo(serverInfo_t *server, const char *info, int ping) {
	if (server) {
		if (info) {
			CL_ParseServerInfoString(server, info);
		}
		server->ping = ping;
	}
}

static void CL_SetServerInfoByAddress(netadr_t from, const char *info, int ping) {
	int maxLocal;
	int maxMplayer;
	int maxGlobal;
	int maxFavorites;
	int i;

	maxLocal = cls.numlocalservers;
	if (maxLocal > MAX_OTHER_SERVERS) {
		maxLocal = MAX_OTHER_SERVERS;
	}
	for (i = 0; i < maxLocal; i++) {
		if (NET_CompareAdr(from, cls.localServers[i].adr)) {
			CL_SetServerInfo(&cls.localServers[i], info, ping);
		}
	}

	maxMplayer = cls.nummplayerservers;
	if (maxMplayer > MAX_OTHER_SERVERS) {
		maxMplayer = MAX_OTHER_SERVERS;
	}
	for (i = 0; i < maxMplayer; i++) {
		if (NET_CompareAdr(from, cls.mplayerServers[i].adr)) {
			CL_SetServerInfo(&cls.mplayerServers[i], info, ping);
		}
	}

	maxGlobal = cls.numglobalservers;
	if (maxGlobal > MAX_GLOBAL_SERVERS) {
		maxGlobal = MAX_GLOBAL_SERVERS;
	}
	for (i = 0; i < maxGlobal; i++) {
		if (NET_CompareAdr(from, cls.globalServers[i].adr)) {
			CL_SetServerInfo(&cls.globalServers[i], info, ping);
		}
	}

	maxFavorites = cls.numfavoriteservers;
	if (maxFavorites > MAX_OTHER_SERVERS) {
		maxFavorites = MAX_OTHER_SERVERS;
	}
	for (i = 0; i < maxFavorites; i++) {
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
