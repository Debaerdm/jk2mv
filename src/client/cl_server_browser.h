#pragma once

// Server browser and ping system
// Handles LAN/internet server discovery, ping requests, and server info queries

void CL_InitServerInfo(serverInfo_t *server, serverAddress_t *address);
serverInfo_t *IsAlreadyInGlobalServerList(serverAddress_t *addr);
void CL_ServersResponsePacket(netadr_t from, msg_t *msg);
void CL_ServerInfoPacket(netadr_t from, msg_t *msg);
serverStatus_t *CL_GetServerStatus(netadr_t from);
int CL_ServerStatus(const char *serverAddress, char *serverStatusString, int maxLen);
void CL_ServerStatusResponse(netadr_t from, msg_t *msg);
void CL_LocalServers_f(void);
void CL_GlobalServers_f(void);
void CL_ServerStatus_f(void);

// Ping management
void CL_GetPing(int n, char *buf, int buflen, int *pingtime);
void CL_UpdateServerInfo(int n);
void CL_GetPingInfo(int n, char *buf, int buflen);
void CL_ClearPing(int n);
int CL_GetPingQueueCount(void);
ping_t *CL_GetFreePing(void);
void CL_Ping_f(void);
qboolean CL_UpdateVisiblePings_f(int source);
