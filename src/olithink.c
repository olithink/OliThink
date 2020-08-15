/* OliThink5 (c) Oliver Brausch 03.Apr.2008, ob112@web.de, http://home.arcor.de/dreamlike */
#define VER "5.1.2"
#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
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

#define CNODES 0xFFFF
#define HEUR 9900000
const int pval[] = {0, 100, 290, 0, 100, 310, 500, 950};
const int capval[] = {0, HEUR+1, HEUR+2, 0, HEUR+1, HEUR+2, HEUR+3, HEUR+4};

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

#define RATT1(f) rays[((f) << 7) | key000(BOARD, f)]
#define RATT2(f) rays[((f) << 7) | key090(BOARD, f) | 0x2000]
#define BATT3(f) rays[((f) << 7) | key045(BOARD, f) | 0x4000]
#define BATT4(f) rays[((f) << 7) | key135(BOARD, f) | 0x6000]
#define RXRAY1(f) rays[((f) << 7) | key000(BOARD, f) | 0x8000]
#define RXRAY2(f) rays[((f) << 7) | key090(BOARD, f) | 0xA000]
#define BXRAY3(f) rays[((f) << 7) | key045(BOARD, f) | 0xC000]
#define BXRAY4(f) rays[((f) << 7) | key135(BOARD, f) | 0xE000]

#define ROCC1(f) (RATT1(f) & BOARD)
#define ROCC2(f) (RATT2(f) & BOARD)
#define BOCC3(f) (BATT3(f) & BOARD)
#define BOCC4(f) (BATT4(f) & BOARD)
#define RMOVE1(f) (RATT1(f) & (~BOARD))
#define RMOVE2(f) (RATT2(f) & (~BOARD))
#define BMOVE3(f) (BATT3(f) & (~BOARD))
#define BMOVE4(f) (BATT4(f) & (~BOARD))
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

#define SHORTMOVE(x) ((x) & ((x)^BOARD))
#define SHORTOCC(x) ((x) & BOARD)
#define SHORTCAP(x, c) ((x) & colorb[(c)^1])

#define NMOVE(x) (SHORTMOVE(nmoves[x]))
#define KMOVE(x) (SHORTMOVE(kmoves[x]))
#define PMOVE(x, c) (pmoves[(x) | ((c)<<6)] & (~BOARD))
#define NOCC(x) (SHORTOCC(nmoves[x]))
#define KOCC(x) (SHORTOCC(kmoves[x]))
#define POCC(x, c) (pcaps[(x) | ((c)<<6)] & BOARD)
#define NCAP(x, c) (SHORTCAP(nmoves[x], (c)))
#define KCAP(x, c) (SHORTCAP(kmoves[x], (c)))
#define PCAP(x, c) (pcaps[(x) | ((c)<<6)] & colorb[(c)^1])
#define PCA3(x, c) (pcaps[(x) | ((c)<<6) | 128] & (colorb[(c)^1] | ((BIT[ENPASS]) & (c ? 0xFF0000LL : 0xFF0000000000LL))))
#define PCA4(x, c) (pcaps[(x) | ((c)<<6) | 256] & (colorb[(c)^1] | ((BIT[ENPASS]) & (c ? 0xFF0000LL : 0xFF0000000000LL))))

#define RANK(x, y) (((x) & 0x38) == (y))
#define TEST(f, b) (BIT[f] & (b))
#define ENPASS (flags & 63)
#define CASTLE (flags & 960)
#define COUNT (count & 0x7FF)

#define HSIZE 0x800000
#define HMASK 0x7FFFFF
#define HINV 0xFFFFFFFFFF800000LL
u64 hashDB[HSIZE];
u64 hashDP[HSIZE];
u64 hashb;
u64 hstack[0x800];

static u64 hashxor[4096];
static u64 rays[0x10000];
static u64 pmoves[128];
static u64 pcaps[384];
static u64 nmoves[64];
static u64 kmoves[64];
static int _knight[8] = {-17,-10,6,15,17,10,-6,-15};
static int _king[8] = {-9,-1,7,8,9,1,-7,-8};
static u64 BIT[64];
static char LSB[0x10000];
static char BITC[0x10000] ;      
static int crevoke[64];
static int nmobil[64];
static int kmobil[64];
static int pawnprg[128];
Move movelist[64*256];
int movenum[64];
Move pv[64][64];
int pvlength[64];
int value[64];
int iter;
const char pieceChar[] = "*PNK.BRQ";
u64 searchtime, maxtime, starttime;
int sabort, noabort, neverabort = 0;
int count, flags, mat, onmove;
int kingpos[2];
u64 pieceb[8];
u64 colorb[2];
#define BOARD (colorb[0] | colorb[1])
#define RQU (pieceb[QUEEN] | pieceb[ROOK])
#define BQU (pieceb[QUEEN] | pieceb[BISHOP])

void setBit(int f, u64 *b) {
	*b |= BIT[f];
}

void xorBit(int f, u64 *b) {
	*b ^= BIT[f];
}

u64 getLowestBit(u64 bb) {
	return bb & (-(long long)bb);
}

void _parse_fen(char *fen) {
	char c, mv, pos[128], cas[5], enps[3];
	int i, j = 0, halfm = 0, fullm = 1, col = 0, row = 7;
	for (i = 0; i < 8; i++) pieceb[i] = 0LL;
	colorb[0] = colorb[1] = hashb = 0LL;
	mat = 0;
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
				else mat += pval[i];
				hashb ^= hashxor[col | row << 3 | i << 6];
				setBit(row*8 + col, pieceb+i);
				setBit(row*8 + (col++), colorb);
				break;
			} else if (pieceChar[i] == c - 32) {
				if (i == KING) kingpos[1] = row*8 + col;
				else mat -= pval[i];
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
	if (enps[0] != '-') flags |= 8*(enps[1] - '1') + enps[0] - 'a'; 
	count = (fullm - 1)*2 + onmove + (halfm << 11);
	for (i = 0; i < COUNT; i++) hstack[i] = 0LL;
}

#define LOW16(x) ((x) & 0xFFFF)
#define LOW32(x) ((x) & 0xFFFFFFFF)
static u32 r_x = 30903, r_y = 30903, r_z = 30903, r_w = 30903, r_carry = 0;
u32 _rand_32() {
	u32 t;
	r_x = LOW32(r_x * 69069 + 1);
	r_y ^= LOW32(r_y << 13);
	r_y ^= LOW32(r_y >> 17);
	r_y ^= LOW32(r_y << 5);
	t = LOW32((r_w << 1) + r_z + r_carry);
	r_carry = (LOW32(r_z >> 2) + LOW32(r_w >> 3) + LOW32(r_carry >> 2)) >> 30;
	r_z = r_w;
	r_w = t;
	return LOW32(r_x + r_y + r_w);
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
	u32 n = (u32) LOW32(bm);
	if (n) {
		if LOW16(n) return LSB[LOW16(n)];
		else return 16 | LSB[LOW16(n >> 16)];
	} else {
		n = (u32)(bm >> 32);
		if LOW16(n) return 32 | LSB[LOW16(n)];
		else return 48 | LSB[LOW16(n >> 16)];
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

char _bitcnt(u64 bit) {
	char c = 0;
	while (bit) { bit &= (bit - 1); c++; }
	return c;
}

char bitcnt (u64 n) {    
     return BITC[LOW16(n)]
         +  BITC[LOW16(n >> 16)]
         +  BITC[LOW16(n >> 32)]
         +  BITC[LOW16(n >> 48)];
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
		int pp = 1 << (c ? 8-i/8 : 1+i/8);
		pawnprg[i + (c << 6)] = pp < 8 ? 0 : pp; 
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

void _init_rays(u64* rays, u64 (*rayFunc)(int, u64, int), int (*key)(u64, int)) {
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

void displaym(Move m) {
	printf("%c%c%c%c", 'a' + FROM(m) % 8, '1' + FROM(m) / 8,
		'a' + TO(m) % 8, '1' + TO(m) / 8);
	if (PROM(m)) printf("%c", pieceChar[PROM(m)]+32);
}

/* This one is the same as in OliThink 4. It's quite annoying code */
int bioskey() {
#ifndef _WIN32
  fd_set readfds;

  FD_ZERO (&readfds);
  FD_SET (fileno(stdin), &readfds);
  tv.tv_sec=0; tv.tv_usec=0;
  select(16, &readfds, 0, 0, &tv);

  return (FD_ISSET(fileno(stdin), &readfds));
#else
   static int init = 0, pipe;
   static HANDLE inh;
   DWORD dw;

   if (!init) {
     init = 1;
     inh = GetStdHandle(STD_INPUT_HANDLE);
     pipe = !GetConsoleMode(inh, &dw);
     if (!pipe) {
        SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
        FlushConsoleInputBuffer(inh);
      }
    }
    if (pipe) {
      if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
      return dw;
    } else {
      GetNumberOfConsoleInputEvents(inh, &dw);
      return dw <= 1 ? 0 : dw;
	}
#endif
}

void displaypv() {
	int i;
	for (i = 0; i < pvlength[0]; i++) {
		displaym(pv[0][i]); printf(" ");
	}
}

int isDraw(u64 hp, int nrep) {
	if (count > 0x1FFF) { //fifty > 3
		int i, c = 0, n = COUNT - (count >> 11);
		if (count >= 0x800*100) return 2; //100 plies
		for (i = COUNT - 2; i >= n; i--) 
			if (hstack[i] == hp && ++c == nrep) return 1; 
	} else if (!pieceb[PAWN] && !RQU) { //Check for mating material
		if (_bitcnt(colorb[0]) <= 2 && _bitcnt(colorb[1]) <= 2) return 3;
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

#define XORHASH(f, p, c) hashb ^= hashxor[(f) | (p) << 6 | (c) << 9]
/* doMove is both makeMove and unmakeMove, only for unmakeMove the globalflags have to be restored (counter, castle, enpass...) */
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
		count &= 0x7FF; //Reset Fifty Counter
		mat += c ? -pval[a] : +pval[a];
	}
	if (p == PAWN) {
		if (((f^t)&8) == 0) flags |= f^24; //Enpassant
		else if ((t&56) == 0 || (t&56) == 56) {
			xorBit(t, pieceb+PAWN);
			xorBit(t, pieceb+PROM(m));
			XORHASH(t, PAWN, c);
			XORHASH(t, PROM(m), c);
			mat += c ? pval[PAWN] - pval[PROM(m)] : -pval[PAWN] + pval[PROM(m)];
		}
		count &= 0x7FF; //Reset Fifty Counter
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
}

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
	xorBit(k, colorb+c);
	registerKing(PREMOVE(k, KING), KCAP(k, c), KMOVE(k), ml, mn, c);
	xorBit(k, colorb+c);
	if (bf > 1) return bf; //Doublecheck
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
	if (ch & (nmoves[k] | kmoves[k])) return 1; //We can't move anything between!

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
		if (RANK(f, c ? 0x20 : 0x18) && (BOARD & BIT[bf]) == 0 && (BIT[c ? f+16 : f-16] & pieceb[PAWN] & colorb[c] & apin))
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
				xorBit(clbd, colorb+c);
				hh = ROCC1(f);
				if (!(hh & BIT[kingpos[c]]) || !(hh & colorb[c^1] & RQU)) {
					a = a | BIT[ENPASS];
				}
				xorBit(clbd, colorb+c);
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

int generateCaps(u64 ch, int c, int ply) {
	int t, f = kingpos[c];
	int *mn = movenum + ply;
	int *ml = movelist + (ply << 8);
	u64 m, b, a, cb = colorb[c];
	u64 pin = pinnedPieces(f, c^1);
	*mn = 0;

	if (ch) return generateCheckEsc(ch, ~pin, c, f, ml, mn);
	registerKing(PREMOVE(f, KING), KCAP(f, c), 0LL, ml, mn, c);

	cb = colorb[c] & (~pin);

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
				xorBit(clbd, colorb+c);
				hh = ROCC1(f);
				if (!(hh & BIT[kingpos[c]]) || !(hh & colorb[c^1] & RQU)) {
					a = a | BIT[ENPASS];
				}
				xorBit(clbd, colorb+c);
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

	b = pieceb[KNIGHT] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, KNIGHT), NCAP(f, c), ml, mn);
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, BISHOP), BCAP(f, c), ml, mn);
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, ROOK), RCAP(f, c), ml, mn);
	}

	b = pieceb[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		registerCaps(PREMOVE(f, QUEEN), RCAP(f, c) | BCAP(f,c), ml, mn);
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

int identColor(int f) {
	if (TEST(f, colorb[1])) return 1;
	return 0;
}

int swap(Move m) //SEE Stuff
{
  int s_list[32];
  int f = FROM(m), t = TO(m), onmv = ONMV(m);
  int a_piece = pval[CAP(m)], piece = PIECE(m), c = onmv^1, nc = 1;
  u64 attacks, temp = 0, colstore0 = colorb[0], colstore1 = colorb[1];

  attacks = attacked(t, 0) | attacked(t, 1);
  s_list[0] = a_piece;
  a_piece = pval[piece];
  colorb[onmv] ^= BIT[f];
  if (piece & 4 || piece == 1) {
	int d = getDir(f, t);
    if (d == 32 || d == 64) attacks |= BOCC(t) & BQU;
	if (d == 8 || d == 16) attacks |= ROCC(t) & RQU;
  }
  attacks &= BOARD;

  while (attacks) {
      if ((temp = pieceb[PAWN] & colorb[c] & attacks)) piece = PAWN;
      else if ((temp = pieceb[KNIGHT] & colorb[c] & attacks)) piece = KNIGHT;
      else if ((temp = pieceb[BISHOP] & colorb[c] & attacks)) piece = BISHOP;
      else if ((temp = pieceb[ROOK] & colorb[c] & attacks)) piece = ROOK;
      else if ((temp = pieceb[QUEEN] & colorb[c] & attacks)) piece = QUEEN;
      else if ((temp = pieceb[KING] & colorb[c] & attacks)) piece = KING;
      else break;
 
	temp &= -(long long)temp;
	colorb[c] ^= temp;
	if (piece & 4 || piece == 1) {
	  if (piece & 1) attacks |= BOCC(t) & BQU;
	  if (piece & 2) attacks |= ROCC(t) & RQU;
	}
	attacks &= BOARD;

	s_list[nc] = -s_list[nc - 1] + a_piece;
	a_piece = pval[piece];
	nc++;
	c ^= 1;
  }

  while (--nc)
    if (s_list[nc] > -s_list[nc - 1])
      s_list[nc - 1] = -s_list[nc];

  colorb[0] = colstore0;
  colorb[1] = colstore1;
  return s_list[0];
}

/* In quiesce the moves are ordered just for the value of the captured piece */
Move qpick(Move* ml, int mn, int s) {
	Move m;
	int i, t, pi = 0, vmax = -HEUR;
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

Move killer[128];
int history[0x1000];
/* In normal search some basic move ordering heuristics are used */
Move spick(Move* ml, int mn, int s, int ply) {
	Move m; int cap;
	int i, t, pi = 0, vmax = -HEUR;
	for (i = s; i < mn; i++) {
		m = ml[i];
		cap = CAP(m);
		if (cap) {
			t = capval[cap];
			if (t > vmax) {
				vmax = t;
				pi = i;
			}
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

/* The evulation for Color c. It's just mobility stuff. Pinned pieces are still awarded for limiting opposite's king */
int evalc(int c, int* sf) {
	int t, f;
	int mn = 0;
	int oc = c^1;
	u64 ocb = colorb[oc];
	u64 m, b, a, cb = colorb[c];
	u64 kn = kmoves[kingpos[oc]];
	u64 pin = pinnedPieces(kingpos[c], oc);

	b = pieceb[PAWN] & colorb[c];
	while (b) {
		f = pullLsb(&b);
		mn += pawnprg[f + (c << 6)];
		m = PMOVE(f, c);
		a = POCC(f, c);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		if (BIT[f] & pin) {
			if (!(getDir(f, kingpos[c]) & 16)) m = 0;
		} else {
			mn += _bitcnt(a & pieceb[PAWN] & colorb[c]) << 2;
		}
		if (m) mn += 8;
	}

	cb =  colorb[c] & (~pin);
	b = pieceb[KNIGHT] & cb;
	while (b) {
		*sf += 1;
		f = pullLsb(&b);
		a = nmoves[f];
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += nmobil[f];
	}

	b = pieceb[KNIGHT] & pin;
	while (b) {
		*sf += 1;
		f = pullLsb(&b);
		a = nmoves[f];
		if (a & kn) mn += _bitcnt(a & kn) << 4;
	}

	xorBit(kingpos[oc], colorb+oc); //Opposite King doesn't block mobility at all
	b = pieceb[QUEEN] & cb;
	while (b) {
		*sf += 4;
		f = pullLsb(&b);
		a = RATT1(f) | RATT2(f) | BATT3(f) | BATT4(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += bitcnt(a);
	}

	colorb[oc] ^= RQU & ocb; //Opposite Queen & Rook doesn't block mobility for bishop
	b = pieceb[BISHOP] & cb;
	while (b) {
		*sf += 1;
		f = pullLsb(&b);
		a = BATT3(f) | BATT4(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += bitcnt(a) << 3;
	}

	colorb[oc] ^= pieceb[ROOK] & ocb; //Opposite Queen doesn't block mobility for rook.
	colorb[c] ^= pieceb[ROOK] & cb; //Own non-pinned Rook doesn't block mobility for rook.
	b = pieceb[ROOK] & cb;
	while (b) {
		*sf += 2;
		f = pullLsb(&b);
		a = RATT1(f) | RATT2(f);
		if (a & kn) mn += _bitcnt(a & kn) << 4;
		mn += bitcnt(a) << 2;
	}

	colorb[c] ^= pieceb[ROOK] & cb; // Back
	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
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

	colorb[oc] ^= pieceb[QUEEN] & ocb; //Back
	xorBit(kingpos[oc], colorb+oc); //Back
	return mn;
}

int eval1 = 0;
int eval(int c) {
	int sf0 = 0, sf1 = 0;
	int ev0 = evalc(0, &sf0);
	int ev1 = evalc(1, &sf1);
	eval1++;

	if (sf1 > 6) ev0 -= kmobil[kingpos[0]];
	else if (sf1 < 6) ev0 += kmobil[kingpos[0]]*(6-sf1);
	if (sf0 > 6) ev1 -= kmobil[kingpos[1]];
	else if (sf0 < 6) ev1 += kmobil[kingpos[1]]*(6-sf0);

	return (c ? (ev1 - ev0) : (ev0 - ev1));
}

u64 nodes;
u64 qnodes;
int quiesce(u64 ch, int c, int ply, int alpha, int beta) {
	int i, w = -99999, poff;
	int flagstore = flags;
	int countstore = count;
	int matstore = mat;
	int cmat = c ? -mat: mat;
	if (!ch) do {
		if (cmat + 200 < alpha) break;
		if (cmat - 200 >= beta) return beta;
		w = eval(c) + cmat;
		if (w > alpha) {
			if (w >= beta) return beta;
			alpha = w;
		}
	} while(0);

	generateCaps(ch, c, ply);
	if (movenum[ply] == 0) return ch ? -32000 + ply : (w == -99999 ? eval(c) + cmat : w);
	poff = ply << 8;

	for (i = 0; i < movenum[ply]; i++) {
		Move m = qpick(movelist + poff, movenum[ply], i);
		if (!ch && !PROM(m) && pval[PIECE(m)] > pval[CAP(m)] && swap(m) < 0) continue;

		doMove(m, c);
		qnodes++;

		w = -quiesce(attacked(kingpos[c^1], c^1), c^1, ply+1, -beta, -alpha);

		doMove(m, c);
		flags = flagstore;
		count = countstore;
		mat = matstore;

		if (w > alpha) {
			if (w >= beta) return beta;
			alpha = w;
		}
	}
	return alpha;
}

#define HASHP (hashb ^ hashxor[flags | 1024 | c << 11])
#define HASHB ((hashb ^ hashxor[flags | 1024]) ^ hashxor[c | d << 1 | 2048])
int search(u64 ch, int c, int d, int ply, int alpha, int beta, int pvnode, int null) {
	int i, j, n, w, poff, asave, first, best;
	int flagstore = flags;
	int countstore = count;
	int matstore = mat;
	Move hsave, hmove;
	u64 hb, hp, he;
	pvlength[ply] = ply;
	if ((++nodes & CNODES) == 0) {
		u64 consumed = getTime() - starttime;
		if (consumed > maxtime || (consumed > searchtime && !noabort) || bioskey()) sabort = 1;
	}
	if (sabort) return 0;

	hp = HASHP;
	if (ply && isDraw(hp, 1)) return 0;

	if (d == 0) return quiesce(ch, c, ply, alpha, beta);
	hstack[COUNT] = hp;

	hb = HASHB;
	
	he = hashDB[hb & HMASK];
	if (!((he^hb) & HINV)) {
		w = (u32)LOW16(he) - 32768;
		if (he & 0x10000) {
			null = 0;
			if (w <= alpha) return alpha;
		} else {
			if (w >= beta) return beta;
		}
	}

	hsave = hmove = 0;
	he = hashDP[hp & HMASK];
	if (!((he^hp) & HINV)) hsave = hmove = (Move)(he & HMASK);

	if (!pvnode && !ch && null && d > 1 && bitcnt(colorb[c] & (~pieceb[PAWN]) & (~pinnedPieces(kingpos[c], c^1))) > 2) {
		flags &= 960;
		count += 0x801;
		w = -search(0LL, c^1, d/2-1, ply+1, -beta, -alpha, 0, 0);
		flags = flagstore;
		count = countstore;
		if (!sabort && w >= beta) {
			hashDB[hb & HMASK] = (hb & HINV) | (w + 32768); 
			return beta;
		}
	}

	if (d >= 4 && hmove == 0) { // Simple version of Internal Iterative Deepening
		w = search(ch, c, d-3, ply, alpha, beta, 0, pvnode);
		he = hashDP[hp & HMASK];
		if (!((he^hp) & HINV)) hsave = hmove = (Move)(he & HMASK);
	}

	poff = ply << 8;
	n = i = -1;
	best = pvnode ? alpha : -32001;
	asave = alpha;
	first = 1;
	while (++i != n) {
		Move m;
		u64 nch;
		int ext = 0;
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
		doMove(m, c);

		nch = attacked(kingpos[c^1], c^1);
		if (nch) ext++; // Check Extension
		else if (PIECE(m) == PAWN && ((TO(m) & 48) == 48  || TO(m) & 48) == 0) ext++; //Pawn short to promote

		if (first && pvnode) {
			w = -search(nch, c^1, d-1+ext, ply+1, -beta, -alpha, 1, 1);
			if (!ply) noabort = (iter > 1 && w < value[iter-1] - 40) ? 1 : neverabort;
		} else {
			w = -search(nch, c^1, d-1+ext, ply+1, -alpha-1, -alpha, 0, 1);
			if (w > alpha && w < beta && pvnode) w = -search(nch, c^1, d-1+ext, ply+1, -beta, -alpha, 1, 1);
		}
		doMove(m, c);
		flags = flagstore;
		count = countstore;
		mat = matstore;

		if (!sabort && w > best) {
			if (w > alpha) {
				hashDP[hp & HMASK] = (hp & HINV) | m;
				alpha = w;
			}
			if (w >= beta) {
				if (CAP(m) == 0) {
					killer[ply] = m;
					history[m & 0xFFF]++;
				}
				hashDB[hb & HMASK] = (hb & HINV) | (w + 32768); 
				return beta;
			}
			if (pvnode && w >= alpha) {
				pv[ply][ply] = m;
				for (j = ply +1; j < pvlength[ply +1]; j++) pv[ply][j] = pv[ply +1][j];
				pvlength[ply] = pvlength[ply +1];
				if (!ply && iter > 1 && w > value[iter-1] - 20) noabort = neverabort;
				if (w == 31999 - ply) return w;
			}
			best = w;
		}
		first = 0;
	}
	if (pvnode) {
		if (!sabort && asave == alpha) hashDB[hb & HMASK] = (hb & HINV) | 0x10000 | (asave + 32768);
	} else {
		if (!sabort && best < beta) hashDB[hb & HMASK] = (hb & HINV) | 0x10000 | (best + 32768);
	}
	return alpha;
}

int execMove(Move m) {
	int i, c;
	doMove(m, onmove);
	onmove ^= 1; 
	c = onmove;
	hstack[COUNT] = HASHP;
	for (i = 0; i < 127; i++) killer[i] = killer[i+1];
	for (i = 0; i < 0x1000; i++) history[i] = 0;
	switch (isDraw(HASHP, 2)) {
		case 1: printf("1/2-1/2 {Draw by Repetition}\n"); return 1;
		case 2: printf("1/2-1/2 {Draw by Fifty Move Rule}\n"); return 2;
		case 3: printf("1/2-1/2 {Insufficient material}\n"); return 3;
		default:
			i = GENERATE(c);
			if (movenum[0] == 0) {
				if (!i) {
					printf("1/2-1/2 {Stalemate}\n"); return 4;
				} else {
					printf(c ? "1-0 {White mates}\n" : "0-1 {Black mates}\n"); return 5 + c;
				}
			}
	}
	return 0;
}

int engine = -1;
int sd = 32;
int ttime = 30000;
int mps = 0;
int base = 5;
int inc = 0;
int ponder = 0;
int post = 1;
char *sfen = "rnbqkbnr/pppppppp/////PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int calc(int sd, int tm, int pon) {
		int t1 = 0, m2go = 32;
		u64 ch = attacked(kingpos[onmove], onmove);
		eval1 = sabort = 0;
		qnodes = nodes = 0LL;
		if (pon) tm = 99999999;
		if (mps > 0) m2go = 1 + mps - ((COUNT/2) % mps);

		searchtime = (tm*10LL)/m2go + inc*1000LL;
		maxtime = inc ? tm*3LL : tm*2LL;

		starttime = getTime();
		for (iter = 1; iter <= sd; iter++) {
			noabort = neverabort;
			value[iter] = search(ch, onmove, iter, 0, -32000, 32000, 1, 0);
			t1 = (int)(getTime() - starttime);
			if (sabort && !pvlength[0]) break;
			if (post && pvlength[0] > 0) { 
				printf("%2d %5d %6d %9llu  ", iter, value[iter], t1/10, nodes + qnodes);
				displaypv(); printf("\n"); 
			}
			if (!neverabort && (iter >= 32000-value[iter] || sabort || t1 > searchtime/2)) break;
		}
		if (pon) return -1;
		printf("%d. ... ", COUNT/2 + 1);
		displaym(pv[0][0]); printf("\n");

		if (post) printf("\nkibitz W: %d Nodes: %llu QNodes: %llu Evals: %d cs: %d knps: %llu\n", value[iter > sd ? sd : iter], nodes, qnodes, eval1, t1/10, (nodes+qnodes)/(t1+1));
		return execMove(pv[0][0]);
}

int input() {
		int i;
		char buf[256];
		fgets(buf,255,stdin);
		if (!strncmp(buf,"xboard",6)) printf("feature setboard=1 myname=\"OliThink " VER "\" colors=0 done=1\n");
		if (!strncmp(buf,"quit",4)) return -2;
		if (!strncmp(buf,"force",5)) engine = -1;
		if (!strncmp(buf,"go",2)) engine = onmove;
		if (!strncmp(buf,"new",2)) { _parse_fen(sfen); engine = 1; }
		if (!strncmp(buf,"setboard",8)) _parse_fen(buf+9);
		if (!strncmp(buf,"sd",2)) sscanf(buf+3,"%d",&sd);
		if (!strncmp(buf,"time",4)) sscanf(buf+5,"%d",&ttime);
		if (!strncmp(buf,"level",4)) sscanf(buf+6,"%d %d %d",&mps, &base, &inc);
		if (!strncmp(buf,"hard",4)) { ponder = 1; return 0; }
		if (!strncmp(buf,"easy",4)) ponder = 0;
		if (!strncmp(buf,"post",4)) post = 1;
		if (!strncmp(buf,"nopost",6)) post = 0;

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
			if (_m == 0) fprintf(stderr,"Illegal move: %s\n",buf);
			else return execMove(_m);
		}
		return -1;
}

int main(int argc, char **argv)
{
	int i, ex = -1;
	setbuf(stdout, NULL);
	setbuf(stdin, NULL);
	signal(SIGINT, SIG_IGN);
	if (argc > 1 && !strncmp(argv[1],"-sd",3)) {
		neverabort = 1;
		if (argc > 2) {
			sscanf(argv[2], "%d", &sd);
			if (argc > 3) sfen = argv[3];
		}
	}
	for (i = 0; i < 0x10000; i++) LSB[i] = _slow_lsb(i);
	for (i = 0; i < 0x10000; i++) BITC[i] = _bitcnt(i);
	for (i = 0; i < 4096; i++) hashxor[i] = _rand_64();
	for (i = 0; i < HSIZE; i++) hashDB[i] = hashDP[i] = 0LL;
	for (i = 0; i < 64; i++) BIT[i] = 1LL << i;
	for (i = 0; i < 128; i++) pmoves[i] = 0LL;
	for (i = 0; i < 384; i++) pcaps[i] = 0LL;
	for (i = 0; i < 64; i++) bmask45[i] = _bishop45(i, 0LL, 0) | BIT[i];
	for (i = 0; i < 64; i++) bmask135[i] = _bishop135(i, 0LL, 0) | BIT[i];
	for (i = 0; i < 64; i++) crevoke[i] = 0x3FF;
	for (i = 0; i < 64; i++) kmoves[i] = nmoves[i] = 0LL;
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
	_init_pawns(pmoves, pcaps, 0);
	_init_pawns(pmoves + 64, pcaps + 64, 1);
	_parse_fen(sfen);
	for (i = 0; i < 64; i++) nmobil[i] = bitcnt(nmoves[i])*8;
	for (i = 0; i < 64; i++) kmobil[i] = (bitcnt(nmoves[i])/2)*2;

	for (;;) {
		if (engine == onmove) ex = calc(sd, ttime, 0);
		else if (ex == 0 && ponder && engine != -1) ex = calc(sd, ttime, 1);

		if (!ponder ||engine == -1 || ex != 0) ex = input();
		if (ex == -2) break;
	}
	return 0;
}
