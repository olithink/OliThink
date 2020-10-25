/* OliPerft 1.0.7 - Bitboard Magic Move (c) Oliver Brausch 25.Oct.2020, ob112@web.de */
/* oliperft 6 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
Nodes: 8229523927 cs: 1156 knps: 713625 (gcc8 64bit AMD EPYC 7502P 1/32-Core Processor)
Nodes: 8229523927 cs: 1526 knps: 568690 (gcc4 OSX i7 8850H 2.6 GHz)
Nodes: 8229523927 ms: 40610 knps: 202647 (VS2005 64bit AMD64 4600+) (1.0.2)
Nodes: 8229523927 ms: 64860 knps: 126881 (VS2005 32bit AMD64 4600+) (1.0.2)
Nodes: 8229523927 ms: 97251 knps: 84621 (gcc4 32bit AMD Opteron 1210HE) (1.0.2)
*/
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
struct _timeb tv;
#define bitcnt(x) __popcnt64(x)
#define getLsb(x) _tzcnt_u64(x)
#else
#include <sys/time.h>
struct timeval tv;
#define bitcnt(x) __builtin_popcountll(x)
#define getLsb(x) __builtin_ctzll(x)
#endif
typedef unsigned long long u64;
typedef unsigned long u32;
typedef int Move;

enum { EMPTY, PAWN, KNIGHT, KING, ENP, BISHOP, ROOK, QUEEN };

#define FROM(x) ((x) & 63)
#define TO(x) (((x) >> 6) & 63)
#define ONMV(x) (((x) >> 12) & 1)
#define PROM(x) (((x) >> 13) & 7)
#define PIECE(x) (((x) >> 16) & 7)
#define CAP(x) (((x) >> 19) & 7)

#define _TO(x) ((x) << 6)
#define _ONMV(x) ((x) << 12)
#define _PROM(x) ((x) << 13)
#define _PIECE(x) ((x) << 16)
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

#define RMOVE1(f) (RATT1(f) & ~board)
#define RMOVE2(f) (RATT2(f) & ~board)
#define BMOVE3(f) (BATT3(f) & ~board)
#define BMOVE4(f) (BATT4(f) & ~board)
#define RCAP1(f, c) (RATT1(f) & colorb[(c)^1])
#define RCAP2(f, c) (RATT2(f) & colorb[(c)^1])
#define BCAP3(f, c) (BATT3(f) & colorb[(c)^1])
#define BCAP4(f, c) (BATT4(f) & colorb[(c)^1])
#define RATT(f) (RATT1(f) | RATT2(f))
#define BATT(f) (BATT3(f) | BATT4(f))
#define RMOVE(f) (RMOVE1(f) | RMOVE2(f))
#define BMOVE(f) (BMOVE3(f) | BMOVE4(f))
#define RCAP(f, c) (RCAP1(f, c) | RCAP2(f, c))
#define BCAP(f, c) (BCAP3(f, c) | BCAP4(f, c))

#define NMOVE(x) (nmoves[x] & ~board)
#define KMOVE(x) (kmoves[x] & ~board)
#define NCAP(x, c) (nmoves[x] & colorb[(c)^1])
#define KCAP(x, c) (kmoves[x] & colorb[(c)^1])
#define PMOVE(x, c) (pmoves[(x) | ((c)<<6)] & ~board)
#define POCC(x, c) (pcaps[(x) | ((c)<<6)] & board)
#define PCAP(x, c) (pcaps[(x) | ((c)<<6)] & colorb[(c)^1])
#define PCA3(x, c) (pcaps[(x) | ((c)<<6) | 128] & (colorb[(c)^1] | (BIT[ENPASS] & ((c) ? 0xFF0000 : 0xFF0000000000))))
#define PCA4(x, c) (pcaps[(x) | ((c)<<6) | 256] & (colorb[(c)^1] | (BIT[ENPASS] & ((c) ? 0xFF0000 : 0xFF0000000000))))

#define RANK7(f, c) (((f) & 0x38) == ((c) ? 0x08 : 0x30))
#define RANK4(f, c) (((f) & 0x38) == ((c) ? 0x20 : 0x18))
#define RANK2(f, c) (((f) & 0x38) == ((c) ? 0x30 : 0x08))
#define ENPASS (flags & 63)
#define CASTLE (flags & 960)

#define HSIZE 0x400000LL
#define HMASK (HSIZE-1)
#define HINV ~HMASK
u64 hashDB[HSIZE], num[128];
u64 hashb;

static u64 BIT[64];
static u64 hashxor[0x8000];
static u64 rays[0x10000];
static u64 nmoves[64];
static u64 kmoves[64];
static u64 pmoves[128];
static u64 pcaps[384];
static int crevoke[64];
static int _knight[8] = {-17,-10,6,15,17,10,-6,-15};
static int _king[8] = {-9,-1,7,8,9,1,-7,-8};

struct Movep {
	int n;
	Move moves[128];
};
typedef struct Movep Movep;
const char pieceChar[] = "*PNK.BRQ";
u64 pieceb[8], colorb[2];
u64 board;
int kingpos[2];
int flags, onmove;

void _parse_fen(char *fen) {
	char s, mv, pos[128], cas[5], enps[3];
	int i, p, halfm = 0, fullm = 1, col = 0, row = 7;
	for (i = 0; i < 8; i++) pieceb[i] = 0LL;
	colorb[0] = colorb[1] = hashb = i = 0LL;
	sscanf(fen, "%s %c %s %s %d %d", pos, &mv, cas, enps, &halfm, &fullm); if (fullm < 1) fullm = 1;
	while ((s = pos[i++])) {
		if (s == '/') {
			row--;
			col = 0;
		} else if (s >= '1' && s <= '8') {
			col += s - '0';
		} else for (p = 1; p < 8; p++) {
			if (pieceChar[p] == s || pieceChar[p] == s - 32) {
				int c = pieceChar[p] != s, t = row*8 + (col++);
				if (p == KING) kingpos[c] = t;
				hashb ^= hashxor[col | row << 3 | p << 6 | c << 9];
				pieceb[p] |= BIT[t];
				colorb[c] |= BIT[t];
				break;
			}
		}
	}
	onmove = mv == 'b' ? 1 : 0;
	flags = i = 0;
	while ((s = cas[i++])) {
		if (s == 'K') flags |= BIT[6];
		if (s == 'k') flags |= BIT[7];
		if (s == 'Q') flags |= BIT[8];
		if (s == 'q') flags |= BIT[9];
	}
	if (enps[0] != '-') flags |= 8*(enps[1] - '1') + enps[0] - 'a';
	board = colorb[0] | colorb[1];
}

void _init_pawns(u64* moves, u64* caps, int c) {
	int i;
	for (i = 0; i < 64; i++) {
		int m = i + (c ? -8 : 8);
		if (m < 0 || m > 63) continue;
		moves[i] |= BIT[m];
		if ((i&7) > 0) {
			m = i + (c ? -9 : 7);
			if (m < 0 || m > 63) continue;
			caps[i] |= BIT[m];
			caps[i + 128*(2 - c)] |= BIT[m];
		}
		if ((i&7) < 7) {
			m = i + (c ? -7 : 9);
			if (m < 0 || m > 63) continue;
			caps[i] |= BIT[m];
			caps[i + 128*(c + 1)] |= BIT[m];
		}
	}
}

void _init_shorts(u64* moves, int* m) {
	int i, j, n;
	for (i = 0; i < 64; i++) {
		for (j = 0; j < 8; j++) {
			n = i + m[j];
			if (n < 64 && n >= 0 && ((n & 7)-(i & 7))*((n & 7)-(i & 7)) <= 4) {
				moves[i] |= BIT[n];
			}
		}
	}
}

u64 _occ_free_board(int bc, int del, u64 free) {
	u64 perm = free;
	int i;
	for (i = 0; i < bc; i++) {
		u64 low = free & (-free); // Lowest bit
		free &= (~low);
		if (!(BIT[i] & del)) perm &= (~low);
	}
	return perm;
}

void _init_rays(u64* ray, u64 (*rayFunc) (int, u64, int), int (*key)(u64, int)) {
	int i, f, bc;
	for (f = 0; f < 64; f++) {
		u64 mmask = (*rayFunc)(f, 0LL, 0) | BIT[f];
		int iperm = 1 << (bc = bitcnt(mmask));
		for (i = 0; i < iperm; i++) {
			u64 bord = _occ_free_board(bc, i, mmask);
			u64 move = (*rayFunc)(f, bord, 1);
			u64 occ = (*rayFunc)(f, bord, 2);
			u64 xray = (*rayFunc)(f, bord, 3);
			int index = (*key)(bord, f);
			ray[(f << 7) + index] = occ | move;
			ray[(f << 7) + index + 0x8000] = xray;
		}
	}
}

#define RAYMACRO { if (BIT[i] & bord) \
	{ if (b) { xray |= BIT[i]; break; } else { occ |= BIT[i]; b = 1; } } if (!b) free |= BIT[i]; }
u64 _rook0(int f, u64 bord, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f+1; i < 64 && i%8 != 0; i++) RAYMACRO
	for (b = 0, i = f-1; i >= 0 && i%8 != 7; i--) RAYMACRO
	return (t < 2) ? free : (t == 2 ? occ : xray);
}

u64 _rook90(int f, u64 bord, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f-8; i >= 0; i-=8) RAYMACRO
	for (b = 0, i = f+8; i < 64; i+=8) RAYMACRO
	return (t < 2) ? free : (t == 2 ? occ : xray);
}

u64 _bishop45(int f, u64 bord, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f+9; i < 64 && (i%8 != 0); i+=9) RAYMACRO
	for (b = 0, i = f-9; i >= 0 && (i%8 != 7); i-=9) RAYMACRO
	return (t < 2) ? free : (t == 2 ? occ : xray);
}

u64 _bishop135(int f, u64 bord, int t) {
	u64 free = 0LL, occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f-7; i >= 0 && (i%8 != 0); i-=7) RAYMACRO
	for (b = 0, i = f+7; i < 64 && (i%8 != 7); i+=7) RAYMACRO
	return (t < 2) ? free : (t == 2 ? occ : xray);
}

u64 getTime() {
#ifdef _WIN32
	return GetTickCount64();
#else
	gettimeofday (&tv, NULL);
	return(tv.tv_sec * 1000LL + (tv.tv_usec / 1000));
#endif
}

int pullLsb(u64* bit) {
	int f = getLsb(*bit);
	*bit &= *bit - 1;
	return f;
}

int identPiece(int f) {
	int i;
	for (i = PAWN; i <= QUEEN; i++) if (i != ENP && (BIT[f] & pieceb[i])) return i;
	return ENP;
}

int identColor(int f) {
	return (BIT[f] & colorb[1]) ? 1 : 0;
}

int key000(u64 b, int f) {
	return (int) ((b >> (f & 56)) & 0x7E);
}

int key090(u64 b, int f) {
	u64 _b = (b >> (f&7)) & 0x0101010101010101LL;
	_b = _b * 0x0080402010080400LL;
	return (int)(_b >> 57);
}

int keyDiag(u64 _b) {
	_b = _b * 0x0202020202020202LL;
	return (int)(_b >> 57);
}

u64 bmask45[64], bmask135[64];
int key045(u64 b, int f) {
   return keyDiag(b & bmask45[f]);
}

int key135(u64 b, int f) {
   return keyDiag(b & bmask135[f]);
}

#define RQU (pieceb[ROOK] | pieceb[QUEEN])
#define BQU (pieceb[BISHOP] | pieceb[QUEEN])
#define DUALATT(x, y, c) (battacked(x, c) || battacked(y, c))
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
		| (RCAP1(f, c) & RQU) | (RCAP2(f, c) & RQU)
		| (BCAP3(f, c) & BQU) | (BCAP4(f, c) & BQU);
}

u64 attacked(int f, int c) {
	return (PCAP(f, c) & pieceb[PAWN]) | reach(f, c);
}

void display64(u64 bb) {
	int i, j;
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			int f = j + (7-i)*8;
			if (bb) printf(" %d", BIT[j + (7-i)*8] & bb ? 1 : 0);
			else printf(" %c", pieceChar[identPiece(f)] + identColor(f)*32);
		}
		printf("\n");
	}
	printf("\n");
}

void displaym(Move m) {
	printf("%c%c%c%c%c%c", pieceChar[PIECE(m)], 'a' + FROM(m) % 8, '1' + FROM(m) / 8,
		CAP(m) == 0 ? '-' : 'x', 'a' + TO(m) % 8, '1' + TO(m) / 8);
	if (PROM(m)) printf("%c", pieceChar[PROM(m)]+32);
}

u64 pinnedPieces(int f, int oc) {
	u64 pin = 0LL;
	u64 b = ((RXRAY1(f) | RXRAY2(f)) & colorb[oc]) & RQU;
	while (b) {
		int t = pullLsb(&b);
		pin |= RATT(f) & RATT(t) & colorb[oc^1];
	}
	b = ((BXRAY3(f) | BXRAY4(f)) & colorb[oc]) & BQU;
	while (b) {
		int t = pullLsb(&b);
		pin |= BATT(f) & BATT(t) & colorb[oc^1];
	}
	return pin;
}

/* precondition: f and t are on common rank (8), file (16), diagonal (32) or antidiagonal (64) */
char getDir(int f, int t) {
	if (!((f ^ t) & 56)) return 8;
	if (!((f ^ t) & 7)) return 16;
	return (!((f - t) % 9)) ? 32 : 64;
}

void move(Move m, int c) {
	int f = FROM(m);
	int t = TO(m);
	int p = PIECE(m);
	int a = CAP(m);

	colorb[c] ^= BIT[f];
	pieceb[p] ^= BIT[f];

	colorb[c] ^= BIT[t];
	pieceb[p] ^= BIT[t];
	hashb ^= hashxor[f | p << 6 | c << 9];
	hashb ^= hashxor[t | p << 6 | c << 9];

	flags &= 960;
	if (a) {
		if (a == ENP) { // Enpassant Capture
			t = (t&7) | (f&56); a = PAWN;
		} else if (a == ROOK && CASTLE) { //Revoke castling rights.
			flags &= crevoke[t];
		}
		pieceb[a] ^= BIT[t];
		colorb[c^1] ^= BIT[t];
		hashb ^= hashxor[t | a << 6 | (c^1) << 9];
	}
	if (p == PAWN) {
		if (((f^t)&8) == 0) flags |= f^24; //Enpassant
		else if ((t&56) == 0 || (t&56) == 56) {
			pieceb[PAWN] ^= BIT[t];
			pieceb[PROM(m)] ^= BIT[t];
			hashb ^= hashxor[t | PAWN << 6 | c << 9];
			hashb ^= hashxor[t | PROM(m) << 6 | c << 9];
		}
	} else if (p == KING) {
		if (kingpos[c] == f) kingpos[c] = t; else kingpos[c] = f;
		flags &= ~(320 << c); // Lose castling rights
		if (((f^t)&3) == 2) { // Castle
			if (t == 6) { f = 7; t = 5; }
			else if (t == 2) { f = 0; t = 3; }
			else if (t == 62) { f = 63; t = 61; }
			else { f = 56; t = 59; }
			colorb[c] ^= BIT[f];
			pieceb[ROOK] ^= BIT[f];
			colorb[c] ^= BIT[t];
			pieceb[ROOK] ^= BIT[t];
			hashb ^= hashxor[f | ROOK << 6 | c << 9];
			hashb ^= hashxor[t | ROOK << 6 | c << 9];
		}
	} else if (p == ROOK && CASTLE) {
		flags &= crevoke[f];
	}
	board = colorb[0] | colorb[1];
}

void registerMoves(Move m, u64 bc, u64 bm, Movep *mp) {
	while (bc) {
		int t = pullLsb(&bc);
		mp->moves[mp->n++] = m | _TO(t) | _CAP(identPiece(t));
	}
	while (bm) {
		mp->moves[mp->n++] = m | _TO(pullLsb(&bm));
	}
}

void registerProms(int f, int c, u64 bc, u64 bm, Movep *mp) {
	while (bc) {
		int t = pullLsb(&bc);
		Move m = f | _ONMV(c) | _PIECE(PAWN) | _TO(t) | _CAP(identPiece(t));
		mp->moves[mp->n++] = m | _PROM(QUEEN);
		mp->moves[mp->n++] = m | _PROM(KNIGHT);
		mp->moves[mp->n++] = m | _PROM(ROOK);
		mp->moves[mp->n++] = m | _PROM(BISHOP);
	}
	while (bm) {
		Move m = f | _ONMV(c) | _PIECE(PAWN) | _TO(pullLsb(&bm));
		mp->moves[mp->n++] = m | _PROM(QUEEN);
		mp->moves[mp->n++] = m | _PROM(KNIGHT);
		mp->moves[mp->n++] = m | _PROM(ROOK);
		mp->moves[mp->n++] = m | _PROM(BISHOP);
	}
}

void registerKing(Move m, u64 bc, u64 bm, Movep *mp, int c) {
	while (bc) {
		int t = pullLsb(&bc);
		if (battacked(t, c)) continue;
		mp->moves[mp->n++] = m | _TO(t) | _CAP(identPiece(t));
	}
	while (bm) {
		int t = pullLsb(&bm);
		if (battacked(t, c)) continue;
		mp->moves[mp->n++] = m | _TO(t);
	}
}

int generateCheckEsc(u64 ch, u64 apin, int c, int k, Movep *mp) {
	u64 cc, fl;
	int d, bf = bitcnt(ch);
	board ^= BIT[k];
	registerKing(PREMOVE(k, KING), KCAP(k, c), KMOVE(k), mp, c);
	board ^= BIT[k];
	if (bf > 1) return bf; //Multicheck
	bf = getLsb(ch);

	cc = attacked(bf, c^1) & apin;  //Can we capture the checker?
	while (cc) {
		int cf = pullLsb(&cc);
		int p = identPiece(cf);
		if (p == PAWN && RANK7(cf, c)) {
			registerProms(cf, c, ch, 0LL, mp);
		} else {
			registerMoves(PREMOVE(cf, p), ch, 0LL, mp);
		}
	}
	if (ENPASS && (ch & pieceb[PAWN])) { //Enpassant capture of attacking Pawn
		cc = PCAP(ENPASS, c^1) & pieceb[PAWN] & apin;
		while (cc) {
			int cf = pullLsb(&cc);
			registerMoves(PREMOVE(cf, PAWN), BIT[ENPASS], 0LL, mp);
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
			registerMoves(PREMOVE(cf, identPiece(cf)), 0LL, BIT[f], mp);
		}
		bf = c ? f+8 : f-8;
		if (bf < 0 || bf > 63 || !(cc = pieceb[PAWN] & colorb[c] & apin)) continue;
		if (BIT[bf] & cc) {
			if (RANK7(bf, c))
				registerProms(bf, c, 0LL, BIT[f], mp);
			else
				registerMoves(PREMOVE(bf, PAWN), 0LL, BIT[f], mp);
		}
		if (RANK4(f, c) && !(board & BIT[bf]) && (BIT[c ? f+16 : f-16] & cc))
			registerMoves(PREMOVE(c ? f+16 : f-16, PAWN), 0LL, BIT[f], mp);
	}
	return 1;
}

int generateMoves(int c, Movep *mp) {
	int t, f = kingpos[c];
	u64 m, b, a, ch = attacked(f, c);
	u64 pin = pinnedPieces(f, c^1);
	mp->n = 0;

	if (ch) return generateCheckEsc(ch, ~pin, c, f, mp);
	registerKing(PREMOVE(f, KING), KCAP(f, c), KMOVE(f), mp, c);

	u64 cb = colorb[c] & (~pin);
	b = pieceb[PAWN] & colorb[c];
	while (b) {
		f = pullLsb(&b);
		t = BIT[f] & pin ? getDir(f, kingpos[c]) : 144;
		if (t & 8) continue;
		else if (t & 16) {
			m = PMOVE(f, c); a = t & 128 ? PCAP(f, c) : 0;
			if (m && RANK2(f, c)) m |= PMOVE(c ? f-8 : f+8, c);
		} else if (t & 32) {
			m = 0; a = PCA3(f, c);
		} else {
			m = 0; a = PCA4(f, c);
		}
		if (RANK7(f, c)) {
			registerProms(f, c, a, m, mp);
		} else {
			if (t & 128 && ENPASS && (BIT[ENPASS] & pcaps[(f) | ((c)<<6)])) {
				board ^= BIT[ENPASS^8];
				if (!(RATT1(f) & BIT[kingpos[c]]) || !(RATT1(f) & colorb[c^1] & RQU)) {
					a |= BIT[ENPASS];
				}
				board ^= BIT[ENPASS^8];
			}
			registerMoves(PREMOVE(f, PAWN), a, m, mp);
		}
	}
	
	b = pieceb[KNIGHT] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, KNIGHT), NCAP(f, c), NMOVE(f), mp);
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, ROOK), RCAP(f, c), RMOVE(f), mp);
		if (CASTLE && !ch) {
			if (c) {
				if ((flags & 128) && (f == 63) && (RATT1(63) & BIT[60]))
					if (!DUALATT(61, 62, c)) registerMoves(PREMOVE(60, KING), 0LL, BIT[62], mp);
				if ((flags & 512) && (f == 56) && (RATT1(56) & BIT[60]))
					if (!DUALATT(59, 58, c)) registerMoves(PREMOVE(60, KING), 0LL, BIT[58], mp);
			} else {
				if ((flags & 64) && (f == 7) && (RATT1(7) & BIT[4]))
					if (!DUALATT(5, 6, c)) registerMoves(PREMOVE(4, KING), 0LL, BIT[6], mp);
				if ((flags & 256) && (f == 0) && (RATT1(0) & BIT[4]))
					if (!DUALATT(3, 2, c)) registerMoves(PREMOVE(4, KING), 0LL, BIT[2], mp);
			}
		}
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, BISHOP), BCAP(f, c), BMOVE(f), mp);
	}

	b = pieceb[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		registerMoves(PREMOVE(f, QUEEN), RCAP(f, c) | BCAP(f,c), RMOVE(f) | BMOVE(f), mp);
	}

	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) registerMoves(PREMOVE(f, p), RCAP1(f, c), RMOVE1(f), mp);
		if ((t & 18) == 18) registerMoves(PREMOVE(f, p), RCAP2(f, c), RMOVE2(f), mp);
		if ((t & 33) == 33) registerMoves(PREMOVE(f, p), BCAP3(f, c), BMOVE3(f), mp);
		if ((t & 65) == 65) registerMoves(PREMOVE(f, p), BCAP4(f, c), BMOVE4(f), mp);
	}
	return 0;
}

void countKing(u64 bm, Movep *mp, int c) {
	while (bm) {
		if (!battacked(pullLsb(&bm), c)) mp->n++;
	}
}

int countMoves(int c, Movep *mp) {
	int t, f = kingpos[c];
	u64 m, b, a, ch = attacked(f, c);
	u64 pin = pinnedPieces(f, c^1);
	mp->n = 0;

	if (ch) return generateCheckEsc(ch, ~pin, c, f, mp);
	countKing(KCAP(f, c) | KMOVE(f), mp, c);

	u64 cb = colorb[c] & (~pin);
	b = pieceb[PAWN] & cb;
	while (b) {
		f = pullLsb(&b);
		m = PMOVE(f, c);
		a = PCAP(f, c);
		if (m && RANK2(f, c)) m |= PMOVE(c ? f-8 : f+8, c);
		if (RANK7(f, c)) {
			mp->n += bitcnt(a | m) << 2;
		} else {
			if (ENPASS && (BIT[ENPASS] & pcaps[(f) | ((c)<<6)])) {
				board ^= BIT[ENPASS^8];
				if (!(RATT1(f) & BIT[kingpos[c]]) || !(RATT1(f) & colorb[c^1] & RQU)) {
					a |= BIT[ENPASS];
				}
				board ^= BIT[ENPASS^8];
			}
			mp->n += bitcnt(a | m);
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
			if (m && RANK2(f, c)) m |= PMOVE(c ? f-8 : f+8, c);
		} else if (t & 32) {
			a = PCA3(f, c);
		} else {
			a = PCA4(f, c);
		}
		if (RANK7(f, c)) {
			mp->n += bitcnt(a | m) << 2;
		} else {
			mp->n += bitcnt(a | m);
		}
	}

	b = pieceb[KNIGHT] & cb;
	while (b) {
		f = pullLsb(&b);
		mp->n += bitcnt(nmoves[f] & ~colorb[c]);
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		mp->n += bitcnt(BATT(f) & ~colorb[c]);
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		mp->n += bitcnt(RATT(f) & ~colorb[c]);
		if (CASTLE && !ch) {
			if (c) {
				if ((flags & 128) && (f == 63) && (RATT1(63) & BIT[60]))
					if (!DUALATT(61, 62, c)) mp->n++;
				if ((flags & 512) && (f == 56) && (RATT1(56) & BIT[60]))
					if (!DUALATT(59, 58, c)) mp->n++;
			} else {
				if ((flags & 64) && (f == 7) && (RATT1(7) & BIT[4]))
					if (!DUALATT(5, 6, c)) mp->n++;
				if ((flags & 256) && (f == 0) && (RATT1(0) & BIT[4]))
					if (!DUALATT(3, 2, c)) mp->n++;
			}
		}
	}

	b = pieceb[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		mp->n += bitcnt((RATT(f) | BATT(f)) & ~colorb[c]);
	}

	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) mp->n += bitcnt(RATT1(f) & ~colorb[c]);
		if ((t & 18) == 18) mp->n += bitcnt(RATT2(f) & ~colorb[c]);
		if ((t & 33) == 33) mp->n += bitcnt(BATT3(f) & ~colorb[c]);
		if ((t & 65) == 65) mp->n += bitcnt(BATT4(f) & ~colorb[c]);
	}
	return 0;
}

#define HASHB (hashb ^ hashxor[flags | c << 10 | d << 11])
void perft(int c, int d, int ply) {
	u64 hb = HASHB;
	u64 he = hashDB[hb & HMASK];
	u64 n0, n1 = he & HMASK;

	if (!((he^hb) & HINV)) {
		num[ply+d] += n1;
		return;
	}

	Movep mp;

	if (d<= 1) {
		countMoves(c, &mp);
		if (mp.n > n1) hashDB[hb & HMASK] = (hb & HINV) | mp.n;
		num[ply+1]+= mp.n;
		return;
	}
	n0 = num[ply+d];

	generateMoves(c, &mp);
	int i, flagstor = flags, poff = ply << 8;
	for (i = 0; i < mp.n; i++) {
		Move m = mp.moves[i];
		move(m, c);

		num[ply+1]++;
		if (d > 1) perft(c^1, d-1, ply+1);

		move(m, c);
		flags = flagstor;
	}
	n0 = num[ply+d] - n0;
	if (n0 < 0x100000 && n0 > n1) {
		hashDB[hb & HMASK] = (hb & HINV) | n0; 
	}
}

int main(int argc, char **argv) {
	int i, divide = 0, sd = 6;
	u64 t1, p, n;
	char *sfen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
	if (argc > 1) sscanf(argv[1], "%d", &sd);
	if (argc > 2) sfen = argv[2];
	if (sd < 0) {
		sd = -sd;
		divide = 1;
	}
	for (i = 0x8000, p = 1, n = 6364136223846793005LL; i--; hashxor[0x7FFF-i] = p = p*n +1LL);
	for (i = 0; i < HSIZE; i++) hashDB[i] = 0LL;
	for (i = 0; i < 64; i++) BIT[i] = 1LL << i;
	for (i = 0; i < 64; i++) crevoke[i] = 0x3FF;
	for (i = 0; i < 128; i++) pmoves[i] = 0LL;
	for (i = 0; i < 384; i++) pcaps[i] = 0LL;
	crevoke[7] ^= BIT[6];
	crevoke[63] ^= BIT[7];
	crevoke[0] ^= BIT[8];
	crevoke[56] ^= BIT[9];

	for (i = 0; i < 64; i++) bmask45[i] = _bishop45(i, 0LL, 0) | BIT[i];
	for (i = 0; i < 64; i++) bmask135[i] = _bishop135(i, 0LL, 0) | BIT[i];

	_init_rays(rays, _rook0, key000);
	_init_rays(rays + 0x2000, _rook90, key090);
	_init_rays(rays + 0x4000, _bishop45, key045);
	_init_rays(rays + 0x6000, _bishop135, key135);
	_init_shorts(nmoves, _knight);
	_init_shorts(kmoves, _king);
	_init_pawns(pmoves, pcaps, 0);
	_init_pawns(pmoves + 64, pcaps + 64, 1);
	_parse_fen(sfen);
	display64(0LL);

	t1 = getTime();

	Movep mp; n = 0;

	if (divide) {
		int flagstor = flags;
		generateMoves(onmove, &mp);
		for (i = 0; i < mp.n; i++) {
			Move m = mp.moves[i];
			move(m, onmove);

			num[1]++;
			perft(onmove^1, sd-1, 1);

			displaym(m); printf(": %llu\n", num[sd]);
			move(m, onmove);
			flags = flagstor;
			n += num[sd];
			num[sd] = 0;
		}
	} else {
		for (i = 1; i <= sd; i++) {
			num[i] = 0;
			perft(onmove, i, 0);
			n += num[i];
			printf("%2d %5d %6llu %11llu\n", i, 0, (getTime() - t1 + 4)/10, num[i]);
		}
	}
	t1 = getTime() - t1 + 1;
	printf("\nNodes: %llu cs: %llu knps: %llu\n", n, (t1 + 3)/10, n/t1);
	return 0;
}