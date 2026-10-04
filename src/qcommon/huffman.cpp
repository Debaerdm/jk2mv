
/* This is based on the Adaptive Huffman algorithm described in Sayood's Data
 * Compression book.  The ranks are not actually stored, but implicitly defined
 * by the location of a node within a doubly-linked list */

#include "../qcommon/q_shared.h"
#include "qcommon.h"

/* The bit position is always passed by the caller: there is no shared state,
 * so separate messages can be encoded at the same time. */

void	Huff_putBit( int bit, byte *fout, int *offset) {
	int bloc = *offset;
	if ((bloc&7) == 0) {
		fout[(bloc>>3)] = 0;
	}
	fout[(bloc>>3)] |= bit << (bloc&7);
	bloc++;
	*offset = bloc;
}

int		Huff_getBit( byte *fin, int *offset) {
	int t;
	int bloc = *offset;
	t = (fin[(bloc>>3)] >> (bloc&7)) & 0x1;
	bloc++;
	*offset = bloc;
	return t;
}

/* Add a bit to the output file (buffered) */
static void add_bit (char bit, byte *fout, int *bloc) {
	if ((*bloc&7) == 0) {
		fout[(*bloc>>3)] = 0;
	}
	fout[(*bloc>>3)] |= bit << (*bloc&7);
	(*bloc)++;
}

/* Receive one bit from the input file (buffered) */
static int get_bit (byte *fin, int *bloc) {
	int t;
	t = (fin[(*bloc>>3)] >> (*bloc&7)) & 0x1;
	(*bloc)++;
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
int Huff_Receive (node_t *node, int *ch, byte *fin, int *offset) {
	while (node && node->symbol == INTERNAL_NODE) {
		if (get_bit(fin, offset)) {
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
	int bloc = *offset;
	while (node && node->symbol == INTERNAL_NODE) {
		if (get_bit(fin, &bloc)) {
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
static void send(node_t *node, node_t *child, byte *fout, int *bloc) {
	if (node->parent) {
		send(node->parent, node, fout, bloc);
	}
	if (child) {
		if (node->right == child) {
			add_bit(1, fout, bloc);
		} else {
			add_bit(0, fout, bloc);
		}
	}
}

/* Send a symbol */
void Huff_transmit (huff_t *huff, int ch, byte *fout, int *offset) {
	int i;
	if (huff->loc[ch] == NULL) {
		/* node_t hasn't been transmitted, send a NYT, then the symbol */
		Huff_transmit(huff, NYT, fout, offset);
		for (i = 7; i >= 0; i--) {
			add_bit((char)((ch >> i) & 0x1), fout, offset);
		}
	} else {
		send(huff->loc[ch], NULL, fout, offset);
	}
}

void Huff_offsetTransmit (huff_t *huff, int ch, byte *fout, int *offset) {
	int bloc = *offset;
	send(huff->loc[ch], NULL, fout, &bloc);
	*offset = bloc;
}

void Huff_Decompress(msg_t *mbuf, int offset) {
	int			ch, cch, i, j, size, bloc;
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
		Huff_Receive(huff.tree, &ch, buffer, &bloc);		/* Get a character */
		if ( ch == NYT ) {								/* We got a NYT, get the symbol associated with it */
			ch = 0;
			for ( i = 0; i < 8; i++ ) {
				ch = (ch<<1) + get_bit(buffer, &bloc);
			}
		}

		seq[j] = ch;									/* Write symbol */

		Huff_addRef(&huff, (byte)ch);								/* Increment node */
	}
	mbuf->cursize = cch + offset;
	Com_Memcpy(mbuf->data + offset, seq, cch);
}

extern	int oldsize;

void Huff_Compress(msg_t *mbuf, int offset) {
	int			i, ch, size, bloc;
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
		Huff_transmit(&huff, ch, seq, &bloc);				/* Transmit symbol */
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


/* Static code tables
 *
 * The netchan tree is built once by MSG_initHuffman and never adapts
 * afterwards, so a symbol always has the same code. Huff_BuildTables reads
 * the codes off the trees: the code of each symbol for the compressor, and
 * the symbol at each value of the next HUFF_LOOKUP_BITS bits for the
 * decompressor. Sending or receiving a symbol is then a table lookup and a
 * few byte writes instead of a walk of the tree one bit at a time, with the
 * same bits as the tree functions. What the tables can't hold (a missing
 * symbol, a code longer than HUFF_MAX_TABLE_CODE bits or, for the decoder,
 * than HUFF_LOOKUP_BITS bits, a broken tree) goes through the tree functions,
 * and so does zeroed memory: tables that were never built. */

#define HUFF_MAX_TABLE_CODE		25		/* a code shifted by 7 bits still fits in 32 bits */
#define HUFF_LOOKUP_MASK		((1 << HUFF_LOOKUP_BITS) - 1)
#define HUFF_LOOKUP_SYMBOL(e)	((e) & 511)
#define HUFF_LOOKUP_LENGTH(e)	((e) >> 9)

void Huff_BuildTables( huffTables_t *tables, huff_t *compressor, huff_t *decompressor ) {
	int ch, i, length;
	uint32_t code;
	const node_t *node;

	Com_Memset( tables, 0, sizeof( *tables ) );
	tables->compressor = compressor;
	tables->decompressor = decompressor;

	/* Codes go up from the leaf like send(), which sends the bit of the
	 * topmost node first: it ends up in bit 0 */
	for ( ch = 0; ch <= HMAX; ch++ ) {
		node = compressor->loc[ch];
		if ( !node ) {
			continue;
		}
		code = 0;
		length = 0;
		while ( node->parent && length <= HUFF_MAX_TABLE_CODE ) {
			code = ( code << 1 ) | ( node->parent->right == node );
			node = node->parent;
			length++;
		}
		if ( length > HUFF_MAX_TABLE_CODE ) {
			continue;
		}
		/* a length of 0 (the leaf is the root) stays on the tree path,
		 * which sends nothing either */
		tables->code[ch] = code;
		tables->codeLength[ch] = (byte)length;
	}

	/* Walk the decompressor tree like Huff_offsetReceive, reading the bits
	 * of the index first bit first */
	for ( i = 0; i < ( 1 << HUFF_LOOKUP_BITS ); i++ ) {
		node = decompressor->tree;
		length = 0;
		while ( node && node->symbol == INTERNAL_NODE && length < HUFF_LOOKUP_BITS ) {
			node = ( ( i >> length ) & 1 ) ? node->right : node->left;
			length++;
		}
		if ( !node || node->symbol == INTERNAL_NODE || length == 0 ||
			node->symbol < 0 || node->symbol > HMAX ) {
			continue;	/* stays 0: longer code, missing child or odd symbol */
		}
		tables->lookup[i] = (uint16_t)( node->symbol | ( length << 9 ) );
	}
}

/* Write the low 'bits' bits of value (0 to 25), first bit in bit 0, exactly
 * like that many Huff_putBit calls: a byte is cleared when its bit 0 is
 * written and bytes past the last bit aren't touched */
static Q_INLINE void put_bits( uint32_t value, int bits, byte *fout, int *offset ) {
	int		bloc = *offset;
	int		shift = bloc & 7;
	int		end = shift + bits;
	byte	*out = fout + ( bloc >> 3 );

	if ( bits <= 0 ) {
		return;
	}
	value <<= shift;
	if ( shift ) {
		*out |= (byte)value;
	} else {
		*out = (byte)value;
	}
	for ( shift = 8; shift < end; shift += 8 ) {
		*++out = (byte)( value >> shift );
	}
	*offset = bloc + bits;
}

void Huff_putBits( int value, int bits, byte *fout, int *offset ) {
	put_bits( (uint32_t)value & ( ( 1u << bits ) - 1 ), bits, fout, offset );
}

/* Read 'bits' bits (0 to 25) like that many Huff_getBit calls, the first one
 * in bit 0, from the same bytes */
int Huff_getBits( int bits, byte *fin, int *offset ) {
	int			bloc = *offset;
	int			have = 8 - ( bloc & 7 );
	const byte	*in = fin + ( bloc >> 3 );
	uint32_t	value;

	if ( bits <= 0 ) {
		return 0;
	}
	value = *in >> ( bloc & 7 );
	while ( have < bits ) {
		value |= (uint32_t)*++in << have;
		have += 8;
	}
	*offset = bloc + bits;
	return (int)( value & ( ( 1u << bits ) - 1 ) );
}

/* Huff_offsetTransmit with the compressor the tables were built from */
void Huff_offsetTransmitTable( const huffTables_t *tables, int ch, byte *fout, int *offset ) {
	if ( (unsigned)ch <= HMAX && tables->codeLength[ch] ) {
		put_bits( tables->code[ch], tables->codeLength[ch], fout, offset );
	} else {
		Huff_offsetTransmit( tables->compressor, ch, fout, offset );
	}
}

/* Huff_offsetReceive from the root of the decompressor the tables were built
 * from. The lookup reads the three bytes from the current one, so it is only
 * used when they are all below finSize; near the end of the data, the tree
 * reads exactly the bytes of the code, like it always did. */
void Huff_offsetReceiveTable( const huffTables_t *tables, int *ch, byte *fin, int *offset, int finSize ) {
	int bloc = *offset;
	int pos = bloc >> 3;

	if ( bloc >= 0 && pos + 3 <= finSize ) {
		const byte *in = fin + pos;
		uint32_t window = ( in[0] | ( in[1] << 8 ) | ( in[2] << 16 ) ) >> ( bloc & 7 );
		int entry = tables->lookup[window & HUFF_LOOKUP_MASK];

		if ( entry ) {
			*ch = HUFF_LOOKUP_SYMBOL( entry );
			*offset = bloc + HUFF_LOOKUP_LENGTH( entry );
			return;
		}
	}
	Huff_offsetReceive( tables->decompressor ? tables->decompressor->tree : NULL, ch, fin, offset );
}
