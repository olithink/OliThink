/* OliThink - Bitboard Magic Move (c) Oliver Brausch 08.Jan.2008, ob112@web.de */
#define VER "5.0.4"
#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <sys/timeb.h>
struct _timeb tv;
#else
#include <sys/time.h>
struct timeval tv;
struct timezone tz;
#endif
typedef unsigned long long u64;
typedef unsigned long u32;
typedef int Move;

#define PAWN 1
#define KNIGHT 2
#define KING 3
#define ENP 4
#define BISHOP 5
#define ROOK 6
#define QUEEN 7

#define HEUR 9900000
const int capval[] = {0, HEUR+2, HEUR+3, 0, HEUR+2, HEUR+3, HEUR+4, HEUR+5};
const int pval[] = {0, 100, 300, 0, 100, 300, 500, 950};

#define FROM(x) ((x) & 63)
#define TO(x) (((x) >> 6) & 63)
#define PROM(x) (((x) >> 12) & 7)
#define PIECE(x) (((x) >> 15) & 7)  
#define ONMV(x) (((x) >> 18) & 1)
#define CAP(x) (((x) >> 19) & 7)

#define _TO(x) ((x) << 6)
#define _PROM(x) ((x) << 12)
#define _PIECE(x) ((x) << 15)
#define _ONMV(x) ((x) << 18)
#define _CAP(x) ((x) << 19)
#define PREMOVE(f, p) ((f) | _ONMV(c) | _PIECE(p))

#define RATT1(f) rays[((f) << 7) | key000(board, f)]
#define RATT2(f) rays[((f) << 7) | key090(board, f) | 0x2000]
#define BATT3(f) rays[((f) << 7) | key045(board, f) | 0x4000]
#define BATT4(f) rays[((f) << 7) | key135(board, f) | 0x6000]
#define RXRAY1(f) rays[((f) << 7) | key000(board, f) | 0x8000]
#define RXRAY2(f) rays[((f) << 7) | key090(board, f) | 0xA000]
#define BXRAY3(f) rays[((f) << 7) | key045(board, f) | 0xC000]
#define BXRAY4(f) rays[((f) << 7) | key135(board, f) | 0xE000]

#define ROCC1(f) (RATT1(f) & board)
#define ROCC2(f) (RATT2(f) & board)
#define BOCC3(f) (BATT3(f) & board)
#define BOCC4(f) (BATT4(f) & board)
#define RMOVE1(f) (RATT1(f) & (~board))
#define RMOVE2(f) (RATT2(f) & (~board))
#define BMOVE3(f) (BATT3(f) & (~board))
#define BMOVE4(f) (BATT4(f) & (~board))
#define RCAP1(f, c) (RATT1(f) & colorb[(c)^1])
#define RCAP2(f, c) (RATT2(f) & colorb[(c)^1])
#define BCAP3(f, c) (BATT3(f) & colorb[(c)^1])
#define BCAP4(f, c) (BATT4(f) & colorb[(c)^1])
#define ROCC(f) (ROCC1(f) | ROCC2(f))
#define BOCC(f) (BOCC3(f) | BOCC4(f))
#define RMOVE(f) (RMOVE1(f) | RMOVE2(f))
#define BMOVE(f) (BMOVE3(f) | BMOVE4(f))
#define RCAP(f, c) (ROCC(f) & colorb[(c)^1])
#define BCAP(f, c) (BOCC(f) & colorb[(c)^1])

#define SHORTMOVE(x) ((x) & ((x)^board))
#define SHORTOCC(x) ((x) & board)
#define SHORTCAP(x, c) ((x) & colorb[(c)^1])

#define NMOVE(x) (SHORTMOVE(nmoves[x]))
#define KMOVE(x) (SHORTMOVE(kmoves[x]))
#define PMOVE(x, c) (pmoves[(x) | ((c)<<6)] & (~board))
#define NOCC(x) (SHORTOCC(nmoves[x]))
#define KOCC(x) (SHORTOCC(kmoves[x]))
#define POCC(x, c) (pcaps[(x) | ((c)<<6)] & board)
#define NCAP(x, c) (SHORTCAP(nmoves[x], (c)))
#define KCAP(x, c) (SHORTCAP(kmoves[x], (c)))
#define PCAP(x, c) (pcaps[(x) | ((c)<<6)] & colorb[(c)^1])
#define PCA3(x, c) (pcaps[(x) | ((c)<<6) | 128] & (colorb[(c)^1] | ((BIT[ENPASS]) & (c ? 0xFF0000LL : 0xFF0000000000LL))))
#define PCA4(x, c) (pcaps[(x) | ((c)<<6) | 256] & (colorb[(c)^1] | ((BIT[ENPASS]) & (c ? 0xFF0000LL : 0xFF0000000000LL))))

#define RANK(x, y) (((x) & 0x38) == (y))
#define TEST(f, b) (BIT[f] & (b))
#define ENPASS (flags & 63)
#define CASTLE (flags & 960)

#define HASHP (hashb ^ hashxor[flags | 1024 | c << 11])
#define HASHB(d) ((hashb ^ hashxor[flags | 1024]) ^ hashxor[c | (d) << 1 | 2048])
#define HSIZE 0x800000
#define HMASK 0x7FFFFF
#define HINV 0xFFFFFFFFFF800000LL
u64 hashDB[HSIZE];
u64 hashDP[HSIZE];
u64 hashDQ[HSIZE];
u64 hashb;
u64 hstack[0x800];
int count;
int flags;

static u64 rays[0x10000];
u64 nmoves[64];
u64 n2moves[64];
u64 kmoves[64];
u64 pmoves[128];
u64 pcaps[384];
int _knight[8] = {-17,-10,6,15,17,10,-6,-15};
int _king[8] = {-9,-1,7,8,9,1,-7,-8};
u64 pieceb[8];
u64 colorb[2];
u64 board;
static u64 BIT[64];
static u64 hashxor[4096];
static char LSB[0x10000];
static char BITC[0x10000] ;      
int crevoke[64];
int nmobil[64];
int kmobil[64];
int onmove;
Move movelist[128*256];
int movenum[128];
int kingpos[2];
Move pv[128][128];
int pvlength[128];
const char pieceChar[] = "*PNK.BRQ";

void setBit(int f, u64 *board) {
	*board |= BIT[f];
}

void xorBit(int f, u64 *board) {
	*board ^= BIT[f];
}

u64 getLowestBit(u64 bb) {
	return bb & (-(long long)bb);
}

void _parse_fen(char *fen) {
	char c, mv, pos[128], cas[4], enps[2];
	int i, j = 0, halfm = 0, fullm = 1, col = 0, row = 7;
	for (i = 0; i < 8; i++) {
		pieceb[i] = 0LL;
	}
	colorb[0] = colorb[1] = hashb = 0LL;
	sscanf(fen, "%s %c %s %s %d %d", pos, &mv, cas, enps, &halfm, &fullm);
	while ((c = pos[j++])) {
		if (c == '/') {
			row--;
			col = 0;
		} else if (c >= '1' && c <= '8') {
			col += c - '0';
		} else for (i = 1; i < 8; i++) {
			if (pieceChar[i] == c) {
				if (i == KING) kingpos[0] = row*8 + col;
				hashb ^= hashxor[col | row << 3 | i << 6];
				setBit(row*8 + col, pieceb+i);
				setBit(row*8 + (col++), colorb);
				break;
			} else if (pieceChar[i] == c - 32) {
				if (i == KING) kingpos[1] = row*8 + col;
				hashb ^= hashxor[col | row << 3 | i << 6 | 512];
				setBit(row*8 + col, pieceb+i);
				setBit(row*8 + (col++), colorb+1);
				break;
			}
		}
	}
	onmove = mv == 'b' ? 1 : 0;
	flags = j = 0;
	while ((c = cas[j++])) {
		if (c == 'K') flags |= BIT[6];
		if (c == 'k') flags |= BIT[7];
		if (c == 'Q') flags |= BIT[8];
		if (c == 'q') flags |= BIT[9];
	}
	if (enps[0] != '-') {
		flags |= 8*(enps[1] - '1') + enps[0] - 'a'; 
	}
	board = colorb[0] | colorb[1];
	count = (fullm - 1)*2 + onmove + (halfm << 11);
	for (i = 0; i < (count & 0x7FF); i++) hstack[i] = 0LL;
}

static u32 r_x = 30903, r_y = 30903, r_z = 30903, r_w = 30903, r_carry = 0;
u32 _rand_32() {
	u32 t;
	r_x = r_x * 69069 + 1;
	r_y ^= r_y << 13;
	r_y ^= r_y >> 17;
	r_y ^= r_y << 5;
	t = (r_w << 1) + r_z + r_carry;
	r_carry = ((r_z >> 2) + (r_w >> 3) + (r_carry >> 2)) >> 30;
	r_z = r_w;
	r_w = t;
	return r_x + r_y + r_w;
}

u64 _rand_64() { u64 c = _rand_32(); return _rand_32() | (c << 32); }

u64 getTime() {
#ifdef _WIN32
	_ftime(&tv);
	return(tv.time * 1000LL + tv.millitm);
#else
	gettimeofday (&tv, &tz);
	return(tv.tv_sec * 1000LL + (tv.tv_usec / 1000));
#endif
}

char getLsb(u64 bm) {
	u32 n = (u32) (bm & 0xFFFFFFFF);
	if (n) {
		if (n & 0xFFFF) return LSB[n & 0xFFFF];
		else return 16 | LSB[(n >> 16) & 0xFFFF];
	} else {
		n = (u32)(bm >> 32);
		if (n & 0xFFFF) return 32 | LSB[n & 0xFFFF];
		else return 48 | LSB[(n >> 16) & 0xFFFF];
	}
}

char _slow_lsb(u64 bm) {
	int k = -1;
	while (bm) { k++; if (bm & 1) break; bm >>= 1; }
	return (char)k;
}

int pullLsb(u64* bit) {
	int f = getLsb(*bit);
	*bit ^= BIT[f];
	return f;
}

int _bitcnt(u64 bit) {
	int c = 0;
	while (bit) { bit &= (bit - 1); c++; }
	return c;
}

int bitcnt (u64 n) {    
     return BITC[n         & 0xFFFF]
         +  BITC[(n >> 16) & 0xFFFF]
         +  BITC[(n >> 32) & 0xFFFF]
         +  BITC[(n >> 48) & 0xFFFF];
}

int identPiece(int f) {
	if (TEST(f, pieceb[PAWN])) return PAWN;
	if (TEST(f, pieceb[KNIGHT])) return KNIGHT;
	if (TEST(f, pieceb[BISHOP])) return BISHOP;
	if (TEST(f, pieceb[ROOK])) return ROOK;
	if (TEST(f, pieceb[QUEEN])) return QUEEN;
	if (TEST(f, pieceb[KING])) return KING;
	return ENP;
}

int identColor(int f) {
	if (TEST(f, colorb[1])) return 1;
	return 0;
}

u64 bmask45[64];
u64 bmask135[64];
int key000(u64 b, int f) {
	return (int) ((b >> (f & 56)) & 0x7E);
}

#if defined(_WIN64) || defined(_LP64)
int key090(u64 b, int f) {
	u64 _b = (b >> (f&7)) & 0x0101010101010101LL;
	_b = _b * 0x0080402010080400LL;
	return (int)(_b >> 57);
}

int keyDiag(u64 _b) {
	_b = _b * 0x0202020202020202LL;
	return (int)(_b >> 57);
}
#else
int key090(u64 b, int f) {
	int h;
	b = b >> (f&7);
	h = (int)((b & 0x1010101) | ((b >> 31) & 0x2020202));
	h = (h & 0x303) | ((h >> 14) & 0xC0C);
	return (h & 0xE) | ((h >> 4) & 0x70);
}

int keyDiag(u64 _b) {
   int h = (int)(_b | _b >> 32);
   h |= h >> 16;
   h |= h >>  8;
   return h & 0x7E;
}
#endif

int key045(u64 b, int f) {
   return keyDiag(b & bmask45[f]);
}

int key135(u64 b, int f) {
   return keyDiag(b & bmask135[f]);
}

#define DUALATT(x, y, c) (battacked(x, c) || battacked(y, c))
#define RQU (pieceb[ROOK] | pieceb[QUEEN])
#define BQU (pieceb[BISHOP] | pieceb[QUEEN])

int battacked(int f, int c) {
	if (PCAP(f, c) & pieceb[PAWN]) return 1;
	if (NCAP(f, c) & pieceb[KNIGHT]) return 1;
	if (KCAP(f, c) & pieceb[KING]) return 1;
	if (RCAP1(f, c) & RQU) return 1; 
	if (RCAP2(f, c) & RQU) return 1; 
	if (BCAP3(f, c) & BQU) return 1;
	if (BCAP4(f, c) & BQU) return 1;
	return 0;
}

u64 reach(int f, int c) {
	return (NCAP(f, c) & pieceb[KNIGHT])
		| (RCAP1(f, c) & RQU)
		| (RCAP2(f, c) & RQU)
		| (BCAP3(f, c) & BQU)
		| (BCAP4(f, c) & BQU);
}

u64 attacked(int f, int c) {
	return (PCAP(f, c) & pieceb[PAWN]) | reach(f, c);
}

void _init_pawns(u64* moves, u64* caps, int c) {
	int i;
	for (i = 0; i < 64; i++) {
		int m = i + (c ? -8 : 8);
		if (m < 0 || m > 63) continue;
		setBit(m, moves + i);
		if ((i&7) > 0) {
			m = i + (c ? -9 : 7);
			if (m < 0 || m > 63) continue;
			setBit(m, caps + i);
			setBit(m, caps + i + 128*(2 - c));
		}
		if ((i&7) < 7) {
			m = i + (c ? -7 : 9);
			if (m < 0 || m > 63) continue;
			setBit(m, caps + i);
			setBit(m, caps + i + 128*(c + 1));
		}
	}
}

void _init_shorts(u64* moves, int* m) {
	int i, j, n;
	for (i = 0; i < 64; i++) {
		for (j = 0; j < 8; j++) {
			n = i + m[j];
			if (n < 64 && n >= 0 && ((n & 7)-(i & 7))*((n & 7)-(i & 7)) <= 4) {
				setBit(n, moves+i);
			}
		}
	}
}

u64 _occ_free_board(int bc, int del, u64 free) {
	u64 low, perm = free;
	int i;
	for (i = 0; i < bc; i++) {
		low = getLowestBit(free);
		free &= (~low);
		if (!TEST(i, del)) perm &= (~low);
	}
	return perm;
}

void _init_rays(u64* rays, u64 (*rayFunc) (int, u64, int), int (*key)(u64, int)) {
	int i, f, iperm, bc, index; 
	u64 board, mmask, occ, move, xray;
	for (f = 0; f < 64; f++) {
		mmask = (*rayFunc)(f, 0LL, 0) | BIT[f];
		iperm = 1 << (bc = bitcnt(mmask));
		for (i = 0; i < iperm; i++) {
			board = _occ_free_board(bc, i, mmask);
			move = (*rayFunc)(f, board, 1);
			occ = (*rayFunc)(f, board, 2);
			xray = (*rayFunc)(f, board, 3);
			index = (*key)(board, f);
			rays[(f << 7) + index] = occ | move;
			rays[(f << 7) + index + 0x8000] = xray;
		}
	}
}

#define RAYMACRO {if (TEST(i, board)) { if (b) { setBit(i, &xray); break; } else { setBit(i, &occ); b = 1; } } if (!b) setBit(i, &free);}
u64 _rook0(int f, u64 board, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f+1; i < 64 && i%8 != 0; i++) RAYMACRO
	for (b = 0, i = f-1; i >= 0 && i%8 != 7; i--) RAYMACRO
	return (t < 2) ? free : (t == 2 ? occ : xray);
}

u64 _rook90(int f, u64 board, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f-8; i >= 0; i-=8) RAYMACRO
	for (b = 0, i = f+8; i < 64; i+=8) RAYMACRO
	return (t < 2) ? free : (t == 2 ? occ : xray);
}

u64 _bishop45(int f, u64 board, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f+9; i < 64 && (i%8 != 0); i+=9) RAYMACRO
		for (b = 0, i = f-9; i >= 0 && (i%8 != 7); i-=9) RAYMACRO
			return (t < 2) ? free : (t == 2 ? occ : xray);
}

u64 _bishop135(int f, u64 board, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f-7; i >= 0 && (i%8 != 0); i-=7) RAYMACRO
		for (b = 0, i = f+7; i < 64 && (i%8 != 7); i+=7) RAYMACRO
			return (t < 2) ? free : (t == 2 ? occ : xray);
}

void display64(u64 bb) {
	int i, j;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			printf(" %d", TEST(j + (7-i)*8, bb) ? 1 : 0);
		} printf("\n");
	} printf("\n");
}

void displayb() {
	int i, j;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			int f = j + (7-i)*8;
			printf(" %c", pieceChar[identPiece(f)] + identColor(f)*32);
		} printf("\n");
	} printf("\n");
}

void displaym(Move m) {
	printf("%c%c%c%c%c", 'a' + FROM(m) % 8, '1' + FROM(m) / 8,
		CAP(m) == 0 ? '-' : 'x', 'a' + TO(m) % 8, '1' + TO(m) / 8);
	if (PROM(m)) printf("%c", pieceChar[PROM(m)]+32);
}

void displaypv() {
	int i;
	for (i = 0; i < pvlength[0]; i++) {
		displaym(pv[0][i]); printf(" ");
	}
}

int isDraw(u64 hp, int nrep) {
	if (count > 0xFFF) { //fifty > 1
		int i, c = 0, n = (count & 0x7FF) - (count >> 11);
		if (count >= 0x800*100) return 2;
		for (i = (count & 0x7FF) - 2; i >= n; i--) if (hstack[i] == hp) if (++c == nrep) return 1;
	}
	return 0;
}

u64 pinnedPieces(int f, int oc) {
	u64 pin = 0LL;
	u64 b = ((RXRAY1(f) | RXRAY2(f)) & colorb[oc]) & RQU;
	while (b) {
		int t = pullLsb(&b);
		pin |= RCAP(t, oc) & ROCC(f);
	}
	b = ((BXRAY3(f) | BXRAY4(f)) & colorb[oc]) & BQU;
	while (b) {
		int t = pullLsb(&b);
		pin |= BCAP(t, oc) & BOCC(f);
	}
	return pin;
}

char getDir(int f, int t) {
	if (!((f ^ t) & 56)) return 8;
	if (!((f ^ t) & 7)) return 16;
	return ((f - t) % 7) ? 32 : 64;
}

void countKing(u64 bm, int* mn, int c) {
	while (bm) {
		if (battacked(pullLsb(&bm), c)) (*mn)++;
	}
}

int eval1 = 0;
int eval2 = 0;
int evalc(int c, int* sf, int* mat, u64 pin) {
	int t, f;
	int mn = 0;
	u64 m, b, a, cb = colorb[c];
	u64 kn = kmoves[kingpos[c^1]];
	if (pin == -1LL) pin = pinnedPieces(kingpos[c], c^1);
		eval1++;
	if (pin)
		eval2++;

	cb = colorb[c] & (~pin);
	b = pieceb[PAWN] & cb;
	while (b) {
		*mat += 92 + (c ? 56 - (f & 56) : (f & 56));
		f = pullLsb(&b);
		m = PMOVE(f, c);
		a = POCC(f, c);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += _bitcnt(a | m) << 3;
	}

	b = pin & pieceb[PAWN]; 
	while (b) {
		*mat += 92 + (c ? 56 - (f & 56) : (f & 56));
		f = pullLsb(&b);
		t = getDir(f, kingpos[c]);
		if (t & 8) continue;
		m = a = 0LL;
		if (t & 16) {
			m = PMOVE(f, c);         
		} else if (t & 32) {
			a = PCA3(f, c);
		} else {
			a = PCA4(f, c);
		}
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += _bitcnt(a | m) << 3;
	}

	b = pieceb[KNIGHT] & cb;
	while (b) {
		*mat += 300;
		*sf += 1;
		f = pullLsb(&b);
		a = NOCC(f) | NMOVE(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += bitcnt(a) << 2;
	}

	b = pieceb[KNIGHT] & pin;
	while (b) {
		*mat += 300;
		*sf += 1;
		f = pullLsb(&b);
		a = NOCC(f) | NMOVE(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		*mat += 300;
		*sf += 1;
		f = pullLsb(&b);
		a = BATT3(f) | BATT4(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += bitcnt(a) << 2;
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		*mat += 500;
		*sf += 2;
		f = pullLsb(&b);
		a = RATT1(f) | RATT2(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += bitcnt(a) << 1;
	}

	b = pieceb[QUEEN] & cb;
	while (b) {
		*mat += 950;
		*sf += 4;
		f = pullLsb(&b);
		a = RATT1(f) | RATT2(f) | BATT3(f) | BATT4(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += bitcnt(a);
	}

	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 

	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		*mat += pval[p];
		if (p == BISHOP) {
			*sf += 1; 
			a = BATT3(f) | BATT4(f);
			if (a & kn) mn += _bitcnt(a & kn) << 4;
		} else if (p == ROOK) {
			*sf += 2; 
			a = RATT1(f) | RATT2(f);
			if (a & kn) mn += _bitcnt(a & kn) << 4;
		} else {
			*sf += 4;
			a = RATT1(f) | RATT2(f) | BATT3(f) | BATT4(f);
			if (a & kn) mn += _bitcnt(a & kn) << 4;
		}
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) mn += _bitcnt(RATT1(f));
		if ((t & 18) == 18) mn += _bitcnt(RATT2(f));
		if ((t & 33) == 33) mn += _bitcnt(BATT3(f));
		if ((t & 65) == 65) mn += _bitcnt(BATT4(f));
	}
	return mn;
}

int eval(int c, u64 pin) {
	int sf0 = 0, sf1 = 0, mat0 = 0, mat1 = 0;
	int ev0 = evalc(0, &sf0, &mat0, c ? -1LL : pin);
	int ev1 = evalc(1, &sf1, &mat1, c ? pin : -1LL);
	if (sf1 < 4) ev0 += kmobil[kingpos[0]];
	else if (sf1 > 4) ev0 -= kmobil[kingpos[0]];
	if (sf0 < 4) ev1 += kmobil[kingpos[1]];
	else if (sf0 > 4) ev1 -= kmobil[kingpos[1]];

	if (c) {
		if (battacked(kingpos[0], 0)) ev0 = 0;
	} else {
		if (battacked(kingpos[1], 1)) ev1 = 0;
	}

	return c ? mat1 - mat0 + (ev1 - ev0)*2 : mat0 - mat1 + (ev0 - ev1)*2;
}

#define XORHASH(f, p, c) hashb ^= hashxor[(f) | (p) << 6 | (c) << 9]
void doMove(Move m, int c) {
	int f = FROM(m);
	int t = TO(m);
	int p = PIECE(m);
	int a = CAP(m);

	xorBit(f, colorb+c);
	xorBit(f, pieceb+p);

	xorBit(t, colorb+c);
	xorBit(t, pieceb+p);
	XORHASH(f, p, c);
	XORHASH(t, p, c);

	flags &= 960;
	count += 0x801;
	if (a) {
		if (a == ENP) { // Enpassant Capture
			t = (t&7) | (f&56);
			a = PAWN;
		} else if (a == ROOK && CASTLE) { //Revoke castling rights.
			flags &= crevoke[t];
		}
		xorBit(t, pieceb+a);
		xorBit(t, colorb+(c^1));
		XORHASH(t, a, c^1);
		count &= 0x7FF;
	}
	if (p == PAWN) {
		if (((f^t)&8) == 0) flags |= f^24; //Enpassant
		else if ((t&56) == 0 || (t&56) == 56) {
			xorBit(t, pieceb+PAWN);
			xorBit(t, pieceb+PROM(m));
			XORHASH(t, PAWN, c);
			XORHASH(t, PROM(m), c);
		}
		count &= 0x7FF;
	} else if (p == KING) {
		if (kingpos[c] == f) kingpos[c] = t; else kingpos[c] = f;
		flags &= ~(320 << c); // Lose castling rights
		if (((f^t)&3) == 2) { // Castle
			if (t == 6) { f = 7; t = 5; }
			else if (t == 2) { f = 0; t = 3; }
			else if (t == 62) { f = 63; t = 61; }
			else { f = 56; t = 59; }
			xorBit(f, colorb+c);
			xorBit(f, pieceb+ROOK);
			xorBit(t, colorb+c);
			xorBit(t, pieceb+ROOK);
			XORHASH(f, ROOK, c);
			XORHASH(t, ROOK, c);
		}
	} else if (p == ROOK && CASTLE) {
		flags &= crevoke[f];
	}
	board = colorb[0] | colorb[1];
}

Move killer[128];
int history[0x1000];

void registerCaps(Move m, u64 bc, int* mlist, int* mn) {
	while (bc) {
		int t = pullLsb(&bc);
		mlist[(*mn)++] = m | _TO(t) | _CAP(identPiece(t));
	}
}

void registerMoves(Move m, u64 bc, u64 bm, int* mlist, int* mn) {
	while (bc) {
		int t = pullLsb(&bc);
		mlist[(*mn)++] = m | _TO(t) | _CAP(identPiece(t));
	}
	while (bm) {
		mlist[(*mn)++] = m | _TO(pullLsb(&bm));
	}
}

void registerProms(int f, int c, u64 bc, u64 bm, int* mlist, int* mn) {
	while (bc) {
		int t = pullLsb(&bc);
		Move m = f | _ONMV(c) | _PIECE(PAWN) | _TO(t) | _CAP(identPiece(t));
		mlist[(*mn)++] = m | _PROM(QUEEN);
		mlist[(*mn)++] = m | _PROM(KNIGHT);
		mlist[(*mn)++] = m | _PROM(ROOK);
		mlist[(*mn)++] = m | _PROM(BISHOP);
	}
	while (bm) {
		Move m = f | _ONMV(c) | _PIECE(PAWN) | _TO(pullLsb(&bm));
		mlist[(*mn)++] = m | _PROM(QUEEN);
		mlist[(*mn)++] = m | _PROM(KNIGHT);
		mlist[(*mn)++] = m | _PROM(ROOK);
		mlist[(*mn)++] = m | _PROM(BISHOP);
	}
}

void registerKing(Move m, u64 bc, u64 bm, int* mlist, int* mn, int c) {
	while (bc) {
		int t = pullLsb(&bc);
		if (battacked(t, c)) continue;
		mlist[(*mn)++] = m | _TO(t) | _CAP(identPiece(t));
	}
	while (bm) {
		int t = pullLsb(&bm);
		if (battacked(t, c)) continue;
		mlist[(*mn)++] = m | _TO(t);
	}
}

int generateCheckEsc(u64 ch, u64 apin, int c, int k, int *ml, int *mn) {
	u64 cc, fl;
	int d, bf = _bitcnt(ch);
	board ^= BIT[k];
	registerKing(PREMOVE(k, KING), KCAP(k, c), KMOVE(k), ml, mn, c);
	board ^= BIT[k];
	if (bf > 1) return bf; //Multicheck
	bf = getLsb(ch);

	cc = attacked(bf, c^1) & apin;  //Can we capture the checker?
	while (cc) {
		int cf = pullLsb(&cc);
		int p = identPiece(cf);
		if (p == PAWN && RANK(cf, c ? 0x08 : 0x30)) {
			registerProms(cf, c, ch, 0LL, ml, mn);
		} else {
			registerMoves(PREMOVE(cf, p), ch, 0LL, ml, mn);
		}
	}
	if (ENPASS && (ch & pieceb[PAWN])) { //Enpassant capture of attacking Pawn
		cc = PCAP(ENPASS, c^1) & pieceb[PAWN] & apin;
		while (cc) {
			int cf = pullLsb(&cc);
			registerMoves(PREMOVE(cf, PAWN), BIT[ENPASS], 0LL, ml, mn);
		}
	}
	if (ch & (nmoves[k] | kmoves[k])) return 1; // We can't move anything between!

	d = getDir(bf, k);
	if (d & 8) fl = RMOVE1(bf) & RMOVE1(k);
	else if (d & 16) fl = RMOVE2(bf) & RMOVE2(k);
	else if (d & 32) fl = BMOVE3(bf) & BMOVE3(k);
	else fl = BMOVE4(bf) & BMOVE4(k);

	while (fl) {
		int f = pullLsb(&fl);
		cc = reach(f, c^1) & apin;
		while (cc) {
			int cf = pullLsb(&cc);
			int p = identPiece(cf);
			registerMoves(PREMOVE(cf, p), 0LL, BIT[f], ml, mn);
		}
		bf = c ? f+8 : f-8;
		if (bf < 0 || bf > 63) continue;
		if (BIT[bf] & pieceb[PAWN] & colorb[c] & apin) {
			if (RANK(bf, c ? 0x08 : 0x30))
				registerProms(bf, c, 0LL, BIT[f], ml, mn);
			else
				registerMoves(PREMOVE(bf, PAWN), 0LL, BIT[f], ml, mn);
		}
		if (RANK(f, c ? 0x20 : 0x18) && (board & BIT[bf]) == 0 && (BIT[c ? f+16 : f-16] & pieceb[PAWN] & colorb[c] & apin))
			registerMoves(PREMOVE(c ? f+16 : f-16, PAWN), 0LL, BIT[f], ml, mn);
	}
	return 1;
}

#define GENERATE(c) generateMoves(attacked(kingpos[c], c), c, 0)
int generateMoves(u64 ch, int c, int ply) {
	int t, f = kingpos[c];
	int *mn = movenum + ply;
	int *ml = movelist + (ply << 8);
	u64 m, b, a, cb = colorb[c];
	u64 pin = pinnedPieces(f, c^1);
	*mn = 0;

	if (ch) return generateCheckEsc(ch, ~pin, c, f, ml, mn);
	registerKing(PREMOVE(f, KING), KCAP(f, c), KMOVE(f), ml, mn, c);

	cb = colorb[c] & (~pin);
	b = pieceb[PAWN] & cb;
	while (b) {
		f = pullLsb(&b);
		m = PMOVE(f, c);
		a = PCAP(f, c);
		if (m && RANK(f, c ? 0x30 : 0x08)) m |= PMOVE(c ? f-8 : f+8, c);
		if (RANK(f, c ? 0x08 : 0x30)) {
			registerProms(f, c, a, m, ml, mn);
		} else {
			if (ENPASS && (BIT[ENPASS] & pcaps[(f) | ((c)<<6)])) {
				u64 hh;
				int clbd = ENPASS^8;
				board ^= BIT[clbd];
				hh = ROCC1(f);
				if (!(hh & BIT[kingpos[c]]) || !(hh & colorb[c^1] & RQU)) {
					a = a | BIT[ENPASS];
				}
				board ^= BIT[clbd];
			}
			registerMoves(PREMOVE(f, PAWN), a, m, ml, mn);
		}
	}

	b = pin & pieceb[PAWN]; 
	while (b) {
		f = pullLsb(&b);
		t = getDir(f, kingpos[c]);
		if (t & 8) continue;
		m = a = 0LL;
		if (t & 16) {
			m = PMOVE(f, c);         
			if (m && RANK(f, c ? 0x30 : 0x08)) m |= PMOVE(c ? f-8 : f+8, c);
		} else if (t & 32) {
			a = PCA3(f, c);
		} else {
			a = PCA4(f, c);
		}
		if (RANK(f, c ? 0x08 : 0x30)) {
			registerProms(f, c, a, m, ml, mn);
		} else {
			registerMoves(PREMOVE(f, PAWN), a, m, ml, mn);
		}
	}

	b = pieceb[KNIGHT] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, KNIGHT), NCAP(f, c), NMOVE(f), ml, mn);
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, ROOK), RCAP(f, c), RMOVE(f), ml, mn);
		if (CASTLE && !ch) {
			if (c) {
				if ((flags & 128) && (f == 63) && (RMOVE1(63) & BIT[61]))
					if (!DUALATT(61, 62, c)) registerMoves(PREMOVE(60, KING), 0LL, BIT[62], ml, mn);
				if ((flags & 512) && (f == 56) && (RMOVE1(56) & BIT[59]))
					if (!DUALATT(59, 58, c)) registerMoves(PREMOVE(60, KING), 0LL, BIT[58], ml, mn);
			} else {
				if ((flags & 64) && (f == 7) && (RMOVE1(7) & BIT[5]))
					if (!DUALATT(5, 6, c)) registerMoves(PREMOVE(4, KING), 0LL, BIT[6], ml, mn);
				if ((flags & 256) && (f == 0) && (RMOVE1(0) & BIT[3]))
					if (!DUALATT(3, 2, c)) registerMoves(PREMOVE(4, KING), 0LL, BIT[2], ml, mn);
			}
		}
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, BISHOP), BCAP(f, c), BMOVE(f), ml, mn);
	}

	b = pieceb[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, QUEEN), RCAP(f, c) | BCAP(f,c), RMOVE(f) | BMOVE(f), ml, mn);
	}

	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) registerMoves(PREMOVE(f, p), RCAP1(f, c), RMOVE1(f), ml, mn);
		if ((t & 18) == 18) registerMoves(PREMOVE(f, p), RCAP2(f, c), RMOVE2(f), ml, mn);
		if ((t & 33) == 33) registerMoves(PREMOVE(f, p), BCAP3(f, c), BMOVE3(f), ml, mn);
		if ((t & 65) == 65) registerMoves(PREMOVE(f, p), BCAP4(f, c), BMOVE4(f), ml, mn);
	}
	return 0;
}

int generateCaps(u64 ch, int c, int ply, u64 pin) {
	int t, f = kingpos[c];
	int *mn = movenum + ply;
	int *ml = movelist + (ply << 8);
	u64 m, b, a, cb = colorb[c];
	*mn = 0;

	if (ch) return generateCheckEsc(ch, ~pin, c, f, ml, mn);
	registerKing(PREMOVE(f, KING), KCAP(f, c), 0LL, ml, mn, c);

	cb = colorb[c] & (~pin);

	b = pieceb[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, QUEEN), RCAP(f, c) | BCAP(f,c), ml, mn);
	}

	b = pieceb[PAWN] & cb;
	while (b) {
		f = pullLsb(&b);
		a = PCAP(f, c);
		if (RANK(f, c ? 0x08 : 0x30)) {
			registerMoves(PREMOVE(f, PAWN) | _PROM(QUEEN), a, PMOVE(f, c), ml, mn);
		} else {
			if (ENPASS && (BIT[ENPASS] & pcaps[(f) | ((c)<<6)])) {
				u64 hh;
				int clbd = ENPASS^8;
				board ^= BIT[clbd];
				hh = ROCC1(f);
				if (!(hh & BIT[kingpos[c]]) || !(hh & colorb[c^1] & RQU)) {
					a = a | BIT[ENPASS];
				}
				board ^= BIT[clbd];
			}
			registerCaps(PREMOVE(f, PAWN), a, ml, mn);
		}
	}

	b = pin & pieceb[PAWN]; 
	while (b) {
		f = pullLsb(&b);
		t = getDir(f, kingpos[c]);
		if (t & 8) continue;
		m = a = 0LL;
		if (t & 16) {
			m = PMOVE(f, c);         
		} else if (t & 32) {
			a = PCA3(f, c);
		} else {
			a = PCA4(f, c);
		}
		if (RANK(f, c ? 0x08 : 0x30)) {
			registerMoves(PREMOVE(f, PAWN) | _PROM(QUEEN), a, m, ml, mn);
		} else {
			registerCaps(PREMOVE(f, PAWN), a, ml, mn);
		}
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, ROOK), RCAP(f, c), ml, mn);
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, BISHOP), BCAP(f, c), ml, mn);
	}

	b = pieceb[KNIGHT] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, KNIGHT), NCAP(f, c), ml, mn);
	}

	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) registerCaps(PREMOVE(f, p), RCAP1(f, c), ml, mn);
		if ((t & 18) == 18) registerCaps(PREMOVE(f, p), RCAP2(f, c), ml, mn);
		if ((t & 33) == 33) registerCaps(PREMOVE(f, p), BCAP3(f, c), ml, mn);
		if ((t & 65) == 65) registerCaps(PREMOVE(f, p), BCAP4(f, c), ml, mn);
	}
	return 0;
}

Move qpick(Move* ml, int mn, int s, int ply) {
	Move m;
	int i, t, pi, vmax = -HEUR;
	for (i = s; i < mn; i++) {
		m = ml[i];
		t = capval[CAP(m)];
		if (t > vmax) {
			vmax = t;
			pi = i;
		}
	}
	m = ml[pi];
	if (pi != s) ml[pi] = ml[s];
	return m;
}

Move spick(Move* ml, int mn, int s, int ply) {
	Move m;
	int i, t, pi, vmax = -HEUR;
	for (i = s; i < mn; i++) {
		m = ml[i];
		t = capval[CAP(m)];
		if (t > vmax) {
			vmax = t;
			pi = i;
		}
		if (vmax < HEUR && m == killer[ply]) {
			vmax = HEUR;
			pi = i;
		}
		if (vmax < history[m & 0xFFF]) {
			vmax = history[m & 0xFFF];
			pi = i;
		}
	}
	m = ml[pi];
	if (pi != s) ml[pi] = ml[s];
	return m;
}

u64 nodes;
u64 qnodes1;
u64 qnodes2;
int quiesce(int c, int ply, int alpha, int beta, int root) {
	int i, w, poff;
	int flagstore = flags;
	int countstore = count;
	u64 ch = attacked(kingpos[c], c);
	u64 pin = pinnedPieces(kingpos[c], c^1);
	if (!ch) {
		w = eval(c, pin);
		if (w > alpha) {
			if (w >= beta) return beta;
			alpha = w;
		}
	}

	generateCaps(ch, c, ply, pin);
	if (movenum[ply] == 0) return ch ? -32000 + ply : w;
	poff = ply << 8;

	for (i = 0; i < movenum[ply]; i++) {
		Move m = qpick(movelist + poff, movenum[ply], i, ply);
		doMove(m, c);
		if (root) qnodes1++; else qnodes2++;

		w = -quiesce(c^1, ply+1, -beta, -alpha, 0);

		doMove(m, c);
		flags = flagstore;
		count = countstore;

		if (w > alpha) {
			if (w >= beta) return beta;
			alpha = w;
		}
	}
	return alpha;
}

Move movestack[128];
void displayms(int ply) {
	int i;
	for (i = 0; i < ply; i++) {
		displaym(movestack[i]); printf(" ");
	}
}

int search(int c, int d, int ply, int alpha, int beta) {
	int i, j, n, w, poff, asave;
	int flagstore = flags;
	int countstore = count;
	Move hsave, hmove;
	u64 hb, hp, he, ch;
	nodes++;
	hp = HASHP;
	pvlength[ply] = ply;
	if (ply && isDraw(hp, 1)) return 0;

	if (d == 0) return quiesce(c, ply, alpha, beta, 1);
	hstack[(count & 0x7FF)] = hp;

	hb = HASHB(d);
	he = hashDB[hb & HMASK];
	if (!((he^hb) & HINV)) {
		w = (u32)(he & 0xFFFF) - 32768;
		if (he & 0x10000) {
			if (w <= alpha) return alpha;
		} else {
			if (w >= beta) return beta;
		}
	}

	hsave = hmove = 0;
	he = hashDP[hp & HMASK];
	if (!((he^hp) & HINV)) {
		hsave = hmove = (Move)(he & HMASK);
	}

	ch = attacked(kingpos[c], c);
	if (ch) d++; // Important to improve game
	poff = ply << 8;
	n = i = -1;
	asave = alpha;
	while (++i != n) {
		Move m;
		if (hmove) { 
			m = hmove;
			hmove = 0;
			i--;
		} else {
			if (n == -1) {
				generateMoves(ch, c, ply);
				n = movenum[ply];
				if (n == 0) return ch ? -32000+ply : 0;
			}
			m = spick(movelist + poff, n, i, ply);
			if (hsave && m == hsave) continue;
		}
		movestack[ply] = m;
		doMove(m, c);
		w = -search(c^1, d-1, ply+1, -beta, -alpha);
		doMove(m, c);
		flags = flagstore;
		count = countstore;


		if (w > alpha) {
			hashDP[hp & HMASK] = (hp & HINV) | m; 
			if (w >= beta) {
				if (CAP(m) == 0) {
					killer[ply] = m;
					history[m & 0xFFF]++;
				}
				hashDB[hb & HMASK] = (hb & HINV) | (w + 32768); 
				return beta;
			}

			pv[ply][ply] = m;
			for (j = ply +1; j < pvlength[ply +1]; j++) pv[ply][j] = pv[ply +1][j];
			pvlength[ply] = pvlength[ply +1];			
			alpha = w;
			if (w == 31999 - ply) return w; 
		}
	}
	if (asave == alpha) hashDB[hb & HMASK] = (hb & HINV) | 0x10000 | (asave + 32768);
	return alpha;
}

int execMove(Move m) {
	int i, c;
	doMove(m, onmove);
	onmove ^= 1; 
	c = onmove;
	hstack[(count & 0x7FF)] = HASHP;
	for (i = 0; i < 127; i++) killer[i] = killer[i+1];
	for (i = 0; i < 0x1000; i++) history[i] = 0;
	switch (isDraw(HASHP, 2)) {
		case 1: printf("1/2-1/2 {Draw by Repetition}\n"); return 1;
		case 2: printf("1/2-1/2 {Draw by Fifty Move Rule}\n"); return 2;
		default:
			i = GENERATE(c);
			if (movenum[0] == 0) {
				if (!i) {
					printf("1/2-1/2 {Stalemate}\n"); return 3;
				} else {
					printf(c ? "1-0 {White mates}\n" : "0-1 {Black mates}\n"); return 4 + c;
				}
			}
	}
	return 0;
}

int calc(int sd) {
		int i, alpha = -32000, beta = 32000, w = 100;
		u64 t1 = getTime();
		qnodes1 = qnodes2 = nodes = 0LL;
		for (i = 1; i <= sd; i++) {
			w = search(onmove, i, 0, alpha, beta);
			if (w >= beta) w = search(onmove, i, 0, beta-1, 32000);
			if (w <= alpha) w = search(onmove, i, 0, -32000, alpha+1);
			alpha = w - 60;
			beta = w + 60;
			printf("%2d %5d %6llu %9llu  ", i, w, (getTime() - t1)/10, nodes);
			displaypv(); printf("\n");

			if (i >= 32000-w || (sd == 64 && nodes > 700000)) break;
		}
		t1 = getTime() - t1 + 1;
		printf("%d. ... ", (count % 0x7FF)/2 + 1);
		displaym(pv[0][0]); printf("\n");

		printf("\nkib Nodes: %llu QNodes1: %llu QNodes2: %llu cs: %llu knps: %llu\n", nodes, qnodes1, qnodes2, t1/10, nodes/t1);
		printf("\nE1:%d E2:%d\n", eval1, eval2);
		return execMove(pv[0][0]);
}

int main(int argc, char **argv)
{
	char buf[256];
	int i, engine = -1, sd = 64;
	char *sfen = "rnbqkbnr/pppppppp/////PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	if (argc > 1) sscanf(argv[1], "%d", &sd);
	if (argc > 2) sfen = argv[2];
	for (i = 0; i < 0x10000; i++) LSB[i] = _slow_lsb(i);
	for (i = 0; i < 0x10000; i++) BITC[i] = _bitcnt(i);
	for (i = 0; i < 4096; i++) hashxor[i] = _rand_64();
	for (i = 0; i < HSIZE; i++) hashDB[i] = hashDP[i] = hashDQ[i] =0LL;
	for (i = 0; i < 64; i++) BIT[i] = 1LL << i;
	for (i = 0; i < 128; i++) pmoves[i] = 0LL;
	for (i = 0; i < 384; i++) pcaps[i] = 0LL;
	for (i = 0; i < 64; i++) bmask45[i] = _bishop45(i, 0LL, 0) | BIT[i];
	for (i = 0; i < 64; i++) bmask135[i] = _bishop135(i, 0LL, 0) | BIT[i];
	for (i = 0; i < 64; i++) crevoke[i] = 0x3FF;
	for (i = 0; i < 64; i++) kmoves[i] = nmoves[i] = n2moves[i] = 0LL;
	crevoke[7] ^= BIT[6];
	crevoke[63] ^= BIT[7];
	crevoke[0] ^= BIT[8];
	crevoke[56] ^= BIT[9];

	_init_rays(rays, _rook0, key000);
	_init_rays(rays + 0x2000, _rook90, key090);
	_init_rays(rays + 0x4000, _bishop45, key045);
	_init_rays(rays + 0x6000, _bishop135, key135);
	_init_shorts(nmoves, _knight);
	_init_shorts(kmoves, _king);
	for (i = 0; i < 64; i++) {
		u64 b = nmoves[i];
		while (b) n2moves[i] |= nmoves[pullLsb(&b)];
		n2moves[i] ^= BIT[i];
	}
	_init_pawns(pmoves, pcaps, 0);
	_init_pawns(pmoves + 64, pcaps + 64, 1);
	_parse_fen(sfen);
	for (i = 0; i < 64; i++) nmobil[i] = bitcnt(NOCC(i) | NMOVE(i))*6;
	for (i = 0; i < 64; i++) kmobil[i] = (bitcnt(NOCC(i) | NMOVE(i))/2)*4;
	for (;;) {
		if (engine == onmove) {
			if (calc(sd)) engine = -1;
			continue;
		}

		fgets(buf,255,stdin);
		if (!strncmp(buf,"xboard",6)) printf("feature setboard=1 myname=\"OliThink " VER "\" done=1\n");
		if (!strncmp(buf,"quit",4)) return 0;
		if (!strncmp(buf,"force",5)) engine = -1;
		if (!strncmp(buf,"go",2)) engine = onmove;
		if (!strncmp(buf,"new",2)) { _parse_fen(sfen); engine = 1; }
		if (!strncmp(buf,"setboard",8)) _parse_fen(buf+9);
		if (!strncmp(buf,"sd",2)) sscanf(buf+3,"%d",&sd);

		if (buf[0] >= 'a' && buf[0] <= 'h' && buf[1] >= '1' && buf[1] <= '8') {
			Move _m = 0;
			int from = 8*(buf[1] - '1') + buf[0] - 'a';
			int to = 8*(buf[3] - '1') + buf[2] - 'a';
			GENERATE(onmove);
			for (i = 0; i < movenum[0]; i++) {
				Move m = movelist[i];
				if (TO(m) != to) continue;
				if (FROM(m) != from) continue;
				if (PROM(m) && pieceChar[PROM(m)] != buf[4]-32) continue;
				_m = m;
			}
			if (_m == 0) {
					fprintf(stderr,"Illegal move: %s\n",buf);
			} else {
					if (execMove(_m)) engine = -1;
			}
		}
	}
	return 0;
}
