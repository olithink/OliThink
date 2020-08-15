/* OliThink5 (c) Oliver Brausch 05.Jul.2020, ob112@web.de, http://brausch.org */
#define VER "5.5.0"
#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#if defined(_WIN32) || defined(_WIN64)
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
const int pval[] = {0, 100, 290, 0, 100, 310, 500, 980};
const int pawnrun[] = {0, 0, 1, 8, 16, 32, 64, 128};

#define FROM(x) ((x) & 63)
#define TO(x) (((x) >> 6) & 63)
#define PROM(x) (((x) >> 12) & 7)
#define PIECE(x) (((x) >> 15) & 7)  
#define ONMV(x) (((x) >> 18) & 1)
#define CAP(x) (((x) >> 19) & 7)
#define MAXSCORE 30000

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
#define COUNT (count & 0x3FF)
#define MEVAL(w) (w > MAXSCORE-500 ? (200000+MAXSCORE+1-w)/2 : (w < 500-MAXSCORE ? (-200000-MAXSCORE-w)/2 : w))
#define NOMATEMAT(s, t, c) ((s <= 4 || (s <= 6 && t > 2)) && (pieceb[PAWN] & colorb[c]) == 0)

#define HSIZEB 0x800000
#define HMASKB (HSIZEB-1)
#define HINVB 0xFFFFFFFFFFFFFFFFLL & ~HMASKB

#define HSIZEP 0x400000
#define HMASKP (HSIZEP-1)
#define HINVP 0xFFFFFFFFFFFFFFFFLL & ~HMASKP

u64 hashDB[HSIZEB];
u64 hashDP[HSIZEP];
u64 hashb;
u64 hstack[0x800];
u64 mstack[0x800];

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
static u64 pawnfree[128];
static u64 pawnfile[128];
static u64 pawnhelp[128];
Move movelist[128*256];
int movenum[128];
Move pv[128][128];
int pvlength[128];
int value[128];
int iter;
const char pieceChar[] = "*PNK.BRQ";
u64 maxtime, starttime;
int sabort, ics = 0, ponder = 0, pondering = 0, analyze = 0, resign = 0, lastw = 0;
Move pon = 0;
int count, flags, mat, onmove, engine =-1;
int sd = 64;
int kingpos[2];
u64 pieceb[8];
u64 colorb[2];
char irbuf[256];
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

int _getpiece(char s, int *c) {
	int i;
	for (i = 1; i < 8; i++) 
		if (pieceChar[i] == s) { *c = 0; return i; } 
		else if (pieceChar[i] == s-32) { *c = 1; return i; }
	return 0;
}

char *sfen = "rnbqkbnr/pppppppp/////PPPPPPPP/RNBQKBNR w KQkq - 0 1";
void reseth(int level);
int book;
void _parse_fen(char *fen) {
	char s, mv, pos[128], cas[5], enps[3];
	int c, i, halfm = 0, fullm = 1, col = 0, row = 7;
	for (i = 0; i < 8; i++) pieceb[i] = 0LL;
	colorb[0] = colorb[1] = hashb = 0LL;
	mat = book = i = c = cas[0] = enps[0] = 0;
	sscanf(fen, "%s %c %s %s %d %d", pos, &mv, cas, enps, &halfm, &fullm);
	while ((s = pos[i++])) {
		if (s == '/') {
			row--;
			col = 0;
		} else if (s >= '1' && s <= '8') {
			col += s - '0';
		} else {
			int p = _getpiece(s, &c);
			if (p == KING) kingpos[c] = row*8 + col;
			else mat += c ? -pval[p] : pval[p];
			hashb ^= hashxor[col | row << 3 | i << 6 | (c ? 512 : 0)];
			setBit(row*8 + col, pieceb + p);
			setBit(row*8 + (col++), colorb + c);			
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
	if (enps[0] >= 'a' && enps[0] <= 'h' && enps[1] >= '1' && enps[1] <= '8') flags |= 8*(enps[1] - '1') + enps[0] - 'a'; 
	count = (fullm - 1)*2 + onmove + (halfm << 10);
	for (i = 0; i < COUNT; i++) hstack[i] = 0LL;
	if (fen != sfen) reseth(3);
}

int parseMoveNExec(char*, int, Move*, int);
int parseMove(char*, int, Move);
int protV2(char*,int);
#define BKSIZE 8192
Move bkmove[BKSIZE*32];
int bkflag[BKSIZE];
int bkcount[3];

void _readbook(char *bk) {
	char buf[256], s0[64], s1[64], s2[64];
	int k, n = 0;
	FILE *in = fopen(bk, "r");
	bkcount[0] = bkcount[1] = 0;
	for (k = 0; k < BKSIZE; k++) bkflag[k] = 2;
	if (in != NULL) {
		while (!feof(in)) {
			fgets(buf, 255, in);
			if (buf[0] == '[') {
				sscanf(buf,"%s %s", s1, s2);
				if (!strncmp(s2, "\"OliThink", 8)) bkflag[n] = strncmp(s1, "[Black", 6) ? 0 : 1;
				else if (!strncmp(s1, "[Result", 7)) {
					if (bkflag[n]) {
						if (!strncmp(s2, "\"0-1", 4)) bkflag[n] = 2;
					} else if (strncmp(s2, "\"1-0", 4)) bkflag[n] = 2;
				}
			} else if (buf[0] == '1' && buf[1] == '.' && bkflag[n] < 2) {
				int i = 0, j = 0;
				_parse_fen(sfen);
				for (;;) {
					if (strchr(buf+i, ' ') == NULL) break;
					sscanf(buf+i,"%s %s", s0, s2);
					if (s0[0] < '1' || s0[0] > '9') break;
					i += (int)(strlen(s0) + strlen(s2) + 2);
					if (s0[strlen(s0)-1] == '.') {
						strcpy(s1, s2); s2[0] = 0;
						sscanf(buf+i,"%s", s2);
						i += (int)(strlen(s2) + 1);
					} else 
					sscanf(s0,"%*[^.].%[^.]", s1);
					parseMoveNExec(s1, 0, bkmove + n*32+ (j++), 0);
					if (s2[0] == 0 || s2[0] == '*') break;
					parseMoveNExec(s2, 1, bkmove + n*32+ (j++), 0);
					if (j > 30 || i >= strlen(buf)) break;
				}
				bkmove[n*32 + j] = 0;
				if (j) bkcount[bkflag[n]]++;
				if (++n == BKSIZE) break;
			} 
		}
		fclose(in);
	}
	_parse_fen(sfen);
	if (bkcount[0] > 0 || bkcount[1] > 0) book = 1;
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

int key090(u64 b, int f) {
	u64 _b = (b >> (f&7)) & 0x0101010101010101LL;
	_b = _b * 0x0080402010080400LL;
	return (int)(_b >> 57);
}

int keyDiag(u64 _b) {
	_b = _b * 0x0202020202020202LL;
	return (int)(_b >> 57);
}

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

void _init_pawns(u64* moves, u64* caps, u64* freep, u64* filep, u64* helpp, int c) {
	int i, j;
	for (i = 0; i < 64; i++) {
		int rank = i/8;
		int file = i&7;
		int m = i + (c ? -8 : 8);
		pawnprg[i + (c << 6)] = pawnrun[c ? 7-rank : rank];
		for (j = 0; j < 64; j++) {
			int jrank = j/8;
			int jfile = j&7;
			int dfile = (jfile - file)*(jfile - file);
			if (dfile > 1) continue;
			if ((c && jrank < rank) || (!c && jrank > rank)) {//The not touched half of the pawn
				if (dfile == 0) setBit(j, filep + i);
				setBit(j, freep + i);
			} else if (dfile != 0 && (jrank - rank)*(jrank - rank) <= 1) {
				setBit(j, helpp + i);
			}
		}
		if (m < 0 || m > 63) continue;
		setBit(m, moves + i);
		if (file > 0) {
			m = i + (c ? -9 : 7);
			if (m < 0 || m > 63) continue;
			setBit(m, caps + i);
			setBit(m, caps + i + 128*(2 - c));
		}
		if (file < 7) {
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
	if (pon && pondering) { printf("("); displaym(pon); printf(") "); }
	for (i = 0; i < pvlength[0]; i++) {
		displaym(pv[0][i]); printf(" ");
	}
}

int isDraw(u64 hp, int nrep) {
	if (count > 0xFFF) { //fifty > 3
		int i, c = 0, n = COUNT - (count >> 10);
		if (count >= 0x400*100) return 2; //100 plies
		for (i = COUNT - 2; i >= n; i--) 
			if (hstack[i] == hp && ++c == nrep) return 1; 
	} else if (!(pieceb[PAWN] | RQU)) { //Check for mating material
		if (_bitcnt(BOARD) <= 3) return 3;
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
	return (!((f - t) % 9)) ? 32 : 64;
}

#define XORHASH(f, p, c) hashb ^= hashxor[(f) | (p) << 6 | (c) << 9]
/* move is for both doMove and undoMove, only for unmakeMove the globalflags have to be restored (counter, castle, enpass...) */
void move(Move m, int c) {
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
	count += 0x401;
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
		count &= 0x3FF; //Reset Fifty Counter
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
		count &= 0x3FF; //Reset Fifty Counter
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

void doMove(Move m, int c) {
	mstack[COUNT] = count | (flags << 17) | (((u64)(mat + 0x4000)) << 27) | (((u64)m) << 42);
	move(m, c);
}

void undoMove(Move m, int c) {
	u64 u = mstack[COUNT - 1];
	move(m, c);
	count = u & 0x1FFFF;
	flags = (u >> 17L) & 0x3FF;
	mat = ((u >> 27L) & 0x7FFF) - 0x4000;
}

void regMoves(Move m, u64 bt, int* mlist, int* mn, int cap) {
	while (bt) {
		int t = pullLsb(&bt);
		mlist[(*mn)++] = m | _TO(t) | (cap ? _CAP(identPiece(t)) : 0LL);
	}
}

void regMovesCaps(Move m, u64 bc, u64 bm, int* mlist, int* mn) {
	regMoves(m, bc, mlist, mn, 1);
	regMoves(m, bm, mlist, mn, 0);
}

void regPromotions(int f, int c, u64 bt, int* mlist, int* mn, int cap, int queen) {
	while (bt) {
		int t = pullLsb(&bt);
		Move m = f | _ONMV(c) | _PIECE(PAWN) | _TO(t) | (cap ? _CAP(identPiece(t)) : 0);
		if (queen) mlist[(*mn)++] = m | _PROM(QUEEN);
		mlist[(*mn)++] = m | _PROM(KNIGHT);
		mlist[(*mn)++] = m | _PROM(ROOK);
		mlist[(*mn)++] = m | _PROM(BISHOP);
	}
}

void regKings(Move m, u64 bt, int* mlist, int* mn, int c, int cap) {
	while (bt) {
		int t = pullLsb(&bt);
		if (battacked(t, c)) continue;
		mlist[(*mn)++] = m | _TO(t) | (cap ? _CAP(identPiece(t)) : 0LL);
	}
}

int generateCheckEsc(u64 ch, u64 apin, int c, int k, int *ml, int *mn) {
	u64 cc, fl;
	int d, bf = _bitcnt(ch);
	xorBit(k, colorb+c);
	regKings(PREMOVE(k, KING), KCAP(k, c), ml, mn, c, 1);
	regKings(PREMOVE(k, KING), KMOVE(k), ml, mn, c, 0);
	xorBit(k, colorb+c);
	if (bf > 1) return bf; //Doublecheck
	bf = getLsb(ch);

	cc = attacked(bf, c^1) & apin;  //Can we capture the checker?
	while (cc) {
		int cf = pullLsb(&cc);
		int p = identPiece(cf);
		if (p == PAWN && RANK(cf, c ? 0x08 : 0x30)) 
			regPromotions(cf, c, ch, ml, mn, 1, 1);
		else 
			regMovesCaps(PREMOVE(cf, p), ch, 0LL, ml, mn);
	}
	if (ENPASS && (ch & pieceb[PAWN])) { //Enpassant capture of attacking Pawn
		cc = PCAP(ENPASS, c^1) & pieceb[PAWN] & apin;
		while (cc) {
			int cf = pullLsb(&cc);
			regMovesCaps(PREMOVE(cf, PAWN), BIT[ENPASS], 0LL, ml, mn);
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
			regMovesCaps(PREMOVE(cf, p), 0LL, BIT[f], ml, mn);
		}
		bf = c ? f+8 : f-8;
		if (bf < 0 || bf > 63) continue;
		if (BIT[bf] & pieceb[PAWN] & colorb[c] & apin) {
			if (RANK(bf, c ? 0x08 : 0x30)) 
				regPromotions(bf, c, BIT[f], ml, mn, 0, 1);
			else
				regMovesCaps(PREMOVE(bf, PAWN), 0LL, BIT[f], ml, mn);
		}
		if (RANK(f, c ? 0x20 : 0x18) && (BOARD & BIT[bf]) == 0 && (BIT[c ? f+16 : f-16] & pieceb[PAWN] & colorb[c] & apin))
			regMovesCaps(PREMOVE(c ? f+16 : f-16, PAWN), 0LL, BIT[f], ml, mn);
	}
	return 1;
}

int generateNonCaps(u64 ch, int c, int f, u64 pin, int *ml, int *mn) {
	int t;
	u64 m, b, cb = colorb[c] & (~pin);

	regKings(PREMOVE(f, KING), KMOVE(f), ml, mn, c, 0);

	b = pieceb[PAWN] & cb;
	while (b) {
		f = pullLsb(&b);
		m = PMOVE(f, c);
		if (m && RANK(f, c ? 0x30 : 0x08)) m |= PMOVE(c ? f-8 : f+8, c);
		if (RANK(f, c ? 0x08 : 0x30)) {
			u64 a = PCAP(f, c);
			regPromotions(f, c, m, ml, mn, 0, 0);
			if (a) regPromotions(f, c, a, ml, mn, 1, 0);
		} else {
			regMoves(PREMOVE(f, PAWN), m, ml, mn, 0);
		}
	}

	b = pin & pieceb[PAWN]; 
	while (b) {
		f = pullLsb(&b);
		t = getDir(f, kingpos[c]);
		if (t & 8) continue;
		m = 0LL;
		if (t & 16) {
			m = PMOVE(f, c);         
			if (m && RANK(f, c ? 0x30 : 0x08)) m |= PMOVE(c ? f-8 : f+8, c);
		}
		if (RANK(f, c ? 0x08 : 0x30)) {
			u64 a = (t & 32) ? PCA3(f, c) : ((t & 64) ? PCA4(f, c) : 0LL);
			regPromotions(f, c, m, ml, mn, 0, 0);
			if (a) regPromotions(f, c, a, ml, mn, 1, 0);
		} else {
			regMoves(PREMOVE(f, PAWN), m, ml, mn, 0);
		}
	}

	b = pieceb[KNIGHT] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, KNIGHT), NMOVE(f), ml, mn, 0);
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, ROOK), RMOVE(f), ml, mn, 0);
		if (CASTLE && !ch) {
			if (c) {
				if ((flags & 128) && (f == 63) && (RMOVE1(63) & BIT[61]))
					if (!DUALATT(61, 62, c)) regMoves(PREMOVE(60, KING), BIT[62], ml, mn, 0);
				if ((flags & 512) && (f == 56) && (RMOVE1(56) & BIT[59]))
					if (!DUALATT(59, 58, c)) regMoves(PREMOVE(60, KING), BIT[58], ml, mn, 0);
			} else {
				if ((flags & 64) && (f == 7) && (RMOVE1(7) & BIT[5]))
					if (!DUALATT(5, 6, c)) regMoves(PREMOVE(4, KING), BIT[6], ml, mn, 0);
				if ((flags & 256) && (f == 0) && (RMOVE1(0) & BIT[3]))
					if (!DUALATT(3, 2, c)) regMoves(PREMOVE(4, KING), BIT[2], ml, mn, 0);
			}
		}
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, BISHOP), BMOVE(f), ml, mn, 0);
	}

	b = pieceb[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, QUEEN), RMOVE(f) | BMOVE(f), ml, mn, 0);
	}

	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) regMoves(PREMOVE(f, p), RMOVE1(f), ml, mn, 0);
		if ((t & 18) == 18) regMoves(PREMOVE(f, p), RMOVE2(f), ml, mn, 0);
		if ((t & 33) == 33) regMoves(PREMOVE(f, p), BMOVE3(f), ml, mn, 0);
		if ((t & 65) == 65) regMoves(PREMOVE(f, p), BMOVE4(f), ml, mn, 0);
	}
	return 0;
}

int generateCaps(u64 ch, int c, int f, u64 pin, int *ml, int *mn) {
	int t;
	u64 m, b, a, cb = colorb[c] & (~pin);

	regKings(PREMOVE(f, KING), KCAP(f, c), ml, mn, c, 1);

	b = pieceb[PAWN] & cb;
	while (b) {
		f = pullLsb(&b);
		a = PCAP(f, c);
		if (RANK(f, c ? 0x08 : 0x30)) {
			regMovesCaps(PREMOVE(f, PAWN) | _PROM(QUEEN), a, PMOVE(f, c), ml, mn);
		} else {
			if (ENPASS && (BIT[ENPASS] & pcaps[(f) | ((c)<<6)])) {
				u64 hh;
				int clbd = ENPASS^8;
				xorBit(clbd, colorb+(c^1));
				hh = ROCC1(f);
				if (!(hh & BIT[kingpos[c]]) || !(hh & colorb[c^1] & RQU)) {
					a = a | BIT[ENPASS];
				}
				xorBit(clbd, colorb+(c^1));
			}
			regMoves(PREMOVE(f, PAWN), a, ml, mn, 1);
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
			regMovesCaps(PREMOVE(f, PAWN) | _PROM(QUEEN), a, m, ml, mn);
		} else {
			regMoves(PREMOVE(f, PAWN), a, ml, mn, 1);
		}
	}

	b = pieceb[KNIGHT] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, KNIGHT), NCAP(f, c), ml, mn, 1);
	}

	b = pieceb[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, BISHOP), BCAP(f, c), ml, mn, 1);
	}

	b = pieceb[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, ROOK), RCAP(f, c), ml, mn, 1);
	}

	b = pieceb[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		regMoves(PREMOVE(f, QUEEN), RCAP(f, c) | BCAP(f,c), ml, mn, 1);
	}

	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) regMoves(PREMOVE(f, p), RCAP1(f, c), ml, mn, 1);
		if ((t & 18) == 18) regMoves(PREMOVE(f, p), RCAP2(f, c), ml, mn, 1);
		if ((t & 33) == 33) regMoves(PREMOVE(f, p), BCAP3(f, c), ml, mn, 1);
		if ((t & 65) == 65) regMoves(PREMOVE(f, p), BCAP4(f, c), ml, mn, 1);
	}
	return 0;
}

#define GENERATE(c) generate(attacked(kingpos[c], c), c, 0, 1, 1)
int generate(u64 ch, int c, int ply, int cap, int noncap) {
	int f = kingpos[c];
	u64 pin = pinnedPieces(f, c^1);
	int *mn = movenum + ply;
	int *ml = movelist + (ply << 8);
	*mn = 0;
	if (ch) return generateCheckEsc(ch, ~pin, c, f, ml, mn);
	if (cap) generateCaps(ch, c, f, pin, ml, mn);
	if (noncap) generateNonCaps(ch, c, f, pin, ml, mn);
	return *mn;
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
	int i, t, pi = 0, vmax = -9999;
	for (i = s; i < mn; i++) {
		m = ml[i];
		t = pval[CAP(m)];
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
long long history[0x1000];
/* In normal search some basic move ordering heuristics are used */
Move spick(Move* ml, int mn, int s, int ply) {
	Move m;
	int i, pi = 0; long long vmax = -9999L;
	for (i = s; i < mn; i++) {
		m = ml[i];
		if (m == killer[ply]) {
			pi = i;
			break;
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

/* The evulation for Color c. It's almost only mobility stuff. Pinned pieces are still awarded for limiting opposite's king */
int evalc(int c, int* sf) {
	int t, f;
	int mn = 0, katt = 0;
	int oc = c^1;
	u64 ocb = colorb[oc];
	u64 m, b, a, cb;
	u64 kn = kmoves[kingpos[oc]];
	u64 pin = pinnedPieces(kingpos[c], oc);

	b = pieceb[PAWN] & colorb[c];
	while (b) {
		int ppos = 0;
		f = pullLsb(&b);
		t = f + (c << 6);
		ppos = pawnprg[t];
		m = PMOVE(f, c);
		a = POCC(f, c);
		if (a & kn) katt += _bitcnt(a & kn) << 3;
		if (BIT[f] & pin) {
			if (!(getDir(f, kingpos[c]) & 16)) m = 0;
		} else {
			ppos += _bitcnt(a & pieceb[PAWN] & colorb[c]) << 2;
		}
		if (m) ppos += 8; else ppos -= 8;
		/* The only non-mobility eval is the detection of free pawns/hanging pawns */
		if (!(pawnfile[t] & pieceb[PAWN] & ocb)) { //Free file?
			if (!(pawnfree[t] & pieceb[PAWN] & ocb)) ppos *= 2; //Free run?
			if (!(pawnhelp[t] & pieceb[PAWN] & colorb[c])) ppos -= 33; //Hanging backpawn?
		}

		mn += ppos;

	}

	cb = colorb[c] & (~pin);
	b = pieceb[KNIGHT] & cb;
	while (b) {
		*sf += 2;
		f = pullLsb(&b);
		a = nmoves[f];
		if (a & kn) katt += _bitcnt(a & kn) << 3;
		mn += nmobil[f];
	}

	b = pieceb[KNIGHT] & pin;
	while (b) {
		*sf += 2;
		f = pullLsb(&b);
		a = nmoves[f];
		if (a & kn) katt += _bitcnt(a & kn) << 3;
	}

	xorBit(kingpos[oc], colorb+oc); //Opposite King doesn't block mobility at all
	colorb[c] ^= pieceb[QUEEN] & cb; //Own non-pinned Queen doesn't block mobility for anybody.
	b = pieceb[QUEEN] & cb;
	while (b) {
		*sf += 9;
		f = pullLsb(&b);

		colorb[c] ^= pieceb[BISHOP] & cb;	

		a = BATT3(f) | BATT4(f);
		if (a & kn) katt += _bitcnt(a & kn) << 3;
		mn += bitcnt(a);

		colorb[c] ^= pieceb[BISHOP] & cb;	
		colorb[c] ^= pieceb[ROOK] & cb;	

		a = RATT1(f) | RATT2(f);
		if (a & kn) katt += _bitcnt(a & kn) << 3;
		mn += bitcnt(a);

		colorb[c] ^= pieceb[ROOK] & cb;	
	}

	colorb[oc] ^= RQU & ocb; //Opposite Queen & Rook doesn't block mobility for bishop
	b = pieceb[BISHOP] & cb;
	while (b) {
		*sf += 3;
		f = pullLsb(&b);
		a = BATT3(f) | BATT4(f);
		if (a & kn) katt += _bitcnt(a & kn) << 3;
		mn += bitcnt(a) << 3;
	}

	colorb[oc] ^= pieceb[ROOK] & ocb; //Opposite Queen doesn't block mobility for rook.
	colorb[c] ^= pieceb[ROOK] & cb; //Own non-pinned Rook doesn't block mobility for rook.
	b = pieceb[ROOK] & cb;
	while (b) {
		*sf += 5;
		f = pullLsb(&b);
		a = RATT1(f) | RATT2(f);
		if (a & kn) katt += _bitcnt(a & kn) << 3;
		mn += bitcnt(a) << 2;
	}

	colorb[c] ^= RQU & cb; // Back
	b = pin & (pieceb[ROOK] | pieceb[BISHOP] | pieceb[QUEEN]); 
	while (b) {
		int p;
		f = pullLsb(&b);
		p = identPiece(f);
		if (p == BISHOP) {
			*sf += 3; 
			a = BATT3(f) | BATT4(f);
			if (a & kn) katt += _bitcnt(a & kn) << 3;
		} else if (p == ROOK) {
			*sf += 5; 
			a = RATT1(f) | RATT2(f);
			if (a & kn) katt += _bitcnt(a & kn) << 3;
		} else {
			*sf += 9;
			a = RATT1(f) | RATT2(f) | BATT3(f) | BATT4(f);
			if (a & kn) katt += _bitcnt(a & kn) << 3;
		}
		t = p | getDir(f, kingpos[c]);
		if ((t & 10) == 10) mn += _bitcnt(RATT1(f));
		if ((t & 18) == 18) mn += _bitcnt(RATT2(f));
		if ((t & 33) == 33) mn += _bitcnt(BATT3(f));
		if ((t & 65) == 65) mn += _bitcnt(BATT4(f));
	}

	colorb[oc] ^= pieceb[QUEEN] & ocb; //Back
	xorBit(kingpos[oc], colorb+oc); //Back
	if (*sf < 14) katt = katt * (*sf) / 14; //Reduce the bonus for attacking king squares
	return mn + katt;
}

int eval1 = 0;
int eval(int c, int matrl) {
	int sf0 = 0, sf1 = 0;
	int ev0 = evalc(0, &sf0);
	int ev1 = evalc(1, &sf1);
	eval1++;

	if (sf1 < 12) ev0 += kmobil[kingpos[0]]*(12-(sf1 > 4 ? sf1 : 4));
	if (sf0 < 12) ev1 += kmobil[kingpos[1]]*(12-(sf0 > 4 ? sf0 : 4));

	if ((matrl < 0 && NOMATEMAT(sf1, sf0, 1)) || (matrl > 0 && NOMATEMAT(sf0, sf1, 0)))
		matrl = 0;
		
	return c ? ev1 - ev0 - matrl : ev0 - ev1 + matrl;
}

u64 nodes;
u64 qnodes;
int quiesce(u64 ch, int c, int ply, int alpha, int beta) {
	int i, w, best = -MAXSCORE, poff;
	int cmat = c ? -mat: mat;

	if (ply == 63) return eval(c, mat);
	if (!ch) do {
		if (cmat - 200 >= beta) return beta;
		if (cmat + 200 <= alpha) break;
		best = eval(c, mat);
		if (best > alpha) {
			alpha = best;
			if (best >= beta) return beta;
		}
	} while(0);

	generate(ch, c, ply, 1, 0);
	if (ch && movenum[ply] == 0) return -MAXSCORE + ply;
	poff = ply << 8;

	for (i = 0; i < movenum[ply]; i++) {
		Move m = qpick(movelist + poff, movenum[ply], i);
		if (!ch && !PROM(m) && pval[PIECE(m)] > pval[CAP(m)] && swap(m) < 0) continue;

		doMove(m, c);
		qnodes++;

		w = -quiesce(attacked(kingpos[c^1], c^1), c^1, ply+1, -beta, -alpha);

		undoMove(m, c);

		if (w > best) {
			best = w;
			if (w > alpha) {
				alpha = w;
				if (w >= beta) return beta;
			}
		}
	}
	return best > -MAXSCORE ? best : eval(c, mat);
}

int retPVMove(int c, int ply) {
	int i;
	GENERATE(c);
	for (i = 0; i < movenum[0]; i++) {
		Move m = movelist[i];
		if (m == pv[0][ply]) return m;
	}
	return 0;
}

int inputSearch() {
	int ex;
	fgets(irbuf,255,stdin);
	ex = protV2(irbuf, 0);
	if (analyze) { if (ex <= -1) return ex; else ex = 0; }
	if (!ponder || ex || engine != onmove) pondering = analyze;
	if (!ex) irbuf[0] = 0;
	if (ex < -1) return ex;
	if (ex != -1) return !pondering;
	ex = parseMove(irbuf, ONMV(pon), pon);
	if (!ex || ex == -1) return 1;
	irbuf[0] = 0;
	return (pon = 0);
}

const int nullvar[] = {13, 43, 149, 519, 1809, 6311, 22027};
static int nullvariance(int delta) {
      int r = 0;
      if (delta >= 4) for (r = 1; r <= 7; r++) if (delta < nullvar[r - 1]) break;
      return r;
}

#define HASHP (hashb ^ hashxor[flags | 1024 | c << 11])
#define HASHB ((hashb ^ hashxor[flags | 1024]) ^ hashxor[c | d << 1 | 2048])
int search(u64 ch, int c, int d, int ply, int alpha, int beta, int pvnode, int null) {
	int i, j, n, w, poff, first;
	Move hmove;
	u64 hb, hp, he, hismax = 0LL;

	pvlength[ply] = ply;
	if ((++nodes & CNODES) == 0) {
		if (bioskey()) sabort = inputSearch();
		if (!pondering && getTime() - starttime > maxtime) sabort = 1;
	}
	if (sabort) return 0;

	hp = HASHP;
	if (ply && isDraw(hp, 1)) return 0;

	if (d == 0 || ply > 100) return quiesce(ch, c, ply, alpha, beta);
	hstack[COUNT] = hp;

	hb = HASHB;
	he = hashDB[hb & HMASKB];
	if (!((he^hb) & HINVB)) {
		w = (u32)LOW16(he) - MAXSCORE;
		if (he & 0x10000) {
			null = 0;
			if (w <= alpha) return alpha;
		} else {
			if (w >= beta) return beta;
		}
	} else w = c ? -mat : mat;

	//Null Move
	if (!pvnode && !ch && null && d > 1 && bitcnt(colorb[c] & (~pieceb[PAWN]) & (~pinnedPieces(kingpos[c], c^1))) > 2) {
		int flagstore = flags;
		int R = (10 + d + nullvariance(w - beta))/4;
		if (R > d) R = d;
		flags &= 960;
		count += 0x401;
		w = -search(0LL, c^1, d-R, ply+1, -beta, -alpha, 0, 0);
		flags = flagstore;
		count -= 0x401;
		if (!sabort && w >= beta) {
			hashDB[hb & HMASKB] = (hb & HINVB) | (w + MAXSCORE); 
			return beta;
		}
	}

	hmove = 0;
	if (ply) {
		he = hashDP[hp & HMASKP];
		if (!((he^hp) & HINVP)) hmove = (Move)(he & HMASKP);

		if (d >= 4 && hmove == 0) { // Simple version of Internal Iterative Deepening
			w = search(ch, c, d-3, ply, alpha, beta, pvnode, 0);
			he = hashDP[hp & HMASKP];
			if (!((he^hp) & HINVP)) hmove = (Move)(he & HMASKP);
		}
	} else {
		hmove = retPVMove(c, ply);
	}

	poff = ply << 8;
	first = 1;
	for (n = 1; n <= (ch ? 2 : 3); n++) {
 	    if (n == 1) {
		if (hmove == 0) continue;
		movenum[ply] = 1;
	    } else if (n == 2) {
		generate(ch, c, ply, 1, 0);
	    } else {
		generate(ch, c, ply, 0, 1);
	    }
	    for (i = 0; i < movenum[ply]; i++) {
		Move m;
		u64 nch;
		int ext = 0;
		if (n == 1) {
			m = hmove;
		} else {
			if (n == 2) m = qpick(movelist + poff, movenum[ply], i);
			else m = spick(movelist + poff, movenum[ply], i, ply);
			if (m == hmove) continue;
		}

		doMove(m, c);
		nch = attacked(kingpos[c^1], c^1);
		if (nch) ext++; // Check Extension
		else if (n == 2 && !pvnode && d >= 2 && !ch && swap(m) < 0) ext--; //Reduce bad exchanges
		else if (n == 3 && !pvnode) { //LMR
			if (m == killer[ply]); //Don't reduce killers
			else if (PIECE(m) == PAWN && !(pawnfree[TO(m) + (c << 6)] & pieceb[PAWN] & colorb[c^1])); //Don't reduce free pawns
			else {
				u32 his = history[m & 0xFFF];
				if (his > hismax) { hismax = his;} 
				else if (d <= 4 && his*his < hismax) { undoMove(m, c); continue; }
				else if (d >= 2) ext--;
				else if (i > 14) { undoMove(m, c); continue; } // Move count pruning
			}
	        }

		if (first == 1 && pvnode) {
			w = -search(nch, c^1, d-1+ext, ply+1, -beta, -alpha, 1, 1);
		} else {
			w = -search(nch, c^1, d-1+ext, ply+1, -alpha-1, -alpha, 0, 1);
			if (w > alpha && ext < 0) w = -search(nch, c^1, d-1, ply+1, -alpha-1, -alpha, 0, 1);
			if (w > alpha && w < beta && pvnode) w = -search(nch, c^1, d-1+ext, ply+1, -beta, -alpha, 1, 1);
		}
		undoMove(m, c);
		if (sabort) return 0;

		if (w > alpha) {
			hashDP[hp & HMASKP] = (hp & HINVP) | m;
			alpha = w;
			first = -1;

			if (w >= beta) {
				if (CAP(m) == 0) {
					killer[ply] = m;
					history[m & 0xFFF]+=d*d;
				}
				hashDB[hb & HMASKB] = (hb & HINVB) | (w + MAXSCORE); 
				return beta;
			}
			if (pvnode && w >= alpha) {
				pv[ply][ply] = m;
				for (j = ply +1; j < pvlength[ply +1]; j++) pv[ply][j] = pv[ply +1][j];
				pvlength[ply] = pvlength[ply +1];
				if (w == MAXSCORE-1  - ply) return w;
			}
		}
		if (first == 1) first = 0;
	    }
	}
	if (sabort) return 0;
	if (first == 1) return ch ? -MAXSCORE+ply : 0;
	else if (!first) hashDB[hb & HMASKB] = (hb & HINVB) | 0x10000 | (alpha + MAXSCORE);
	return alpha;
}

void reseth(int level) {
        int i, istart = level < 0 ? 1 : 0;
        for (i = 0; i < 0x1000; i++) history[i] = 0LL;
        for (i = istart; i < 127; i++) killer[i] = level <= 1 ? killer[i+level] : 0;
        for (i = istart; i < 127; i++) pv[0][i] = level <= 1 ? pv[0][i+level] : 0;
        pvlength[0] = level <= 1 && pvlength[0] - level > 0 ? pvlength[0] - level : 0;

	memset(hashDB, 0, sizeof(hashDB)); // Until depth decrementing tables
        if (level >= 2) pvlength[0] = pv[0][0] = 0;
	if (level == 3) memset(hashDP, 0, sizeof(hashDP));
}

int execMove(Move m, int r) {
	int i, c;
	doMove(m, onmove);
	onmove ^= 1; 
	c = onmove;
	if (book) for (i = 0; i < BKSIZE; i++) {
		if (bkflag[i] < 2 && (bkmove[i*32 + COUNT - 1] != m || bkmove[i*32 + COUNT] == 0)) {
			bkcount[bkflag[i]]--;
			bkflag[i] = 2;
		}
	}
	hstack[COUNT] = HASHP;
	if (r) reseth(1);
	i = GENERATE(c);
	if (pondering) return (movenum[0] == 0);
	if (movenum[0] == 0) {
		if (!i) {
			printf("1/2-1/2 {Stalemate}\n"); return 4;
		} else {
			printf(c ? "1-0 {White mates}\n" : "0-1 {Black mates}\n"); return 5 + c;
		}
	}
	switch (isDraw(HASHP, 2)) {
		case 1: printf("1/2-1/2 {Draw by Repetition}\n"); return 1;
		case 2: printf("1/2-1/2 {Draw by Fifty Move Rule}\n"); return 2;
		case 3: printf("1/2-1/2 {Insufficient material}\n"); return 3;
	}
	return 0;
}

#define ISRANK(c) (c >= '1' && c <= '8')
#define ISFILE(c) (c >= 'a' && c <= 'h')
int ismove(Move m, int to, int from, int piece, int prom, int h) {
	if (TO(m) != to) return 0;
	if (from < 0 && PIECE(m) != piece) return 0;
	if (from >= 0 && FROM(m) != from) return 0;
	if (ISFILE(h) && (FROM(m) & 7) != h - 'a') return 0;
	if (ISRANK(h) && (FROM(m) & 56) != 8*(h - '1')) return 0;
	if (prom && PROM(m) != prom) return 0;
	return 1;
}

int parseMove(char *s, int c, Move p) {
	int i, to, from = -1, piece = PAWN, prom = 0;
    	char h = 0, c1, c2;
	if (!strncmp(s, "O-O-O", 5)) strcpy(s, c ? "Kc8" : "Kc1");
	else if (!strncmp(s, "O-O", 3)) strcpy(s, c ? "Kg8" : "Kg1");
	if (s[0] >= 'A' && s[0] <= 'Z') if ((piece = _getpiece(*s++, &i)) < 1) return -1;
	if (s[0] == 'x') s++;
	if (ISRANK(s[0])) {h = *s++; if (s[0] == 'x') s++; }
	if (!ISFILE(s[0])) return -1;
	c1 = *s++;
	if (s[0] == 'x') s++;
	if (ISFILE(s[0])) {h = c1; c1 = *s++;}
	c2 = *s++;
	if (!ISRANK(c2)) return -1;
	if (*s >= '0') {
		if (*s == '=') prom = _getpiece(s[1], &i);
		else if (*s == '+');
		else { // Algebraic Notation
			from = c1 - 'a' + 8*(c2 - '1');
			c1 = *s++; c2 = *s++;
			if (!ISFILE(c1) || !ISRANK(c2)) return -1;
			prom = _getpiece(*s, &i);
		}
	}
	to = c1 - 'a' + 8*(c2 - '1');
	if (p) {
		if (ismove(p, to, from, piece, prom, h)) return p;
		return 0;
	}
	GENERATE(c);
	for (i = 0; i < movenum[0]; i++) if (ismove(movelist[i], to, from, piece, prom, h)) return movelist[i];
	return 0;
}

int parseMoveNExec(char *s, int c, Move *m, int r) {
	*m = parseMove(s, c, 0);
	if (*m == -1) printf("UNKNOWN COMMAND: %s\n", s);
	else if (*m == 0) fprintf(stderr,"Illegal move: %s\n",s);
	else return execMove(*m, r);
	return -1;
}

void undo() {
        int cnt = COUNT - 1;
	if (cnt < 0) return;
        onmove ^= 1;
        Move m = mstack[cnt] >> 42L;
        undoMove(m, onmove);
        reseth(-1);
        pv[0][0] = m;
}

int ttime = 30000, mps = 0, inc = 0, post = 1, st = 0; char base[16];

int calc(int sd, int tm) {
	int i, j, t1 = 0, m2go = mps == 0 ? 32 : 1 + mps - ((COUNT/2) % mps);
	u64 ch = attacked(kingpos[onmove], onmove);
	long searchtime = tm*5L/m2go + inc*500L;
	long extendtime = tm*24L/m2go + inc*1000L; if (extendtime > tm*10L-200) extendtime = tm*10L-200;
	if (searchtime > tm*10L-200) searchtime = tm*10L-200;
	if (st > 0) maxtime = searchtime = st*1000LL;

	maxtime = extendtime;
	starttime = getTime();

	eval1 = sabort = iter = value[0] = 0;
	qnodes = nodes = 0LL;
	if (book) {
		if (!bkcount[onmove]) book = 0;
		else {
			srand((u32)starttime);
			j = rand() % bkcount[onmove];
			for (i = 0; i < BKSIZE; i++) {
				if (bkflag[i] == onmove && j == t1++) { pv[0][0] = bkmove[i*32 + COUNT]; break; }
			}
		}
	}
	if (!book || analyze) for (iter = 1; iter <= sd; iter++) {
		int pvlengsave = pvlength[0];
		value[iter] = search(ch, onmove, iter, 0, -MAXSCORE, MAXSCORE, 1, 0);
		if (pvlength[0] == 0 && iter > 1) { pvlength[0] = pvlengsave; value[iter] = value[iter -1]; }
		t1 = (int)(getTime() - starttime);
		if (post && pvlength[0] > 0 && (!sabort || (!analyze && sabort>=1))) { 
			printf("%2d %5d %6d %9lu  ", iter, MEVAL(value[iter]), t1/10, (u32)(nodes + qnodes));
			displaypv(); printf("\n"); 
		}
		if (sabort) break;
		if (pondering) continue;
		if (iter >= MAXSCORE-value[iter]) break;
		if (t1 < searchtime || iter == 1) continue;

		if (value[iter] - value[iter-1] < -40 && maxtime == extendtime) {
			maxtime = extendtime*3L; if (maxtime < tm*10L-199) maxtime = tm*10L-199;
			continue;
		}
		break;
	}
	if (analyze) return 1;
	pondering = 0;
	if (sabort < -1) { pon = 0; return sabort; }
	if (pon) {
	 	undo();	
		pon = 0;
		return engine != onmove;
	}
	printf("move "); displaym(pv[0][0]); printf("\n");

	if (post && ics) printf("kibitz W: %d Nodes: %lu QNodes: %lu Evals: %d cs: %d knps: %lu\n", MEVAL(value[iter > sd ? sd : iter]), (u32)nodes, (u32)qnodes, eval1, t1/10, (u32)(nodes+qnodes)/(t1+1));

	if (resign > 0) {
		int w = value[iter > sd ? sd : iter];
		if (w < -resign && lastw < -resign) printf("resign\n");
		int dt = COUNT > 40 ? (COUNT - 40)/4 : 0;
		if (w < dt && w > -dt && lastw < dt && lastw > -dt) printf("offer draw\n");
		lastw = w;
	}

	return execMove(pv[0][0], 1);
}

int doponder(int c) {
	pon = retPVMove(c, 0);
	if (pon) {
		pondering = 1;
		if (execMove(pon, 1)) {
			pondering = 0;
			undo();
			pon = 0;
		}
	}
	return pondering ? 0 : -1;
}

u64 perft(int c, int d, int div) {
	int i, ply = 63 - d, poff = ply << 8;
	u64 n, cnt = 0LL;

	generate(attacked(kingpos[c], c), c, ply, 1, 1);
	if (d == 1 || bioskey()) return (u64)movenum[ply];
	for (i = 0; i < movenum[ply]; i++) {
		Move m = movelist[poff + i];
		doMove(m, c);
		cnt += n = perft(c^1, d - 1, 0);
		if (div) { displaym(m); printf(" %llu\n", n); }
		undoMove(m, c);
	}
	return cnt;
}

void newGame(int level) {
	_readbook("olibook.pgn");
	reseth(level);
	engine = 1;
	sd = 64;
}

int protV2(char* buf, int parse) {
		if (!strncmp(buf,"protover",8)) printf("feature setboard=1 myname=\"OliThink " VER "\" colors=0 analyze=1 done=1\n");
		else if (!strncmp(buf,"xboard",6));
		else if (!strncmp(buf,"quit",4)) return -2;
		else if (!strncmp(buf,"new",3)) return -3; 
		else if (!strncmp(buf,"remove",6)) return -4;
		else if (!strncmp(buf,"force",5)) return -7;
		else if (!strncmp(buf,"go",2)) engine = pondering ? onmove^1 : onmove;
		else if (!strncmp(buf,"setboard",8)) if (parse) _parse_fen(buf+9); else return -9;
		else if (!strncmp(buf,"undo",4)) return -8;
		else if (!strncmp(buf,"easy",4)) ponder = 0;
		else if (!strncmp(buf,"hard",4)) ponder = 1;
		else if (!strncmp(buf,"analyze",7)) return -5;
		else if (!strncmp(buf,"exit",4)) return -6;
		else if (!strncmp(buf,"sd",2)) sscanf(buf+3,"%d",&sd);
		else if (!strncmp(buf,"time",4)) sscanf(buf+5,"%d",&ttime);
		else if (!strncmp(buf,"level",4)) sscanf(buf+6,"%d %s %d",&mps, base, &inc);
		else if (!strncmp(buf,"post",4)) post = 1;
		else if (!strncmp(buf,"nopost",6)) post = 0;
		else if (!strncmp(buf,"result",6)) return -6; //result 0-1 {Black mates}
		else if (!strncmp(buf,"otim",4));//otim <optime>
		else if (!strncmp(buf,"draw",4));//draw offer 
		else if (!strncmp(buf,"st",2)) sscanf(buf+3,"%d",&st);
		else if (!strncmp(buf,"?",1)) return 1;
		else if (!strncmp(buf,".",1));
		else if (!strncmp(buf,"bk",2));
		else if (!strncmp(buf,"hint",4));
		else if (!strncmp(buf,"computer",8));
		else if (!strncmp(buf,"accepted",8));//accepted <feature>
		else if (!strncmp(buf,"random",6));
		else if (!strncmp(buf,"rating",6)) ics = 1;//ICS: rating <myrat> <oprat>
		else if (!strncmp(buf,"name",4));//ICS: name <opname>
		else if (!strncmp(buf,"perft",5)) {int i; for (i = 1; i <= sd && !bioskey(); i++) printf("Depth: %d Nodes: %lld%c\n", i, perft(onmove, i, 0), bioskey() ? '+' : ' ');}
		else if (!strncmp(buf,"divide",5)) perft(onmove, sd, 1);
		else if (strchr(buf, '/') != NULL && strlen(buf)>16) { engine = -1; analyze = pondering = 1; if (parse) _parse_fen(buf); else return -9; }
		else if (strlen(buf) == 0);
		else return -1;
		return 0;
}

int input(int c) {
		Move m;
		int ex;
		char buf[256];
		if (irbuf[0]) strcpy(buf,irbuf); else fgets(buf,255,stdin);
		irbuf[0] = 0;
		ex = protV2(buf, 1);	
		if (ex == -1) return parseMoveNExec(buf, c, &m, 1);
		return ex;
}

int main(int argc, char **argv)
{
	int i, ex = -1;

	setbuf(stdout, NULL);
	setbuf(stdin, NULL);
	signal(SIGINT, SIG_IGN);
	for (i = 0; i < 0x10000; i++) LSB[i] = _slow_lsb(i);
	for (i = 0; i < 0x10000; i++) BITC[i] = _bitcnt(i);
	for (i = 0; i < 4096; i++) hashxor[i] = _rand_64();
	for (i = 0; i < 64; i++) BIT[i] = 1LL << i;
	for (i = 0; i < 128; i++) pmoves[i] = pawnfree[i] = pawnfile[i] = pawnhelp[i] = 0LL;
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
	_init_pawns(pmoves, pcaps, pawnfree, pawnfile, pawnhelp, 0);
	_init_pawns(pmoves + 64, pcaps + 64, pawnfree + 64, pawnfile + 64, pawnhelp + 64, 1);
	newGame(3);

	for (i = 0; i < 64; i++) nmobil[i] = (bitcnt(nmoves[i])-1)*6;
	for (i = 0; i < 64; i++) kmobil[i] = (bitcnt(nmoves[i])/2);
	if (argc > 1 && !strncmp(argv[1],"-sd",3)) {
		ttime = 99999999;
		if (argc > 2) {
			sscanf(argv[2], "%d", &sd);
			if (argc > 3) { _parse_fen(argv[3]); calc(sd, ttime); return 0; }
		}
	} else if (argc > 1 && !strncmp(argv[1],"-resign",7)) sscanf(argv[2], "%d", &resign); 
	irbuf[0] = 0;

	for (;;) {
		if (engine == onmove || analyze) ex = calc(sd, ttime);
		else if (ex == 0 && ponder && engine != -1 && !book) ex = doponder(onmove);

		if (!ponder || book || engine == -1 || ex != 0) ex = input(onmove);
		if (ex == -2) break;
		if (ex == -3) newGame(analyze || resign > 0 ? 3 : 2);
		if (ex == -4) { undo(); undo(); }
		if (ex == -5) { analyze = pondering = 1; engine = -1; }
		if (ex == -6) analyze = pondering = 0;
		if (ex == -7) { pondering = 0; engine = -1; }
		if (ex == -8) undo();
	}
	return 0;
}
