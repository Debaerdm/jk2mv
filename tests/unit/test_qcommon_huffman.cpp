// test_qcommon_huffman.cpp - table-driven netchan Huffman codes
//
// Links the real engine sources (src/qcommon/huffman.cpp and msg.cpp) and
// checks them against a copy of the tree-walking code they replaced: the
// netchan bits must stay the same for every input, in both directions, at
// every bit offset. The benchmarks at the end print the speedups.

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

// ==============================================================================
// Engine stubs for msg.cpp (no huffman.dat: the trees are built from msg_hData)
// ==============================================================================

// --- engine stubs begin
static cvar_t stubCvar;			// integer 0: no debug output
cvar_t *com_debugMessage = &stubCvar;
cvar_t *cl_shownet = &stubCvar;
static mvversion_t stubGameVersion = VERSION_1_04;

void QDECL Com_Printf( const char *fmt, ... ) {
}

void QDECL Com_DPrintf( const char *fmt, ... ) {
}

void QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	char text[1024];
	va_list ap;

	va_start( ap, fmt );
	vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	throw std::runtime_error( text );
}

void Q_strncpyz( char *dest, const char *src, int destsize ) {
	strncpy( dest, src, destsize - 1 );
	dest[destsize - 1] = 0;
}

fileHandle_t FS_SV_FOpenFileWrite( const char *filename, module_t module ) {
	return 0;
}

int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp, module_t module ) {
	*fp = 0;
	return -1;
}

int FS_Write( const void *buffer, int len, fileHandle_t f, module_t module ) {
	return 0;
}

int FS_Read( void *buffer, int len, fileHandle_t f, module_t module ) {
	return 0;
}

void FS_FCloseFile( fileHandle_t f, module_t module ) {
}

unsigned Com_BlockChecksum( const void *buffer, int length ) {
	return 0;
}

mvversion_t MV_GetCurrentGameversion() {
	return stubGameVersion;
}
// --- engine stubs end

// ==============================================================================
// The original implementation (src/qcommon/huffman.cpp before the tables),
// kept verbatim as the reference. It is compiled under other names: its calls
// would also find the engine functions of the same signature (argument
// dependent lookup on huff_t and node_t).
// ==============================================================================

namespace orig {

#define Huff_putBit Old_Huff_putBit
#define Huff_getBit Old_Huff_getBit
#define Huff_addRef Old_Huff_addRef
#define Huff_offsetReceive Old_Huff_offsetReceive
#define Huff_offsetTransmit Old_Huff_offsetTransmit
#define Huff_Decompress Old_Huff_Decompress
#define Huff_Compress Old_Huff_Compress
#define Huff_Init Old_Huff_Init


static int			bloc = 0;

void	Huff_putBit( int bit, byte *fout, int *offset) {
	bloc = *offset;
	if ((bloc&7) == 0) {
		fout[(bloc>>3)] = 0;
	}
	fout[(bloc>>3)] |= bit << (bloc&7);
	bloc++;
	*offset = bloc;
}

int		Huff_getBit( byte *fin, int *offset) {
	int t;
	bloc = *offset;
	t = (fin[(bloc>>3)] >> (bloc&7)) & 0x1;
	bloc++;
	*offset = bloc;
	return t;
}

/* Add a bit to the output file (buffered) */
static void add_bit (char bit, byte *fout) {
	if ((bloc&7) == 0) {
		fout[(bloc>>3)] = 0;
	}
	fout[(bloc>>3)] |= bit << (bloc&7);
	bloc++;
}

/* Receive one bit from the input file (buffered) */
static int get_bit (byte *fin) {
	int t;
	t = (fin[(bloc>>3)] >> (bloc&7)) & 0x1;
	bloc++;
	return t;
}

static node_t **get_ppnode(huff_t* huff) {
	node_t **tppnode;
	if (!huff->freelist) {
		return &(huff->nodePtrs[huff->blocPtrs++]);
	} else {
		tppnode = huff->freelist;
		huff->freelist = (node_t **)*tppnode;
		return tppnode;
	}
}

static void free_ppnode(huff_t* huff, node_t **ppnode) {
	*ppnode = (node_t *)huff->freelist;
	huff->freelist = ppnode;
}

/* Swap the location of these two nodes in the tree */
static void swap (huff_t* huff, node_t *node1, node_t *node2) {
	node_t *par1, *par2;

	par1 = node1->parent;
	par2 = node2->parent;

	if (par1) {
		if (par1->left == node1) {
			par1->left = node2;
		} else {
	      par1->right = node2;
		}
	} else {
		huff->tree = node2;
	}

	if (par2) {
		if (par2->left == node2) {
			par2->left = node1;
		} else {
			par2->right = node1;
		}
	} else {
		huff->tree = node1;
	}

	node1->parent = par2;
	node2->parent = par1;
}

/* Swap these two nodes in the linked list (update ranks) */
static void swaplist(node_t *node1, node_t *node2) {
	node_t *par1;

	par1 = node1->next;
	node1->next = node2->next;
	node2->next = par1;

	par1 = node1->prev;
	node1->prev = node2->prev;
	node2->prev = par1;

	if (node1->next == node1) {
		node1->next = node2;
	}
	if (node2->next == node2) {
		node2->next = node1;
	}
	if (node1->next) {
		node1->next->prev = node1;
	}
	if (node2->next) {
		node2->next->prev = node2;
	}
	if (node1->prev) {
		node1->prev->next = node1;
	}
	if (node2->prev) {
		node2->prev->next = node2;
	}
}

/* Do the increments */
static void increment(huff_t* huff, node_t *node) {
	node_t *lnode;

	if (!node) {
		return;
	}

	if (node->next != NULL && node->next->weight == node->weight) {
	    lnode = *node->head;
		if (lnode != node->parent) {
			swap(huff, lnode, node);
		}
		swaplist(lnode, node);
	}
	if (node->prev && node->prev->weight == node->weight) {
		*node->head = node->prev;
	} else {
	    *node->head = NULL;
		free_ppnode(huff, node->head);
	}
	node->weight++;
	if (node->next && node->next->weight == node->weight) {
		node->head = node->next->head;
	} else {
		node->head = get_ppnode(huff);
		*node->head = node;
	}
	if (node->parent) {
		increment(huff, node->parent);
		if (node->prev == node->parent) {
			swaplist(node, node->parent);
			if (*node->head == node) {
				*node->head = node->parent;
			}
		}
	}
}

void Huff_addRef(huff_t* huff, byte ch) {
	node_t *tnode, *tnode2;
	if (huff->loc[ch] == NULL) { /* if this is the first transmission of this node */
		tnode = &(huff->nodeList[huff->blocNode++]);
		tnode2 = &(huff->nodeList[huff->blocNode++]);

		tnode2->symbol = INTERNAL_NODE;
		tnode2->weight = 1;
		tnode2->next = huff->lhead->next;
		if (huff->lhead->next) {
			huff->lhead->next->prev = tnode2;
			if (huff->lhead->next->weight == 1) {
				tnode2->head = huff->lhead->next->head;
			} else {
				tnode2->head = get_ppnode(huff);
				*tnode2->head = tnode2;
			}
		} else {
			tnode2->head = get_ppnode(huff);
			*tnode2->head = tnode2;
		}
		huff->lhead->next = tnode2;
		tnode2->prev = huff->lhead;

		tnode->symbol = ch;
		tnode->weight = 1;
		tnode->next = huff->lhead->next;
		if (huff->lhead->next) {
			huff->lhead->next->prev = tnode;
			if (huff->lhead->next->weight == 1) {
				tnode->head = huff->lhead->next->head;
			} else {
				/* this should never happen */
				tnode->head = get_ppnode(huff);
				*tnode->head = tnode2;
		    }
		} else {
			/* this should never happen */
			tnode->head = get_ppnode(huff);
			*tnode->head = tnode;
		}
		huff->lhead->next = tnode;
		tnode->prev = huff->lhead;
		tnode->left = tnode->right = NULL;

		if (huff->lhead->parent) {
			if (huff->lhead->parent->left == huff->lhead) { /* lhead is guaranteed to by the NYT */
				huff->lhead->parent->left = tnode2;
			} else {
				huff->lhead->parent->right = tnode2;
			}
		} else {
			huff->tree = tnode2;
		}

		tnode2->right = tnode;
		tnode2->left = huff->lhead;

		tnode2->parent = huff->lhead->parent;
		huff->lhead->parent = tnode->parent = tnode2;

		huff->loc[ch] = tnode;

		increment(huff, tnode2->parent);
	} else {
		increment(huff, huff->loc[ch]);
	}
}

/* Get a symbol */
int Huff_Receive (node_t *node, int *ch, byte *fin) {
	while (node && node->symbol == INTERNAL_NODE) {
		if (get_bit(fin)) {
			node = node->right;
		} else {
			node = node->left;
		}
	}
	if (!node) {
		return 0;
//		Com_Error(ERR_DROP, "Illegal tree!");
	}
	return (*ch = node->symbol);
}

/* Get a symbol */
void Huff_offsetReceive (node_t *node, int *ch, byte *fin, int *offset) {
	bloc = *offset;
	while (node && node->symbol == INTERNAL_NODE) {
		if (get_bit(fin)) {
			node = node->right;
		} else {
			node = node->left;
		}
	}
	if (!node) {
		*ch = 0;
		return;
//		Com_Error(ERR_DROP, "Illegal tree!");
	}
	*ch = node->symbol;
	*offset = bloc;
}

/* Send the prefix code for this node */
static void send(node_t *node, node_t *child, byte *fout) {
	if (node->parent) {
		send(node->parent, node, fout);
	}
	if (child) {
		if (node->right == child) {
			add_bit(1, fout);
		} else {
			add_bit(0, fout);
		}
	}
}

/* Send a symbol */
void Huff_transmit (huff_t *huff, int ch, byte *fout) {
	int i;
	if (huff->loc[ch] == NULL) {
		/* node_t hasn't been transmitted, send a NYT, then the symbol */
		Huff_transmit(huff, NYT, fout);
		for (i = 7; i >= 0; i--) {
			add_bit((char)((ch >> i) & 0x1), fout);
		}
	} else {
		send(huff->loc[ch], NULL, fout);
	}
}

void Huff_offsetTransmit (huff_t *huff, int ch, byte *fout, int *offset) {
	bloc = *offset;
	send(huff->loc[ch], NULL, fout);
	*offset = bloc;
}

void Huff_Decompress(msg_t *mbuf, int offset) {
	int			ch, cch, i, j, size;
	byte		seq[65536];
	byte*		buffer;
	huff_t		huff;

	size = mbuf->cursize - offset;
	buffer = mbuf->data + offset;

	if ( size <= 0 ) {
		return;
	}

	Com_Memset(&huff, 0, sizeof(huff_t));
	// Initialize the tree & list with the NYT node
	huff.tree = huff.lhead = huff.ltail = huff.loc[NYT] = &(huff.nodeList[huff.blocNode++]);
	huff.tree->symbol = NYT;
	huff.tree->weight = 0;
	huff.lhead->next = huff.lhead->prev = NULL;
	huff.tree->parent = huff.tree->left = huff.tree->right = NULL;

	cch = buffer[0]*256 + buffer[1];
	// don't overflow with bad messages
	if ( cch > mbuf->maxsize - offset ) {
		cch = mbuf->maxsize - offset;
	}
	bloc = 16;

	for ( j = 0; j < cch; j++ ) {
		ch = 0;
		// don't overflow reading from the messages
		// FIXME: would it be better to have a overflow check in get_bit ?
		if ( (bloc >> 3) > size ) {
			seq[j] = 0;
			break;
		}
		Huff_Receive(huff.tree, &ch, buffer);				/* Get a character */
		if ( ch == NYT ) {								/* We got a NYT, get the symbol associated with it */
			ch = 0;
			for ( i = 0; i < 8; i++ ) {
				ch = (ch<<1) + get_bit(buffer);
			}
		}

		seq[j] = ch;									/* Write symbol */

		Huff_addRef(&huff, (byte)ch);								/* Increment node */
	}
	mbuf->cursize = cch + offset;
	Com_Memcpy(mbuf->data + offset, seq, cch);
}


void Huff_Compress(msg_t *mbuf, int offset) {
	int			i, ch, size;
	byte		seq[65536];
	byte*		buffer;
	huff_t		huff;

	size = mbuf->cursize - offset;
	buffer = mbuf->data+ + offset;

	if (size<=0) {
		return;
	}

	Com_Memset(&huff, 0, sizeof(huff_t));
	// Add the NYT (not yet transmitted) node into the tree/list */
	huff.tree = huff.lhead = huff.loc[NYT] =  &(huff.nodeList[huff.blocNode++]);
	huff.tree->symbol = NYT;
	huff.tree->weight = 0;
	huff.lhead->next = huff.lhead->prev = NULL;
	huff.tree->parent = huff.tree->left = huff.tree->right = NULL;
	huff.loc[NYT] = huff.tree;

	seq[0] = (size>>8);
	seq[1] = size&0xff;

	bloc = 16;

	for (i=0; i<size; i++ ) {
		ch = buffer[i];
		Huff_transmit(&huff, ch, seq);						/* Transmit symbol */
		Huff_addRef(&huff, (byte)ch);								/* Do update */
	}

	bloc += 8;												// next byte

	mbuf->cursize = (bloc>>3) + offset;
	Com_Memcpy(mbuf->data+offset, seq, (bloc>>3));
}

void Huff_Init(huffman_t *huff) {

	Com_Memset(&huff->compressor, 0, sizeof(huff_t));
	Com_Memset(&huff->decompressor, 0, sizeof(huff_t));

	// Initialize the tree & list with the NYT node
	huff->decompressor.tree = huff->decompressor.lhead = huff->decompressor.ltail = huff->decompressor.loc[NYT] = &(huff->decompressor.nodeList[huff->decompressor.blocNode++]);
	huff->decompressor.tree->symbol = NYT;
	huff->decompressor.tree->weight = 0;
	huff->decompressor.lhead->next = huff->decompressor.lhead->prev = NULL;
	huff->decompressor.tree->parent = huff->decompressor.tree->left = huff->decompressor.tree->right = NULL;

	// Add the NYT (not yet transmitted) node into the tree/list */
	huff->compressor.tree = huff->compressor.lhead = huff->compressor.loc[NYT] =  &(huff->compressor.nodeList[huff->compressor.blocNode++]);
	huff->compressor.tree->symbol = NYT;
	huff->compressor.tree->weight = 0;
	huff->compressor.lhead->next = huff->compressor.lhead->prev = NULL;
	huff->compressor.tree->parent = huff->compressor.tree->left = huff->compressor.tree->right = NULL;
	huff->compressor.loc[NYT] = huff->compressor.tree;
}

// MSG_WriteBits and MSG_ReadBits of msg.cpp before the tables, on the given
// trees, without the _NEWHUFFTABLE_ code. Their globals are copies here, so
// that the benchmarks compare the same work.
typedef struct {
	const char	*name;
} netField_t;

static int oldsize = 0;
static int overflows;
static int fieldIndex;
static netField_t noField = { "<none>" };
static netField_t *gLastField = &noField;

void MSG_WriteBits( huffman_t *huff, msg_t *msg, int value, int bits ) {
	int	i;

	oldsize += bits;

	// this isn't an exact overflow check, but close enough
	if (msg->maxsize - msg->cursize < 4) {
		msg->overflowed = qtrue;
		return;
	}

	if (bits == 0 || bits < -31 || bits > 32) {
		Com_Error(ERR_DROP, "MSG_WriteBits: bad bits %i", bits);
	}

	// check for overflows
	if (com_debugMessage->integer && bits != 32) {
		if (bits > 0) {
			if (value > ((1 << bits) - 1) || value < 0) {
				overflows++;
				Com_Printf ("MSG_WriteBits: overflow writing %d in %d bits [field %s offset %d]\n", value, bits, gLastField->name, fieldIndex);
				assert (gLastField != &noField); // this means engine bug
			}
		} else {
			int	r;

			r = 1 << (- bits - 1);

			if (value >  r - 1 || value < -r) {
				overflows++;
				Com_Printf ("MSG_WriteBits: overflow writing %d in %d bits [field %s offset %d]\n", value, bits, gLastField->name, fieldIndex);
				assert (gLastField != &noField);  // this means engine bug
			}
		}
	}
	if (bits < 0) {
		bits = -bits;
	}
	if (msg->oob) {
		if (bits == 8) {
			msg->data[msg->cursize] = value;
			msg->cursize += 1;
			msg->bit += 8;
		} else if (bits == 16) {
			unsigned short *sp = (unsigned short *)&msg->data[msg->cursize];
			*sp = LittleShort(value);
			msg->cursize += 2;
			msg->bit += 16;
		} else if (bits == 32) {
			unsigned int *ip = (unsigned int *)&msg->data[msg->cursize];
			*ip = LittleLong(value);
			msg->cursize += 4;
			msg->bit += 8;
		} else {
			Com_Error(ERR_DROP, "can't read %d bits", bits);
		}
	} else {
		value &= (0xffffffff >> (32 - bits));
		if (bits & 7) {
			int nbits;
			nbits = bits & 7;
			for (i = 0; i<nbits; i++) {
				Huff_putBit((value & 1), msg->data, &msg->bit);
				value = (value >> 1);
			}
			bits = bits - nbits;
		}
		if (bits) {
			for (i = 0; i<bits; i += 8) {
				Huff_offsetTransmit(&huff->compressor, (value & 0xff), msg->data, &msg->bit);
				value = (value >> 8);
			}
		}
		msg->cursize = (msg->bit >> 3) + 1;
	}
}

int MSG_ReadBits( huffman_t *huff, msg_t *msg, int bits ) {
	int			value;
	int			get;
	qboolean	sgn;
	int			i, nbits;
	value = 0;

	if (bits < 0) {
		bits = -bits;
		sgn = qtrue;
	} else {
		sgn = qfalse;
	}

	if (msg->oob) {
		if (bits == 8) {
			value = msg->data[msg->readcount];
			msg->readcount += 1;
			msg->bit += 8;
		} else if (bits == 16) {
			unsigned short *sp = (unsigned short *)&msg->data[msg->readcount];
			value = LittleShort(*sp);
			msg->readcount += 2;
			msg->bit += 16;
		} else if (bits == 32) {
			unsigned int *ip = (unsigned int *)&msg->data[msg->readcount];
			value = LittleLong(*ip);
			msg->readcount += 4;
			msg->bit += 32;
		} else {
			Com_Error(ERR_DROP, "can't read %d bits", bits);
		}
	} else {
		nbits = 0;
		if (bits & 7) {
			nbits = bits & 7;
			for (i = 0; i<nbits; i++) {
				value |= (Huff_getBit(msg->data, &msg->bit) << i);
			}
			bits = bits - nbits;
		}
		if (bits) {
			for (i = 0; i<bits; i += 8) {
				Huff_offsetReceive(huff->decompressor.tree, &get, msg->data, &msg->bit);
				value |= (get << (i + nbits));
			}
		}
		msg->readcount = (msg->bit >> 3) + 1;
	}
	if (sgn) {
		assert(bits > 0 && bits < 32); // can't fit 32 bit unsigned integer
		if (value & (1 << (bits - 1))) {
			value |= -1 ^ ((1 << bits) - 1);
		}
	}

	return value;
}


#undef Huff_putBit
#undef Huff_getBit
#undef Huff_addRef
#undef Huff_offsetReceive
#undef Huff_offsetTransmit
#undef Huff_Decompress
#undef Huff_Compress
#undef Huff_Init

void Huff_putBit( int bit, byte *fout, int *offset ) { Old_Huff_putBit( bit, fout, offset ); }
int Huff_getBit( byte *fin, int *offset ) { return Old_Huff_getBit( fin, offset ); }
void Huff_addRef( huff_t *huff, byte ch ) { Old_Huff_addRef( huff, ch ); }
void Huff_offsetReceive( node_t *node, int *ch, byte *fin, int *offset ) { Old_Huff_offsetReceive( node, ch, fin, offset ); }
void Huff_offsetTransmit( huff_t *huff, int ch, byte *fout, int *offset ) { Old_Huff_offsetTransmit( huff, ch, fout, offset ); }
void Huff_Decompress( msg_t *mbuf, int offset ) { Old_Huff_Decompress( mbuf, offset ); }
void Huff_Compress( msg_t *mbuf, int offset ) { Old_Huff_Compress( mbuf, offset ); }
void Huff_Init( huffman_t *huff ) { Old_Huff_Init( huff ); }

} // namespace orig

// ==============================================================================
// Real messages: deltas written by the engine's own msg.cpp functions
// ==============================================================================

namespace {

// --- golden begin
struct Rng {
	uint32_t s;

	explicit Rng( uint32_t seed ) : s( seed ? seed : 1 ) {}

	uint32_t Next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}

	int Range( int lo, int hi ) {
		return lo + (int)( Next() % (uint32_t)( hi - lo + 1 ) );
	}
};

// exact floats (eighths), half of them integral
float Coord( Rng &r ) {
	return ( r.Next() & 1 ) ? (float)r.Range( -4096, 4095 ) : r.Range( -32768, 32767 ) / 8.0f;
}

void FillEntity( Rng &r, entityState_t *es, int number ) {
	memset( es, 0, sizeof( *es ) );
	es->number = number;
	es->eType = r.Range( 0, 12 );
	es->eFlags = (int)( r.Next() & 0x00ff13f7 );
	es->pos.trType = (trType_t)r.Range( 0, 5 );
	es->pos.trTime = r.Range( 0, 2000000 );
	es->pos.trDuration = r.Range( 0, 1000 );
	es->apos.trType = (trType_t)r.Range( 0, 2 );
	for ( int i = 0; i < 3; i++ ) {
		es->pos.trBase[i] = Coord( r );
		es->pos.trDelta[i] = ( r.Next() & 3 ) ? 0.0f : Coord( r );
		es->apos.trBase[i] = r.Range( 0, 2879 ) / 8.0f;
		es->origin[i] = es->pos.trBase[i];
		es->angles[i] = es->apos.trBase[i];
	}
	es->modelindex = r.Range( -128, 127 );
	es->clientNum = r.Range( 0, 31 );
	es->frame = r.Range( 0, 65535 );
	es->solid = (int)( r.Next() & 0xffffff );
	es->event = r.Range( 0, 1023 );
	es->eventParm = r.Range( 0, 255 );
	es->groundEntityNum = r.Range( 0, MAX_GENTITIES - 1 );
	es->legsAnim = r.Range( 0, 65535 );
	es->torsoAnim = r.Range( 0, 65535 );
	es->weapon = r.Range( 0, 15 );
	es->powerups = (int)( r.Next() & 0xffff );
	es->time = r.Range( 0, 2000000 );
	es->constantLight = (int)r.Next();
	es->loopSound = r.Range( 0, 255 );
	es->speed = r.Range( 0, 4000 ) / 4.0f;
	es->saberMove = r.Range( 0, 255 );
	es->forcePowersActive = (int)( r.Next() & 0x3ffff );
	es->isJediMaster = (qboolean)( r.Next() & 1 );
}

// what changes between two snapshots: mostly the position
void ChangeEntity( Rng &r, const entityState_t *from, entityState_t *to ) {
	entityState_t fresh;

	*to = *from;
	FillEntity( r, &fresh, from->number );
	if ( r.Next() & 1 ) {
		to->pos = fresh.pos;
		VectorCopy( fresh.origin, to->origin );
	}
	if ( ( r.Next() & 3 ) == 0 ) {
		to->apos = fresh.apos;
		VectorCopy( fresh.angles, to->angles );
	}
	if ( ( r.Next() & 7 ) == 0 ) {
		to->event = fresh.event;
		to->eventParm = fresh.eventParm;
	}
	if ( ( r.Next() & 7 ) == 0 ) {
		to->legsAnim = fresh.legsAnim;
		to->torsoAnim = fresh.torsoAnim;
	}
	if ( ( r.Next() & 15 ) == 0 ) {
		to->weapon = fresh.weapon;
		to->eFlags = fresh.eFlags;
	}
}

void FillPlayerState( Rng &r, playerState_t *ps ) {
	memset( ps, 0, sizeof( *ps ) );
	ps->commandTime = r.Range( 0, 2000000 );
	ps->pm_type = r.Range( 0, 7 );
	ps->bobCycle = r.Range( 0, 255 );
	ps->pm_flags = r.Range( 0, 65535 );
	ps->pm_time = r.Range( -32768, 32767 );
	for ( int i = 0; i < 3; i++ ) {
		ps->origin[i] = Coord( r );
		ps->velocity[i] = ( r.Next() & 1 ) ? 0.0f : Coord( r );
		ps->viewangles[i] = r.Range( -1440, 1439 ) / 8.0f;
		ps->delta_angles[i] = r.Range( 0, 65535 );
	}
	ps->weaponTime = r.Range( -32768, 32767 );
	ps->gravity = 800;
	ps->speed = 250;
	ps->groundEntityNum = r.Range( 0, MAX_GENTITIES - 1 );
	ps->legsAnim = r.Range( 0, 65535 );
	ps->torsoAnim = r.Range( 0, 65535 );
	ps->movementDir = r.Range( 0, 7 );
	ps->eFlags = (int)( r.Next() & 0x00ff13f7 );
	ps->eventSequence = r.Range( 0, 65535 );
	ps->events[0] = r.Range( 0, 1023 );
	ps->events[1] = r.Range( 0, 1023 );
	ps->clientNum = r.Range( 0, 31 );
	ps->weapon = r.Range( 0, 15 );
	ps->weaponstate = r.Range( 0, 7 );
	ps->viewheight = r.Range( -128, 127 );
	ps->saberMove = r.Range( 0, 255 );
	ps->fd.forcePowersKnown = (int)( r.Next() & 0x3ffff );
	ps->fd.forcePower = r.Range( 0, 100 );
	for ( int i = 0; i < 6; i++ ) {
		ps->stats[i] = r.Range( 0, 200 );
		ps->ammo[i] = r.Range( 0, 300 );
	}
	for ( int i = 0; i < 4; i++ ) {
		ps->persistant[i] = r.Range( 0, 50 );
	}
	const int powerup = r.Range( 0, 7 );
	ps->powerups[powerup] = r.Range( 0, 2000000 );
}

const char *const realStrings[] = {
	"\\sv_hostname\\^1JK2MV ^7Duel Server\\g_gametype\\3\\sv_maxclients\\32\\mapname\\ffa_bespin\\protocol\\16",
	"models/players/kyle/model.glm",
	"sound/weapons/saber/saberhum1.wav",
	"n\\Padawan\\t\\0\\model\\kyle/default\\c1\\4\\c2\\4\\hc\\100\\w\\0\\l\\0\\tt\\0\\tl\\0",
	"print \"Padawan^7 connected\n\"",
	"chat \"\x19Padawan^7\x19: gg\"",
	"gfx/effects/blaster_shot",
	"scores 4 0 0 1 12 0 47 100 0 0 0 0 0 3 2 0 25 0 0",
};

// A run of server messages (gamestate strings, server commands, player and
// entity deltas) and client messages (usercmds), each in a buffer filled
// with a pattern, hashed with the whole buffer: it also catches a write to a
// byte the encoder never touched.
uint64_t GoldenMessagesHash( mvversion_t version ) {
	static byte buffer[16384];
	static entityState_t ents[2][64];
	entityState_t nullEnt;
	playerState_t ps[2];
	usercmd_t cmds[2];
	Rng r( 0x5eed1234u );
	uint64_t hash = 14695981039346656037ull;
	msg_t msg;

	stubGameVersion = version;
	memset( &nullEnt, 0, sizeof( nullEnt ) );
	memset( &cmds, 0, sizeof( cmds ) );
	FillPlayerState( r, &ps[0] );
	for ( int i = 0; i < 64; i++ ) {
		FillEntity( r, &ents[0][i], r.Range( 32, MAX_GENTITIES - 2 ) );
	}

	for ( int m = 0; m < 48; m++ ) {
		entityState_t *from = ents[m & 1], *to = ents[( m + 1 ) & 1];
		playerState_t *psFrom = &ps[m & 1], *psTo = &ps[( m + 1 ) & 1];

		MSG_Init( &msg, buffer, sizeof( buffer ) );
		for ( int i = 0; i < (int)sizeof( buffer ); i++ ) {
			buffer[i] = (byte)( i * 37 + 11 );
		}
		// odd starts: the client sends a few raw bits first in some paths
		if ( m & 3 ) {
			MSG_WriteBits( &msg, m, m & 3 );
		}

		MSG_WriteLong( &msg, m * 1000 + 17 );
		if ( m == 0 ) {
			// gamestate: strings and baselines
			for ( int i = 0; i < 120; i++ ) {
				MSG_WriteByte( &msg, 3 );
				MSG_WriteShort( &msg, i );
				MSG_WriteBigString( &msg, realStrings[i % ARRAY_LEN( realStrings )] );
			}
			for ( int i = 0; i < 64; i++ ) {
				MSG_WriteByte( &msg, 4 );
				MSG_WriteDeltaEntity( &msg, &nullEnt, &from[i], qtrue );
			}
		}
		MSG_WriteByte( &msg, 5 );
		MSG_WriteLong( &msg, m );
		MSG_WriteString( &msg, realStrings[m % ARRAY_LEN( realStrings )] );

		MSG_WriteByte( &msg, 7 );
		*psTo = *psFrom;
		psTo->commandTime += 50;
		for ( int i = 0; i < 3; i++ ) {
			psTo->origin[i] += r.Range( -64, 64 ) / 8.0f;
		}
		if ( r.Next() & 1 ) {
			psTo->viewangles[1] = r.Range( 0, 2879 ) / 8.0f;
			psTo->stats[0] = r.Range( 0, 100 );
		}
		MSG_WriteDeltaPlayerstate( &msg, ( m & 7 ) ? psFrom : NULL, psTo );

		for ( int i = 0; i < 64; i++ ) {
			ChangeEntity( r, &from[i], &to[i] );
			if ( ( r.Next() & 31 ) == 0 ) {
				MSG_WriteDeltaEntity( &msg, &from[i], NULL, qtrue );
			} else {
				MSG_WriteDeltaEntity( &msg, &from[i], &to[i], (qboolean)( i & 1 ) );
			}
		}
		MSG_WriteBits( &msg, MAX_GENTITIES - 1, GENTITYNUM_BITS );

		// a client message with usercmds
		for ( int i = 0; i < 4; i++ ) {
			usercmd_t *cmdFrom = &cmds[i & 1], *cmdTo = &cmds[( i + 1 ) & 1];

			*cmdTo = *cmdFrom;
			cmdTo->serverTime += 16;
			cmdTo->angles[1] += r.Range( -300, 300 );
			cmdTo->buttons = r.Range( 0, 3 );
			cmdTo->forwardmove = (signed char)r.Range( -127, 127 );
			cmdTo->rightmove = (signed char)( ( r.Next() & 1 ) ? 0 : 127 );
			MSG_WriteDeltaUsercmdKey( &msg, m * 31 + i, cmdFrom, cmdTo );
		}

		for ( int i = 0; i < (int)sizeof( buffer ); i++ ) {
			hash = ( hash ^ buffer[i] ) * 1099511628211ull;
		}
		hash = ( hash ^ (uint32_t)msg.bit ) * 1099511628211ull;
		hash = ( hash ^ (uint32_t)msg.cursize ) * 1099511628211ull;
		hash = ( hash ^ (uint32_t)msg.overflowed ) * 1099511628211ull;
	}

	stubGameVersion = VERSION_1_04;
	return hash;
}
// --- golden end

// recorded with the tree-walking implementation (jk2mv before the tables)
const uint64_t goldenHash102 = 0x41719951d3a777e4ull;
const uint64_t goldenHash104 = 0xa70db108f16948e8ull;

// ==============================================================================
// Helpers
// ==============================================================================

// msg_hData of msg.cpp (the Q3 TA frequency table)
const int hData[256] = {
	250315, 41193, 6292, 7106, 3730, 3750, 6110, 23283,
	33317, 6950, 7838, 9714, 9257, 17259, 3949, 1778,
	8288, 1604, 1590, 1663, 1100, 1213, 1238, 1134,
	1749, 1059, 1246, 1149, 1273, 4486, 2805, 3472,
	21819, 1159, 1670, 1066, 1043, 1012, 1053, 1070,
	1726, 888, 1180, 850, 960, 780, 1752, 3296,
	10630, 4514, 5881, 2685, 4650, 3837, 2093, 1867,
	2584, 1949, 1972, 940, 1134, 1788, 1670, 1206,
	5719, 6128, 7222, 6654, 3710, 3795, 1492, 1524,
	2215, 1140, 1355, 971, 2180, 1248, 1328, 1195,
	1770, 1078, 1264, 1266, 1168, 965, 1155, 1186,
	1347, 1228, 1529, 1600, 2617, 2048, 2546, 3275,
	2410, 3585, 2504, 2800, 2675, 6146, 3663, 2840,
	14253, 3164, 2221, 1687, 3208, 2739, 3512, 4796,
	4091, 3515, 5288, 4016, 7937, 6031, 5360, 3924,
	4892, 3743, 4566, 4807, 5852, 6400, 6225, 8291,
	23243, 7838, 7073, 8935, 5437, 4483, 3641, 5256,
	5312, 5328, 5370, 3492, 2458, 1694, 1821, 2121,
	1916, 1149, 1516, 1367, 1236, 1029, 1258, 1104,
	1245, 1006, 1149, 1025, 1241, 952, 1287, 997,
	1713, 1009, 1187, 879, 1099, 929, 1078, 951,
	1656, 930, 1153, 1030, 1262, 1062, 1214, 1060,
	1621, 930, 1106, 912, 1034, 892, 1158, 990,
	1175, 850, 1121, 903, 1087, 920, 1144, 1056,
	3462, 2240, 4397, 12136, 7758, 1345, 1307, 3278,
	1950, 886, 1023, 1112, 1077, 1042, 1061, 1071,
	1484, 1001, 1096, 915, 1052, 995, 1070, 876,
	1111, 851, 1059, 805, 1112, 923, 1103, 817,
	1899, 1872, 976, 841, 1127, 956, 1159, 950,
	7791, 954, 1289, 933, 1127, 3207, 1020, 927,
	1355, 768, 1040, 745, 952, 805, 1073, 740,
	1013, 805, 1008, 796, 996, 1057, 11457, 13504,
};

// the netchan trees, built the old way
huffman_t *ReferenceTrees() {
	static huffman_t *huff = NULL;

	if ( !huff ) {
		huff = new huffman_t;
		orig::Huff_Init( huff );
		for ( int i = 0; i < 256; i++ ) {
			for ( int j = 0; j < hData[i]; j++ ) {
				orig::Huff_addRef( &huff->compressor, (byte)i );
				orig::Huff_addRef( &huff->decompressor, (byte)i );
			}
		}
	}
	return huff;
}

void FillRandom( Rng &r, byte *data, size_t size ) {
	for ( size_t i = 0; i < size; i++ ) {
		data[i] = (byte)r.Next();
	}
}

int CodeLength( const huff_t *huff, int ch ) {
	int length = 0;
	for ( const node_t *node = huff->loc[ch]; node->parent; node = node->parent ) {
		length++;
	}
	return length;
}

// Compare encoding every symbol, at every start offset, over three kinds of
// existing buffer content, then decoding at every bit offset of random data
// and of a stream of codes, between the old tree code and the tables built
// from the tree. The engine's tree functions, which no longer share a bit
// position, are checked along the way.
void ExpectTablesMatchTree( huff_t *huff, uint32_t seed, bool intact = true ) {
	huffTables_t *tables = new huffTables_t;
	Rng r( seed );

	Huff_BuildTables( tables, huff, huff );

	for ( int ch = 0; ch <= HMAX; ch++ ) {
		if ( !huff->loc[ch] ) {
			EXPECT_EQ( 0, tables->codeLength[ch] ) << "symbol " << ch;
			continue;	// the tree function crashes on these
		}
		for ( int start = 0; start < 24; start++ ) {
			for ( int fill = 0; fill < 3; fill++ ) {
				byte a[64], b[64], c[64];
				int offA = start, offB = start, offC = start;

				if ( fill == 2 ) {
					FillRandom( r, a, sizeof( a ) );
				} else {
					memset( a, fill ? 0xff : 0x00, sizeof( a ) );
				}
				memcpy( b, a, sizeof( b ) );
				memcpy( c, a, sizeof( c ) );
				orig::Huff_offsetTransmit( huff, ch, a, &offA );
				Huff_offsetTransmitTable( tables, ch, b, &offB );
				Huff_offsetTransmit( huff, ch, c, &offC );
				ASSERT_EQ( offA, offB ) << "symbol " << ch << " start " << start;
				ASSERT_EQ( 0, memcmp( a, b, sizeof( a ) ) ) << "symbol " << ch << " start " << start;
				ASSERT_EQ( offA, offC ) << "symbol " << ch << " start " << start;
				ASSERT_EQ( 0, memcmp( a, c, sizeof( a ) ) ) << "symbol " << ch << " start " << start;
			}
		}
	}

	// random data, read at every offset, the last ones by the tree
	const int size = 2048;
	std::vector<byte> data( size + 16 );
	FillRandom( r, data.data(), data.size() );
	for ( int offset = 0; offset < size * 8; offset++ ) {
		int chA = -1, chB = -1, chC = -1, offA = offset, offB = offset, offC = offset;

		orig::Huff_offsetReceive( huff->tree, &chA, data.data(), &offA );
		Huff_offsetReceiveTable( tables, &chB, data.data(), &offB, size );
		Huff_offsetReceive( huff->tree, &chC, data.data(), &offC );
		ASSERT_EQ( chA, chB ) << "offset " << offset;
		ASSERT_EQ( offA, offB ) << "offset " << offset;
		ASSERT_EQ( chA, chC ) << "offset " << offset;
		ASSERT_EQ( offA, offC ) << "offset " << offset;
	}

	// a stream of codes from an odd offset, written and read both ways
	std::vector<int> symbols;
	for ( int i = 0; i < 20000; i++ ) {
		int ch = ( r.Next() & 1 ) ? r.Range( 0, 15 ) : r.Range( 0, HMAX );
		if ( huff->loc[ch] ) {
			symbols.push_back( ch );
		}
	}
	std::vector<byte> a( symbols.size() * 8 + 16, 0xa5 ), b( a );
	int offA = 5, offB = 5;
	for ( size_t i = 0; i < symbols.size(); i++ ) {
		orig::Huff_offsetTransmit( huff, symbols[i], a.data(), &offA );
		Huff_offsetTransmitTable( tables, symbols[i], b.data(), &offB );
	}
	ASSERT_EQ( offA, offB );
	ASSERT_EQ( 0, memcmp( a.data(), b.data(), a.size() ) );
	const int end = offA;
	offA = offB = 5;
	for ( size_t i = 0; i < symbols.size(); i++ ) {
		int chA = -1, chB = -1;

		orig::Huff_offsetReceive( huff->tree, &chA, b.data(), &offA );
		Huff_offsetReceiveTable( tables, &chB, a.data(), &offB, ( end >> 3 ) + 1 );
		if ( intact ) {
			ASSERT_EQ( symbols[i], chA );
		}
		ASSERT_EQ( chA, chB );
		ASSERT_EQ( offA, offB );
	}

	delete tables;
}

// The bitstream part of the old MSG_WriteBits and MSG_ReadBits on one tree:
// the low bits & 7 bits one at a time, then a tree code for each whole byte
void OldWriteBits( huff_t *huff, int value, int bits, byte *fout, int *offset ) {
	const int nbits = bits & 7;

	for ( int i = 0; i < nbits; i++ ) {
		orig::Huff_putBit( ( value >> i ) & 1, fout, offset );
	}
	for ( int i = nbits; i < bits; i += 8 ) {
		orig::Huff_offsetTransmit( huff, ( value >> i ) & 0xff, fout, offset );
	}
}

int OldReadBits( huff_t *huff, int bits, byte *fin, int *offset ) {
	const int nbits = bits & 7;
	uint32_t value = 0;
	int get;

	for ( int i = 0; i < nbits; i++ ) {
		value |= (uint32_t)orig::Huff_getBit( fin, offset ) << i;
	}
	for ( int i = nbits; i < bits; i += 8 ) {
		orig::Huff_offsetReceive( huff->tree, &get, fin, offset );
		value |= (uint32_t)get << i;
	}
	return (int)value;
}

// Huff_WriteBits and Huff_ReadBits against the old code: every width at
// every start offset over three kinds of buffer content, each reading what
// the other wrote with and without room for the lookup, then random data
// read with random widths up to and past finSize
void ExpectBitsMatchTree( huff_t *huff, uint32_t seed, bool intact = true ) {
	std::unique_ptr<huffTables_t> tables( new huffTables_t );
	std::vector<int> symbols;
	Rng r( seed );

	Huff_BuildTables( tables.get(), huff, huff );
	for ( int ch = 0; ch < 256; ch++ ) {
		if ( huff->loc[ch] ) {
			symbols.push_back( ch );
		}
	}
	ASSERT_FALSE( symbols.empty() );

	for ( int bits = 1; bits <= 32; bits++ ) {
		const int nbits = bits & 7;

		for ( int start = 0; start < 16; start++ ) {
			for ( int fill = 0; fill < 3; fill++ ) {
				byte a[48], b[48];
				int offA = start, offB = start;

				// made of bytes the tree has: the old code crashes on the others
				uint32_t value = r.Next() & ( ( 1u << nbits ) - 1 );
				for ( int i = nbits; i < bits; i += 8 ) {
					value |= (uint32_t)symbols[r.Range( 0, (int)symbols.size() - 1 )] << i;
				}
				if ( fill == 2 ) {
					FillRandom( r, a, sizeof( a ) );
				} else {
					memset( a, fill ? 0xff : 0x00, sizeof( a ) );
				}
				memcpy( b, a, sizeof( b ) );
				OldWriteBits( huff, (int)value, bits, a, &offA );
				Huff_WriteBits( tables.get(), (int)value, bits, b, &offB );
				ASSERT_EQ( offA, offB ) << bits << " bits at " << start;
				ASSERT_EQ( 0, memcmp( a, b, sizeof( a ) ) ) << bits << " bits at " << start;

				// the encoder ORs into the bits above the start in its first
				// byte, so the value only comes back when they were 0
				const bool clean = fill == 0 || ( start & 7 ) == 0;
				const int finSizes[2] = { (int)sizeof( b ), ( offA >> 3 ) + 1 - r.Range( 0, 3 ) };
				for ( int f = 0; f < 2; f++ ) {
					offA = offB = start;
					const int valueA = OldReadBits( huff, bits, b, &offA );
					const int valueB = Huff_ReadBits( tables.get(), bits, a, &offB, finSizes[f] );
					if ( intact && clean ) {
						ASSERT_EQ( (int)value, valueA ) << bits << " bits at " << start;
					}
					ASSERT_EQ( valueA, valueB ) << bits << " bits at " << start;
					ASSERT_EQ( offA, offB ) << bits << " bits at " << start;
				}
			}
		}
	}

	std::vector<byte> data( 1024 + 64 );
	FillRandom( r, data.data(), data.size() );
	for ( int run = 0; run < 16; run++ ) {
		const int finSize = r.Range( 0, 1024 );
		int offA = r.Range( 0, 7 ), offB = offA;

		for ( int n = 0; n < 4096 && ( offA >> 3 ) < 1024; n++ ) {
			const int bits = r.Range( 1, 32 );
			const int valueA = OldReadBits( huff, bits, data.data(), &offA );
			const int valueB = Huff_ReadBits( tables.get(), bits, data.data(), &offB, finSize );

			ASSERT_EQ( valueA, valueB ) << bits << " bits at " << offA;
			ASSERT_EQ( offA, offB );
		}
	}
}

struct BitOp {
	int value;
	int bits;
};

void AddString( std::vector<BitOp> &ops, const char *s ) {
	do {
		BitOp op = { (byte)*s, 8 };
		ops.push_back( op );
	} while ( *s++ );
}

void AddValue( Rng &r, std::vector<BitOp> &ops, int bits ) {
	// mostly small numbers, like the deltas
	int width = bits < 0 ? -bits : bits;
	int value;

	switch ( r.Next() & 3 ) {
	case 0:
		value = 0;
		break;
	case 1:
		value = r.Range( 0, 15 );
		break;
	case 2:
		value = (int)( r.Next() & ( 0xffffffffu >> ( 32 - width ) ) );
		break;
	default:
		value = r.Range( -8, 8 );
		break;
	}
	BitOp op = { value, bits };
	ops.push_back( op );
}

// The field widths of the entity and player state deltas, the usercmds and
// the float encoding (1, 13 and 32 bits), strings and the message headers.
std::vector<BitOp> RealLookingOps( Rng &r, int messages ) {
	static const int widths[] = { 1, 1, 1, 1, 2, 4, 5, 6, 8, 8, 10, 13, 16, -16, -8, 24, 32 };
	std::vector<BitOp> ops;

	for ( int m = 0; m < messages; m++ ) {
		BitOp seq = { m * 3, 32 };
		ops.push_back( seq );
		if ( ( r.Next() & 3 ) == 0 ) {
			BitOp cmd = { 5, 8 };
			ops.push_back( cmd );
			AddString( ops, realStrings[r.Range( 0, ARRAY_LEN( realStrings ) - 1 )] );
		}
		for ( int e = r.Range( 0, 40 ); e > 0; e-- ) {
			BitOp number = { r.Range( 0, MAX_GENTITIES - 1 ), GENTITYNUM_BITS };
			BitOp flags = { 2, 2 };
			ops.push_back( number );
			ops.push_back( flags );
			int lc = r.Range( 1, 60 );
			BitOp count = { lc, 8 };
			ops.push_back( count );
			for ( int f = 0; f < lc; f++ ) {
				if ( r.Next() & 3 ) {
					BitOp unchanged = { 0, 1 };
					ops.push_back( unchanged );
					continue;
				}
				AddValue( r, ops, widths[r.Range( 0, ARRAY_LEN( widths ) - 1 )] );
			}
		}
		BitOp endMark = { MAX_GENTITIES - 1, GENTITYNUM_BITS };
		ops.push_back( endMark );
	}
	return ops;
}

std::vector<BitOp> RandomOps( Rng &r, int count ) {
	std::vector<BitOp> ops;

	for ( int i = 0; i < count; i++ ) {
		int bits = r.Range( 1, 32 );
		if ( bits >= 8 && bits < 32 && ( r.Next() & 3 ) == 0 ) {
			bits = -bits;	// negative widths below 8 can't be read back
		}
		BitOp op = { (int)r.Next(), bits };
		ops.push_back( op );
	}
	return ops;
}

// what MSG_ReadBits gives back for a written value (its sign extension looks
// at the bit below the whole bytes)
int Expected( const BitOp &op ) {
	int width = op.bits < 0 ? -op.bits : op.bits;
	int value = op.value & (int)( 0xffffffffu >> ( 32 - width ) );

	if ( op.bits < 0 ) {
		int bits = width - ( width & 7 );
		if ( value & ( 1 << ( bits - 1 ) ) ) {
			value |= -1 ^ ( ( 1 << bits ) - 1 );
		}
	}
	return value;
}

struct MsgBuffer {
	std::vector<byte> data;
	msg_t msg;

	// Random content, a few bytes past maxsize too (the old reader reads past
	// the end), except the bits from the start in its byte: the encoder ORs
	// into them, they are 0 in a real message.
	MsgBuffer( int maxsize, Rng &r, int start ) : data( maxsize + 64 ) {
		FillRandom( r, data.data(), data.size() );
		data[start >> 3] &= ( 1 << ( start & 7 ) ) - 1;
		MSG_Init( &msg, data.data(), maxsize );
		msg.bit = start;
		msg.cursize = start ? ( start >> 3 ) + 1 : 0;
	}
};

// write the ops with the engine and with the old code, which must give the
// same buffers, then read each buffer back with the other implementation
void ExpectSameMessages( const std::vector<BitOp> &ops, int maxsize, uint32_t seed ) {
	huffman_t *ref = ReferenceTrees();
	Rng r( seed ), fill( seed );
	const int start = r.Range( 0, 1 ) ? r.Range( 0, 63 ) : 0;
	MsgBuffer a( maxsize, fill, start );
	Rng fill2( seed );
	MsgBuffer b( maxsize, fill2, start );
	size_t written = 0;

	for ( size_t i = 0; i < ops.size(); i++ ) {
		orig::MSG_WriteBits( ref, &a.msg, ops[i].value, ops[i].bits );
		MSG_WriteBits( &b.msg, ops[i].value, ops[i].bits );
		ASSERT_EQ( a.msg.bit, b.msg.bit ) << "op " << i;
		ASSERT_EQ( a.msg.overflowed, b.msg.overflowed ) << "op " << i;
		if ( !a.msg.overflowed ) {
			written = i + 1;
		}
	}
	ASSERT_EQ( a.msg.cursize, b.msg.cursize );
	ASSERT_EQ( 0, memcmp( a.data.data(), b.data.data(), a.data.size() ) );

	// the engine reads what the old code wrote, the old code what the engine wrote
	a.msg.bit = b.msg.bit = start;
	for ( size_t i = 0; i < written; i++ ) {
		const int valueA = MSG_ReadBits( &a.msg, ops[i].bits );
		const int valueB = orig::MSG_ReadBits( ref, &b.msg, ops[i].bits );

		ASSERT_EQ( Expected( ops[i] ), valueA ) << "op " << i;
		ASSERT_EQ( valueA, valueB ) << "op " << i;
		ASSERT_EQ( a.msg.bit, b.msg.bit ) << "op " << i;
		ASSERT_EQ( a.msg.readcount, b.msg.readcount ) << "op " << i;
	}
}

double Seconds( std::chrono::steady_clock::time_point since ) {
	return std::chrono::duration<double>( std::chrono::steady_clock::now() - since ).count();
}

} // namespace

// ==============================================================================
// Symbol codes: tables against the tree
// ==============================================================================

TEST(HuffmanTables, NetchanCodesAllFitTheLookup) {
	huffman_t *ref = ReferenceTrees();
	huffTables_t *tables = new huffTables_t;
	int shortest = 99, longest = 0;

	Huff_BuildTables( tables, &ref->compressor, &ref->decompressor );
	for ( int ch = 0; ch <= HMAX; ch++ ) {
		const int length = CodeLength( &ref->compressor, ch );
		EXPECT_GE( length, 1 );
		EXPECT_LE( length, HUFF_LOOKUP_BITS );
		EXPECT_EQ( length, tables->codeLength[ch] ) << "symbol " << ch;
		shortest = std::min( shortest, length );
		longest = std::max( longest, length );
	}
	// as qcommon.h says
	EXPECT_EQ( 2, shortest );
	EXPECT_EQ( 11, longest );
	// so the tree is only used for the last bytes of a message
	for ( int i = 0; i < ( 1 << HUFF_LOOKUP_BITS ); i++ ) {
		EXPECT_NE( 0, tables->lookup[i] ) << "lookup " << i;
	}
	delete tables;
}

TEST(HuffmanTables, NetchanTablesMatchTree) {
	ExpectTablesMatchTree( &ReferenceTrees()->compressor, 101 );
}

TEST(HuffmanTables, NetchanBitsMatchTree) {
	ExpectBitsMatchTree( &ReferenceTrees()->compressor, 102 );
}

TEST(HuffmanTables, SingleBitsMatchOldCode) {
	// Huff_putBit and Huff_getBit lost the shared bit position
	Rng r( 7 );
	byte a[64], b[64];
	int offA = 3, offB = 3;

	FillRandom( r, a, sizeof( a ) );
	memcpy( b, a, sizeof( b ) );
	for ( int i = 0; i < 400; i++ ) {
		const int bit = (int)( r.Next() & 1 );
		orig::Huff_putBit( bit, a, &offA );
		Huff_putBit( bit, b, &offB );
		ASSERT_EQ( offA, offB );
	}
	ASSERT_EQ( 0, memcmp( a, b, sizeof( a ) ) );
	offA = offB = 0;
	for ( int i = 0; i < 64 * 8; i++ ) {
		ASSERT_EQ( orig::Huff_getBit( a, &offA ), Huff_getBit( a, &offB ) );
		ASSERT_EQ( offA, offB );
	}
}

TEST(HuffmanTables, DeepTreeFallsBackToTheTree) {
	// Fibonacci weights make the deepest Huffman tree: codes longer than the
	// lookup, and longer than the code table holds
	huffman_t *huff = new huffman_t;
	huffTables_t *tables = new huffTables_t;
	int a = 1, b = 1, maxLength = 0;

	Huff_Init( huff );
	for ( int ch = 0; ch < 30; ch++ ) {
		for ( int i = 0; i < a; i++ ) {
			Huff_addRef( &huff->compressor, (byte)( ch * 7 + 3 ) );
		}
		const int next = a + b;
		a = b;
		b = next;
	}
	for ( int ch = 0; ch <= HMAX; ch++ ) {
		if ( huff->compressor.loc[ch] ) {
			maxLength = std::max( maxLength, CodeLength( &huff->compressor, ch ) );
		}
	}
	ASSERT_GT( maxLength, 25 );

	Huff_BuildTables( tables, &huff->compressor, &huff->compressor );
	int noCode = 0, noLookup = 0;
	for ( int ch = 0; ch <= HMAX; ch++ ) {
		noCode += huff->compressor.loc[ch] && !tables->codeLength[ch];
	}
	for ( int i = 0; i < ( 1 << HUFF_LOOKUP_BITS ); i++ ) {
		noLookup += !tables->lookup[i];
	}
	EXPECT_GT( noCode, 0 );
	EXPECT_GT( noLookup, 0 );

	ExpectTablesMatchTree( &huff->compressor, 202 );
	ExpectBitsMatchTree( &huff->compressor, 203 );
	delete tables;
	delete huff;
}

TEST(HuffmanTables, BrokenTreeFallsBackToTheTree) {
	// a missing child (the tree gives symbol 0 and doesn't move) and leaves
	// with symbols out of range, like a damaged huffman.dat could give
	huffman_t *huff = new huffman_t;
	Rng r( 303 );
	int broken = 0;

	Huff_Init( huff );
	for ( int ch = 0; ch < 40; ch++ ) {
		for ( int i = r.Range( 1, 300 ); i > 0; i-- ) {
			Huff_addRef( &huff->compressor, (byte)( ch * 5 ) );
		}
	}
	for ( int i = 0; i < huff->compressor.blocNode; i++ ) {
		node_t *node = &huff->compressor.nodeList[i];
		if ( node->symbol == INTERNAL_NODE && node->parent && node->parent->parent && broken == 0 ) {
			node->right = NULL;
			broken++;
		} else if ( node->symbol == 25 ) {
			node->symbol = 1000;
		} else if ( node->symbol == 30 ) {
			node->symbol = -3;
		}
	}
	ASSERT_EQ( 1, broken );

	ExpectTablesMatchTree( &huff->compressor, 404, false );
	ExpectBitsMatchTree( &huff->compressor, 405, false );
	delete huff;
}

TEST(HuffmanTables, UnbuiltTablesReadLikeNoTree) {
	huffTables_t *tables = new huffTables_t;
	byte data[8] = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 };
	int ch = -1, offset = 3;

	memset( tables, 0, sizeof( *tables ) );
	Huff_offsetReceiveTable( tables, &ch, data, &offset, sizeof( data ) );
	EXPECT_EQ( 0, ch );
	EXPECT_EQ( 3, offset );
	EXPECT_EQ( 0, Huff_ReadBits( tables, 24, data, &offset, sizeof( data ) ) );
	EXPECT_EQ( 3, offset );
	delete tables;
}

// ==============================================================================
// Messages: MSG_WriteBits / MSG_ReadBits against the old code
// ==============================================================================

TEST(HuffmanMessages, RandomBitsMatchOldCode) {
	Rng r( 11 );

	// one run in four writes more than MAX_MSGLEN: up to the overflow
	for ( int n = 0; n < 60; n++ ) {
		ExpectSameMessages( RandomOps( r, ( n & 3 ) ? 3000 : 12000 ), MAX_MSGLEN, 1000 + n );
		if ( HasFatalFailure() ) {
			return;
		}
	}
}

TEST(HuffmanMessages, RealLookingMessagesMatchOldCode) {
	Rng r( 12 );

	// one run in four writes more than MAX_MSGLEN: up to the overflow
	for ( int n = 0; n < 40; n++ ) {
		ExpectSameMessages( RealLookingOps( r, ( n & 3 ) ? 8 : 80 ), MAX_MSGLEN, 2000 + n );
		if ( HasFatalFailure() ) {
			return;
		}
	}
}

TEST(HuffmanMessages, OverflowMatchesOldCode) {
	Rng r( 13 );

	for ( int n = 0; n < 40; n++ ) {
		ExpectSameMessages( RandomOps( r, 400 ), r.Range( 8, 600 ), 3000 + n );
		if ( HasFatalFailure() ) {
			return;
		}
	}
}

TEST(HuffmanMessages, GarbageReadsMatchOldCode) {
	// random data read with random widths, past cursize and past maxsize: the
	// codes that aren't bytes (the 11-bit NYT gives 256) and the tail read by
	// the tree come out the same
	huffman_t *ref = ReferenceTrees();
	Rng r( 14 );

	for ( int n = 0; n < 200; n++ ) {
		const int maxsize = r.Range( 4, 3000 );
		Rng fill( 5000 + n ), fill2( 5000 + n );
		MsgBuffer a( maxsize, fill, 0 ), b( maxsize, fill2, 0 );

		a.msg.cursize = b.msg.cursize = r.Range( 0, maxsize );
		a.msg.bit = b.msg.bit = r.Range( 0, 7 );
		while ( ( a.msg.bit >> 3 ) < maxsize + 16 ) {
			int bits = r.Range( 1, 32 );
			if ( bits >= 8 && ( r.Next() & 3 ) == 0 ) {
				bits = -std::min( bits, 31 );
			}
			const int valueA = orig::MSG_ReadBits( ref, &a.msg, bits );
			const int valueB = MSG_ReadBits( &b.msg, bits );
			ASSERT_EQ( valueA, valueB ) << "bits " << bits << " at " << a.msg.bit;
			ASSERT_EQ( a.msg.bit, b.msg.bit );
			ASSERT_EQ( a.msg.readcount, b.msg.readcount );
		}
	}
}

TEST(HuffmanMessages, OutOfBandMatchesOldCode) {
	// connectionless messages carry their bytes as they are; this is the
	// other way through MSG_WriteBits and MSG_ReadBits
	static const int widths[] = { 8, -8, 16, -16, 32 };
	huffman_t *ref = ReferenceTrees();
	Rng r( 19 );

	for ( int n = 0; n < 40; n++ ) {
		const int maxsize = ( n & 1 ) ? r.Range( 4, 600 ) : MAX_MSGLEN;
		std::vector<byte> a( maxsize + 8 ), b;
		std::vector<BitOp> ops;
		size_t written = 0;
		msg_t ma, mb;

		FillRandom( r, a.data(), a.size() );
		b = a;
		MSG_InitOOB( &ma, a.data(), maxsize );
		MSG_InitOOB( &mb, b.data(), maxsize );
		for ( int i = r.Range( 1, 1000 ); i > 0; i-- ) {
			BitOp op = { (int)r.Next(), widths[r.Range( 0, ARRAY_LEN( widths ) - 1 )] };

			ops.push_back( op );
			orig::MSG_WriteBits( ref, &ma, op.value, op.bits );
			MSG_WriteBits( &mb, op.value, op.bits );
			ASSERT_EQ( ma.cursize, mb.cursize );
			ASSERT_EQ( ma.bit, mb.bit );
			ASSERT_EQ( ma.overflowed, mb.overflowed );
			if ( !ma.overflowed ) {
				written = ops.size();
			}
		}
		ASSERT_EQ( 0, memcmp( a.data(), b.data(), a.size() ) );

		MSG_BeginReadingOOB( &ma );
		MSG_BeginReadingOOB( &mb );
		for ( size_t i = 0; i < written; i++ ) {
			ASSERT_EQ( orig::MSG_ReadBits( ref, &mb, ops[i].bits ), MSG_ReadBits( &ma, ops[i].bits ) ) << "op " << i;
			ASSERT_EQ( ma.readcount, mb.readcount );
			ASSERT_EQ( ma.bit, mb.bit );
		}
	}
}

TEST(HuffmanMessages, RealDeltasMatchGoldenHash) {
	EXPECT_EQ( goldenHash102, GoldenMessagesHash( VERSION_1_02 ) );
	EXPECT_EQ( goldenHash104, GoldenMessagesHash( VERSION_1_04 ) );
}

TEST(HuffmanMessages, RealDeltasRoundTrip) {
	static byte buffer[MAX_MSGLEN];
	entityState_t from, to, got;
	playerState_t psFrom, psTo, psGot;
	Rng r( 15 );
	msg_t msg;

	for ( int n = 0; n < 200; n++ ) {
		FillEntity( r, &from, r.Range( 0, MAX_GENTITIES - 2 ) );
		ChangeEntity( r, &from, &to );
		FillPlayerState( r, &psFrom );
		FillPlayerState( r, &psTo );

		MSG_Init( &msg, buffer, sizeof( buffer ) );
		MSG_WriteBits( &msg, n, n & 7 ? n & 7 : 1 );
		MSG_WriteDeltaEntity( &msg, &from, &to, qtrue );
		MSG_WriteDeltaPlayerstate( &msg, &psFrom, &psTo );
		MSG_WriteString( &msg, realStrings[n % ARRAY_LEN( realStrings )] );

		MSG_BeginReading( &msg );
		EXPECT_EQ( n & ( ( 1 << ( n & 7 ? n & 7 : 1 ) ) - 1 ), MSG_ReadBits( &msg, n & 7 ? n & 7 : 1 ) );
		const int number = MSG_ReadBits( &msg, GENTITYNUM_BITS );
		ASSERT_EQ( to.number, number );
		MSG_ReadDeltaEntity( &msg, &from, &got, number );
		EXPECT_EQ( 0, memcmp( &to, &got, sizeof( to ) ) ) << "entity " << n;
		MSG_ReadDeltaPlayerstate( &msg, &psFrom, &psGot );
		EXPECT_EQ( 0, memcmp( &psTo, &psGot, sizeof( psTo ) ) ) << "player state " << n;
		EXPECT_STREQ( realStrings[n % ARRAY_LEN( realStrings )], MSG_ReadString( &msg ) );
		EXPECT_LE( msg.readcount, msg.cursize );
	}
}

// ==============================================================================
// Adaptive Huffman (the connect packet): unchanged, without the shared bit
// position
// ==============================================================================

// an empty adaptive tree, like Huff_Compress and Huff_Decompress start with
static void InitAdaptive( huff_t *huff ) {
	memset( huff, 0, sizeof( *huff ) );
	huff->tree = huff->lhead = huff->ltail = huff->loc[NYT] = &huff->nodeList[huff->blocNode++];
	huff->tree->symbol = NYT;
}

TEST(HuffmanAdaptive, TransmitAndReceiveMatchOldCode) {
	// they took the bit position from the shared 'bloc' and now take it as
	// an argument
	std::unique_ptr<huff_t> encA( new huff_t ), encB( new huff_t ), decA( new huff_t ), decB( new huff_t );
	std::vector<byte> a( 16384 ), b( 16384 );
	std::vector<int> symbols;
	Rng r( 20 );
	int offB = 16;

	InitAdaptive( encA.get() );
	InitAdaptive( encB.get() );
	InitAdaptive( decA.get() );
	InitAdaptive( decB.get() );
	orig::bloc = 16;
	for ( int i = 0; i < 4000; i++ ) {
		const int ch = ( r.Next() & 3 ) ? r.Range( 'a', 'z' ) : r.Range( 0, 255 );

		symbols.push_back( ch );
		orig::Huff_transmit( encA.get(), ch, a.data() );
		Huff_transmit( encB.get(), ch, b.data(), &offB );
		ASSERT_EQ( orig::bloc, offB ) << "symbol " << i;
		orig::Huff_addRef( encA.get(), (byte)ch );
		Huff_addRef( encB.get(), (byte)ch );
	}
	ASSERT_EQ( 0, memcmp( a.data(), b.data(), a.size() ) );

	orig::bloc = offB = 16;
	for ( size_t i = 0; i < symbols.size(); i++ ) {
		int chA = -1, chB = -1;

		orig::Huff_Receive( decA->tree, &chA, a.data() );
		Huff_Receive( decB->tree, &chB, b.data(), &offB );
		ASSERT_EQ( chA, chB ) << "symbol " << i;
		if ( chA == NYT ) {
			chA = chB = 0;
			for ( int bit = 0; bit < 8; bit++ ) {
				chA = ( chA << 1 ) + orig::get_bit( a.data() );
				chB = ( chB << 1 ) + Huff_getBit( b.data(), &offB );
			}
		}
		ASSERT_EQ( symbols[i], chA ) << "symbol " << i;
		ASSERT_EQ( chA, chB ) << "symbol " << i;
		ASSERT_EQ( orig::bloc, offB ) << "symbol " << i;
		orig::Huff_addRef( decA.get(), (byte)chA );
		Huff_addRef( decB.get(), (byte)chB );
	}
}

TEST(HuffmanAdaptive, CompressAndDecompressMatchOldCode) {
	static byte a[MAX_MSGLEN * 2], b[MAX_MSGLEN * 2];
	Rng r( 16 );

	for ( int n = 0; n < 60; n++ ) {
		const int size = r.Range( 1, 1500 );
		msg_t ma, mb;

		memset( &ma, 0, sizeof( ma ) );
		ma.data = a;
		ma.maxsize = sizeof( a );
		ma.cursize = size + 12;
		if ( n & 1 ) {
			FillRandom( r, a, ma.cursize );
		} else {
			// a connect string: text
			for ( int i = 0; i < ma.cursize; i++ ) {
				a[i] = realStrings[0][( i + n ) % strlen( realStrings[0] )];
			}
		}
		memcpy( b, a, sizeof( b ) );
		mb = ma;
		mb.data = b;

		orig::Huff_Compress( &ma, 12 );
		Huff_Compress( &mb, 12 );
		ASSERT_EQ( ma.cursize, mb.cursize );
		ASSERT_EQ( 0, memcmp( a, b, ma.cursize ) );

		// each decompresses the other's output
		std::vector<byte> original( a, a + ma.cursize );
		orig::Huff_Decompress( &mb, 12 );
		Huff_Decompress( &ma, 12 );
		ASSERT_EQ( ma.cursize, mb.cursize );
		ASSERT_EQ( 0, memcmp( a, b, ma.cursize ) );

		// damaged and truncated input decompress the same too
		for ( int damage = 0; damage < 3; damage++ ) {
			memcpy( a, original.data(), original.size() );
			if ( damage == 0 ) {
				FillRandom( r, a + 14, std::min( 50, (int)original.size() - 14 ) );
			} else if ( damage == 1 ) {
				FillRandom( r, a + 12, 2 );	// the size
			}
			memcpy( b, a, sizeof( b ) );
			ma.cursize = mb.cursize = damage == 2 ? r.Range( 12, (int)original.size() ) : (int)original.size();
			ma.maxsize = mb.maxsize = r.Range( 16, sizeof( a ) );
			orig::Huff_Decompress( &ma, 12 );
			Huff_Decompress( &mb, 12 );
			ASSERT_EQ( ma.cursize, mb.cursize );
			ASSERT_EQ( 0, memcmp( a, b, sizeof( a ) ) );
		}
	}
}

// ==============================================================================
// Benchmarks: the old code against the tables, each the best of several runs
// in turn (other programs share the machine)
// ==============================================================================

namespace {

// write the calls with MSG_WriteBits and read them back with MSG_ReadBits,
// pass 0 with the old code and pass 1 with the engine
void BenchBitOps( const std::vector<BitOp> &ops, int runs, double write[2], double read[2], int *size ) {
	huffman_t *ref = ReferenceTrees();
	static byte buffer[1 << 20];
	volatile int sink = 0;
	msg_t msg;

	write[0] = write[1] = read[0] = read[1] = 1e30;
	MSG_Init( &msg, buffer, sizeof( buffer ) );	// builds the engine trees
	for ( int run = 0; run < runs; run++ ) {
		for ( int pass = 0; pass < 2; pass++ ) {
			std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
			int sum = 0;

			MSG_Init( &msg, buffer, sizeof( buffer ) );
			if ( pass ) {
				for ( size_t i = 0; i < ops.size(); i++ ) {
					MSG_WriteBits( &msg, ops[i].value, ops[i].bits );
				}
			} else {
				for ( size_t i = 0; i < ops.size(); i++ ) {
					orig::MSG_WriteBits( ref, &msg, ops[i].value, ops[i].bits );
				}
			}
			write[pass] = std::min( write[pass], Seconds( start ) );
			ASSERT_FALSE( msg.overflowed );

			start = std::chrono::steady_clock::now();
			MSG_BeginReading( &msg );
			if ( pass ) {
				for ( size_t i = 0; i < ops.size(); i++ ) {
					sum += MSG_ReadBits( &msg, ops[i].bits );
				}
			} else {
				for ( size_t i = 0; i < ops.size(); i++ ) {
					sum += orig::MSG_ReadBits( ref, &msg, ops[i].bits );
				}
			}
			read[pass] = std::min( read[pass], Seconds( start ) );
			sink += sum;
		}
	}
	*size = msg.cursize;
	(void)sink;
}

} // namespace

TEST(HuffmanBenchmark, MessageBitsSpeedup) {
	// the field widths and values of snapshots: mostly 1-bit "unchanged" flags
	Rng r( 17 );
	const std::vector<BitOp> ops = RealLookingOps( r, 400 );
	const double ns = 1e9 / ops.size();
	double write[2], read[2];
	int size;

	BenchBitOps( ops, 15, write, read, &size );
	printf( "[ BENCH    ] %d snapshot-like calls, %d KB of Huffman data\n", (int)ops.size(), size / 1024 );
	printf( "[ BENCH    ] MSG_WriteBits: %.2f -> %.2f ns per call (%.2fx)\n", write[0] * ns, write[1] * ns, write[0] / write[1] );
	printf( "[ BENCH    ] MSG_ReadBits:  %.2f -> %.2f ns per call (%.2fx)\n", read[0] * ns, read[1] * ns, read[0] / read[1] );
}

TEST(HuffmanBenchmark, WidthSpeedup) {
	static const int widths[] = { 1, 4, 8, 13, 16, 32 };
	Rng r( 18 );

	for ( int w = 0; w < (int)ARRAY_LEN( widths ); w++ ) {
		std::vector<BitOp> ops;
		double write[2], read[2];
		int size;

		for ( int i = 0; i < 100000; i++ ) {
			AddValue( r, ops, widths[w] );
		}
		const double ns = 1e9 / ops.size();
		BenchBitOps( ops, 9, write, read, &size );
		printf( "[ BENCH    ] %2d bits: write %.2f -> %.2f ns (%.2fx), read %.2f -> %.2f ns (%.2fx)\n", widths[w],
			write[0] * ns, write[1] * ns, write[0] / write[1], read[0] * ns, read[1] * ns, read[0] / read[1] );
	}
}

TEST(HuffmanBenchmark, SymbolSpeedup) {
	huffman_t *ref = ReferenceTrees();
	std::unique_ptr<huffTables_t> tables( new huffTables_t );
	Rng r( 19 );
	const int count = 1 << 20;
	std::vector<byte> symbols( count );
	std::vector<byte> out( count * 2 + 16 );
	volatile int sink = 0;
	double t[4] = { 1e30, 1e30, 1e30, 1e30 };

	// bytes with the netchan frequencies
	{
		std::vector<int> cumulative( 256 );
		int total = 0;
		for ( int i = 0; i < 256; i++ ) {
			total += hData[i];
			cumulative[i] = total;
		}
		for ( int i = 0; i < count; i++ ) {
			const int pick = (int)( r.Next() % (uint32_t)total );
			symbols[i] = (byte)( std::upper_bound( cumulative.begin(), cumulative.end(), pick ) - cumulative.begin() );
		}
	}
	Huff_BuildTables( tables.get(), &ref->compressor, &ref->decompressor );

	for ( int run = 0; run < 7; run++ ) {
		for ( int pass = 0; pass < 2; pass++ ) {
			std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
			int offset = 0, sum = 0;

			if ( pass ) {
				for ( int i = 0; i < count; i++ ) {
					Huff_offsetTransmitTable( tables.get(), symbols[i], out.data(), &offset );
				}
			} else {
				for ( int i = 0; i < count; i++ ) {
					orig::Huff_offsetTransmit( &ref->compressor, symbols[i], out.data(), &offset );
				}
			}
			t[pass] = std::min( t[pass], Seconds( start ) );

			const int size = ( offset >> 3 ) + 1;
			offset = 0;
			start = std::chrono::steady_clock::now();
			for ( int i = 0; i < count; i++ ) {
				int ch;
				if ( pass ) {
					Huff_offsetReceiveTable( tables.get(), &ch, out.data(), &offset, size );
				} else {
					orig::Huff_offsetReceive( ref->decompressor.tree, &ch, out.data(), &offset );
				}
				sum += ch;
			}
			t[2 + pass] = std::min( t[2 + pass], Seconds( start ) );
			sink += sum;
		}
	}

	printf( "[ BENCH    ] %d symbols: transmit %.1f -> %.1f ns (%.2fx), receive %.1f -> %.1f ns (%.2fx)\n", count,
		t[0] * 1e9 / count, t[1] * 1e9 / count, t[0] / t[1], t[2] * 1e9 / count, t[3] * 1e9 / count, t[2] / t[3] );
	(void)sink;
}
