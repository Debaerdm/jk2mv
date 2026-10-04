// ghoul2_harness.h - runs the engine's Ghoul2 code without a renderer
//
// The reference tests and benchmarks of FE-16 link the real Ghoul2 sources
// (renderer/tr_ghoul2.cpp, renderer/matcomp.c, ghoul2/*.cpp, qcommon/q_math.cpp
// and q_shared.cpp), compiled with the engine's own definitions. This harness
// provides the little they need from the rest of the engine (cvars, an in
// memory model registry, the draw surface list, tess), builds synthetic
// GLA/GLM files, reads the retail model from a pk3 when there is one, and
// hashes results bit for bit.
//
// Nothing here may depend on the platform's libm beyond sqrt (correctly
// rounded everywhere): the synthetic models must come out byte-identical on
// every platform, or their reference hashes would only hold on the machine
// that recorded them. For the same reason the scenes give the engine's
// trigonometry angles of 0 only (sinf and cosf of 0 are exact), except for
// the fixed 270 degree turn of G2API_GetBoltMatrix, whose output no test
// hashes.

#ifndef GHOUL2_HARNESS_H
#define GHOUL2_HARNESS_H

#include "renderer/tr_local.h"
#include "ghoul2/G2.h"
#include "ghoul2/G2_local.h"
#include "qcommon/MiniHeap.h"

#include <stdint.h>
#include <string>
#include <vector>

// From tr_ghoul2.cpp and G2_API.cpp (not in a header)
void G2_TransformGhoulBones( mdxaHeader_t *header, int *usedBoneList, boneInfo_v &rootBoneList, mdxaBone_v &bonePtr, boltInfo_v &boltList,
							mdxaBone_t &rootMatrix, CGhoul2Info &ghoul2, int time, int boneCount );
void UnCompressBone( float mat[3][4], int iBoneIndex, const mdxaHeader_t *pMDXAHeader, int iFrame );
void FixGhoul2InfoLeaks( bool ricksCrazyOnServer );

namespace g2t {

typedef std::vector<unsigned char> Bytes;

//
// engine state
//

// Resets the harness: no model, no Ghoul2 instance, only the default shader
// and skin, engine cvars of a server. Call it before each test.
void Init( void );

// The Ghoul2 cvars at their engine defaults. client: the renderer registered
// r_ghoul2animsmooth (.3) and r_ghoul2unsqashaftersmooth (1), so bone
// smoothing runs, as on a client or listen server; otherwise these cvars do
// not exist, as on a dedicated server.
void SetEngineCvars( bool client );
extern cvar_t	animSmooth;		// r_ghoul2animsmooth
extern cvar_t	unsquash;		// r_ghoul2unsqashaftersmooth
extern cvar_t	shadows;		// cg_shadows
extern cvar_t	lodBias;		// r_lodbias

// ProjectRadius() returns this, which drives the LOD choice of
// R_AddGhoulSurfaces (0: the entity touches the near plane, LOD 0)
extern float	projectedRadius;

// The in-memory file system RE_RegisterModel reads
void AddFile( const std::string &name, const Bytes &data );
const Bytes *FindFile( const std::string &name );

// Forgets every registered model and Ghoul2 instance (not the files)
void ResetModels( void );

// The shader handle registered for a name (surface shaders, skins)
qhandle_t ShaderHandle( const char *name );
// Registers a skin: pairs of surface name, shader name
qhandle_t RegisterSkin( const char *name, const std::vector<std::string> &surfaceShaderPairs );

// Draw surfaces queued by R_AddDrawSurf since the last ClearDrawSurfs
struct DrawSurf {
	CRenderableSurface	*surf;
	shader_t			*shader;
	int					fogIndex;
};
extern std::vector<DrawSurf> drawSurfs;
void ClearDrawSurfs( void );

// Calls of RB_CheckOverflow (a surface that did not fit in tess)
extern int tessOverflows;

//
// synthetic models
//

// The synthetic character: a 26-bone humanoid skeleton (with always
// transformed bones and bones no surface uses), 48 compressed frames, and a
// mesh of 12 surfaces in 3 LODs: off surfaces, two bolt tag surfaces (the
// right hand one is empty in LOD 2, like in some JKA models), 1 to 4 bone
// weights per vertex, and a surface that references 22 bones
extern const char *const kCharGLA;		// "models/test/g2char.gla"
extern const char *const kCharGLM;		// "models/test/g2char.glm"
// A small prop (sword-like) to bolt onto the character: 3 bones, 2 LODs,
// 2 surfaces and a bolt tag surface
extern const char *const kPropGLA;
extern const char *const kPropGLM;
enum { kCharBones = 26, kCharFrames = 48, kCharSurfaces = 12, kCharLods = 3 };

Bytes BuildCharGLA( void );
Bytes BuildCharGLM( void );
Bytes BuildPropGLA( void );
Bytes BuildPropGLM( void );
// Puts the four synthetic files in the file system
void AddSyntheticModels( void );

//
// retail assets (optional)
//

// The folder that holds the retail assets0.pk3: $JK2MV_TEST_BASE, or the base
// folder next to the test executable. Empty when there is none.
std::string FindRetailBase( const char *argv0 );
// Reads files out of a pk3 into the file system. False if one is missing.
bool LoadFromPk3( const std::string &pk3, const std::vector<std::string> &names );
// Records argv[0] for FindRetailBase (the tests' main does not see it)
void SetProgramPath( const char *argv0 );
const std::string &ProgramPath( void );

//
// bit-exact digests
//

// FNV-1a 64 over the bit patterns of the values. With $G2_DUMP_DIR set, the
// values are also written to <dir>/<name>.txt, to diff a run before and after
// a change and find the first value that moved.
class Digest {
public:
	explicit Digest( const std::string &name );
	~Digest();
	void		Int( int v );
	void		Float( float f );
	void		Floats( const float *f, int count );
	void		Matrix( const mdxaBone_t &m );
	void		String( const char *text );
	void		Label( const char *text );	// only in the dump
	uint64_t	Value( void ) const { return hash; }
	const std::string &Name( void ) const { return name; }
private:
	void		Word( uint32_t w );
	std::string	name;
	uint64_t	hash;
	FILE		*dump;
};

// Bones of every model of an instance (all mdxaBone_v entries, stale ones
// included, with their smoothing time stamps), then the bolts
void HashBones( Digest &d, CGhoul2Info_v &ghoul2 );
void HashBolts( Digest &d, CGhoul2Info_v &ghoul2 );

//
// scenes: Ghoul2 instances driven through the API the game and cgame use
//

// A rotation (and translation) from a quaternion normalized in double
// precision: the same floats everywhere, no sin or cos
mdxaBone_t RotationMatrix( double w, double x, double y, double z, float tx = 0.0f, float ty = 0.0f, float tz = 0.0f );

struct Bolts {
	int		rHand, lHand;	// tag surfaces
	int		tagBone;		// rhang_tag_bone, always transformed
	int		motion;			// Motion, no surface uses it
	int		lumbar;			// lower_lumbar
	int		face;			// face (never transformed in the synthetic skeleton)
	int		headTop;		// *head_top, a tag surface of Kyle
	int		generated;		// a point on a generated surface
	int		flash;			// on the attached model
	Bolts() : rHand( -1 ), lHand( -1 ), tagBone( -1 ), motion( -1 ), lumbar( -1 ), face( -1 ), headTop( -1 ), generated( -1 ), flash( -1 ) {}
};

// Each returns false when a G2API call fails
// the synthetic character as model 0 of a new instance
bool CreateCharacter( g2handle_t *handle );
// what cg_players.c does: whole body and torso animations, the torso
// blending into a new one 100 ms later, torso aim and head look (POSTMULT),
// plus an arm replaced and a PREMULT on the root
bool AnimateCharacter( g2handle_t handle, int t0 );
// bolts on tag surfaces, on bones, and on a generated surface (optional)
bool AddCharacterBolts( g2handle_t handle, Bolts *bolts, bool generatedSurface );
// the prop as model 1, bolted to the right hand tag, with its own animation
bool AttachProp( g2handle_t handle, int t0, Bolts *bolts );
// all of the above
bool SetupCharacter( g2handle_t *handle, int t0, Bolts *bolts );

// Kyle (models/players/kyle/model.glm and _humanoid.gla, read from the retail
// assets0.pk3), animated and bolted like a player
extern const char *const kKyleGLM;
extern const char *const kHumanoidGLA;
bool SetupKyle( g2handle_t *handle, int t0, Bolts *bolts );

// 16-byte aligned scratch vertexes for the skinning kernels
enum { kMaxSkinVerts = SHADER_MAX_VERTEXES };
extern vec4_t *skinXyz;
extern vec4_t *skinNormal;

// The model of a registered name (handle lookup through RE_RegisterModel)
model_t *ModelByName( const char *name );

} // namespace g2t

#endif // GHOUL2_HARNESS_H
