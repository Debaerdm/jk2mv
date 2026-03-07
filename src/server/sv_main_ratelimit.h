// sv_main_ratelimit.h -- DDoS protection (rate limiting + whitelist)
// Extracted from sv_main.cpp as part of aggressive server refactoring

#ifndef SV_MAIN_RATELIMIT_H
#define SV_MAIN_RATELIMIT_H

#include <map>
#include <set>

/*
================
SVC_BucketForAddress

Find or allocate a bucket for an address
================
*/
static leakyBucket_t *SVC_BucketForAddress(netadr_t address, int burst, int period, int now) {
	static std::map<int, leakyBucket_t> bucketMap;
	static unsigned int	callCounter = 0;

	if (address.type != NA_IP) {
		return NULL;
	}

	callCounter++;

	if ((callCounter & 0xffffu) == 0) {
		auto it = bucketMap.begin();

		while (it != bucketMap.end()) {
			int interval = now - it->second.lastTime;

			if (interval > it->second.burst * period || interval < 0) {
				it = bucketMap.erase(it);
			} else {
				++it;
			}
		}
	}

	return &bucketMap[address.ipi];
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
	leakyBucket_t *bucket = SVC_BucketForAddress(from, burst, period, now);

	return SVC_RateLimit(bucket, burst, period, now);
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
