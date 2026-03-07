// sv_world_trace.h -- Collision detection and ray tracing
// Extracted from sv_world.cpp as part of aggressive server refactoring

#ifndef SV_WORLD_TRACE_H
#define SV_WORLD_TRACE_H

/*
================
SV_ClipHandleForEntity

Returns a headnode that can be used for testing or clipping to a
given entity.  If the entity is a bsp model, the headnode will
be returned, otherwise a custom box tree will be constructed.
================
*/
clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent ) {
	if ( ent->r.bmodel ) {
		// explicit hulls in the BSP model
		return CM_InlineModel( ent->s.modelindex );
	}
	if ( ent->r.svFlags & SVF_CAPSULE ) {
		// create a temp capsule from bounding box sizes
		return CM_TempBoxModel( ent->r.mins, ent->r.maxs, qtrue );
	}

	// create a temp tree from bounding box sizes
	return CM_TempBoxModel( ent->r.mins, ent->r.maxs, qfalse );
}

typedef struct {
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	const float	*mins;
	const float *maxs;	// size of the moving object
	vec3_t		start;
	vec3_t		end;
	int			passEntityNum;
	int			contentmask;
	qboolean	capsule;
	int			traceFlags;
	int			useLod;
	trace_t		trace;
} moveclip_t;

/*
====================
SV_ClipToEntity

====================
*/
void SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask, qboolean capsule ) {
	sharedEntity_t	*touch;
	clipHandle_t	clipHandle;
	const float		*origin, *angles;

	touch = SV_GentityNum( entityNum );

	Com_Memset(trace, 0, sizeof(trace_t));

	// if it doesn't have any brushes of a type we
	// are looking for, ignore it
	if ( ! ( contentmask & touch->r.contents ) ) {
		trace->fraction = 1.0;
		return;
	}

	// might intersect, so do an exact clip
	clipHandle = SV_ClipHandleForEntity (touch);

	origin = touch->r.currentOrigin;
	angles = touch->r.currentAngles;

	if ( !touch->r.bmodel ) {
		angles = vec3_origin;	// boxes don't rotate
	}

	CM_TransformedBoxTrace ( trace, start, end,
		mins, maxs, clipHandle,  contentmask,
		origin, angles, capsule);

	if ( trace->fraction < 1 ) {
		trace->entityNum = touch->s.number;
	}
}

/*
====================
SV_ClipMoveToEntities

====================
*/
void SV_ClipMoveToEntities( moveclip_t *clip ) {
	int			i, num;
	int			touchlist[MAX_GENTITIES];
	sharedEntity_t *touch;
	int			passOwnerNum;
	trace_t		trace, oldTrace;
	clipHandle_t	clipHandle;
	const float		*origin, *angles;
	int			thisOwnerShared = 1;

	num = SV_AreaEntities( clip->boxmins, clip->boxmaxs, touchlist, MAX_GENTITIES);

	if ( clip->passEntityNum < 0 ) {
		passOwnerNum = -1;	// common bad API usage in original modules
	} else if ( clip->passEntityNum != ENTITYNUM_NONE ) {
		const sharedEntity_t *passEnt = SV_GentityNum( clip->passEntityNum );

		passOwnerNum = passEnt->r.ownerNum;
		if ( passOwnerNum == ENTITYNUM_NONE ) {
			passOwnerNum = -1;
		}
		if ( passEnt->r.svFlags & SVF_OWNERNOTSHARED )
		{
			thisOwnerShared = 0;
		}
	} else {
		passOwnerNum = -1;
	}

	for ( i=0 ; i<num ; i++ ) {
		if ( clip->trace.allsolid ) {
			return;
		}
		touch = SV_GentityNum( touchlist[i] );

		// see if we should ignore this entity
		if ( clip->passEntityNum != ENTITYNUM_NONE ) {
			if ( touchlist[i] == clip->passEntityNum ) {
				continue;	// don't clip against the pass entity
			}
			if ( touch->r.ownerNum == clip->passEntityNum) {
				if (touch->r.svFlags & SVF_OWNERNOTSHARED)
				{
					if ( clip->contentmask != (MASK_SHOT | CONTENTS_LIGHTSABER) &&
						clip->contentmask != (MASK_SHOT))
					{ //it's not a laser hitting the other "missile", don't care then
						continue;
					}
				}
				else
				{
					continue;	// don't clip against own missiles
				}
			}
			if ( touch->r.ownerNum == passOwnerNum &&
				!(touch->r.svFlags & SVF_OWNERNOTSHARED) &&
				!thisOwnerShared ) {
				continue;	// don't clip against other missiles from our owner
			}
		}

		// if it doesn't have any brushes of a type we
		// are looking for, ignore it
		if ( ! ( clip->contentmask & touch->r.contents ) ) {
			continue;
		}

		if ((clip->contentmask == (MASK_SHOT|CONTENTS_LIGHTSABER) || clip->contentmask == MASK_SHOT) && (touch->r.contents > 0 && (touch->r.contents & CONTENTS_NOSHOT)))
		{
			continue;
		}

		// might intersect, so do an exact clip
		clipHandle = SV_ClipHandleForEntity (touch);

		origin = touch->r.currentOrigin;
		angles = touch->r.currentAngles;

		if ( !touch->r.bmodel ) {
			angles = vec3_origin;	// boxes don't rotate
		}

		CM_TransformedBoxTrace ( &trace, clip->start, clip->end,
			clip->mins, clip->maxs, clipHandle,  clip->contentmask,
			origin, angles, clip->capsule);

		// keep these older variables around for a bit, incase we need to replace them in the Ghoul2 Collision check
		oldTrace = clip->trace;

		if ( trace.allsolid ) {
			clip->trace.allsolid = qtrue;
			trace.entityNum = touch->s.number;
		} else if ( trace.startsolid ) {
			clip->trace.startsolid = qtrue;
			trace.entityNum = touch->s.number;
		}

		if ( trace.fraction < clip->trace.fraction ) {
			qboolean	oldStart;

			// make sure we keep a startsolid from a previous trace
			oldStart = clip->trace.startsolid;

			trace.entityNum = touch->s.number;
			clip->trace = trace;
			clip->trace.startsolid = (qboolean)((unsigned)clip->trace.startsolid | (unsigned)oldStart);
		}
	}
}

/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.
passEntityNum and entities owned by passEntityNum are explicitly not checked.
==================
*/
void SV_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean capsule, int traceFlags, int useLod ) {
	moveclip_t	clip;
	int			i;

	if ( !mins ) {
		mins = vec3_origin;
	}
	if ( !maxs ) {
		maxs = vec3_origin;
	}

	Com_Memset ( &clip, 0, sizeof ( moveclip_t ) );

	// clip to world
	CM_BoxTrace( &clip.trace, start, end, mins, maxs, 0, contentmask, capsule );
	clip.trace.entityNum = clip.trace.fraction != 1.0f ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	if ( clip.trace.fraction == 0 ) {
		*results = clip.trace;
		return;		// blocked immediately by the world
	}

	clip.contentmask = contentmask;
	VectorCopy( start, clip.start );
	clip.traceFlags = traceFlags;
	clip.useLod = useLod;
	VectorCopy( end, clip.end );
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passEntityNum = passEntityNum;
	clip.capsule = capsule;

	// create the bounding box of the entire move
	// we can limit it to the part of the move not
	// already clipped off by the world, which can be
	// a significant savings for line of sight and shot traces
	for ( i=0 ; i<3 ; i++ ) {
		if ( end[i] > start[i] ) {
			clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
		} else {
			clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
		}
	}

	// clip to other solid entities
	SV_ClipMoveToEntities ( &clip );

	*results = clip.trace;
}

/*
=============
SV_PointContents
=============
*/
int SV_PointContents( const vec3_t p, int passEntityNum ) {
	int			touch[MAX_GENTITIES];
	sharedEntity_t *hit;
	int			i, num;
	int			contents, c2;
	clipHandle_t	clipHandle;
	const float		*angles;

	// get base contents from world
	contents = CM_PointContents( p, 0 );

	// or in contents from all the other entities
	num = SV_AreaEntities( p, p, touch, MAX_GENTITIES );

	for ( i=0 ; i<num ; i++ ) {
		if ( touch[i] == passEntityNum ) {
			continue;
		}
		hit = SV_GentityNum( touch[i] );
		// might intersect, so do an exact clip
		clipHandle = SV_ClipHandleForEntity( hit );
		angles = hit->s.angles;
		if ( !hit->r.bmodel ) {
			angles = vec3_origin;	// boxes don't rotate
		}

		c2 = CM_TransformedPointContents (p, clipHandle, hit->s.origin, hit->s.angles);

		contents |= c2;
	}

	return contents;
}

#endif // SV_WORLD_TRACE_H
