// sv_main_ratelimit.h -- DDoS protection (rate limiting + whitelist)
// Extracted from sv_main.cpp as part of aggressive server refactoring

#ifndef SV_MAIN_RATELIMIT_H
#define SV_MAIN_RATELIMIT_H

#include <map>
#include <set>

// per-address buckets for one kind of packet
typedef struct svcBucketTable_s {
	std::map<int, leakyBucket_t>	bucketMap;
	unsigned int					callCounter;
} svcBucketTable_t;

/*
================
SVC_BucketForAddress

Find or allocate a bucket for an address
================
*/
static leakyBucket_t *SVC_BucketForAddress(svcBucketTable_t &table, netadr_t address, int burst, int period, int now) {
	if (address.type != NA_IP) {
		return NULL;
	}

	table.callCounter++;

	if ((table.callCounter & 0xffffu) == 0) {
		auto it = table.bucketMap.begin();

		while (it != table.bucketMap.end()) {
			int interval = now - it->second.lastTime;

			if (interval > it->second.burst * period || interval < 0) {
				it = table.bucketMap.erase(it);
			} else {
				++it;
			}
		}
	}

	return &table.bucketMap[address.ipi];
}

/*
================
SVC_RateLimit

Allows one packet per period msec with bucket size of burst
================
*/
qboolean SVC_RateLimit(leakyBucket_t *bucket, int burst, int period, int now) {
	if (bucket != NULL) {
		int interval = now - bucket->lastTime;
		int expired = interval / period;
		int expiredRemainder = interval % period;

		if (expired > bucket->burst || interval < 0) {
			bucket->burst = 0;
			bucket->lastTime = now;
		} else {
			bucket->burst -= expired;
			bucket->lastTime = now - expiredRemainder;
		}

		if (bucket->burst < burst) {
			bucket->burst++;

			return qfalse;
		}

		return qtrue;
	} else {
		return qfalse;
	}
}

/*
================
SVC_RateLimitAddress

Rate limit for a particular address
================
*/
static qboolean SVC_RateLimitAddress(netadr_t from, int burst, int period, int now) {
	static svcBucketTable_t table;
	leakyBucket_t *bucket = SVC_BucketForAddress(table, from, burst, period, now);

	return SVC_RateLimit(bucket, burst, period, now);
}

/*
================
SVC_RateLimitDisconnect

Rate limit for the "disconnect" replies to sequenced packets from unknown
addresses: 10 at once, then 10 per second. The buckets are not those of
connectionless packets, so a client the server no longer knows, which sends
packets for a few more seconds, does not use up the requests of its address.
================
*/
static qboolean SVC_RateLimitDisconnect(netadr_t from, int now) {
	static svcBucketTable_t table;
	leakyBucket_t *bucket = SVC_BucketForAddress(table, from, 10, 100, now);

	return SVC_RateLimit(bucket, 10, 100, now);
}

// ============================================================================
// WHITELIST SYSTEM
// ============================================================================

#define WHITELIST_FILE			"ipwhitelist.dat"

static std::set<int32_t>	svc_whitelist;

void SVC_LoadWhitelist( void ) {
	fileHandle_t f;
	int32_t *data = NULL;
	int len = FS_SV_FOpenFileRead(WHITELIST_FILE, &f);

	if (len <= 0) {
		return;
	}

	data = (int32_t *)Z_Malloc(len, TAG_TEMP_WORKSPACE);

	FS_FLock(f, FLOCK_SH, qfalse);
	FS_Read(data, len, f);
	FS_FLock(f, FLOCK_UN, qfalse);
	FS_FCloseFile(f);

	len /= sizeof(int32_t);

	for (int i = 0; i < len; i++) {
		svc_whitelist.insert(data[i]);
	}

	Z_Free(data);
	data = NULL;
}

void SVC_WhitelistAdr( netadr_t adr ) {
	fileHandle_t f;

	if (adr.type != NA_IP) {
		return;
	}

	if (!svc_whitelist.insert(adr.ipi).second) {
		return;
	}

	Com_DPrintf("Whitelisting %s\n", NET_AdrToString(adr));

	f = FS_SV_FOpenFileAppend(WHITELIST_FILE);
	if (!f) {
		Com_Printf("Couldn't open " WHITELIST_FILE ".\n");
		return;
	}

	FS_FLock(f, FLOCK_EX, qfalse);
	FS_Write(adr.ip, sizeof(adr.ip), f);
	FS_FLock(f, FLOCK_UN, qfalse);
	FS_FCloseFile(f);
}

static bool SVC_IsWhitelisted( netadr_t adr ) {
	if (adr.type == NA_IP) {
		return svc_whitelist.find(adr.ipi) != svc_whitelist.end();
	} else {
		return true;
	}
}

#endif // SV_MAIN_RATELIMIT_H
