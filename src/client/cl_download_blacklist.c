#include "client.h"
#include "cl_download_blacklist.h"

/*
=================
CL_BlacklistCurrentFile
=================
*/
void CL_BlacklistCurrentFile(void) {
	// create a new buffer
	blacklistentry_t *entrys = (blacklistentry_t *)Z_Malloc(
		(int)((cls.downloadBlacklistLen + 1) * sizeof(blacklistentry_t)),
		TAG_DOWNLOADBLACKLIST, qtrue);

	// in case we already had a blacklist, copy it
	if (cls.downloadBlacklist) {
		Com_Memcpy(entrys, cls.downloadBlacklist,
			cls.downloadBlacklistLen * sizeof(blacklistentry_t));
		Z_Free(cls.downloadBlacklist);
	}

	// write new blacklist entry to the end
	blacklistentry_t *entry = &entrys[cls.downloadBlacklistLen++];
	Q_strncpyz(entry->name, cl_downloadName->string, sizeof(entry->name));
	entry->checksum = clc.downloadChksums[clc.downloadIndex];
	entry->time = time(0);
	entry->server = clc.serverAddress;

	cls.downloadBlacklist = entrys;
}

/*
=================
CL_ReadBlacklistFile

Reads dlblacklist.dat which contains a list of files which
the user blocked permanently.
=================
*/
void CL_ReadBlacklistFile(void) {
	fileHandle_t fblacklist;
	int len;

	cls.downloadBlacklistLen = 0;
	len = FS_SV_FOpenFileRead("dlblacklist.dat", &fblacklist);
	if (len >= (int)sizeof(uint8_t)) {
		uint8_t version;

		FS_Read(&version, sizeof(uint8_t), fblacklist);
		if (version == BLACKLIST_FILE_VERSION) {
			int entryslen = len - sizeof(uint8_t);
			if (entryslen) {
				cls.downloadBlacklist =
					(blacklistentry_t *)Z_Malloc((int)entryslen,
						TAG_DOWNLOADBLACKLIST, qtrue);
				FS_Read(cls.downloadBlacklist, entryslen, fblacklist);
				cls.downloadBlacklistLen = entryslen / (int)sizeof(blacklistentry_t);
			}
		} else {
			Com_Printf("blacklist file version mismatch\n");
		}

		FS_FCloseFile(fblacklist);
	}
}

/*
=================
CL_BlacklistRemoveFile

Remove a file from the blacklist
=================
*/
qboolean CL_BlacklistRemoveFile(const blacklistentry_t *file) {
	int index = file - cls.downloadBlacklist;

	if (index < 0 || index >= cls.downloadBlacklistLen) {
		return qtrue;
	}

	if (index < cls.downloadBlacklistLen - 1) {
		memmove(&cls.downloadBlacklist[index], &cls.downloadBlacklist[index + 1],
			(cls.downloadBlacklistLen - index - 1) * sizeof(blacklistentry_t));
	}

	cls.downloadBlacklistLen--;
	return qfalse;
}

/*
=================
CL_BlacklistWriteCloseFile

Writes dlblacklist.dat which contains a list of files which
the user blocked permanently.
=================
*/
void CL_BlacklistWriteCloseFile(void) {
	if (cls.downloadBlacklist) {
		fileHandle_t fblacklist = FS_SV_FOpenFileWrite("dlblacklist.dat");
		uint8_t version = BLACKLIST_FILE_VERSION;

		if (fblacklist) {
			if (FS_Write(&version, sizeof(version), fblacklist)) {
				if (FS_Write(cls.downloadBlacklist,
						(int)(cls.downloadBlacklistLen * sizeof(blacklistentry_t)),
						fblacklist)) {
					Com_DPrintf("blacklist file written to dlblacklist.dat\n");
				}
			}

			FS_FCloseFile(fblacklist);
		} else {
			Com_Printf("could not write blacklist file dlblacklist.dat\n");
		}

		Z_Free(cls.downloadBlacklist);
		cls.downloadBlacklist = NULL;
		cls.downloadBlacklistLen = 0;
	}
}
