#define VER "5.11.9uci"
/* OliThink5 (c) Oliver Brausch 19.Aug.2025, ob112@web.de, http://brausch.org (uci by Jim Abblet) */
#include <stdio.h>
#include <string.h>
#ifdef _WIN64
#include <windows.h>
#include <conio.h>
#include <intrin.h>
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
enum { LOWER, EXACT, UPPER };
enum { NO_MOVE, ANY_MOVE, GOOD_MOVE };
enum { HASH, NOISY, QUIET, EXIT };

#define HMASK 0x7FFFFFLL
#define CNODES 0x3FFF
#define BKSIZE 8192
const int pval[] = {0, 100, 290, 0, 100, 310, 500, 950};
const int fval[] = {0, 0, 2, 0, 0, 3, 5, 9};
const int cornbase[] = {4, 4, 2, 1, 0, 0 ,0};

#define FROM(x) (x & 63)
#define TO(x) ((x >> 6) & 63)
#define ONMV(x) ((x >> 12) & 1)
#define PROM(x) ((x >> 13) & 7)
#define PIECE(x) ((x >> 16) & 7)
#define CAP(x) ((x >> 19) & 7)
#define MAXSCORE 16384

#define _TO(x) (x << 6)
#define _ONMV(x) (x << 12)
#define _PROM(x) (x << 13)
#define _PIECE(x) (x << 16)
#define _CAP(x) (x << 19)
#define PREMOVE(f, p) ((f) | _ONMV(c) | _PIECE(p))

#define RATT1(f, occ) raysRank[f&7][key000(occ, f)] & rmask0[f]
#define RATT2(f, occ) raysAFile[f>>3][key090(occ, f)] << (f&7)
#define BATT3(f, occ) raysRank[f&7][key045(occ, f)] & bmask45[f]
#define BATT4(f, occ) raysRank[f&7][key135(occ, f)] & bmask135[f]
#define RXRAY1(f, occ) (xrayRank[f&7][key000(occ, f)] & rmask0[f])
#define RXRAY2(f, occ) (xrayAFile[f>>3][key090(occ, f)] << (f&7))
#define BXRAY3(f, occ) (xrayRank[f&7][key045(occ, f)] & bmask45[f])
#define BXRAY4(f, occ) (xrayRank[f&7][key135(occ, f)] & bmask135[f])

#define RATT(f, occ) (RATT1(f, occ) | RATT2(f, occ))
#define BATT(f, occ) (BATT3(f, occ) | BATT4(f, occ))

#define KMOVE(x, occ) (kmoves[x] & ~occ)
#define KCAP(x, c) (kmoves[x] & P.color[c^1])
#define PMOVE(x, c, occ) (pmoves[c][x] & ~occ)
#define PCAP(x, c) (pcaps[c][x] & P.color[c^1])
#define PCA3(x, c) (pcaps[c][(x) | 64] & (P.color[c^1] | (BIT[ENPASS] & (c ? 0xFF0000 : 0xFF0000000000))))
#define PCA4(x, c) (pcaps[c][(x) | 128] & (P.color[c^1] | (BIT[ENPASS] & (c ? 0xFF0000 : 0xFF0000000000))))

#define ENPASS (flags & 63)
#define CASTLE(c) (flags & (320 << (c)))
#define COUNT (count & 0x3FF)
#define MEVAL(w) (w > MAXSCORE-500 ? (200001+MAXSCORE-w)/2 : w < 500-MAXSCORE ? (-200000-MAXSCORE-w)/2 : (w))
#define NOMATEMAT(c) ((P.sf[c] <= 4 || (P.sf[c] <= 8 && P.sf[c] <= P.sf[c^1] + 3)) && (P.piece[PAWN] & P.color[c]) == 0)

typedef struct {
	int n;
	Move list[128];
} Movep;

typedef struct {
	u64 key;
	Move move;
	short value;
	char depth;
	char type;
} entry;

typedef struct {
	u64 hash;
	int king[2];
	int sf[3];
	u64 color[3];
	u64 piece[8];
} Pos;

static Pos P; static u64 node_limit;
static entry hashDB[HMASK+1];
static u64 hstack[0x400], mstack[0x400], hashxor[0x1000];
static u64 pmoves[2][64],pawnprg[2][64], pawnfree[2][64], pawnfile[2][64], pawnhelp[2][64], RANK[2][64], pcaps[2][192];
static u64 BIT[64], nmoves[64], kmoves[64], bmask135[64], bmask45[64], rmask0[64];
static u64 rankb[8], fileb[8], raysRank[8][64], raysAFile[8][64], xrayRank[8][64], xrayAFile[8][64];
static u64 whitesq, centr, centr2, maxtime, starttime, eval1, nodes, qnodes;
static u32 crevoke[64], count, flags, ponder = 0, pondering = 0, analyze = 0;
static Move pv[128][128], pon = 0, bkmove[BKSIZE*32], killer[128];
static int wstack[0x400], history[0x2000];
static int kmobil[64], bishcorn[64];
static int _knight[8] = {-17,-10,6,15,17,10,-6,-15};
static int _king[8] = {-9,-1,7,8,9,1,-7,-8};
static int book, bkflag[BKSIZE], bkcount[3];
static int sabort, onmove, randm, sd = 64, ttime = 30000, mps = 0, inc = 0, post = 1, st = 0;
static char irbuf[256], base[16];
static char *sfen = "rnbqkbnr/pppppppp/////PPPPPPPP/RNBQKBNR w KQkq - 0 1";
const char pieceChar[] = "*PNK.BRQ";
#define MAT P.sf[2]
#define BOARD P.color[2]
#define RQU (P.piece[QUEEN] | P.piece[ROOK])
#define BQU (P.piece[QUEEN] | P.piece[BISHOP])

int changeMat(int, int, int);
void doMove(Move, int c);
int parseMove(char*, int, Move);
int protV2(char*,int);

int _getpiece(char s, int *c) {
	int i;
	for (i = 1; i < 8; i++)
		if (pieceChar[i] == s) { *c = 0; return i; }
		else if (pieceChar[i] == s-32) { *c = 1; return i; }
	return 0;
}

void _parse_fen(char *fen, int reset) {
	char s, mv, pos[128], cas[5], enps[3];
	int c, i, halfm = 0, fullm = 1, col = 0, row = 7;
	memset(&P, 0, sizeof(P));
	book = i = c = 0; cas[0] = enps[0] = pv[0][0] = 0;
	sscanf(fen, "%s %c %s %s %d %d", pos, &mv, cas, enps, &halfm, &fullm); if (fullm < 1) fullm = 1;
	while ((s = pos[i++])) {
		if (s == '/') {
			row--; col = 0;
		} else if (s >= '1' && s <= '8') {
			col += s - '0';
		} else {
			int p = _getpiece(s, &c), t = row*8 + (col++);
			if (p == KING) P.king[c] = t;
			else MAT += changeMat(_CAP(p) | _TO(t), c^1, -1);
			P.hash ^= hashxor[col | row << 3 | p << 6 | c << 9];
			P.piece[p] |= BIT[t];
			P.color[c] |= BIT[t];
		}
	}
	onmove = mv == 'b' ? 1 : 0;
	flags = i = 0, count = (fullm - 1)*2 + onmove + (halfm << 10);
	while ((s = cas[i++])) {
		int b = s == 'K' ? 6 : s == 'k' ? 7 : s == 'Q' ? 8 : s == 'q' ? 9 : 0;
		if (b) flags |= BIT[b];
	}
	if (enps[0] >= 'a' && enps[0] <= 'h' && enps[1] >= '1' && enps[1] <= '8') flags |= 8*(enps[1] -'1') + enps[0] -'a';
	for (i = 0; i < (int)COUNT; i++) hstack[i] = 0LL;
	if (reset) memset(hashDB, 0, sizeof(hashDB));
	BOARD = P.color[0] | P.color[1];
}

void _newGame() {
	char buf[256], s0[64], s1[64], s2[64];
	int k, n = 0;
	FILE *in = fopen("olibook.pgn", "r");
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
				u32 i = 0, j = 0;
				_parse_fen(sfen, 0);
				for (;;) {
					if (strchr(buf+i, ' ') == NULL) break;
					sscanf(buf+i,"%s %s", s0, s2);
					if (s0[0] < '1' || s0[0] > '9') break;
					i += (int)(strlen(s0) + strlen(s2) + 2);
					if (s0[strlen(s0)-1] == '.') {
						strcpy(s1, s2); s2[0] = 0;
						sscanf(buf+i,"%s", s2);
						i += (int)(strlen(s2) + 1);
					} else sscanf(s0,"%*[^.].%[^.]", s1);
					doMove(bkmove[n*32+ (j++)] = parseMove(s1, 0, 0), 0);
					if (s2[0] == 0 || s2[0] == '*') break;
					doMove(bkmove[n*32+ (j++)] = parseMove(s2, 1, 0), 1);
					if (j > 30 || i >= strlen(buf)) break;
				}
				bkmove[n*32 + j] = 0;
				if (j) bkcount[bkflag[n]]++;
				if (++n == BKSIZE) break;
			}
		}
		fclose(in);
	}
	_parse_fen(sfen, 1);
	if (bkcount[0] > 0 || bkcount[1] > 0) book = 1;
	randm = 0;
}

void _init_pawns(u64* moves, u64* caps, u64* freep, u64* filep, u64* helpp, u64* prgp, int c) {
	int i, j;
	for (i = 0; i < 64; i++) {
		int rank = i/8, file = i&7;
		int m = i + (c ? -8 : 8);
		prgp[i] = 1 << (c ? 7-rank : rank);
		for (j = 0; j < 64; j++) {
			int jrank = j/8, dfile = ((j&7) - file)*((j&7) - file);
			if (dfile > 1) continue;
			if ((c && jrank < rank) || (!c && jrank > rank)) {//The not touched part of the board
				if (dfile == 0) filep[i] |= BIT[j];
				freep[i] |= BIT[j];
			} else if (dfile != 0 && (jrank - rank)*(jrank - rank) <= 1) {
				helpp[i] |= BIT[j];
			}
		}
		if (m < 0 || m > 63) continue;
		moves[i] |= BIT[m];
		if (file > 0) {
			m = i + (c ? -9 : 7);
			if (m < 0 || m > 63) continue;
			caps[i] |= BIT[m];
			caps[i + 64*(2 - c)] |= BIT[m];
		}
		if (file < 7) {
			m = i + (c ? -7 : 9);
			if (m < 0 || m > 63) continue;
			caps[i] |= BIT[m];
			caps[i + 64*(c + 1)] |= BIT[m];
		}
	}
}

void _init_shorts(u64* moves, int* m) {
	u32 i, j, n;
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
	int i;
	u64 low, perm = free;
	for (i = 0; i < bc; i++) {
		low = free & (-free); // Lowest bit
		free &= (~low);
		if (!(BIT[i] & del)) perm &= (~low);
	}
	return perm;
}

void _init_rays(u64* rays, u64* xrays, u64 (*rayFunc) (int, u64, int), int (*key)(u64, int), int file) {
	int i, f, bc;
	for (f = 0; f < 64; f+=file) {
		u64 mmask = (*rayFunc)(f, 0LL, 0) | BIT[f];
		int iperm = 1 << (bc = bitcnt(mmask));
		for (i = 0; i < iperm; i++) {
			u64 board = _occ_free_board(bc, i, mmask);
			u64 occ = (*rayFunc)(f, board, 0);
			u64 xray = (*rayFunc)(f, board, 1);
			int index = (*key)(board, f), ix = (f/file)&7;
			rays[(ix << 6) + index] |= occ;
			xrays[(ix << 6) + index] |= xray;
		}
	}
}

#define RAYMACRO { if (BIT[i] & board) \
	{ if (b) { xray |= BIT[i]; break; } else { occ |= BIT[i]; b = 1; } } if (!b) occ |= BIT[i]; }
u64 _rook0(int f, u64 board, int t) {
	u64 occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f+1; i < 64 && i%8 != 0; i++) RAYMACRO
	for (b = 0, i = f-1; i >= 0 && i%8 != 7; i--) RAYMACRO
	return t ? xray : occ;
}

u64 _rook90(int f, u64 board, int t) {
	u64 occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f-8; i >= 0; i-=8) RAYMACRO
	for (b = 0, i = f+8; i < 64; i+=8) RAYMACRO
	return t ? xray : occ;
}

u64 _bishop45(int f, u64 board, int t) {
	u64 occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f+9; i < 64 && (i%8 != 0); i+=9) RAYMACRO
	for (b = 0, i = f-9; i >= 0 && (i%8 != 7); i-=9) RAYMACRO
	return t ? xray : occ;
}

u64 _bishop135(int f, u64 board, int t) {
	u64 occ = 0LL, xray = 0LL;
	int i, b;
	for (b = 0, i = f-7; i >= 0 && (i%8 != 0); i-=7) RAYMACRO
	for (b = 0, i = f+7; i < 64 && (i%8 != 7); i+=7) RAYMACRO
	return t ? xray : occ;
}

#ifdef _WIN32
u64 getTime() {
    return GetTickCount();
}

int bioskey() {
    static int init = 0, pipe;
    static HANDLE inh;
    DWORD dw;

    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        pipe = !GetConsoleMode(inh, &dw);
    }
    if (pipe) {
        if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
        return dw;
    } else return _kbhit();
}
#else
u64 getTime() {
	gettimeofday (&tv, NULL);
	return(tv.tv_sec * 1000LL + (tv.tv_usec / 1000));
}

int bioskey() {
	fd_set readfds;

	FD_ZERO (&readfds);
	FD_SET (fileno(stdin), &readfds);
	tv.tv_sec=0; tv.tv_usec=0;
	select(16, &readfds, 0, 0, &tv);

	return (FD_ISSET(fileno(stdin), &readfds));
}
#endif

int pullLsb(u64* bit) {
	int f = getLsb(*bit);
	*bit &= *bit - 1;
	return f;
}

int identPiece(int f) {
	int i;
	for (i = PAWN; i <= QUEEN; i++) if (i != ENP && (BIT[f] & P.piece[i])) return i;
	return ENP;
}

int key000(u64 b, int f) {
	return (int) ((b >> ((f & 56) + 1)) & 0x3F);
}

int key090(u64 b, int f) {
	u64 _b = (b >> (f&7)) & 0x0101010101010101LL;
	return (int)((_b * 0x0080402010080400LL) >> 58);
}

int key045(u64 b, int f) {
	return (int)(((b & bmask45[f]) * 0x0202020202020202LL) >> 58);
}

int key135(u64 b, int f) {
	return (int)(((b & bmask135[f]) * 0x0202020202020202LL) >> 58);
}

#define DUALATT(x, y, c) (attacked(x, c) | attacked(y, c) | ((KCAP(x, c) | KCAP(y, c)) & P.piece[KING]))
u64 reach(int f, int c, u64 occ) {
	return ((nmoves[f] & P.piece[KNIGHT]) | (RATT(f, occ) & RQU) | (BATT(f, occ) & BQU)) & P.color[c^1];
}

u64 attacked(int f, int c) {
	return (PCAP(f, c) & P.piece[PAWN]) | reach(f, c, BOARD);
}

u64 pinnedPieces(int k, int oc) {
	u64 pin = 0LL, occ = BOARD, b = ((RXRAY1(k, occ) | RXRAY2(k, occ)) & P.color[oc]) & RQU;
	while (b) {
		int t = pullLsb(&b);
		pin |= RATT(k, occ) & RATT(t, occ) & P.color[oc^1];
	}
	b = ((BXRAY3(k, occ) | BXRAY4(k, occ)) & P.color[oc]) & BQU;
	while (b) {
		int t = pullLsb(&b);
		pin |= BATT(k, occ) & BATT(t, occ) & P.color[oc^1];
	}
	return pin;
}

/* precondition: f and t are on common rank (8), file (16), diagonal (32) or antidiagonal (64) */
int getDir(int f, int t) {
	if (!((f ^ t) & 56)) return 8;
	if (!((f ^ t) & 7)) return 16;
	return (!((f - t) % 9)) ? 32 : 64;
}

int changeMat(int m, int c, int d) {
	int dm = pval[CAP(m)];
	if (PROM(m)) dm += -pval[PAWN] + pval[PROM(m)];
	P.sf[c] += d*fval[PROM(m)];
	P.sf[c^1] -= d*fval[CAP(m)];
	return c ? -d*dm : d*dm;
}

/* move is for both doMove and undoMove, only for undo the globalflags have to be restored (count, castle, enpass..) */
void move(Move m, int c, int d) {
	int f = FROM(m), t = TO(m), p = PIECE(m), a = CAP(m);

	P.color[c] ^= BIT[f] | BIT[t];
	P.piece[p] ^= BIT[f] | BIT[t];
	P.hash ^= hashxor[f | p << 6 | c << 9];
	P.hash ^= hashxor[t | p << 6 | c << 9];

	if (a) {
		if (a == ENP) { // Enpassant Capture
			t = (t&7) | (f&56); a = PAWN;
		} else if (a == ROOK) { //Revoke castling rights.
			flags &= crevoke[t];
		}
		P.piece[a] ^= BIT[t];
		P.color[c^1] ^= BIT[t];
		P.hash ^= hashxor[t | a << 6 | (c^1) << 9];
		count &= 0x3FF; //Reset Fifty Counter
		MAT += changeMat(m, c, d);
	}
	if (p == PAWN) {
		if (((f^t)&8) == 0) flags |= f^24; //Enpassant
		else if ((t&56) == 0 || (t&56) == 56) {
			P.piece[PAWN] ^= BIT[t];
			P.piece[PROM(m)] ^= BIT[t];
			P.hash ^= hashxor[t | PAWN << 6 | c << 9];
			P.hash ^= hashxor[t | PROM(m) << 6 | c << 9];
			if (!a) MAT += changeMat(m, c, d);
		}
		count &= 0x3FF; //Reset Fifty Counter
	} else if (p == KING) {
		if (P.king[c] == f) P.king[c] = t; else P.king[c] = f;
		flags &= ~(320 << c); // Lose castling rights
		if (((f^t)&3) == 2) { // Castle
			t = (f < t) ? f + 1 : f - 1;
			f = (f < t) ? f + 3 : f - 4;
			P.color[c] ^= BIT[f] | BIT[t];
			P.piece[ROOK] ^= BIT[f] | BIT[t];
			P.hash ^= hashxor[f | ROOK << 6 | c << 9];
			P.hash ^= hashxor[t | ROOK << 6 | c << 9];
		}
	} else if (p == ROOK) {
		flags &= crevoke[f];
	}
	BOARD = P.color[0] | P.color[1];
}

void doMove(Move m, int c) {
	mstack[COUNT] = count | (flags << 17) | (((u64)m) << 27);
	flags &= 960; // reset en-passant flags
	count += 0x401; // counter++
	if (m) move(m, c, 1);
}

void undoMove(Move m, int c) {
	u64 u = mstack[COUNT - 1];
	if (m) move(m, c, -1);
	count = u & 0x1FFFF;
	flags = (u >> 17L) & 0x3FF;
}

void regMoves(Move m, u64 bt, Movep* mp, int cap) {
	while (bt) {
		int t = pullLsb(&bt);
		mp->list[mp->n++] = m | _TO(t) | (cap ? _CAP(identPiece(t)) : 0LL);
	}
}

void regPromotions(int f, int c, u64 bt, Movep* mp, int cap, int queen) {
	while (bt) {
		int t = pullLsb(&bt);
		Move m = f | _ONMV(c) | _PIECE(PAWN) | _TO(t) | (cap ? _CAP(identPiece(t)) : 0);
		if (queen) mp->list[mp->n++] = m | _PROM(QUEEN);
		mp->list[mp->n++] = m | _PROM(KNIGHT);
		mp->list[mp->n++] = m | _PROM(ROOK);
		mp->list[mp->n++] = m | _PROM(BISHOP);
	}
}

void regKings(Move m, u64 bt, Movep* mp, int c, int cap) {
	while (bt) {
		int t = pullLsb(&bt);
		if (attacked(t, c) | (KCAP(t, c) & P.piece[KING])) continue;
		mp->list[mp->n++] = m | _TO(t) | (cap ? _CAP(identPiece(t)) : 0LL);
	}
}

int generateCheckEsc(u64 ch, u64 apin, int c, int k, Movep *mp) {
	u64 cc, fl, occ = BOARD;
	int d, bf = bitcnt(ch);
	BOARD ^= BIT[k]; // cannot use local occ here, because of method "attacked"
	regKings(PREMOVE(k, KING), KCAP(k, c), mp, c, 1);
	regKings(PREMOVE(k, KING), KMOVE(k, occ), mp, c, 0);
	BOARD ^= BIT[k];
	if (bf > 1) return bf; //Doublecheck
	bf = getLsb(ch);

	cc = attacked(bf, c^1) & apin;  //Can we capture the checker?
	while (cc) {
		int cf = pullLsb(&cc), p = identPiece(cf);
		if (p == PAWN && RANK[c][cf] == 7) regPromotions(cf, c, ch, mp, 1, 1);
		else regMoves(PREMOVE(cf, p), ch, mp, 1);
	}
	if (ENPASS && (ch & P.piece[PAWN])) { //Enpassant capture of attacking Pawn
		cc = PCAP(ENPASS, c^1) & P.piece[PAWN] & apin;
		while (cc) {
			int cf = pullLsb(&cc);
			regMoves(PREMOVE(cf, PAWN), BIT[ENPASS], mp, 1);
		}
	}
	if (ch & (nmoves[k] | kmoves[k])) return 1; //We can't move anything in between!

	d = getDir(bf, k);
	if (d == 8) fl = RATT1(bf, occ) & RATT1(k, occ);
	else if (d == 16) fl = RATT2(bf, occ) & RATT2(k, occ);
	else if (d == 32) fl = BATT3(bf, occ) & BATT3(k, occ);
	else fl = BATT4(bf, occ) & BATT4(k, occ);

	while (fl) {
		int f = pullLsb(&fl);
		cc = reach(f, c^1, occ) & apin;
		while (cc) {
			int cf = pullLsb(&cc);
			regMoves(PREMOVE(cf, identPiece(cf)), BIT[f], mp, 0);
		}
		bf = c ? f+8 : f-8;
		if (bf < 0 || bf > 63 || !(cc = P.piece[PAWN] & P.color[c] & apin)) continue;
		if (BIT[bf] & cc) {
			if (RANK[c][bf] == 7) regPromotions(bf, c, BIT[f], mp, 0, 1);
			else regMoves(PREMOVE(bf, PAWN), BIT[f], mp, 0);
		}
		if (RANK[c][f] == 4 && !(occ & BIT[bf]) && (BIT[c ? f + 16 : f - 16] & cc))
			regMoves(PREMOVE(c ? f+16 : f-16, PAWN), BIT[f], mp, 0);
	}
	return 1;
}

#define GSLIDES(p, m) for (b = P.piece[p] & cb; b;) { int f = pullLsb(&b); regMoves(PREMOVE(f, p), m & tb, mp, q); }
void generateSlides(int c, int k, u64 pin, Movep* mp, u64 tb, u64 cb, int q) {
	u64 b, occ = BOARD;
	GSLIDES(KNIGHT, nmoves[f]);
	GSLIDES(ROOK, RATT(f, occ));
	GSLIDES(BISHOP, BATT(f, occ));
	GSLIDES(QUEEN, (RATT(f, occ) | BATT(f, occ)));

	if (pin) for (b = pin & (RQU | P.piece[BISHOP]); b;) {
		int f = pullLsb(&b), p = identPiece(f), t = p | getDir(f, k);
		if ((t & 10) == 10) regMoves(PREMOVE(f, p), RATT1(f, occ) & tb, mp, q);
		if ((t & 18) == 18) regMoves(PREMOVE(f, p), RATT2(f, occ) & tb, mp, q);
		if ((t & 33) == 33) regMoves(PREMOVE(f, p), BATT3(f, occ) & tb, mp, q);
		if ((t & 65) == 65) regMoves(PREMOVE(f, p), BATT4(f, occ) & tb, mp, q);
	}
}

void generateQuiet(int c, int k, u64 pin, Movep* mp) {
	int f, r; u64 b, cb = P.color[c] & (~pin), tb = ~BOARD, occ = BOARD;

	for (b = P.piece[PAWN] & P.color[c]; b;) {
		f = pullLsb(&b);
		u32 t = BIT[f] & pin ? getDir(f, k) : 17;
		if (t == 8) continue; else r = RANK[c][f];
		u64 m = t & 16 ? PMOVE(f, c, occ) : 0;
		if (m && r == 2) m |= PMOVE(c ? f - 8 : f + 8, c, occ);
		if (r == 7) {
			u64 a = (t == 17) ? PCAP(f, c) : (t == 32) ? PCA3(f, c) : (t == 64) ? PCA4(f, c) : 0LL;
			if (a) regPromotions(f, c, a, mp, 1, 0);
			if (m) regPromotions(f, c, m, mp, 0, 0);
		} else if (r != 6) {
			regMoves(PREMOVE(f, PAWN), m, mp, 0);
		}
	}

	if (CASTLE(c)) {
		for (b = P.piece[ROOK] & cb; b;) {
			f = pullLsb(&b);
			if (f == 63 && (flags & 128) && !(occ & (3LL << 61)))
				if (!DUALATT(61, 62, c)) regMoves(PREMOVE(60, KING), 1LL << 62, mp, 0);
			if (f == 56 && (flags & 512) && !(occ & (7LL << 57)))
				if (!DUALATT(59, 58, c)) regMoves(PREMOVE(60, KING), 1LL << 58, mp, 0);
			if (f == 7 && (flags & 64) && !(occ & (3LL << 5)))
				if (!DUALATT(5, 6, c)) regMoves(PREMOVE(4, KING), 1LL << 6, mp, 0);
			if (f == 0 && (flags & 256) && !(occ & (7LL << 1)))
				if (!DUALATT(3, 2, c)) regMoves(PREMOVE(4, KING), 1LL << 2, mp, 0);
		}
	}

	generateSlides(c, k, pin, mp, tb, cb, 0);
	regKings(PREMOVE(k, KING), kmoves[k] & tb, mp, c, 0);
}

void generateNoisy(int c, int k, u64 pin, Movep *mp) {
	int f, r; u64 b, cb = P.color[c] & (~pin), tb = P.color[c^1], occ = BOARD;

	for (b = P.piece[PAWN] & P.color[c]; b;) {
		f = pullLsb(&b);
		u32 t = BIT[f] & pin ? getDir(f, k) : 17;
		if (t == 8) continue; else r = RANK[c][f];
		u64 m = t & 16 ? PMOVE(f, c, occ) : 0;
		u64 a = (t == 17) ? PCAP(f, c) : (t == 32) ? PCA3(f, c) : (t == 64) ? PCA4(f, c) : 0LL;
		if (r >= 6) {
			if (a) regMoves(PREMOVE(f, PAWN) | (r == 7 ? _PROM(QUEEN) : 0), a, mp, 1);
			if (m) regMoves(PREMOVE(f, PAWN) | (r == 7 ? _PROM(QUEEN) : 0), m, mp, 0);
		} else {
			if (t == 17 && ENPASS && (BIT[ENPASS] & pcaps[c][f])) {
				occ ^= BIT[ENPASS ^ 8];
				if (!(RATT1(f, occ) & BIT[k]) || !(RATT1(f, occ) & tb & RQU)) {
					a = a | BIT[ENPASS];
				}
				occ ^= BIT[ENPASS ^ 8];
			}
			regMoves(PREMOVE(f, PAWN), a, mp, 1);
		}
	}
	generateSlides(c, k, pin, mp, tb, cb, 1);
	regKings(PREMOVE(k, KING), kmoves[k] & tb, mp, c, 1);
}

#define GENERATE(c, mp) generate(attacked(P.king[c], c), c, mp, 1, 1)
int generate(u64 ch, int c, Movep *mp, int noisy, int quiet) {
	int k = P.king[c];
	u64 pin = pinnedPieces(k, c^1);
	mp->n = 0;
	if (ch) return generateCheckEsc(ch, ~pin, c, k, mp);
	if (noisy) generateNoisy(c, k, pin, mp);
	if (quiet) generateQuiet(c, k, pin, mp);
	return 0;
}

int swap(Move m) { //SEE
	int s_list[32], f = FROM(m), t = TO(m), c = ONMV(m), piece = PIECE(m), nc = 1;
	u64 temp, cAttacks, occ = BOARD, attacks = ((PCAP(t, 0) | PCAP(t, 1)) & P.piece[PAWN])
						| (nmoves[t] & P.piece[KNIGHT]) | (kmoves[t] & P.piece[KING]);

	s_list[0] = pval[CAP(m)];
	occ &= ~BIT[f];

	do {
		s_list[nc] = -s_list[nc - 1] + pval[piece];
		c ^= 1;
		attacks |= (BATT(t, occ) & BQU) | (RATT(t, occ) & RQU);
		attacks &= occ;
		if (!(cAttacks = attacks & P.color[c])) break;

		if ((temp = P.piece[PAWN] & cAttacks)) piece = PAWN;
		else if ((temp = P.piece[KNIGHT] & cAttacks)) piece = KNIGHT;
		else if ((temp = P.piece[BISHOP] & cAttacks)) piece = BISHOP;
		else if ((temp = P.piece[ROOK] & cAttacks)) piece = ROOK;
		else if ((temp = P.piece[QUEEN] & cAttacks)) piece = QUEEN;
		else { nc += !(P.color[c^1] & attacks); break; } // KING

		occ ^= temp & -temp;
	} while (pval[piece] >= s_list[nc++]);

	while (--nc)
		if (s_list[nc] > -s_list[nc - 1])
			s_list[nc - 1] = -s_list[nc];

	return s_list[0];
}

/* In normal search basic move ordering heuristics are used, in quiesce (ply < 0) value of captured piece */
Move pick(Movep* mp, int s, int ply) {
	Move m;
	int i, t, pi = 0, vmax = -9999;
	for (i = s; i < mp->n; i++) {
		m = mp->list[i];
		if (ply < 0) {
			t = pval[CAP(m)] - fval[PIECE(m)];
		} else if (m == killer[ply]) {
			pi = i;
			break;
		} else t = history[m & 0x1FFF];
		if (t > vmax) vmax = t, pi = i;
	}
	m = mp->list[pi];
	if (pi != s) mp->list[pi] = mp->list[s];
	return m;
}

u64 pawnAttack(int c) {
	u64 p = P.color[c] & P.piece[PAWN];
	return c ? (p &~ fileb[7]) >> 7 | (p &~ fileb[0]) >> 9 : (p &~ fileb[0]) << 7 | (p &~ fileb[7]) << 9;
}

u64 mobilityb(int c, u64 occ) {
	u64 b = c ? rankb[6] | (occ << 8) : rankb[1] | (occ >> 8);
	b &= P.color[c] & P.piece[PAWN];
	return ~(b | pawnAttack(c^1));
}

int kmobilf(int c) {
	int km = kmobil[P.king[c]], sfo = P.sf[c^1];
	if (!P.sf[c] && sfo == 5 && P.piece[BISHOP] && !P.piece[PAWN]) { // BNK_vs_k
		int bc = bishcorn[P.king[c]] << 5;
		if (P.piece[BISHOP] & whitesq) km += bc; else km -= bc;
	}
	return sfo < 14 ? km : km * (16 - sfo) /4;
}

#define MOBILITY(a, mb) (bitcnt(a) + bitcnt(a & mb) + bitcnt(a & mb & c3))
/* The eval for Color c. It's almost only mobility. */
int evalc(int c) {
	int f, mn = 0, katt = 0, oc = c^1, egf = 10400/(80 + P.sf[c] + P.sf[oc]) + randm;
	u64 b, a, cb = P.color[c], ocb = P.color[oc], occ = BOARD, mb = mobilityb(c, occ) & centr;
	u64 kn = kmoves[P.king[oc]] & (~P.piece[PAWN]), c3 = centr2 | rankb[c ? 1 : 6];

	b = P.piece[PAWN] & cb;
	while (b) {
		f = pullLsb(&b);

		/* The only non-mobility eval is the detection of free pawns/hanging pawns */
		int ppos = pawnprg[c][f] * egf * egf / 96 / 96;
		if (!(pawnfree[c][f] & P.piece[PAWN] & ocb)) ppos <<= 1; //Free run?

		if (!(pawnhelp[c][f] & P.piece[PAWN] & cb)) { // No support
			int openfile = !(pawnfile[c][f] & P.piece[PAWN] & ocb);
			ppos -= (openfile ? 32 : 10); // Open file
		}

		a = pcaps[c][f] & occ;
		ppos += bitcnt(a & cb) << 2;
		katt += bitcnt(a & kn);
		mn += ppos;
	}

	b = P.piece[KNIGHT] & cb;
	while (b) {
		a = nmoves[pullLsb(&b)];
		katt += bitcnt(a & kn);
		mn += MOBILITY(a, mb) << 2;
	}

	occ ^= BIT[P.king[oc]]; //Opposite King doesn't block mobility at all
	occ ^= P.piece[QUEEN]; //Queens don't block mobility for anybody.
	b = P.piece[QUEEN] & cb;
	while (b) {
		f = pullLsb(&b);
		a = BATT(f, occ) | RATT(f, occ);
		katt += bitcnt(a & kn);
		mn += MOBILITY(a, mb) * egf * egf / 80 / 80;
	}

	occ ^= P.piece[ROOK]; //Rooks don't block mobility for bishop
	b = P.piece[BISHOP] & cb;
	while (b) {
		f = pullLsb(&b);
		a = BATT(f, occ);
		katt += bitcnt(a & kn);
		mn += MOBILITY(a, mb) << 2;
	}

	b = P.piece[ROOK] & cb;
	while (b) {
		f = pullLsb(&b);
		a = RATT(f, occ);
		katt += bitcnt(a & kn);
		mn += MOBILITY(a, mb) * egf / 40;
	}

	return mn + kmobilf(c) + katt * (P.sf[c] + 2); //Adapt bonus for attacking king squares
}

int eval(int c) {
	int ev = evalc(c) - evalc(c^1);
	eval1++;

	if ((MAT < 0 && NOMATEMAT(1)) || (MAT > 0 && NOMATEMAT(0))) return ev;

	return ev + (c ? -MAT : MAT);
}

int quiesce(u64 ch, int c, int ply, int alpha, int beta) {
	int i, best = -MAXSCORE;

	if ((++qnodes & CNODES) == 0) {
		if (bioskey() && !sabort) sabort = 1;
		if (!pondering && getTime() - starttime > maxtime) sabort = 1;
		if (node_limit > 0 && (nodes + qnodes) >= node_limit) sabort = 1;
	}
	if (sabort) return -MAXSCORE;

	if (ply == 127) return eval(c);
	if (!ch) do {
		int cmat = (c ? -MAT : MAT);
		if (cmat - 125 >= beta) return beta;
		if (cmat + 85 <= alpha) break;
		best = eval(c);
		if (best >= beta) return beta;
		if (best > alpha) alpha = best;
	} while(0);

	Movep mp; generate(ch, c, &mp, 1, 0);
	if (mp.n == 0) return ch ? -MAXSCORE+ply : best > -MAXSCORE ? best : eval(c);

	Pos pos; pos.hash = 0;
	for (i = 0; i < mp.n; i++) {
		Move m = pick(&mp, i, -1);
		if (!ch && pval[PIECE(m)] > pval[CAP(m)] && swap(m) < 0) continue;

		if (!pos.hash) memcpy(&pos, &P, sizeof(Pos));
		doMove(m, c);
		qnodes++;

		int w = -quiesce(attacked(P.king[c^1], c^1), c^1, ply + 1, -beta, -alpha);

		undoMove(0, c); memcpy(&P, &pos, sizeof(Pos));

		if (w > best) {
			if ((best = w) > alpha) {
				if ((alpha = w) >= beta) return beta;
			}
		}
		if (ch) break;
	}
	return best > -MAXSCORE ? best : eval(c);
}

int retPVMove(int c) {
	int i;
	Movep mp; GENERATE(c, &mp);
	for (i = 0; i < mp.n; i++) {
		Move m = mp.list[i];
		if (m == pv[0][0]) return m;
	}
	return 0;
}

int isDraw(u64 hp, int nrep) {
	if (count > 0xFFF) { //fifty > 3
		int i, c = 0, n = COUNT - (count >> 10);
		if (count >= 0x400*100) return 2; //100 plies
		for (i = COUNT - 2; i >= n; i-=2)
			if (hstack[i] == hp && ++c == nrep) return 1;
	}
	return (!P.piece[PAWN] && !RQU && bitcnt(BOARD) <= 3) ? 3 : 0; //Check for mating material
}

const int nullvar[] = {13, 43, 149, 519, 1809, 6311, 22027};
static int nullvariance(int delta) {
	int r = 0;
	if (delta >= 4) for (r = 1; r <= 7; r++) if (delta < nullvar[r - 1]) break;
	return r;
}

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define HASHP (P.hash ^ hashxor[flags | 1024 | c << 11])
int search(u64 ch, int c, int d, int ply, int alpha, int beta, int null, Move sem) {
	int i, j, n, w = alpha, oc = c^1, pvnode = beta > alpha + 1;

	if (ply) pv[ply][ply] = 0;
	if ((++nodes & CNODES) == 0) {
		if (bioskey() && !sabort) sabort = 1;
		if (!pondering && getTime() - starttime > maxtime) sabort = 1;
		if (node_limit > 0 && (nodes + qnodes) >= node_limit) sabort = 1; // Fix: Added node limit check
	}
	// FIX: Return a special value on abort to indicate unreliability
	if (sabort) return -MAXSCORE;

	u64 hp = HASHP;
	if (ply && isDraw(hp, 1)) return 0;

	if (ch) d++; // Check extension
	if (d <= 0 || ply > 100) return quiesce(ch, c, ply, alpha, beta);

	if (alpha < -MAXSCORE+ply) alpha = -MAXSCORE+ply;
	if (beta > MAXSCORE-ply-1) beta = MAXSCORE-ply-1;
	if (alpha >= beta) return alpha;

	Move hmove = ply ? 0 : retPVMove(c);

	entry* he = &hashDB[hp & HMASK];
	if (he->key == hp && !sem) {
		if (he->depth >= d) {
			if (he->type <= EXACT && he->value >= beta) return beta;
			if (he->type >= EXACT && he->value <= alpha) return alpha;
		}
		if (!hmove) hmove = he->move;
	}

	int wstat = wstack[COUNT] = ch ? -MAXSCORE : he->key == hp ? he->value : eval(c);
	if (!ch && he->key != hp) *he = (entry) {.key = hp, .move = hmove, .value = wstat, .depth = d, .type = LOWER};
	if (!ch && !pvnode) {
		if (d <= 3 && wstat + 400 < beta) { w = quiesce(ch, c, ply, alpha, beta); if (w < beta) return w; }
		if (d <= 8 && wstat - 88*d > beta) return beta;
	}

	int raising = !ch && ply >= 2 && wstat >= wstack[COUNT-2] && wstack[COUNT-2] != -MAXSCORE;
	hstack[COUNT] = hp;
	//Null Move - pvnode => null == 0
	null = null && !ch && beta > -MAXSCORE+500 && d > 1 && wstat > alpha && (ply < 2 || (mstack[COUNT-2] >> 27));
	if (null && bitcnt(P.color[c] & (~P.piece[PAWN]) & (~pinnedPieces(P.king[c], oc))) > 1) {
		int R = (9 + d + nullvariance(wstat - alpha))/4 + raising;
		doMove(0, c);
		w = -search(0LL, oc, d-R, ply+1, -beta, 1-beta, 0, 0);
		undoMove(0, c);
		if (w >= beta) return beta;
	}

	if (d >= 4 && !hmove) d--; // Internal Iterative Reduction (IIR)

	Movep mp, mpq; mpq.n = 0;
	Pos pos; pos.hash = 0;
	int first = NO_MOVE, hismax = -9999;
	for (n = HASH; n <= (ch ? NOISY : QUIET); n++) {
		int nd = d - 1;
		if (n == HASH) {
			if (!hmove) continue;
			mp.n = 1;
			if (d >= 8 && ply && he->type == LOWER && he->depth >= d - 3) {
				int bc = he->value - d;
				int ss = search(ch, c, d >> 1, ply, bc-1, bc, 0, hmove);  // Singular extensions
				if (ss < bc) nd++; else if (ss >= beta) return (ss + beta)/2;
			}
		} else if (n == NOISY) {
			generate(ch, c, &mp, 1, 0);
		} else {
			generate(ch, c, &mp, 0, 1);
		}
		for (i = 0; i < mp.n; i++) {
			Move m = n == HASH ? hmove : n == NOISY ? pick(&mp, i, -1) : pick(&mp, i, ply);
			if ((n != HASH && m == hmove) || m == sem) continue;
			if (!ch && n == QUIET && mpq.n > 2*d*(raising+1)) {
				n = EXIT; break; // LMP
			}
			if (n != HASH && alpha > -MAXSCORE+500 && d < 8 && swap(m) < -d*60) continue;

			int ext = 0, quiet = !CAP(m) && !PROM(m);
			if (!pos.hash) memcpy(&pos, &P, sizeof(Pos));
			doMove(m, c);
			if (quiet) mpq.list[mpq.n++] = m;
			u64 nch = attacked(P.king[oc], oc);
			if (nch || pvnode || ch || (PIECE(m) == PAWN && !(pawnfree[c][TO(m)] & P.piece[PAWN] & P.color[oc])));
			else if (n == NOISY && d >= 2 && swap(m) < 0) ext-= (d + 1)/(3+raising); //Reduce bad exchanges
			else if (n == QUIET && m != killer[ply]) { // LMR, but don't reduce killers
				int his = history[m & 0x1FFF];
				if (his > hismax) hismax = his;
				else if (d < 5 && (his < 0 || his*his < hismax)) {
					undoMove(0, c); memcpy(&P, &pos, sizeof(Pos)); continue;
				} else ext-= (d + 1)/3;
			}

			int firstPVNode = first == NO_MOVE && pvnode;
			if (!firstPVNode) w = -search(nch, oc, nd+ext, ply+1, -alpha-1, -alpha, 1, 0);
			if (ext < 0 && w > alpha) w = -search(nch, oc, nd, ply+1, -alpha-1, -alpha, 1, 0);
			if (firstPVNode || (w > alpha && w < beta)) w = -search(nch, oc, nd+ext, ply+1, -beta, -alpha, 0, 0);

			undoMove(0, c); memcpy(&P, &pos, sizeof(Pos));
			// FIX: Check for abort after each recursive call
			if (sabort) return -MAXSCORE;

			if (w > alpha) {
				alpha = w, first = GOOD_MOVE;
				pv[ply][ply] = m;
				for (j = ply +1; (pv[ply][j] = pv[ply +1][j]); j++);

				if (w >= beta) {
					if (quiet) {
						int his = MIN(d*d, 512);
						killer[ply] = m;
						history[m & 0x1FFF] += his - history[m & 0x1FFF]*his/512;

						for (j = 0; j < mpq.n - 1; j++) {
							Move m2 = mpq.list[j];
							history[m2 & 0x1FFF] += -his - history[m2 & 0x1FFF]*his/512;
						}
					}
					n = EXIT; break;
				}
			} else if (first == NO_MOVE) first = ANY_MOVE;
		}
	}
	if (first == NO_MOVE) alpha = ch || sem ? -MAXSCORE + ply : 0;

	char type = UPPER;
	if (first == GOOD_MOVE) type = alpha >= beta ? LOWER : EXACT, hmove = pv[ply][ply]; // Found good move

	if (!sem) *he = (entry) {.key = hp, .move = hmove, .value = alpha, .depth = d, .type = type};

	return alpha;
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
	if (ISRANK(s[0])) { h = *s++; if (s[0] == 'x') s++; }
	if (!ISFILE(s[0])) return -1;
	c1 = *s++;
	if (s[0] == 'x') s++;
	if (ISFILE(s[0])) { h = c1; c1 = *s++; }
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
	if (p) return ismove(p, to, from, piece, prom, h) ? p : 0;

	Movep mp; GENERATE(c, &mp);
	for (i = 0; i < mp.n; i++) if (ismove(mp.list[i], to, from, piece, prom, h)) return mp.list[i];
	return 0;
}


void displaym(Move m) {
	printf("%c%c%c%c", 'a' + FROM(m) % 8, '1' + FROM(m) / 8, 'a' + TO(m) % 8, '1' + TO(m) / 8);
	if (PROM(m)) printf("%c", pieceChar[PROM(m)]+32);
}

void displaypv() {
	int i;
	for (i = 0; pv[0][i]; i++) {
		displaym(pv[0][i]); printf(" ");
	}
}

void calc(int tm) {
	int i, j, w, d, m2go;
	nodes = qnodes = 0LL;
	node_limit = 0;
	if (!mps) {
		m2go = mps == 0 ? 32 : 1 + mps - ((COUNT/2) % mps);
	} else {
		m2go = mps; // uci movestogo
	}
	u32 searchtime = 0;
	if (st > 0) searchtime = st*1000LL;
	else {
		u32 tmsh = MAX(tm*8L-50-m2go*5, 5);
		searchtime = MIN(tm*6UL/m2go + inc*500L, tmsh);
	}
	maxtime = searchtime;

	if (pondering) {
		// If pondering, use half the remaining time
		maxtime = (ttime + inc * m2go)/2;
	}

	starttime = getTime();
	u64 ch = attacked(P.king[onmove], onmove);
	memset(history, 0, sizeof(history));
	memset(killer, 0, sizeof(killer));

	sabort = w = d = 0;
	eval1 = qnodes = nodes = 0LL;
	if (book) {
		if (!bkcount[onmove]) book = 0;
		else {
			j = (int)(hashxor[starttime & 4095] & 0xFFFFFF) % bkcount[onmove];
			for (i = 0; i < BKSIZE; i++) {
				if (bkflag[i] == onmove && j == d++) { pv[0][0] = bkmove[i*32 + COUNT]; break; }
			}
		}
	}
	if (!book) for (d = 1; d <= sd; d++) {
		int alpha = d > 6 ? w - 13 : -MAXSCORE, beta = d > 6 ? w + 13: MAXSCORE, delta = 18;
		Move bestm = pv[0][0];

		for (;;) {
			w = search(ch, onmove, d, 0, alpha, beta, 0, 0);
			if (sabort) break;

			if (w <= alpha) alpha -= delta, beta = (alpha + beta)/2;
			else if (w >= beta) beta += delta;
			else break;
			delta += delta * 2 / 3;
		}

		u32 t1 = (u32)(getTime() - starttime);
		if (pv[0][0] && w > -MAXSCORE && (!pon || pondering)) {
			printf("info depth %d score cp %d time %lu nodes %llu nps %llu pv ", d, MEVAL(w), t1, nodes + qnodes, nodes * 1000 / (t1 + 1));
			displaypv();
			printf("\n");
		} //
		if (sabort) break;
		if (d >= sd) break;
		if (t1 > maxtime) break;
	} //

	// FIX: Ensure a legal move is always found, even if search is aborted early
	if (pv[0][0] == 0) {
		Movep mp;
		generate(attacked(P.king[onmove], onmove), onmove, &mp, 1, 1);
		if (mp.n > 0) {
			pv[0][0] = mp.list[0];
		} //
	} //

	Move final_move = pv[0][0];
	Move ponder_move = 0;
	if (pv[0][1]) {
		ponder_move = pv[0][1];
	} else { // No ponder from search, generate one move ahead
		Pos temp_pos;
		memcpy(&temp_pos, &P, sizeof(Pos));
		doMove(final_move, onmove);
		Movep mp;
		generate(attacked(P.king[onmove^1], onmove^1), onmove^1, &mp, 1, 1);
		undoMove(final_move, onmove);
		memcpy(&P, &temp_pos, sizeof(Pos));
		if (mp.n > 0) {
			ponder_move = mp.list[0];
		} //
	} //

	printf("bestmove "); displaym(final_move);
	if (ponder_move) {
		printf(" ponder "); displaym(ponder_move);
	}
	printf("\n");
}

void do_uci_position(char* line) {
	char* token = strtok(line, " ");

	if (strcmp(token, "startpos") == 0) {
		_parse_fen(sfen, 1);
		onmove = 0;
	} else if (strcmp(token, "fen") == 0) {
		char fen_str[256];
		char* ptr = fen_str;

		token = strtok(NULL, " ");
		while (token != NULL && strcmp(token, "moves") != 0) {
			strcpy(ptr, token);
			ptr += strlen(token);
			*ptr = ' ';
			ptr++;
			token = strtok(NULL, " ");
		} //
		*--ptr = '\0';

		_parse_fen(fen_str, 1);
	} //
	token = strtok(NULL, " ");
	if (token != NULL && strcmp(token, "moves") == 0) {
		token = strtok(NULL, " ");
		while (token != NULL) {
			Move m = parseMove(token, onmove, 0);
			if (m) {
				doMove(m, onmove);
				onmove ^= 1;
			} //
			token = strtok(NULL, " ");
		} //
	} //
} //

void uci_loop() {
	char line[8192];

	while(fgets(line, sizeof(line), stdin)) {
		if (strncmp(line, "uci", 3) == 0) {
			printf("id name OliThink "VER"\n");
			printf("id author Oliver Brausch\n");
			// TODO: Add options
			printf("uciok\n");
		} else if (strncmp(line, "isready", 7) == 0) {
			printf("readyok\n");
		} else if (strncmp(line, "ucinewgame", 10) == 0) {
			_newGame();
		} else if (strncmp(line, "position", 8) == 0) {
			do_uci_position(line + 9);
		} else if (strncmp(line, "go", 2) == 0) {
			char *token = strtok(line + 3, " ");
			ttime = 30000;
			inc = 0;
			sd = 64;
			st = 0;
			mps = 0; // Initialize mps
			pondering = 0;
			node_limit = 0; // Initialize node_limit to 0

			while (token != NULL) {
				if (strcmp(token, "wtime") == 0) {
					token = strtok(NULL, " ");
					if (onmove == 0) {
						ttime = atoi(token);
						if (ttime > 1000) ttime /= 8.0;
					}
				} else if (strcmp(token, "btime") == 0) {
					token = strtok(NULL, " ");
					if (onmove == 1) {
						ttime = atoi(token);
						if (ttime > 1000) ttime /= 8.0;
					}
				} else if (strcmp(token, "winc") == 0) {
					token = strtok(NULL, " ");
					if (onmove == 0) inc = atoi(token);
				} else if (strcmp(token, "binc") == 0) {
					token = strtok(NULL, " ");
					if (onmove == 1) inc = atoi(token);
				} else if (strcmp(token, "movetime") == 0) {
					token = strtok(NULL, " ");
					st = atoi(token);
				} else if (strcmp(token, "depth") == 0) {
					token = strtok(NULL, " ");
					sd = atoi(token);
				} else if (strcmp(token, "nodes") == 0) {
					token = strtok(NULL, " ");
					node_limit = strtoull(token, NULL, 10);
				} else if (strcmp(token, "ponder") == 0) {
					pondering = 1;
				} else if (strcmp(token, "movestogo") == 0) {
					token = strtok(NULL, " ");
					mps = atoi(token);
				}
				token = strtok(NULL, " ");
			} //
			if (pondering) {
				// Pondering logic is already in calc, just need to call it.
				calc(ttime);
			} else {
				calc(ttime);
			} //
		} else if (strncmp(line, "stop", 4) == 0) {
			sabort = 1;
		} else if (strncmp(line, "ponderhit", 9) == 0) {
			pondering = 0;
			sabort = 0;
		} else if (strncmp(line, "quit", 4) == 0) {
			break;
		} //
	} //
} //

int main(int argc, char **argv) {
	int i; u64 m, n;

	setbuf(stdout, NULL);
	setbuf(stdin, NULL);
	for (i = 4096, n = 1, m = 6364136223846793005LL; i--; hashxor[4095-i] = n = n*m +1LL);
	for (i = 0; i < 64; i++) BIT[i] = 1LL << i;
	for (i = 0; i < 128; i++) pmoves[0][i] = pawnfree[0][i] = pawnfile[0][i] = pawnhelp[0][i] = 0LL;
	for (i = 0; i < 384; i++) pcaps[0][i] = 0LL;
	for (i = 0; i < 64; i++) bmask45[i] = _bishop45(i, 0LL, 0), bmask135[i] = _bishop135(i, 0LL, 0);
	for (i = 0; i < 64; i++) rmask0[i] = _rook0(i, 0LL, 0);
	for (i = 0; i < 64; i++) crevoke[i] = 0x3FF, rankb[i/8] |= BIT[i], fileb[i&7] |= BIT[i];
	for (i = 0; i < 64; i++) kmoves[i] = nmoves[i] = 0LL, RANK[0][i] = RANK[1][63-i] = 1 + (i >> 3);
	for (i = 0; i < 64; i++) if ((i/8)%2 != (i&7)%2) whitesq |= BIT[i];
	crevoke[7] ^= BIT[6];
	crevoke[63] ^= BIT[7];
	crevoke[0] ^= BIT[8];
	crevoke[56] ^= BIT[9];

	_init_rays((u64*) raysRank, (u64*) xrayRank, _rook0, key000, 1);
	_init_rays((u64*) raysAFile, (u64*) xrayAFile, _rook90, key090, 8);
	_init_shorts(nmoves, _knight);
	_init_shorts(kmoves, _king);
	_init_pawns(pmoves[0], pcaps[0], pawnfree[0], pawnfile[0], pawnhelp[0], pawnprg[0], 0);
	_init_pawns(pmoves[1], pcaps[1], pawnfree[1], pawnfile[1], pawnhelp[1], pawnprg[1], 1);

	for (i = 0; i < 64; i++) n = bitcnt(nmoves[i]), kmobil[i] = n == 2 ? 33 : n*10;
	for (i = 0; i < 64; i++) n = bitcnt(nmoves[i]), centr |= n >= 4 ? BIT[i] : 0L, centr2 |= n >= 8 ? BIT[i] : 0L;
	for (i = 0; i < 32; i++) bishcorn[i] = bishcorn[63-i] = (i&7) < 4 ? cornbase[(i&7) +i/8] : -cornbase[7-(i&7) +i/8];
	_newGame();

	uci_loop();

	return 0;
}