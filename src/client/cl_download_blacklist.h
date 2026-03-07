#pragma once

// Download blacklist management
// Tracks files the user has permanently blocked from downloading

void CL_BlacklistCurrentFile(void);
void CL_ReadBlacklistFile(void);
qboolean CL_BlacklistRemoveFile(const blacklistentry_t *file);
void CL_BlacklistWriteCloseFile(void);
