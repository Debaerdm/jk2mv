// test_sv_world_clip.cpp - world queries of the server on the real code:
// unlinking from a world sector in O(1) and the order of the area query
//
// Unlike the older server tests, this links the real engine sources: the
// server's world module (src/server/sv_world.cpp) and the collision model
// (src/qcommon/cm_*.cpp), on maps built in memory and loaded by CM_LoadMap.

#include <gtest/gtest.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <list>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "server/server.h"
#include "qcommon/qfiles.h"

// the real SV_GentityNum, SV_SvEntityForGentity and SV_GEntityForSvEntity
#include "server/sv_game_entities.h"

// sv_world_sectors.h, which sv_world.cpp includes
typedef struct worldSector_s {
	int		axis;		// -1 = leaf node
	float	dist;
	struct worldSector_s	*children[2];
	svEntity_t	*entities;
} worldSector_t;
extern worldSector_t	sv_worldSectors[64];
extern int				sv_numworldSectors;

server_t	sv;
cvar_t		*mv_fixturretcrash;
cvar_t		*mv_fixplayerghosting;

// ===========================================================================
// the engine services the collision model and the world module use

namespace {

std::string					g_printed;	// what Com_Printf printed
std::vector<unsigned char>	g_bsp;		// the map file
const char					*kMapName = "maps/sv_world_clip.bsp";
size_t						g_bspRead;
std::vector<unsigned char>	g_hunk( 64 << 20 );
size_t						g_hunkUsed;
std::map<std::string, cvar_t>	g_cvars;

void *HunkAlloc( int size ) {
	const size_t aligned = ( (size_t)size + 31 ) & ~(size_t)31;
	if ( g_hunkUsed + aligned > g_hunk.size() ) {
		throw std::runtime_error( "test hunk exhausted" );
	}
	void *p = &g_hunk[g_hunkUsed];
	g_hunkUsed += aligned;
	memset( p, 0, size );
	return p;
}

cvar_t *GetCvar( const char *name, const char *value ) {
	std::map<std::string, cvar_t>::iterator it = g_cvars.find( name );
	if ( it == g_cvars.end() ) {
		cvar_t cv;
		memset( &cv, 0, sizeof( cv ) );
		cv.value = (float)atof( value );
		cv.integer = atoi( value );
		it = g_cvars.insert( std::make_pair( std::string( name ), cv ) ).first;
	}
	return &it->second;
}

} // namespace

void QDECL Com_Printf( const char *fmt, ... ) {
	char	text[4096];
	va_list	ap;
	va_start( ap, fmt );
	vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	g_printed += text;
}

void QDECL Com_DPrintf( const char *fmt, ... ) {
}

void QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	char	text[4096];
	va_list	ap;
	va_start( ap, fmt );
	vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	throw std::runtime_error( text );
}

cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	return GetCvar( name, value );
}

#ifdef HUNK_DEBUG
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line ) {
	return HunkAlloc( size );
}
#else
void *Hunk_Alloc( int size, ha_pref preference ) {
	return HunkAlloc( size );
}
#endif

void *Z_Malloc( int size, memtag_t tag, qboolean zero ) {
	return calloc( 1, size );
}

void Z_Free( void *ptr ) {
	free( ptr );
}

unsigned Com_BlockChecksum( const void *buffer, int length ) {
	return 0;
}

int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE, module_t module, qboolean skipJKA ) {
	if ( strcmp( qpath, kMapName ) ) {
		*file = 0;		// the .ent override
		return -1;
	}
	*file = 1;
	g_bspRead = 0;
	return (int)g_bsp.size();
}

int FS_Read( void *buffer, int len, fileHandle_t f, module_t module ) {
	memcpy( buffer, &g_bsp[g_bspRead], len );
	g_bspRead += len;
	return len;
}

void FS_FCloseFile( fileHandle_t f, module_t module ) {
}

void BotDrawDebugPolygons( void (*drawPoly)(int color, int numPoints, float *points), int value ) {
}

namespace {

// ===========================================================================
// maps built in memory: solid boxes

struct Brush {
	vec3_t	mins, maxs;
};

struct Model {
	std::vector<Brush>	brushes;
};

Brush BoxBrush( float x0, float y0, float z0, float x1, float y1, float z1 ) {
	Brush b;
	VectorSet( b.mins, x0, y0, z0 );
	VectorSet( b.maxs, x1, y1, z1 );
	return b;
}

void GrowBounds( const vec3_t v, vec3_t mins, vec3_t maxs ) {
	for ( int i = 0; i < 3; i++ ) {
		if ( v[i] < mins[i] ) mins[i] = v[i];
		if ( v[i] > maxs[i] ) maxs[i] = v[i];
	}
}

template<typename T> void AppendLump( std::vector<unsigned char> &file, dheader_t &header, int lump, const std::vector<T> &items ) {
	while ( file.size() % 4 ) {
		file.push_back( 0 );
	}
	header.lumps[lump].fileofs = (int)file.size();
	header.lumps[lump].filelen = (int)( items.size() * sizeof( T ) );
	if ( !items.empty() ) {
		const unsigned char *p = (const unsigned char *)&items[0];
		file.insert( file.end(), p, p + items.size() * sizeof( T ) );
	}
}

// one node over two leafs, the world's brushes in the open leaf
std::vector<unsigned char> BuildBsp( const Model &world ) {
	std::vector<dshader_t>		shaders( 1 );
	std::vector<dplane_t>		planes;
	std::vector<dbrushside_t>	sides;
	std::vector<dbrush_t>		brushes;
	std::vector<drawVert_t>		verts;
	std::vector<dsurface_t>		surfaces;
	std::vector<dmodel_t>		models( 1 );
	std::vector<int>			leafbrushes, leafsurfaces;
	std::vector<dleaf_t>		leafs( 2 );
	std::vector<dnode_t>		nodes( 1 );
	std::vector<char>			entities;

	memset( &shaders[0], 0, sizeof( shaders[0] ) );
	strcpy( shaders[0].shader, "textures/test/solid" );
	shaders[0].contentFlags = CONTENTS_SOLID;

	// the node's plane is far below everything, so all of it is in front
	dplane_t nodePlane = { { 0, 0, 1 }, -16384 };
	planes.push_back( nodePlane );

	dmodel_t &out = models[0];
	memset( &out, 0, sizeof( out ) );
	VectorSet( out.mins, 99999, 99999, 99999 );
	VectorSet( out.maxs, -99999, -99999, -99999 );
	out.numBrushes = (int)world.brushes.size();
	for ( size_t b = 0; b < world.brushes.size(); b++ ) {
		const Brush &br = world.brushes[b];
		dbrush_t db;
		db.firstSide = (int)sides.size();
		db.numSides = 6;
		db.shaderNum = 0;
		for ( int s = 0; s < 6; s++ ) {
			// -x +x -y +y -z +z, which CM_BoundBrush expects
			dplane_t p;
			memset( &p, 0, sizeof( p ) );
			const int axis = s >> 1;
			p.normal[axis] = ( s & 1 ) ? 1.0f : -1.0f;
			p.dist = ( s & 1 ) ? br.maxs[axis] : -br.mins[axis];
			dbrushside_t side = { (int)planes.size(), 0, -1 };
			planes.push_back( p );
			sides.push_back( side );
		}
		brushes.push_back( db );
		GrowBounds( br.mins, out.mins, out.maxs );
		GrowBounds( br.maxs, out.mins, out.maxs );
		leafbrushes.push_back( (int)b );
	}

	memset( &leafs[0], 0, leafs.size() * sizeof( leafs[0] ) );
	leafs[0].cluster = 0;
	leafs[0].area = 0;
	leafs[0].firstLeafBrush = 0;
	leafs[0].numLeafBrushes = (int)leafbrushes.size();
	leafs[1].cluster = -1;		// the solid leaf
	leafs[1].area = -1;
	memset( &nodes[0], 0, sizeof( nodes[0] ) );
	nodes[0].planeNum = 0;
	nodes[0].children[0] = -1;
	nodes[0].children[1] = -2;

	const char ents[] = "{\n\"classname\" \"worldspawn\"\n}\n";
	entities.assign( ents, ents + sizeof( ents ) );

	std::vector<unsigned char> file( sizeof( dheader_t ), 0 );
	dheader_t header;
	memset( &header, 0, sizeof( header ) );
	header.ident = BSP_IDENT;
	header.version = BSP_VERSION;
	AppendLump( file, header, LUMP_ENTITIES, entities );
	AppendLump( file, header, LUMP_SHADERS, shaders );
	AppendLump( file, header, LUMP_PLANES, planes );
	AppendLump( file, header, LUMP_NODES, nodes );
	AppendLump( file, header, LUMP_LEAFS, leafs );
	AppendLump( file, header, LUMP_LEAFSURFACES, leafsurfaces );
	AppendLump( file, header, LUMP_LEAFBRUSHES, leafbrushes );
	AppendLump( file, header, LUMP_MODELS, models );
	AppendLump( file, header, LUMP_BRUSHES, brushes );
	AppendLump( file, header, LUMP_BRUSHSIDES, sides );
	AppendLump( file, header, LUMP_DRAWVERTS, verts );
	AppendLump( file, header, LUMP_SURFACES, surfaces );
	memcpy( &file[0], &header, sizeof( header ) );
	return file;
}

sharedEntity_t	g_ents[MAX_GENTITIES];

void ResetEntities() {
	memset( g_ents, 0, sizeof( g_ents ) );
	for ( int i = 0; i < MAX_GENTITIES; i++ ) {
		g_ents[i].s.number = i;
		g_ents[i].r.ownerNum = ENTITYNUM_NONE;
	}
	sv.gentities = g_ents;
	sv.gentitySize = sizeof( sharedEntity_t );
	sv.num_entities = MAX_GENTITIES;
}

void LoadMap( const Model &world ) {
	int checksum;

	mv_fixturretcrash = GetCvar( "mv_fixturretcrash", "0" );
	mv_fixplayerghosting = GetCvar( "mv_fixplayerghosting", "0" );
	g_bsp = BuildBsp( world );
	g_hunkUsed = 0;
	CM_LoadMap( kMapName, qfalse, &checksum );
	SV_ClearWorld();
	ResetEntities();
	g_printed.clear();
}

sharedEntity_t *BoxEntity( int num, float x, float y, float z, const vec3_t mins, const vec3_t maxs, int contents ) {
	sharedEntity_t *ent = &g_ents[num];
	VectorSet( ent->r.currentOrigin, x, y, z );
	VectorCopy( mins, ent->r.mins );
	VectorCopy( maxs, ent->r.maxs );
	ent->r.contents = contents;
	SV_LinkEntity( ent );
	return ent;
}

// a floor, so the arena has a world below everything
Model FloorWorld() {
	Model world;
	world.brushes.push_back( BoxBrush( -2048, -2048, -64, 2048, 2048, 0 ) );
	return world;
}

// seeded, so that a failure can be replayed
class Rng {
public:
	explicit Rng( unsigned seed ) : gen( seed ) {}
	float Range( float a, float b ) { return a < b ? std::uniform_real_distribution<float>( a, b )( gen ) : a; }
	int Int( int a, int b ) { return std::uniform_int_distribution<int>( a, b )( gen ); }
	bool Chance( float p ) { return Range( 0, 1 ) < p; }
private:
	std::mt19937 gen;
};

// ===========================================================================
// unlinking in O(1): the lists keep the order the old code gave them, new
// entities first and the others where they were

void ExpectSectors( const std::vector< std::list<int> > &expected ) {
	for ( int s = 0; s < sv_numworldSectors; s++ ) {
		std::vector<int> got;
		const svEntity_t *prev = NULL;
		for ( const svEntity_t *e = sv_worldSectors[s].entities; e; e = e->nextEntityInWorldSector ) {
			ASSERT_EQ( e->prevEntityInWorldSector, prev ) << "sector " << s;
			ASSERT_EQ( e->worldSector, &sv_worldSectors[s] );
			got.push_back( (int)( e - sv.svEntities ) );
			prev = e;
			ASSERT_LT( got.size(), (size_t)MAX_GENTITIES );
		}
		ASSERT_EQ( got, std::vector<int>( expected[s].begin(), expected[s].end() ) ) << "sector " << s;
	}
}

void PreOrder( const worldSector_t *node, const std::vector< std::list<int> > &expected, std::vector<int> &out ) {
	const std::list<int> &l = expected[node - sv_worldSectors];
	out.insert( out.end(), l.begin(), l.end() );
	if ( node->axis != -1 ) {
		PreOrder( node->children[0], expected, out );
		PreOrder( node->children[1], expected, out );
	}
}

TEST(SvWorldUnlink, ListsKeepTheirOrder) {
	LoadMap( FloorWorld() );
	ASSERT_EQ( sv_numworldSectors, 31 );

	Rng rng( 4242 );
	std::vector< std::list<int> > expected( sv_numworldSectors );
	std::vector<int> sectorOf( 200, -1 );

	for ( int op = 0; op < 20000; op++ ) {
		const int num = rng.Int( 0, 199 );
		sharedEntity_t *e = &g_ents[num];
		svEntity_t *sve = &sv.svEntities[num];

		if ( sectorOf[num] >= 0 ) {
			expected[sectorOf[num]].remove( num );
			sectorOf[num] = -1;
		}
		if ( rng.Chance( 0.3f ) ) {
			SV_UnlinkEntity( e );
		} else {
			// small and big boxes, some across the middle, some outside the world
			const float size = rng.Chance( 0.8f ) ? rng.Range( 1, 64 ) : rng.Range( 64, 1500 );
			VectorSet( e->r.mins, -size, -size, -rng.Range( 0, 64 ) );
			VectorSet( e->r.maxs, size, size, rng.Range( 1, 64 ) );
			VectorSet( e->r.currentOrigin, rng.Range( -2500, 2500 ), rng.Range( -2500, 2500 ), rng.Range( -100, 600 ) );
			e->r.contents = CONTENTS_BODY;
			SV_LinkEntity( e );
			if ( sve->worldSector ) {
				sectorOf[num] = (int)( (worldSector_t *)sve->worldSector - sv_worldSectors );
				expected[sectorOf[num]].push_front( num );
			}
		}
		if ( !sve->worldSector ) {
			EXPECT_EQ( sve->nextEntityInWorldSector, (svEntity_t *)NULL );
			EXPECT_EQ( sve->prevEntityInWorldSector, (svEntity_t *)NULL );
		}
		if ( op % 97 == 0 ) {
			ExpectSectors( expected );
			if ( HasFatalFailure() ) {
				return;
			}
			// the area query walks the sectors in the same order
			const vec3_t mins = { -99999, -99999, -99999 }, maxs = { 99999, 99999, 99999 };
			int list[MAX_GENTITIES];
			const int n = SV_AreaEntities( mins, maxs, list, MAX_GENTITIES );
			std::vector<int> want;
			PreOrder( sv_worldSectors, expected, want );
			ASSERT_EQ( std::vector<int>( list, list + n ), want );
		}
	}
	ExpectSectors( expected );
	EXPECT_EQ( g_printed, "" );
}

// Over a smaller box, the area query returns the entities of the bigger
// box's list that touch the smaller one, in the same order, whatever sectors
// they are in: the lists keep their order.
TEST(SvWorldArea, SmallerBoxKeepsTheOrder) {
	LoadMap( FloorWorld() );
	Rng rng( 1234 );
	for ( int i = 0; i < 400; i++ ) {
		const float size = rng.Chance( 0.8f ) ? rng.Range( 1, 48 ) : rng.Range( 48, 1200 );
		const vec3_t mins = { -size, -size, -rng.Range( 0, 48 ) };
		const vec3_t maxs = { size, size, rng.Range( 1, 64 ) };
		BoxEntity( i, rng.Range( -2100, 2100 ), rng.Range( -2100, 2100 ), rng.Range( -50, 400 ), mins, maxs, CONTENTS_BODY );
	}

	int big[MAX_GENTITIES], small[MAX_GENTITIES];
	long listed = 0;
	for ( int n = 0; n < 3000; n++ ) {
		vec3_t bigMins, bigMaxs, mins, maxs;
		for ( int k = 0; k < 3; k++ ) {
			const float a = rng.Range( -2300, 2300 ), b = rng.Range( -2300, 2300 );
			bigMins[k] = a < b ? a : b;
			bigMaxs[k] = a < b ? b : a;
			// one end pulled in, or both
			mins[k] = rng.Chance( 0.6f ) ? bigMins[k] : rng.Range( bigMins[k], bigMaxs[k] );
			maxs[k] = rng.Chance( 0.6f ) ? bigMaxs[k] : rng.Range( mins[k], bigMaxs[k] );
		}
		const int nb = SV_AreaEntities( bigMins, bigMaxs, big, MAX_GENTITIES );
		const int ns = SV_AreaEntities( mins, maxs, small, MAX_GENTITIES );
		std::vector<int> want;
		for ( int j = 0; j < nb; j++ ) {
			const sharedEntity_t *e = &g_ents[big[j]];
			if ( !( e->r.absmin[0] > maxs[0] || e->r.absmin[1] > maxs[1] || e->r.absmin[2] > maxs[2]
				|| e->r.absmax[0] < mins[0] || e->r.absmax[1] < mins[1] || e->r.absmax[2] < mins[2] ) ) {
				want.push_back( big[j] );
			}
		}
		ASSERT_EQ( std::vector<int>( small, small + ns ), want ) << "query " << n;
		listed += ns;
	}
	EXPECT_GT( listed, 3000L * 10 );
	EXPECT_EQ( g_printed, "" );
}

} // namespace
