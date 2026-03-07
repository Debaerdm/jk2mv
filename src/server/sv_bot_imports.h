// sv_bot_imports.h -- Botlib import functions (trace, memory, filesystem, BSP)
// Extracted from sv_bot.cpp as part of aggressive server refactoring

#ifndef SV_BOT_IMPORTS_H
#define SV_BOT_IMPORTS_H

/*
==================
BotImport_Print
==================
*/
 __attribute__ ((format (printf, 2, 3)))
void QDECL BotImport_Print(int type, char *fmt, ...)
{
	char str[2048];
	va_list ap;

	va_start(ap, fmt);
	Q_vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);

	switch(type) {
		case PRT_MESSAGE: {
			Com_Printf("%s", str);
			break;
		}
		case PRT_WARNING: {
			Com_Printf(S_COLOR_YELLOW "Warning: %s", str);
			break;
		}
		case PRT_ERROR: {
			Com_Printf(S_COLOR_RED "Error: %s", str);
			break;
		}
		case PRT_FATAL: {
			Com_Printf(S_COLOR_RED "Fatal: %s", str);
			break;
		}
		case PRT_EXIT: {
			Com_Error(ERR_DROP, S_COLOR_RED "Exit: %s", str);
			break;
		}
		default: {
			Com_Printf("unknown print type\n");
			break;
		}
	}
}

/*
==================
BotImport_Trace
==================
*/
void BotImport_Trace(bsp_trace_t *bsptrace, const vec3_t start, const vec3_t mins,
					 const vec3_t maxs, const vec3_t end, int passent, int contentmask)
{
	trace_t trace;

	SV_Trace(&trace, start, mins, maxs, end, passent, contentmask, qfalse, 0, 10);
	//copy the trace information
	bsptrace->allsolid = trace.allsolid;
	bsptrace->startsolid = trace.startsolid;
	bsptrace->fraction = trace.fraction;
	VectorCopy(trace.endpos, bsptrace->endpos);
	bsptrace->plane.dist = trace.plane.dist;
	VectorCopy(trace.plane.normal, bsptrace->plane.normal);
	bsptrace->plane.signbits = trace.plane.signbits;
	bsptrace->plane.type = trace.plane.type;
	bsptrace->surface.value = trace.surfaceFlags;
	bsptrace->ent = trace.entityNum;
	bsptrace->exp_dist = 0;
	bsptrace->sidenum = 0;
	bsptrace->contents = 0;
}

/*
==================
BotImport_EntityTrace
==================
*/
void BotImport_EntityTrace(bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask) {
	trace_t trace;

	SV_ClipToEntity(&trace, start, mins, maxs, end, entnum, contentmask, qfalse);
	//copy the trace information
	bsptrace->allsolid = trace.allsolid;
	bsptrace->startsolid = trace.startsolid;
	bsptrace->fraction = trace.fraction;
	VectorCopy(trace.endpos, bsptrace->endpos);
	bsptrace->plane.dist = trace.plane.dist;
	VectorCopy(trace.plane.normal, bsptrace->plane.normal);
	bsptrace->plane.signbits = trace.plane.signbits;
	bsptrace->plane.type = trace.plane.type;
	bsptrace->surface.value = trace.surfaceFlags;
	bsptrace->ent = trace.entityNum;
	bsptrace->exp_dist = 0;
	bsptrace->sidenum = 0;
	bsptrace->contents = 0;
}

/*
==================
BotImport_PointContents
==================
*/
int BotImport_PointContents(const vec3_t point) {
	return SV_PointContents(point, -1);
}

/*
==================
BotImport_inPVS
==================
*/
int BotImport_inPVS(vec3_t p1, vec3_t p2) {
	return SV_inPVS (p1, p2);
}

/*
==================
BotImport_BSPEntityData
==================
*/
char *BotImport_BSPEntityData(void) {
	return CM_EntityString();
}

/*
==================
BotImport_BSPNumInlineModels
==================
*/
int BotImport_BSPNumInlineModels(void) {
	return CM_NumInlineModels();
}

/*
==================
BotImport_BSPModelMinsMaxsOrigin
==================
*/
void BotImport_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t outmins, vec3_t outmaxs, vec3_t origin) {
	clipHandle_t h;
	vec3_t mins, maxs;
	float max;
	int	i;

	h = CM_InlineModel(modelnum);
	CM_ModelBounds(h, mins, maxs);
	//if the model is rotated
	if ((angles[0] || angles[1] || angles[2])) {
		// expand for rotation

		max = RadiusFromBounds(mins, maxs);
		for (i = 0; i < 3; i++) {
			mins[i] = -max;
			maxs[i] = max;
		}
	}
	if (outmins) VectorCopy(mins, outmins);
	if (outmaxs) VectorCopy(maxs, outmaxs);
	if (origin) VectorClear(origin);
}

/*
==================
BotImport_GetMemoryGame
==================
*/
void *Bot_GetMemoryGame(int size) {
	void *ptr;

	ptr = Z_Malloc( size, TAG_BOTGAME, qtrue );

	return ptr;
}

/*
==================
BotImport_FreeMemoryGame
==================
*/
void Bot_FreeMemoryGame(void *ptr) {
	Z_Free(ptr);
}

/*
==================
BotImport_GetMemory
==================
*/
void *BotImport_GetMemory(int size) {
	void *ptr;

	ptr = Z_Malloc( size, TAG_BOTLIB, qtrue );
	return ptr;
}

/*
==================
BotImport_FreeMemory
==================
*/
void BotImport_FreeMemory(void *ptr) {
	Z_Free(ptr);
}

/*
=================
BotImport_HunkAlloc
=================
*/
void *BotImport_HunkAlloc( int size ) {
	if( Hunk_CheckMark() ) {
		Com_Error( ERR_DROP, "SV_Bot_HunkAlloc: Alloc with marks already set" );
	}
	return Hunk_Alloc( size, h_high );
}

// there's no such thing as this now, since the zone is unlimited, but I have to provide something
//	so it doesn't run out of control alloc-wise (since the bot code calls this in a while() loop to free
//	up bot mem until zone has > 1MB available again. So, simulate a reasonable limit...
//
static int bot_Z_AvailableMemory(void)
{
	const int iMaxBOTLIBMem = 8 * 1024 * 1024;	// adjust accordingly.
	return iMaxBOTLIBMem - Z_MemSize( TAG_BOTLIB );
}

static int BotImport_FS_FOpenFileByMode(const char *qpath, fileHandle_t *f, fsMode_t mode) {
	return FS_FOpenFileByMode(qpath, f, mode, MODULE_BOTLIB);
}

static int BotImport_FS_FOpenFileByModeHash( const char *qpath, fileHandle_t *f, fsMode_t mode, unsigned long *hash ) {
	return FS_FOpenFileByModeHash(qpath, f, mode, hash, MODULE_BOTLIB);
}

static int BotImport_FS_Read2( void *buffer, int len, fileHandle_t f ) {
	return FS_Read2(buffer, len, f, MODULE_BOTLIB);
}

static void BotImport_FS_FCloseFile( fileHandle_t f ) {
	FS_FCloseFile( f, MODULE_BOTLIB );
}
static int BotImport_FS_Write( const void *buffer, int len, fileHandle_t h ) {
	return FS_Write(buffer, len, h, MODULE_BOTLIB);
}

static int BotImport_FS_Seek( fileHandle_t f, int offset, int origin ) {
	return FS_Seek(f, offset, origin, MODULE_BOTLIB);
}

#endif // SV_BOT_IMPORTS_H
