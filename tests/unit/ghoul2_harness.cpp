// ghoul2_harness.cpp - what the real Ghoul2 sources need from the rest of the
// engine, for the FE-16 reference tests and benchmarks (see ghoul2_harness.h)

#include "ghoul2_harness.h"

#include <unzip.h>	// minizip, to read the retail pk3

#include <map>
#include <stdexcept>
#include <string.h>

//
// engine globals the Ghoul2 code reads
//

trGlobals_t			tr;
shaderCommands_t	tess;
refimport_t			ri;
hitMatReg_t			hitMatReg[MAX_HITMAT_ENTRIES];

const int lightmapsNone[MAXLIGHTMAPS] = { LIGHTMAP_NONE, LIGHTMAP_NONE, LIGHTMAP_NONE, LIGHTMAP_NONE };
const byte stylesDefault[MAXLIGHTMAPS] = { LS_NORMAL, LS_LSNONE, LS_LSNONE, LS_LSNONE };

namespace g2t {

cvar_t	animSmooth;
cvar_t	unsquash;
cvar_t	shadows;
cvar_t	lodBias;
static cvar_t	lodScale;
static cvar_t	autoLodScale;
static cvar_t	noServerGhoul2;
static cvar_t	convertModelBones;
static cvar_t	verbose;

float	projectedRadius;

std::vector<DrawSurf>	drawSurfs;
int						tessOverflows;

static void SetCvar( cvar_t *cv, const char *name, float value ) {
	cv->name = name;
	cv->value = value;
	cv->integer = (int)value;
}

} // namespace g2t

cvar_t	*r_Ghoul2AnimSmooth;
cvar_t	*r_Ghoul2UnSqashAfterSmooth;
cvar_t	*r_noServerGhoul2 = &g2t::noServerGhoul2;
cvar_t	*r_convertModelBones = &g2t::convertModelBones;
cvar_t	*r_verbose = &g2t::verbose;
cvar_t	*r_lodbias = &g2t::lodBias;
cvar_t	*r_lodscale = &g2t::lodScale;
cvar_t	*r_autolodscalevalue = &g2t::autoLodScale;
cvar_t	*r_shadows = &g2t::shadows;

//
// common
//

static bool HarnessVerbose( void ) {
	static int verbose = -1;
	if ( verbose < 0 ) {
		const char *v = getenv( "G2_HARNESS_VERBOSE" );
		verbose = ( v && v[0] && v[0] != '0' ) ? 1 : 0;
	}
	return verbose != 0;
}

void QDECL Com_Printf( const char *fmt, ... ) {
	if ( !HarnessVerbose() ) {
		return;
	}
	va_list ap;
	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
}

// The engine longjmps out of a Com_Error; the harness throws, so a test
// fails with the message instead of the process dying
Q_NORETURN void QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	char msg[1024];
	va_list ap;
	va_start( ap, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, ap );
	va_end( ap );
	throw std::runtime_error( std::string( "Com_Error: " ) + msg );
}

static void QDECL HarnessRendererPrintf( int printLevel, const char *fmt, ... ) {
	if ( !HarnessVerbose() ) {
		return;
	}
	va_list ap;
	va_start( ap, fmt );
	vprintf( fmt, ap );
	va_end( ap );
}

static Q_NORETURN void QDECL HarnessRendererError( errorParm_t code, const char *fmt, ... ) {
	char msg[1024];
	va_list ap;
	va_start( ap, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, ap );
	va_end( ap );
	throw std::runtime_error( std::string( "ri.Error: " ) + msg );
}

// The zone allocator, as plain heap memory
void *Z_Malloc( int iSize, memtag_t eTag, qboolean bZeroit ) {
	void *p = malloc( iSize > 0 ? iSize : 1 );
	if ( !p ) {
		Com_Error( ERR_FATAL, "Z_Malloc: out of memory (%d bytes)", iSize );
	}
	if ( bZeroit ) {
		memset( p, 0, iSize );
	}
	return p;
}

void Z_Free( void *ptr ) {
	free( ptr );
}

//
// renderer front end
//

int R_CullLocalPointAndRadius( vec3_t origin, float radius ) {
	return CULL_IN;
}

float ProjectRadius( float r, vec3_t location ) {
	return g2t::projectedRadius;
}

void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) {
}

void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, int fogIndex, int dlightMap ) {
	g2t::DrawSurf ds;
	ds.surf = (CRenderableSurface *)surface;
	ds.shader = shader;
	ds.fogIndex = fogIndex;
	g2t::drawSurfs.push_back( ds );
}

void RB_CheckOverflow( int verts, int indexes ) {
	// the back end flushes the batch here; the tests size their batches so
	// that this never happens, and count it when it does
	g2t::tessOverflows++;
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}

//
// shaders and skins
//

namespace g2t {

static std::vector<shader_t *>	shaders;	// handle = index; 0 is the default shader
static std::vector<skin_t *>	skins;		// handle = index; 0 is no skin

static shader_t *NewShader( const char *name ) {
	shader_t *sh = (shader_t *)calloc( 1, sizeof( shader_t ) );
	Q_strncpyz( sh->name, name, sizeof( sh->name ) );
	sh->index = (int)shaders.size();
	sh->sortedIndex = sh->index;
	sh->sort = SS_OPAQUE;
	shaders.push_back( sh );
	return sh;
}

static shader_t *FindShader( const char *name ) {
	for ( size_t i = 1; i < shaders.size(); i++ ) {
		if ( !Q_stricmp( shaders[i]->name, name ) ) {
			return shaders[i];
		}
	}
	return NewShader( name );
}

qhandle_t ShaderHandle( const char *name ) {
	return FindShader( name )->index;
}

qhandle_t RegisterSkin( const char *name, const std::vector<std::string> &surfaceShaderPairs ) {
	skin_t *skin = (skin_t *)calloc( 1, sizeof( skin_t ) );
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	for ( size_t i = 0; i + 1 < surfaceShaderPairs.size() && skin->numSurfaces < 256; i += 2 ) {
		skinSurface_t *surf = (skinSurface_t *)calloc( 1, sizeof( skinSurface_t ) );
		Q_strncpyz( surf->name, surfaceShaderPairs[i].c_str(), sizeof( surf->name ) );
		Q_strlwr( surf->name );	// skin surface names are lowercased, see RenderSurfaces
		surf->shader = FindShader( surfaceShaderPairs[i + 1].c_str() );
		skin->surfaces[skin->numSurfaces++] = surf;
	}
	skins.push_back( skin );
	tr.numSkins = (int)skins.size();
	return (qhandle_t)skins.size() - 1;
}

} // namespace g2t

shader_t *R_FindShader( const char *name, const int *lightmapIndex, const byte *styles, qboolean mipRawImage, qboolean isAdvancedRemap ) {
	return g2t::FindShader( name );
}

shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	if ( hShader < 0 || hShader >= (int)g2t::shaders.size() ) {
		return g2t::shaders[0];
	}
	return g2t::shaders[hShader];
}

skin_t *R_GetSkinByHandle( qhandle_t hSkin ) {
	if ( hSkin < 1 || hSkin >= (int)g2t::skins.size() ) {
		return g2t::skins[0];
	}
	return g2t::skins[hSkin];
}

//
// models: the contract of tr_model.cpp (one model_t per name, handle 0 and
// tr.models[0] for a model that failed), loaded by the real R_LoadMDXM and
// R_LoadMDXA from the harness file system
//

namespace g2t {

static std::map<std::string, Bytes>		files;
static std::map<std::string, qhandle_t>	modelHandles;
static std::vector<void *>				modelMemory;	// model_t and model data

static std::string Lower( const char *s ) {
	std::string out( s );
	for ( size_t i = 0; i < out.size(); i++ ) {
		out[i] = (char)tolower( (unsigned char)out[i] );
		if ( out[i] == '\\' ) {
			out[i] = '/';
		}
	}
	return out;
}

void AddFile( const std::string &name, const Bytes &data ) {
	files[Lower( name.c_str() )] = data;
}

const Bytes *FindFile( const std::string &name ) {
	std::map<std::string, Bytes>::const_iterator it = files.find( Lower( name.c_str() ) );
	return it == files.end() ? NULL : &it->second;
}

static model_t *AllocModel( void ) {
	if ( tr.numModels >= MAX_MOD_KNOWN ) {
		Com_Error( ERR_DROP, "AllocModel: MAX_MOD_KNOWN" );
	}
	model_t *mod = (model_t *)calloc( 1, sizeof( model_t ) );
	mod->index = tr.numModels;
	tr.models[tr.numModels++] = mod;
	modelMemory.push_back( mod );
	return mod;
}

void ResetModels( void ) {
	FixGhoul2InfoLeaks( false );
	FixGhoul2InfoLeaks( true );
	for ( size_t i = 0; i < modelMemory.size(); i++ ) {
		free( modelMemory[i] );
	}
	modelMemory.clear();
	modelHandles.clear();
	tr.numModels = 0;
	model_t *bad = AllocModel();	// the default model, type MOD_BAD
	Q_strncpyz( bad->name, "** BAD MODEL **", sizeof( bad->name ) );
}

model_t *ModelByName( const char *name ) {
	return R_GetModelByHandle( RE_RegisterModel( name ) );
}

} // namespace g2t

model_t *R_GetModelByHandle( qhandle_t index ) {
	// out of range gets the default model
	if ( index < 1 || index >= tr.numModels ) {
		return tr.models[0];
	}
	return tr.models[index];
}

qhandle_t RE_RegisterModel( const char *name ) {
	if ( !name || !name[0] || strlen( name ) >= MAX_QPATH ) {
		return 0;
	}

	const std::string key = g2t::Lower( name );
	std::map<std::string, qhandle_t>::const_iterator it = g2t::modelHandles.find( key );
	if ( it != g2t::modelHandles.end() ) {
		return it->second;
	}

	model_t *mod = g2t::AllocModel();
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );
	mod->numLods = 0;

	qboolean loaded = qfalse;
	const g2t::Bytes *file = g2t::FindFile( key );
	if ( file && file->size() >= 8 ) {
		// a private copy, as FS_ReadFile gives: the loaders copy what they keep
		g2t::Bytes buffer( *file );
		int ident;
		memcpy( &ident, &buffer[0], sizeof( ident ) );
		ident = LittleLong( ident );
		if ( ident == MDXA_IDENT ) {
			loaded = R_LoadMDXA( mod, &buffer[0], name, qfalse );
		} else if ( ident == MDXM_IDENT ) {
			loaded = R_LoadMDXM( mod, &buffer[0], name, qfalse );
		}
	}

	if ( loaded ) {
		mod->numLods++;	// as RE_RegisterModel_Actual does after a load
		g2t::modelHandles[key] = mod->index;
		return mod->index;
	}

	// kept, so asking again does not search again, but handle 0
	mod->type = MOD_BAD;
	mod->index = 0;
	g2t::modelHandles[key] = 0;
	return 0;
}

qhandle_t RE_RegisterServerModel( const char *name ) {
	return RE_RegisterModel( name );
}

void *RE_RegisterModels_Malloc( int iSize, const char *psModelFileName, qboolean *pqbAlreadyFound, memtag_t eTag ) {
	void *p = calloc( 1, iSize );
	g2t::modelMemory.push_back( p );
	*pqbAlreadyFound = qfalse;
	return p;
}

void RE_RegisterModels_StoreShaderRequest( const char *psModelFileName, const char *psShaderName, int *piShaderIndexPoke ) {
}

//
// harness state
//

namespace g2t {

// aligned like tess.xyz, for the SSE2 kernel's aligned stores
struct skinBuffers_t {
	alignas(16) vec4_t	xyz[kMaxSkinVerts];
	alignas(16) vec4_t	normal[kMaxSkinVerts];
};
static skinBuffers_t skinBuffers;
vec4_t *skinXyz = skinBuffers.xyz;
vec4_t *skinNormal = skinBuffers.normal;

void SetEngineCvars( bool client ) {
	SetCvar( &animSmooth, "r_ghoul2animsmooth", 0.3f );
	SetCvar( &unsquash, "r_ghoul2unsqashaftersmooth", 1.0f );
	r_Ghoul2AnimSmooth = client ? &animSmooth : NULL;
	r_Ghoul2UnSqashAfterSmooth = client ? &unsquash : NULL;

	SetCvar( &shadows, "cg_shadows", 1.0f );
	SetCvar( &lodBias, "r_lodbias", 0.0f );
	SetCvar( &lodScale, "r_lodscale", 5.0f );
	SetCvar( &autoLodScale, "r_autolodscalevalue", 0.0f );
	SetCvar( &noServerGhoul2, "r_noserverghoul2", 0.0f );
	SetCvar( &convertModelBones, "r_convertModelBones", 1.0f );
	SetCvar( &verbose, "r_verbose", 0.0f );
}

void ClearDrawSurfs( void ) {
	drawSurfs.clear();
}

static void ResetShadersAndSkins( void ) {
	for ( size_t i = 0; i < shaders.size(); i++ ) {
		free( shaders[i] );
	}
	shaders.clear();
	for ( size_t i = 0; i < skins.size(); i++ ) {
		for ( int s = 0; s < skins[i]->numSurfaces; s++ ) {
			free( skins[i]->surfaces[s] );
		}
		free( skins[i] );
	}
	skins.clear();

	NewShader( "<default>" )->defaultShader = qtrue;
	tr.defaultShader = shaders[0];
	skins.push_back( (skin_t *)calloc( 1, sizeof( skin_t ) ) );	// handle 0: no skin
	tr.numSkins = 1;
}

void Init( void ) {
	memset( &ri, 0, sizeof( ri ) );
	ri.Printf = HarnessRendererPrintf;
	ri.Error = HarnessRendererError;

	// models first: they point at the shaders
	ResetModels();
	ResetShadersAndSkins();
	SetEngineCvars( false );
	projectedRadius = 0.0f;
	ClearDrawSurfs();
	tessOverflows = 0;
	tess.numVertexes = 0;
	tess.numIndexes = 0;
	memset( &tr.refdef, 0, sizeof( tr.refdef ) );
	tr.refdef.rdflags = RDF_NOWORLDMODEL;	// no fog volumes to look up
}

//
// retail files
//

static std::string programPath;

void SetProgramPath( const char *argv0 ) {
	programPath = argv0 ? argv0 : "";
}

const std::string &ProgramPath( void ) {
	return programPath;
}

static bool FileExists( const std::string &path ) {
	FILE *f = fopen( path.c_str(), "rb" );
	if ( !f ) {
		return false;
	}
	fclose( f );
	return true;
}

std::string FindRetailBase( const char *argv0 ) {
	const char *env = getenv( "JK2MV_TEST_BASE" );
	if ( env && env[0] ) {
		std::string base( env );
		if ( FileExists( base + "/assets0.pk3" ) ) {
			return base;
		}
		if ( FileExists( base + "/base/assets0.pk3" ) ) {
			return base + "/base";
		}
		return std::string();
	}

	// the portable layout: base\ next to the executables
	std::string dir( argv0 ? argv0 : "" );
	const size_t slash = dir.find_last_of( "/\\" );
	dir = ( slash == std::string::npos ) ? std::string( "." ) : dir.substr( 0, slash );
	if ( FileExists( dir + "/base/assets0.pk3" ) ) {
		return dir + "/base";
	}
	return std::string();
}

bool LoadFromPk3( const std::string &pk3, const std::vector<std::string> &names ) {
	unzFile zf = unzOpen( pk3.c_str() );
	if ( !zf ) {
		return false;
	}

	bool ok = true;
	for ( size_t i = 0; ok && i < names.size(); i++ ) {
		unz_file_info info;
		ok = unzLocateFile( zf, names[i].c_str(), 2 ) == UNZ_OK	// 2: not case sensitive
			&& unzGetCurrentFileInfo( zf, &info, NULL, 0, NULL, 0, NULL, 0 ) == UNZ_OK
			&& info.uncompressed_size > 0
			&& unzOpenCurrentFile( zf ) == UNZ_OK;
		if ( !ok ) {
			break;
		}
		Bytes data( info.uncompressed_size );
		const int read = unzReadCurrentFile( zf, &data[0], (unsigned)data.size() );
		ok = ( unzCloseCurrentFile( zf ) == UNZ_OK ) && read == (int)data.size();
		if ( ok ) {
			AddFile( names[i], data );
		}
	}

	unzClose( zf );
	return ok;
}

//
// digests
//

Digest::Digest( const std::string &digestName ) : name( digestName ), hash( 14695981039346656037ULL ), dump( NULL ) {
	const char *dir = getenv( "G2_DUMP_DIR" );
	if ( dir && dir[0] ) {
		std::string file( name );
		for ( size_t i = 0; i < file.size(); i++ ) {
			if ( file[i] == '/' || file[i] == '\\' || file[i] == ':' ) {
				file[i] = '_';
			}
		}
		dump = fopen( ( std::string( dir ) + "/" + file + ".txt" ).c_str(), "w" );
	}
}

Digest::~Digest() {
	if ( dump ) {
		fprintf( dump, "digest %016llx\n", (unsigned long long)hash );
		fclose( dump );
	}
}

void Digest::Word( uint32_t w ) {
	for ( int i = 0; i < 4; i++ ) {
		hash ^= ( w >> ( i * 8 ) ) & 0xff;
		hash *= 1099511628211ULL;
	}
}

void Digest::Int( int v ) {
	Word( (uint32_t)v );
	if ( dump ) {
		fprintf( dump, "i %d\n", v );
	}
}

void Digest::Float( float f ) {
	uint32_t bits;
	memcpy( &bits, &f, sizeof( bits ) );
	Word( bits );
	if ( dump ) {
		fprintf( dump, "f %08x %.9g\n", bits, f );
	}
}

void Digest::Floats( const float *f, int count ) {
	for ( int i = 0; i < count; i++ ) {
		Float( f[i] );
	}
}

void Digest::Matrix( const mdxaBone_t &m ) {
	Floats( &m.matrix[0][0], 12 );
}

void Digest::String( const char *text ) {
	const int len = (int)strlen( text );
	Int( len );
	for ( int i = 0; i < len; i++ ) {
		Word( (unsigned char)text[i] );
	}
	if ( dump ) {
		fprintf( dump, "s %s\n", text );
	}
}

void Digest::Label( const char *text ) {
	if ( dump ) {
		fprintf( dump, "# %s\n", text );
	}
}

void HashBones( Digest &d, CGhoul2Info_v &ghoul2 ) {
	for ( size_t i = 0; i < ghoul2.size(); i++ ) {
		const CGhoul2Info &g = ghoul2[i];
		char label[64];
		Com_sprintf( label, sizeof( label ), "model %d bones", (int)i );
		d.Label( label );
		d.Int( g.mModelindex );
		d.Int( (int)g.mTempBoneList.size() );
		for ( size_t b = 0; b < g.mTempBoneList.size(); b++ ) {
			d.Int( g.mTempBoneList[b].first );
			d.Matrix( g.mTempBoneList[b].second );
		}
	}
}

void HashBolts( Digest &d, CGhoul2Info_v &ghoul2 ) {
	for ( size_t i = 0; i < ghoul2.size(); i++ ) {
		const CGhoul2Info &g = ghoul2[i];
		char label[64];
		Com_sprintf( label, sizeof( label ), "model %d bolts", (int)i );
		d.Label( label );
		d.Int( (int)g.mBltlist.size() );
		for ( size_t b = 0; b < g.mBltlist.size(); b++ ) {
			d.Int( g.mBltlist[b].boneNumber );
			d.Int( g.mBltlist[b].surfaceNumber );
			d.Int( g.mBltlist[b].surfaceType );
			d.Matrix( g.mBltlist[b].position );
		}
	}
}


//
// scenes
//

const char *const kKyleGLM = "models/players/kyle/model.glm";
const char *const kHumanoidGLA = "models/players/_humanoid/_humanoid.gla";

mdxaBone_t RotationMatrix( double w, double x, double y, double z, float tx, float ty, float tz ) {
	const double n = sqrt( w * w + x * x + y * y + z * z );
	w /= n;
	x /= n;
	y /= n;
	z /= n;
	mdxaBone_t m;
	m.matrix[0][0] = (float)( 1.0 - 2.0 * ( y * y + z * z ) );
	m.matrix[0][1] = (float)( 2.0 * ( x * y - w * z ) );
	m.matrix[0][2] = (float)( 2.0 * ( x * z + w * y ) );
	m.matrix[1][0] = (float)( 2.0 * ( x * y + w * z ) );
	m.matrix[1][1] = (float)( 1.0 - 2.0 * ( x * x + z * z ) );
	m.matrix[1][2] = (float)( 2.0 * ( y * z - w * x ) );
	m.matrix[2][0] = (float)( 2.0 * ( x * z - w * y ) );
	m.matrix[2][1] = (float)( 2.0 * ( y * z + w * x ) );
	m.matrix[2][2] = (float)( 1.0 - 2.0 * ( x * x + y * y ) );
	m.matrix[0][3] = tx;
	m.matrix[1][3] = ty;
	m.matrix[2][3] = tz;
	return m;
}

static const vec3_t noAngles = { 0, 0, 0 };

bool CreateCharacter( g2handle_t *handle ) {
	return G2API_InitGhoul2Model( handle, kCharGLM, 1 ) == 0;
}

bool AnimateCharacter( g2handle_t handle, int t0 ) {
	CGhoul2Info_v *ghoul2 = G2API_GetGhoul2Model( handle );
	if ( !ghoul2 || ghoul2->empty() ) {
		return false;
	}
	CGhoul2Info *g = &( *ghoul2 )[0];
	return G2API_SetBoneAnim( handle, 0, "model_root", 2, 40, BONE_ANIM_OVERRIDE_LOOP, 1.0f, t0, -1, -1 )
		&& G2API_SetBoneAnim( handle, 0, "lower_lumbar", 10, 30, BONE_ANIM_OVERRIDE_LOOP, 0.8f, t0, -1, -1 )
		&& G2API_SetBoneAnim( handle, 0, "lower_lumbar", 30, 46, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1.2f, t0 + 100, 33.5f, 150 )
		&& G2API_SetBoneAnglesMatrix( g, "upper_lumbar", RotationMatrix( 0.96, 0.10, -0.20, 0.15 ), BONE_ANGLES_POSTMULT, NULL, 0, t0 )
		// angles 0: sinf and cosf are exact, the matrix does not depend on the libm
		&& G2API_SetBoneAngles( handle, 0, "cranium", noAngles, BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, POSITIVE_X, NULL, 0, t0 )
		&& G2API_SetBoneAnglesMatrix( g, "rhumerus", RotationMatrix( 0.90, 0.30, 0.10, -0.20, 0.5f, -0.25f, 1.0f ), BONE_ANGLES_REPLACE, NULL, 0, t0 )
		&& G2API_SetBoneAnglesMatrix( g, "model_root", RotationMatrix( 0.99, 0.0, 0.0, 0.12 ), BONE_ANGLES_PREMULT, NULL, 0, t0 );
}

bool AddCharacterBolts( g2handle_t handle, Bolts *bolts, bool generatedSurface ) {
	bolts->rHand = G2API_AddBolt( handle, 0, "*r_hand" );
	bolts->lHand = G2API_AddBolt( handle, 0, "*l_hand" );
	bolts->tagBone = G2API_AddBolt( handle, 0, "rhang_tag_bone" );
	bolts->motion = G2API_AddBolt( handle, 0, "Motion" );
	bolts->lumbar = G2API_AddBolt( handle, 0, "lower_lumbar" );
	bolts->face = G2API_AddBolt( handle, 0, "face" );

	if ( generatedSurface ) {
		// a point on triangle 5 of the torso
		CGhoul2Info_v *ghoul2 = G2API_GetGhoul2Model( handle );
		const int generated = ghoul2 ? G2API_AddSurface( &( *ghoul2 )[0], 1, 5, 0.25f, 0.5f, 0 ) : -1;
		bolts->generated = generated >= 0 ? G2API_AddBoltSurfNum( &( *ghoul2 )[0], generated ) : -1;
		if ( bolts->generated < 0 ) {
			return false;
		}
	}

	return bolts->rHand >= 0 && bolts->lHand >= 0 && bolts->tagBone >= 0 && bolts->motion >= 0
		&& bolts->lumbar >= 0 && bolts->face >= 0;
}

bool AttachProp( g2handle_t handle, int t0, Bolts *bolts ) {
	if ( G2API_InitGhoul2Model( &handle, kPropGLM, 2 ) != 1
		|| !G2API_AttachG2Model( handle, 1, handle, bolts->rHand, 0 )
		|| !G2API_SetBoneAnim( handle, 1, "prop_root", 0, 7, BONE_ANIM_OVERRIDE_LOOP, 1.5f, t0, -1, -1 ) ) {
		return false;
	}
	bolts->flash = G2API_AddBolt( handle, 1, "*flash" );
	return bolts->flash >= 0;
}

bool SetupCharacter( g2handle_t *handle, int t0, Bolts *bolts ) {
	return CreateCharacter( handle )
		&& AnimateCharacter( *handle, t0 )
		&& AddCharacterBolts( *handle, bolts, true )
		&& AttachProp( *handle, t0, bolts );
}

bool SetupKyle( g2handle_t *handle, int t0, Bolts *bolts ) {
	if ( G2API_InitGhoul2Model( handle, kKyleGLM, 1 ) != 0 ) {
		return false;
	}
	model_t *gla = R_GetModelByHandle( ModelByName( kKyleGLM )->mdxm->animIndex );
	if ( !gla->mdxa || gla->mdxa->numFrames < 6200 ) {
		return false;
	}
	CGhoul2Info *g = &( *G2API_GetGhoul2Model( *handle ) )[0];
	bolts->rHand = G2API_AddBolt( *handle, 0, "*r_hand" );
	bolts->lHand = G2API_AddBolt( *handle, 0, "*l_hand" );
	bolts->tagBone = G2API_AddBolt( *handle, 0, "rhang_tag_bone" );
	bolts->motion = G2API_AddBolt( *handle, 0, "Motion" );
	bolts->lumbar = G2API_AddBolt( *handle, 0, "lower_lumbar" );
	bolts->face = G2API_AddBolt( *handle, 0, "face" );
	bolts->headTop = G2API_AddBolt( *handle, 0, "*head_top" );
	return bolts->rHand >= 0 && bolts->lHand >= 0 && bolts->tagBone >= 0 && bolts->motion >= 0
		&& bolts->lumbar >= 0 && bolts->face >= 0 && bolts->headTop >= 0
		&& G2API_SetBoneAnim( *handle, 0, "model_root", 5000, 5040, BONE_ANIM_OVERRIDE_LOOP, 1.0f, t0, -1, -1 )
		&& G2API_SetBoneAnim( *handle, 0, "Motion", 5000, 5040, BONE_ANIM_OVERRIDE_LOOP, 1.0f, t0, -1, -1 )
		&& G2API_SetBoneAnim( *handle, 0, "lower_lumbar", 6000, 6030, BONE_ANIM_OVERRIDE_LOOP, 1.0f, t0, -1, -1 )
		&& G2API_SetBoneAnim( *handle, 0, "lower_lumbar", 6100, 6150, BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND, 1.25f, t0 + 120, 6105.5f, 150 )
		&& G2API_SetBoneAnglesMatrix( g, "upper_lumbar", RotationMatrix( 0.97, 0.05, -0.15, 0.12 ), BONE_ANGLES_POSTMULT, NULL, 0, t0 )
		&& G2API_SetBoneAngles( *handle, 0, "cranium", noAngles, BONE_ANGLES_POSTMULT, POSITIVE_Z, NEGATIVE_Y, POSITIVE_X, NULL, 0, t0 );
}

} // namespace g2t
