#define VER "5.11.9uci"
/* OliThink5 (c) Oliver Brausch 20.Aug.2025, ob112@web.de, http://brausch.org
 * (uci by Jim Ablett) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN64
#include <conio.h>
#include <intrin.h>
#include <windows.h>
#define bitcnt(x) __popcnt64(x)
#define getLsb(x) _tzcnt_u64(x)
#else
#include <sys/time.h>
struct timeval tv;
#define bitcnt(x) __builtin_popcountll(x)
#define getLsb(x) __builtin_ctzll(x)
#endif

#include "cerebrum.h"

NN_Accumulator nn_accumulator, nn_accumulator_copy;

static int use_nnue = 1; // default: ON
static int nnue_reload_pending = 0;
static char nnue_file_path[256] = "";
int validate_nnue_state();

//#define VALIDATE_NN      // nnue safety check - don't use on production


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

const int cornbase[] = {4, 4, 2, 1, 0, 0, 0};

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

#define RATT1(f, occ) raysRank[f & 7][key000(occ, f)] & rmask0[f]
#define RATT2(f, occ) raysAFile[f >> 3][key090(occ, f)] << (f & 7)
#define BATT3(f, occ) raysRank[f & 7][key045(occ, f)] & bmask45[f]
#define BATT4(f, occ) raysRank[f & 7][key135(occ, f)] & bmask135[f]
#define RXRAY1(f, occ) (xrayRank[f & 7][key000(occ, f)] & rmask0[f])
#define RXRAY2(f, occ) (xrayAFile[f >> 3][key090(occ, f)] << (f & 7))
#define BXRAY3(f, occ) (xrayRank[f & 7][key045(occ, f)] & bmask45[f])
#define BXRAY4(f, occ) (xrayRank[f & 7][key135(occ, f)] & bmask135[f])

#define RATT(f, occ) (RATT1(f, occ) | RATT2(f, occ))
#define BATT(f, occ) (BATT3(f, occ) | BATT4(f, occ))

#define KMOVE(x, occ) (kmoves[x] & ~occ)
#define KCAP(x, c) (kmoves[x] & P.color[c ^ 1])
#define PMOVE(x, c, occ) (pmoves[c][x] & ~occ)
#define PCAP(x, c) (pcaps[c][x] & P.color[c ^ 1])
#define PCA3(x, c)                                                             \
  (pcaps[c][(x) | 64] &                                                        \
   (P.color[c ^ 1] | (BIT[ENPASS] & (c ? 0xFF0000 : 0xFF0000000000))))
#define PCA4(x, c)                                                             \
  (pcaps[c][(x) | 128] &                                                       \
   (P.color[c ^ 1] | (BIT[ENPASS] & (c ? 0xFF0000 : 0xFF0000000000))))

#define ENPASS (flags & 63)
#define CASTLE(c) (flags & (320 << (c)))
#define COUNT (count & 0x3FF)
#define MEVAL(w)                                                               \
  (w > MAXSCORE - 500   ? (200001 + MAXSCORE - w) / 2                          \
   : w < 500 - MAXSCORE ? (-200000 - MAXSCORE - w) / 2                         \
                        : (w))
#define NOMATEMAT(c)                                                           \
  ((P.sf[c] <= 4 || (P.sf[c] <= 8 && P.sf[c] <= P.sf[c ^ 1] + 3)) &&           \
   (P.piece[PAWN] & P.color[c]) == 0)

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

// Add a global flag to track NNUE availability
static int nnue_available = 0;
static char nnue_filename[256] = NN_FILE; // default network file
void setup_nnue_for_position();

int eval(int c);
static Pos P;
static u64 node_limit;
static entry hashDB[HMASK + 1];
static u64 hstack[0x400], mstack[0x400], hashxor[0x1000];
static u64 pmoves[2][64], pawnprg[2][64], pawnfree[2][64], pawnfile[2][64],
    pawnhelp[2][64], RANK[2][64], pcaps[2][192];
static u64 BIT[64], nmoves[64], kmoves[64], bmask135[64], bmask45[64],
    rmask0[64], hash_hits;
static u64 rankb[8], fileb[8], raysRank[8][64], raysAFile[8][64],
    xrayRank[8][64], xrayAFile[8][64];
static u64 whitesq, centr, centr2, maxtime, starttime, eval1, nodes, qnodes;
static u32 crevoke[64], count, flags, pondering = 0, analyze = 0;
static Move pv[128][128], pon = 0, bkmove[BKSIZE * 32], killer[128];
static int wstack[0x400], history[0x2000];
static int kmobil[64], bishcorn[64];
static int _knight[8] = {-17, -10, 6, 15, 17, 10, -6, -15};
static int _king[8] = {-9, -1, 7, 8, 9, 1, -7, -8};
static int book, bkflag[BKSIZE], bkcount[3];
static int sabort, onmove, randm, sd = 64, ttime = 30000, mps = 0, inc = 0,
                                  st = 0, movestogo = 0, thinking = 0;
static char *sfen = "rnbqkbnr/pppppppp/////PPPPPPPP/RNBQKBNR w KQkq - 0 1";

const char* benchmark_positions[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",
    "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
    "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14 moves d4e6",
    "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14 moves g2g4",
    "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
    "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
    "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
    "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
    "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
    "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
    "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
    "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
    "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
    "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
    "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
    "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
    "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1 moves g5g6 f3e3 g6g5 e3f3",
    "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
    "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
    "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
    "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
    "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
    "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
    "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
    "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
    "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
    "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
    "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
    "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
    "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
    "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
    "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
    "4k3/3q1r2/1N2r1b1/3ppN2/2nPP3/1B1R2n1/2R1Q3/3K4 w - - 5 1",
    "5k2/8/3PK3/8/8/8/8/8 w - - 0 1"
};


static inline void build_nn_arrays(uint64_t wp[6], uint64_t bp[6]) {
  wp[0] = P.piece[PAWN] & P.color[0];
  wp[1] = P.piece[KNIGHT] & P.color[0];
  wp[2] = P.piece[BISHOP] & P.color[0];
  wp[3] = P.piece[ROOK] & P.color[0];
  wp[4] = P.piece[QUEEN] & P.color[0];
  wp[5] = P.piece[KING] & P.color[0];

  bp[0] = P.piece[PAWN] & P.color[1];
  bp[1] = P.piece[KNIGHT] & P.color[1];
  bp[2] = P.piece[BISHOP] & P.color[1];
  bp[3] = P.piece[ROOK] & P.color[1];
  bp[4] = P.piece[QUEEN] & P.color[1];
  bp[5] = P.piece[KING] & P.color[1];
}


// define the piece mapping array using the enum values
static const int nn_piece_map[] = {
    [EMPTY] = -1,
    [PAWN] = 0,
    [KNIGHT] = 1,
    [KING] = 5,
    [ENP] = -1,
    [BISHOP] = 2,
    [ROOK] = 3,
    [QUEEN] = 4,
};


// Add helper function to safely map pieces
int get_nn_piece(int piece) {
  if (piece < 0 || piece >= 8)
    return -1;
  return nn_piece_map[piece];
}

void benchmark(int depth);
const char pieceChar[] = "*PNK.BRQ";
#define MAT P.sf[2]
#define BOARD P.color[2]
#define RQU (P.piece[QUEEN] | P.piece[ROOK])
#define BQU (P.piece[QUEEN] | P.piece[BISHOP])

int changeMat(int, int, int);
void doMove(Move, int c);
int parseMove(char *, int, Move);
int protV2(char *, int);

int _getpiece(char s, int *c) {
  int i;
  for (i = 1; i < 8; i++)
    if (pieceChar[i] == s) {
      *c = 0;
      return i;
    } else if (pieceChar[i] == s - 32) {
      *c = 1;
      return i;
    }
  return 0;
}

void _parse_fen(char *fen, int reset) {
  char s, mv, pos[128], cas[5], enps[3];
  int c, i, halfm = 0, fullm = 1, col = 0, row = 7;
  memset(&P, 0, sizeof(P));
  book = i = c = 0;
  cas[0] = enps[0] = pv[0][0] = 0;
  sscanf(fen, "%s %c %s %s %d %d", pos, &mv, cas, enps, &halfm, &fullm);
  if (fullm < 1)
    fullm = 1;
  while ((s = pos[i++])) {
    if (s == '/') {
      row--;
      col = 0;
    } else if (s >= '1' && s <= '8') {
      col += s - '0';
    } else {
      int p = _getpiece(s, &c), t = row * 8 + (col++);
      if (p == KING)
        P.king[c] = t;
      else
        MAT += changeMat(_CAP(p) | _TO(t), c ^ 1, -1);
      P.hash ^= hashxor[col | row << 3 | p << 6 | c << 9];
      P.piece[p] |= BIT[t];
      P.color[c] |= BIT[t];
    }
  }
  onmove = mv == 'b' ? 1 : 0;
  flags = i = 0, count = (fullm - 1) * 2 + onmove + (halfm << 10);
  while ((s = cas[i++])) {
    int b = s == 'K' ? 6 : s == 'k' ? 7 : s == 'Q' ? 8 : s == 'q' ? 9 : 0;
    if (b)
      flags |= BIT[b];
  }
  if (enps[0] >= 'a' && enps[0] <= 'h' && enps[1] >= '1' && enps[1] <= '8')
    flags |= 8 * (enps[1] - '1') + enps[0] - 'a';
  for (i = 0; i < (int)COUNT; i++)
    hstack[i] = 0LL;
  if (reset)
    memset(hashDB, 0, sizeof(hashDB));
  BOARD = P.color[0] | P.color[1];
}

void verify_position_sync() {
  if (!nnue_available)
    return;

  // Create fresh accumulator
  NN_Accumulator temp_acc;
  uint64_t white_pieces[6], black_pieces[6];

  // Set up properly ordered arrays
  white_pieces[0] = P.piece[PAWN] & P.color[0];
  white_pieces[1] = P.piece[KNIGHT] & P.color[0];
  white_pieces[2] = P.piece[BISHOP] & P.color[0];
  white_pieces[3] = P.piece[ROOK] & P.color[0];
  white_pieces[4] = P.piece[QUEEN] & P.color[0];
  white_pieces[5] = P.piece[KING] & P.color[0];

  black_pieces[0] = P.piece[PAWN] & P.color[1];
  black_pieces[1] = P.piece[KNIGHT] & P.color[1];
  black_pieces[2] = P.piece[BISHOP] & P.color[1];
  black_pieces[3] = P.piece[ROOK] & P.color[1];
  black_pieces[4] = P.piece[QUEEN] & P.color[1];
  black_pieces[5] = P.piece[KING] & P.color[1];

  nn_update_all_pieces(temp_acc, white_pieces, black_pieces);

  float current_eval = nn_evaluate(nn_accumulator, 0);
  float fresh_eval = nn_evaluate(temp_acc, 0);

  if (fabs(current_eval - fresh_eval) > 0.01f) {
    printf("info string NNUE sync error: %.6f vs %.6f, resetting\n",
           current_eval, fresh_eval);
    memcpy(&nn_accumulator, &temp_acc, sizeof(nn_accumulator));
  }
}



void _newGame() {
  char buf[256], s0[64], s1[64], s2[64];
  int k, n = 0;
  FILE *in = fopen("olibook.pgn", "r");
  bkcount[0] = bkcount[1] = 0;
  for (k = 0; k < BKSIZE; k++)
    bkflag[k] = 2;
  if (in != NULL) {
    while (!feof(in)) {
      fgets(buf, 255, in);
      if (buf[0] == '[') {
        sscanf(buf, "%s %s", s1, s2);
        if (!strncmp(s2, "\"OliThink", 8))
          bkflag[n] = strncmp(s1, "[Black", 6) ? 0 : 1;
        else if (!strncmp(s1, "[Result", 7)) {
          if (bkflag[n]) {
            if (!strncmp(s2, "\"0-1", 4))
              bkflag[n] = 2;
          } else if (strncmp(s2, "\"1-0", 4))
            bkflag[n] = 2;
        }
      } else if (buf[0] == '1' && buf[1] == '.' && bkflag[n] < 2) {
        u32 i = 0, j = 0;
        _parse_fen(sfen, 0);
        for (;;) {
          if (strchr(buf + i, ' ') == NULL)
            break;
          sscanf(buf + i, "%s %s", s0, s2);
          if (s0[0] < '1' || s0[0] > '9')
            break;
          i += (int)(strlen(s0) + strlen(s2) + 2);
          if (s0[strlen(s0) - 1] == '.') {
            strcpy(s1, s2);
            s2[0] = 0;
            sscanf(buf + i, "%s", s2);
            i += (int)(strlen(s2) + 1);
          } else
            sscanf(s0, "%*[^.].%[^.]", s1);
          doMove(bkmove[n * 32 + (j++)] = parseMove(s1, 0, 0), 0);
          if (s2[0] == 0 || s2[0] == '*')
            break;
          doMove(bkmove[n * 32 + (j++)] = parseMove(s2, 1, 0), 1);
          if (j > 30 || i >= strlen(buf))
            break;
        }
        bkmove[n * 32 + j] = 0;
        if (j)
          bkcount[bkflag[n]]++;
        if (++n == BKSIZE)
          break;
      }
    }
    fclose(in);
  }

  _parse_fen(sfen, 1);

  
  // WITH this corrected logic:
  nnue_available = 0;  // Start with NNUE disabled
  
  // Determine which file to load
  char* file_to_load = (nnue_file_path[0] != '\0') ? nnue_file_path : NN_FILE;
  
  // Try to load the NNUE file
  int load_result = nn_load(file_to_load);
  
  if (load_result < 0) {
    // First attempt failed, try to convert from text file
    printf("info string NNUE file not found, attempting to convert from text file\n");
    
    if (nn_convert() == 0) {
      // Conversion successful, try loading again
      load_result = nn_load(file_to_load);
      if (load_result >= 0) {
        printf("info string NNUE file converted and loaded successfully: %s\n", file_to_load);
        nnue_available = 1;
      } else {
        printf("info string Failed to load converted NNUE file - using traditional evaluation\n");
        nnue_available = 0;
      }
    } else {
      printf("info string Failed to convert NNUE file - using traditional evaluation\n");
      nnue_available = 0;
    }
  } else {
    // Loading successful
    printf("info string NNUE file loaded successfully: %s\n", file_to_load);
    nnue_available = 1;
  }

  // Initialize NNUE accumulator if available
  if (nnue_available) {
    nn_init_accumulator(nn_accumulator);
    setup_nnue_for_position();
	#ifdef VALIDATE_NN
	validate_nnue_state();
	#endif
  }
  
  // Clear any pending reload since we just loaded
  nnue_reload_pending = 0;
  
  
  
}



void periodic_nnue_check() {
    static int check_counter = 0;
    
    // Check every 10,000 nodes to minimize performance impact
    if (++check_counter % 10000 == 0 && nnue_available && use_nnue) {
        verify_position_sync();
		#ifdef VALIDATE_NN
		validate_nnue_state();
		#endif
    }
}




void setup_nnue_for_position() {
  if (!nnue_available)
    return;

  // Create properly ordered arrays for the NNUE function
  uint64_t white_pieces[6];
  uint64_t black_pieces[6];

  build_nn_arrays(white_pieces, black_pieces);

  nn_update_all_pieces(nn_accumulator, white_pieces, black_pieces);
}

void _init_pawns(u64 *moves, u64 *caps, u64 *freep, u64 *filep, u64 *helpp,
                 u64 *prgp, int c) {
  int i, j;
  for (i = 0; i < 64; i++) {
    int rank = i / 8, file = i & 7;
    int m = i + (c ? -8 : 8);
    prgp[i] = 1 << (c ? 7 - rank : rank);
    for (j = 0; j < 64; j++) {
      int jrank = j / 8, dfile = ((j & 7) - file) * ((j & 7) - file);
      if (dfile > 1)
        continue;
      if ((c && jrank < rank) ||
          (!c && jrank > rank)) { // The not touched part of the board
        if (dfile == 0)
          filep[i] |= BIT[j];
        freep[i] |= BIT[j];
      } else if (dfile != 0 && (jrank - rank) * (jrank - rank) <= 1) {
        helpp[i] |= BIT[j];
      }
    }
    if (m < 0 || m > 63)
      continue;
    moves[i] |= BIT[m];
    if (file > 0) {
      m = i + (c ? -9 : 7);
      if (m < 0 || m > 63)
        continue;
      caps[i] |= BIT[m];
      caps[i + 64 * (2 - c)] |= BIT[m];
    }
    if (file < 7) {
      m = i + (c ? -7 : 9);
      if (m < 0 || m > 63)
        continue;
      caps[i] |= BIT[m];
      caps[i + 64 * (c + 1)] |= BIT[m];
    }
  }
}

void _init_shorts(u64 *moves, int *m) {
  u32 i, j, n;
  for (i = 0; i < 64; i++) {
    for (j = 0; j < 8; j++) {
      n = i + m[j];
      if (n < 64 && n >= 0 && ((n & 7) - (i & 7)) * ((n & 7) - (i & 7)) <= 4) {
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
    if (!(BIT[i] & del))
      perm &= (~low);
  }
  return perm;
}

void _init_rays(u64 *rays, u64 *xrays, u64 (*rayFunc)(int, u64, int),
                int (*key)(u64, int), int file) {
  int i, f, bc;
  for (f = 0; f < 64; f += file) {
    u64 mmask = (*rayFunc)(f, 0LL, 0) | BIT[f];
    int iperm = 1 << (bc = bitcnt(mmask));
    for (i = 0; i < iperm; i++) {
      u64 board = _occ_free_board(bc, i, mmask);
      u64 occ = (*rayFunc)(f, board, 0);
      u64 xray = (*rayFunc)(f, board, 1);
      int index = (*key)(board, f), ix = (f / file) & 7;
      rays[(ix << 6) + index] |= occ;
      xrays[(ix << 6) + index] |= xray;
    }
  }
}

#define RAYMACRO                                                               \
  {                                                                            \
    if (BIT[i] & board) {                                                      \
      if (b) {                                                                 \
        xray |= BIT[i];                                                        \
        break;                                                                 \
      } else {                                                                 \
        occ |= BIT[i];                                                         \
        b = 1;                                                                 \
      }                                                                        \
    }                                                                          \
    if (!b)                                                                    \
      occ |= BIT[i];                                                           \
  }
u64 _rook0(int f, u64 board, int t) {
  u64 occ = 0LL, xray = 0LL;
  int i, b;
  for (b = 0, i = f + 1; i < 64 && i % 8 != 0; i++)
    RAYMACRO
  for (b = 0, i = f - 1; i >= 0 && i % 8 != 7; i--)
    RAYMACRO
  return t ? xray : occ;
}

u64 _rook90(int f, u64 board, int t) {
  u64 occ = 0LL, xray = 0LL;
  int i, b;
  for (b = 0, i = f - 8; i >= 0; i -= 8)
    RAYMACRO
  for (b = 0, i = f + 8; i < 64; i += 8)
    RAYMACRO
  return t ? xray : occ;
}

u64 _bishop45(int f, u64 board, int t) {
  u64 occ = 0LL, xray = 0LL;
  int i, b;
  for (b = 0, i = f + 9; i < 64 && (i % 8 != 0); i += 9)
    RAYMACRO
  for (b = 0, i = f - 9; i >= 0 && (i % 8 != 7); i -= 9)
    RAYMACRO
  return t ? xray : occ;
}

u64 _bishop135(int f, u64 board, int t) {
  u64 occ = 0LL, xray = 0LL;
  int i, b;
  for (b = 0, i = f - 7; i >= 0 && (i % 8 != 0); i -= 7)
    RAYMACRO
  for (b = 0, i = f + 7; i < 64 && (i % 8 != 7); i += 7)
    RAYMACRO
  return t ? xray : occ;
}

#ifdef _WIN32
u64 getTime() { return GetTickCount(); }

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
    if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL))
      return 1;
    return dw;
  } else
    return _kbhit();
}
#else
u64 getTime() {
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000));
}

int bioskey() {
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_SET(fileno(stdin), &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  select(fileno(stdin) + 1, &readfds, 0, 0, &tv);

  return (FD_ISSET(fileno(stdin), &readfds));
}
#endif

int pullLsb(u64 *bit) {
  int f = getLsb(*bit);
  *bit &= *bit - 1;
  return f;
}

int identPiece(int f) {
  int i;
  for (i = PAWN; i <= QUEEN; i++)
    if (i != ENP && (BIT[f] & P.piece[i]))
      return i;
  return ENP;
}

int key000(u64 b, int f) { return (int)((b >> ((f & 56) + 1)) & 0x3F); }

int key090(u64 b, int f) {
  u64 _b = (b >> (f & 7)) & 0x0101010101010101LL;
  return (int)((_b * 0x0080402010080400LL) >> 58);
}

int key045(u64 b, int f) {
  return (int)(((b & bmask45[f]) * 0x0202020202020202LL) >> 58);
}

int key135(u64 b, int f) {
  return (int)(((b & bmask135[f]) * 0x0202020202020202LL) >> 58);
}

#define DUALATT(x, y, c)                                                       \
  (attacked(x, c) | attacked(y, c) |                                           \
   ((KCAP(x, c) | KCAP(y, c)) & P.piece[KING]))
u64 reach(int f, int c, u64 occ) {
  return ((nmoves[f] & P.piece[KNIGHT]) | (RATT(f, occ) & RQU) |
          (BATT(f, occ) & BQU)) &
         P.color[c ^ 1];
}

u64 attacked(int f, int c) {
  return (PCAP(f, c) & P.piece[PAWN]) | reach(f, c, BOARD);
}

u64 pinnedPieces(int k, int oc) {
  u64 pin = 0LL, occ = BOARD,
      b = ((RXRAY1(k, occ) | RXRAY2(k, occ)) & P.color[oc]) & RQU;
  while (b) {
    int t = pullLsb(&b);
    pin |= RATT(k, occ) & RATT(t, occ) & P.color[oc ^ 1];
  }
  b = ((BXRAY3(k, occ) | BXRAY4(k, occ)) & P.color[oc]) & BQU;
  while (b) {
    int t = pullLsb(&b);
    pin |= BATT(k, occ) & BATT(t, occ) & P.color[oc ^ 1];
  }
  return pin;
}

/* precondition: f and t are on common rank (8), file (16), diagonal (32) or
 * antidiagonal (64) */
int getDir(int f, int t) {
  if (!((f ^ t) & 56))
    return 8;
  if (!((f ^ t) & 7))
    return 16;
  return (!((f - t) % 9)) ? 32 : 64;
}

int changeMat(int m, int c, int d) {
  int dm = pval[CAP(m)];
  if (PROM(m))
    dm += -pval[PAWN] + pval[PROM(m)];
  P.sf[c] += d * fval[PROM(m)];
  P.sf[c ^ 1] -= d * fval[CAP(m)];
  return c ? -d * dm : d * dm;
}

/* move is for both doMove and undoMove, only for undo the globalflags have to
 * be restored (count, castle, enpass..) */
void move(Move m, int c, int d) {
  int f = FROM(m), t = TO(m), p = PIECE(m), a = CAP(m);

  P.color[c] ^= BIT[f] | BIT[t];
  P.piece[p] ^= BIT[f] | BIT[t];
  P.hash ^= hashxor[f | p << 6 | c << 9];
  P.hash ^= hashxor[t | p << 6 | c << 9];

  if (a) {
    if (a == ENP) { // Enpassant Capture
      t = (t & 7) | (f & 56);
      a = PAWN;
    } else if (a == ROOK) { // Revoke castling rights.
      flags &= crevoke[t];
    }
    P.piece[a] ^= BIT[t];
    P.color[c ^ 1] ^= BIT[t];
    P.hash ^= hashxor[t | a << 6 | (c ^ 1) << 9];
    count &= 0x3FF; // Reset Fifty Counter
    MAT += changeMat(m, c, d);
  }
  if (p == PAWN) {
    if (((f ^ t) & 8) == 0)
      flags |= f ^ 24; // Enpassant
    else if ((t & 56) == 0 || (t & 56) == 56) {
      P.piece[PAWN] ^= BIT[t];
      P.piece[PROM(m)] ^= BIT[t];
      P.hash ^= hashxor[t | PAWN << 6 | c << 9];
      P.hash ^= hashxor[t | PROM(m) << 6 | c << 9];
      if (!a)
        MAT += changeMat(m, c, d);
    }
    count &= 0x3FF; // Reset Fifty Counter
  } else if (p == KING) {
    if (P.king[c] == f)
      P.king[c] = t;
    else
      P.king[c] = f;
    flags &= ~(320 << c);     // Lose castling rights
    if (((f ^ t) & 3) == 2) { // Castle
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


void update_nnue_for_move(Move m, int c) {
    if (!nnue_available || !use_nnue) return;
    
    int from = FROM(m);
    int to = TO(m);
    int piece = PIECE(m);
    int captured = CAP(m);
    
    // Validate inputs
    if (from < 0 || from > 63 || to < 0 || to > 63) {
        setup_nnue_for_position();
        return;
    }
    
    // Handle special moves with full rebuild
    if (PROM(m) || (piece == KING && abs(from - to) == 2)) {
        setup_nnue_for_position();
        return;
    }
    
    // Handle normal moves incrementally
    int nn_piece = get_nn_piece(piece);
    if (nn_piece >= 0) {
        nn_mov_piece(nn_accumulator, nn_piece, c, from, to);
        
        if (captured != EMPTY && captured != ENP) {
            int nn_cap = get_nn_piece(captured);
            if (nn_cap >= 0) {
                nn_del_piece(nn_accumulator, nn_cap, c ^ 1, to);
            }
        }
    }
}


int validate_nnue_state() {
    if (!nnue_available) return 0;
    
    // Create reference accumulator
    NN_Accumulator reference;
    uint64_t wp[6], bp[6];
    build_nn_arrays(wp, bp);
    nn_update_all_pieces(reference, wp, bp);
    
    // Compare with current
    float current = nn_evaluate(nn_accumulator, 0);
    float expected = nn_evaluate(reference, 0);
    
    if (fabs(current - expected) > 0.001f) {
        printf("info string NNUE desync detected: %.6f vs %.6f\n", 
               current, expected);
        memcpy(&nn_accumulator, &reference, sizeof(nn_accumulator));
        return -1;
    }
    return 0;
}


void doMove(Move m, int c) {
  int from = FROM(m);
  int to   = TO(m);
  int pc   = PIECE(m);
  int cap  = CAP(m);
  int prom = PROM(m);

  // Store move+flags for undo
  mstack[COUNT] = count | (flags << 17) | (((u64)m) << 27);
  flags &= 960;
  count += 0x401;

  if (!m) return;                // null move: no board or NNUE change here

  // --- Execute board move first ---
  move(m, c, 1);

  if (!nnue_available) return;

  // --- NNUE updates ---
  if (prom || ONMV(m) || pc == KING || cap == ENP) {
    // Special cases: promotion, castling/other specials, king move, en passant
    setup_nnue_for_position();
  } else {
    // Quiet or standard capture
    int nn_pc = get_nn_piece(pc);
    if (nn_pc >= 0) {
      nn_mov_piece(nn_accumulator, nn_pc, c, from, to);
    }
    if (cap != EMPTY && cap != ENP) {
      int nn_cap = get_nn_piece(cap);
      if (nn_cap >= 0) {
        nn_del_piece(nn_accumulator, nn_cap, c ^ 1, to);
      }
    }
  }
}




void undoMove(Move m, int c) {
  // Pop stack and restore counters/flags first
  u64 u = mstack[COUNT - 1];
  count = u & 0x1FFFF;
  flags = (u >> 17L) & 0x3FF;

  if (!m) return;                // null move: nothing to undo NNUE-wise

  int from = FROM(m);
  int to   = TO(m);
  int pc   = PIECE(m);
  int cap  = CAP(m);
  int prom = PROM(m);

  // --- First undo on the board ---
  move(m, c, -1);

  if (!nnue_available) return;

  // --- NNUE reverse updates ---
  if (prom || ONMV(m) || pc == KING || cap == ENP) {
    // For specials, just rebuild after the board is restored
    setup_nnue_for_position();
  } else {
    // Reverse quiet/capture:
    // 1) put mover back to original square
    int nn_pc = get_nn_piece(pc);
    if (nn_pc >= 0) {
      nn_mov_piece(nn_accumulator, nn_pc, c, to, from);
    }
    // 2) if there was a normal capture, re-add the captured piece at 'to'
    if (cap != EMPTY && cap != ENP) {
      int nn_cap = get_nn_piece(cap);
      if (nn_cap >= 0) {
        nn_add_piece(nn_accumulator, nn_cap, c ^ 1, to);
      }
    }
  }
}





void regMoves(Move m, u64 bt, Movep *mp, int cap) {
  while (bt) {
    int t = pullLsb(&bt);
    mp->list[mp->n++] = m | _TO(t) | (cap ? _CAP(identPiece(t)) : 0LL);
  }
}

void regPromotions(int f, int c, u64 bt, Movep *mp, int cap, int queen) {
  while (bt) {
    int t = pullLsb(&bt);
    Move m =
        f | _ONMV(c) | _PIECE(PAWN) | _TO(t) | (cap ? _CAP(identPiece(t)) : 0);
    if (queen)
      mp->list[mp->n++] = m | _PROM(QUEEN);
    mp->list[mp->n++] = m | _PROM(KNIGHT);
    mp->list[mp->n++] = m | _PROM(ROOK);
    mp->list[mp->n++] = m | _PROM(BISHOP);
  }
}

void regKings(Move m, u64 bt, Movep *mp, int c, int cap) {
  while (bt) {
    int t = pullLsb(&bt);
    if (attacked(t, c) | (KCAP(t, c) & P.piece[KING]))
      continue;
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
  if (bf > 1)
    return bf; // Doublecheck
  bf = getLsb(ch);

  cc = attacked(bf, c ^ 1) & apin; // Can we capture the checker?
  while (cc) {
    int cf = pullLsb(&cc), p = identPiece(cf);
    if (p == PAWN && RANK[c][cf] == 7)
      regPromotions(cf, c, ch, mp, 1, 1);
    else
      regMoves(PREMOVE(cf, p), ch, mp, 1);
  }
  if (ENPASS && (ch & P.piece[PAWN])) { // Enpassant capture of attacking Pawn
    cc = PCAP(ENPASS, c ^ 1) & P.piece[PAWN] & apin;
    while (cc) {
      int cf = pullLsb(&cc);
      regMoves(PREMOVE(cf, PAWN), BIT[ENPASS], mp, 1);
    }
  }
  if (ch & (nmoves[k] | kmoves[k]))
    return 1; // We can't move anything in between!

  d = getDir(bf, k);
  if (d == 8)
    fl = RATT1(bf, occ) & RATT1(k, occ);
  else if (d == 16)
    fl = RATT2(bf, occ) & RATT2(k, occ);
  else if (d == 32)
    fl = BATT3(bf, occ) & BATT3(k, occ);
  else
    fl = BATT4(bf, occ) & BATT4(k, occ);

  while (fl) {
    int f = pullLsb(&fl);
    cc = reach(f, c ^ 1, occ) & apin;
    while (cc) {
      int cf = pullLsb(&cc);
      regMoves(PREMOVE(cf, identPiece(cf)), BIT[f], mp, 0);
    }
    bf = c ? f + 8 : f - 8;
    if (bf < 0 || bf > 63 || !(cc = P.piece[PAWN] & P.color[c] & apin))
      continue;
    if (BIT[bf] & cc) {
      if (RANK[c][bf] == 7)
        regPromotions(bf, c, BIT[f], mp, 0, 1);
      else
        regMoves(PREMOVE(bf, PAWN), BIT[f], mp, 0);
    }
    if (RANK[c][f] == 4 && !(occ & BIT[bf]) && (BIT[c ? f + 16 : f - 16] & cc))
      regMoves(PREMOVE(c ? f + 16 : f - 16, PAWN), BIT[f], mp, 0);
  }
  return 1;
}

#define GSLIDES(p, m)                                                          \
  for (b = P.piece[p] & cb; b;) {                                              \
    int f = pullLsb(&b);                                                       \
    regMoves(PREMOVE(f, p), m & tb, mp, q);                                    \
  }
void generateSlides(int c, int k, u64 pin, Movep *mp, u64 tb, u64 cb, int q) {
  u64 b, occ = BOARD;
  GSLIDES(KNIGHT, nmoves[f]);
  GSLIDES(ROOK, RATT(f, occ));
  GSLIDES(BISHOP, BATT(f, occ));
  GSLIDES(QUEEN, (RATT(f, occ) | BATT(f, occ)));

  if (pin)
    for (b = pin & (RQU | P.piece[BISHOP]); b;) {
      int f = pullLsb(&b), p = identPiece(f), t = p | getDir(f, k);
      if ((t & 10) == 10)
        regMoves(PREMOVE(f, p), RATT1(f, occ) & tb, mp, q);
      if ((t & 18) == 18)
        regMoves(PREMOVE(f, p), RATT2(f, occ) & tb, mp, q);
      if ((t & 33) == 33)
        regMoves(PREMOVE(f, p), BATT3(f, occ) & tb, mp, q);
      if ((t & 65) == 65)
        regMoves(PREMOVE(f, p), BATT4(f, occ) & tb, mp, q);
    }
}

void generateQuiet(int c, int k, u64 pin, Movep *mp) {
  int f, r;
  u64 b, cb = P.color[c] & (~pin), tb = ~BOARD, occ = BOARD;

  for (b = P.piece[PAWN] & P.color[c]; b;) {
    f = pullLsb(&b);
    u32 t = BIT[f] & pin ? getDir(f, k) : 17;
    if (t == 8)
      continue;
    else
      r = RANK[c][f];
    u64 m = t & 16 ? PMOVE(f, c, occ) : 0;
    if (m && r == 2)
      m |= PMOVE(c ? f - 8 : f + 8, c, occ);
    if (r == 7) {
      u64 a = (t == 17)   ? PCAP(f, c)
              : (t == 32) ? PCA3(f, c)
              : (t == 64) ? PCA4(f, c)
                          : 0LL;
      if (a)
        regPromotions(f, c, a, mp, 1, 0);
      if (m)
        regPromotions(f, c, m, mp, 0, 0);
    } else if (r != 6) {
      regMoves(PREMOVE(f, PAWN), m, mp, 0);
    }
  }

  if (CASTLE(c)) {
    for (b = P.piece[ROOK] & cb; b;) {
      f = pullLsb(&b);
      if (f == 63 && (flags & 128) && !(occ & (3LL << 61)))
        if (!DUALATT(61, 62, c))
          regMoves(PREMOVE(60, KING), 1LL << 62, mp, 0);
      if (f == 56 && (flags & 512) && !(occ & (7LL << 57)))
        if (!DUALATT(59, 58, c))
          regMoves(PREMOVE(60, KING), 1LL << 58, mp, 0);
      if (f == 7 && (flags & 64) && !(occ & (3LL << 5)))
        if (!DUALATT(5, 6, c))
          regMoves(PREMOVE(4, KING), 1LL << 6, mp, 0);
      if (f == 0 && (flags & 256) && !(occ & (7LL << 1)))
        if (!DUALATT(3, 2, c))
          regMoves(PREMOVE(4, KING), 1LL << 2, mp, 0);
    }
  }

  generateSlides(c, k, pin, mp, tb, cb, 0);
  regKings(PREMOVE(k, KING), kmoves[k] & tb, mp, c, 0);
}

void generateNoisy(int c, int k, u64 pin, Movep *mp) {
  int f, r;
  u64 b, cb = P.color[c] & (~pin), tb = P.color[c ^ 1], occ = BOARD;

  for (b = P.piece[PAWN] & P.color[c]; b;) {
    f = pullLsb(&b);
    u32 t = BIT[f] & pin ? getDir(f, k) : 17;
    if (t == 8)
      continue;
    else
      r = RANK[c][f];
    u64 m = t & 16 ? PMOVE(f, c, occ) : 0;
    u64 a = (t == 17)   ? PCAP(f, c)
            : (t == 32) ? PCA3(f, c)
            : (t == 64) ? PCA4(f, c)
                        : 0LL;
    if (r >= 6) {
      if (a)
        regMoves(PREMOVE(f, PAWN) | (r == 7 ? _PROM(QUEEN) : 0), a, mp, 1);
      if (m)
        regMoves(PREMOVE(f, PAWN) | (r == 7 ? _PROM(QUEEN) : 0), m, mp, 0);
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
  u64 pin = pinnedPieces(k, c ^ 1);
  mp->n = 0;
  if (ch)
    return generateCheckEsc(ch, ~pin, c, k, mp);
  if (noisy)
    generateNoisy(c, k, pin, mp);
  if (quiet)
    generateQuiet(c, k, pin, mp);
  return 0;
}

int swap(Move m) { // SEE
  int s_list[32], f = FROM(m), t = TO(m), c = ONMV(m), piece = PIECE(m), nc = 1;
  u64 temp, cAttacks,
      occ = BOARD,
      attacks = ((PCAP(t, 0) | PCAP(t, 1)) & P.piece[PAWN]) |
                (nmoves[t] & P.piece[KNIGHT]) | (kmoves[t] & P.piece[KING]);

  s_list[0] = pval[CAP(m)];
  occ &= ~BIT[f];

  do {
    s_list[nc] = -s_list[nc - 1] + pval[piece];
    c ^= 1;
    attacks |= (BATT(t, occ) & BQU) | (RATT(t, occ) & RQU);
    attacks &= occ;
    if (!(cAttacks = attacks & P.color[c]))
      break;

    if ((temp = P.piece[PAWN] & cAttacks))
      piece = PAWN;
    else if ((temp = P.piece[KNIGHT] & cAttacks))
      piece = KNIGHT;
    else if ((temp = P.piece[BISHOP] & cAttacks))
      piece = BISHOP;
    else if ((temp = P.piece[ROOK] & cAttacks))
      piece = ROOK;
    else if ((temp = P.piece[QUEEN] & cAttacks))
      piece = QUEEN;
    else {
      nc += !(P.color[c ^ 1] & attacks);
      break;
    } // KING

    occ ^= temp & -temp;
  } while (pval[piece] >= s_list[nc++]);

  while (--nc)
    if (s_list[nc] > -s_list[nc - 1])
      s_list[nc - 1] = -s_list[nc];

  return s_list[0];
}

/* In normal search basic move ordering heuristics are used, in quiesce (ply <
 * 0) value of captured piece */
Move pick(Movep *mp, int s, int ply) {
  Move m;
  int i, t, pi = 0, vmax = -9999;
  for (i = s; i < mp->n; i++) {
    m = mp->list[i];
    if (ply < 0) {
      t = pval[CAP(m)] - fval[PIECE(m)];
    } else if (m == killer[ply]) {
      pi = i;
      break;
    } else
      t = history[m & 0x1FFF];
    if (t > vmax)
      vmax = t, pi = i;
  }
  m = mp->list[pi];
  if (pi != s)
    mp->list[pi] = mp->list[s];
  return m;
}

u64 pawnAttack(int c) {
  u64 p = P.color[c] & P.piece[PAWN];
  return c ? (p & ~fileb[7]) >> 7 | (p & ~fileb[0]) >> 9
           : (p & ~fileb[0]) << 7 | (p & ~fileb[7]) << 9;
}

u64 mobilityb(int c, u64 occ) {
  u64 b = c ? rankb[6] | (occ << 8) : rankb[1] | (occ >> 8);
  b &= P.color[c] & P.piece[PAWN];
  return ~(b | pawnAttack(c ^ 1));
}

int kmobilf(int c) {
  int km = kmobil[P.king[c]], sfo = P.sf[c ^ 1];
  if (!P.sf[c] && sfo == 5 && P.piece[BISHOP] && !P.piece[PAWN]) { // BNK_vs_k
    int bc = bishcorn[P.king[c]] << 5;
    if (P.piece[BISHOP] & whitesq)
      km += bc;
    else
      km -= bc;
  }
  return sfo < 14 ? km : km * (16 - sfo) / 4;
}

#define MOBILITY(a, mb) (bitcnt(a) + bitcnt(a & mb) + bitcnt(a & mb & c3))
/* The eval for Color c. It's almost only mobility. */
int evalc(int c) {
  int f, mn = 0, katt = 0, oc = c ^ 1,
         egf = 10400 / (80 + P.sf[c] + P.sf[oc]) + randm;
  u64 b, a, cb = P.color[c], ocb = P.color[oc], occ = BOARD,
            mb = mobilityb(c, occ) & centr;
  u64 kn = kmoves[P.king[oc]] & (~P.piece[PAWN]),
      c3 = centr2 | rankb[c ? 1 : 6];

  b = P.piece[PAWN] & cb;
  while (b) {
    f = pullLsb(&b);

    /* The only non-mobility eval is the detection of free pawns/hanging pawns
     */
    int ppos = pawnprg[c][f] * egf * egf / 96 / 96;
    if (!(pawnfree[c][f] & P.piece[PAWN] & ocb))
      ppos <<= 1; // Free run?

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

  occ ^= BIT[P.king[oc]]; // Opposite King doesn't block mobility at all
  occ ^= P.piece[QUEEN];  // Queens don't block mobility for anybody.
  b = P.piece[QUEEN] & cb;
  while (b) {
    f = pullLsb(&b);
    a = BATT(f, occ) | RATT(f, occ);
    katt += bitcnt(a & kn);
    mn += MOBILITY(a, mb) * egf * egf / 80 / 80;
  }

  occ ^= P.piece[ROOK]; // Rooks don't block mobility for bishop
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

  return mn + kmobilf(c) +
         katt * (P.sf[c] + 2); // Adapt bonus for attacking king squares
}

// Traditional evaluation function (extracted from your existing evalc)
int eval_traditional(int c) {
  return evalc(c) - evalc(c ^ 1) + (c ? -MAT : MAT);
}

int eval(int c) {
  if (nnue_available && use_nnue) {
    // NNUE returns a float (side-to-move perspective), usually ~[-1.0, 1.0]
    // Multiply by 1000 to convert to centipawns (consistent with OliThink)
    float nn_val = nn_evaluate(nn_accumulator, c);
    return (int)(nn_val * 1000.0f);
  } else {
    eval1++;
    return eval_traditional(c);
  }
}

// Optional: Function to manually disable NNUE and fall back to traditional eval
void disable_nnue() {
  nnue_available = 0;
  printf("info string NNUE disabled, using traditional evaluation\n");
}

// Optional: Function to try re-enabling NNUE
int try_enable_nnue() {
  if (nn_load(NN_FILE) >= 0) {
    uint64_t wp[6], bp[6];
    build_nn_arrays(wp, bp);
    nn_update_all_pieces(nn_accumulator, wp, bp);

    nnue_available = 1;
    printf("info string NNUE re-enabled\n");
    return 1;
  }
  return 0;
}

int quiesce(u64 ch, int c, int ply, int alpha, int beta) {
  int i, best = -MAXSCORE;

  // Check for time-out and set abort flag unconditionally
  if (!pondering && getTime() - starttime > maxtime)
    sabort = 1;

  // Perform other periodic checks
  if ((++qnodes & CNODES) == 0) {
    if (bioskey() && !sabort)
      sabort = 1;
    if (node_limit > 0 && (nodes + qnodes) >= node_limit)
      sabort = 1;
  }
  if (sabort)
    return -MAXSCORE;

  if (ply == 127)
    return eval(c);
  if (!ch)
    do {
      int cmat = (c ? -MAT : MAT);
      if (cmat - 125 >= beta)
        return beta;
      if (cmat + 85 <= alpha)
        break;
      best = eval(c);
      if (best >= beta)
        return beta;
      if (best > alpha)
        alpha = best;
    } while (0);

  Movep mp;
  generate(ch, c, &mp, 1, 0);
  if (mp.n == 0)
    return ch ? -MAXSCORE + ply : best > -MAXSCORE ? best : eval(c);

  Pos pos;
  pos.hash = 0;
  for (i = 0; i < mp.n; i++) {
    Move m = pick(&mp, i, -1);
    if (!ch && pval[PIECE(m)] > pval[CAP(m)] && swap(m) < 0)
      continue;

    if (!pos.hash)
      memcpy(&pos, &P, sizeof(Pos));
    doMove(m, c);
    qnodes++;

    int w =
        -quiesce(attacked(P.king[c ^ 1], c ^ 1), c ^ 1, ply + 1, -beta, -alpha);

    if (sabort) { // Fix: Check for abort after recursive call in quiesce
      undoMove(0, c);
      memcpy(&P, &pos, sizeof(Pos));
      return -MAXSCORE;
    }

    undoMove(0, c);
    memcpy(&P, &pos, sizeof(Pos));

    if (w > best) {
      if ((best = w) > alpha) {
        if ((alpha = w) >= beta)
          return beta;
      }
    }
    if (ch)
      break;
  }
  return best > -MAXSCORE ? best : eval(c);
}

int retPVMove(int c) {
  int i;
  Movep mp;
  GENERATE(c, &mp);
  for (i = 0; i < mp.n; i++) {
    Move m = mp.list[i];
    if (m == pv[0][0])
      return m;
  }
  return 0;
}

int isDraw(u64 hp, int nrep) {
  if (count > 0xFFF) { // fifty > 3
    int i, c = 0, n = COUNT - (count >> 10);
    if (count >= 0x400 * 100)
      return 2; // 100 plies
    for (i = COUNT - 2; i >= n; i -= 2)
      if (hstack[i] == hp && ++c == nrep)
        return 1;
  }
  return (!P.piece[PAWN] && !RQU && bitcnt(BOARD) <= 3)
             ? 3
             : 0; // Check for mating material
}

const int nullvar[] = {13, 43, 149, 519, 1809, 6311, 22027};
static int nullvariance(int delta) {
  int r = 0;
  if (delta >= 4)
    for (r = 1; r <= 7; r++)
      if (delta < nullvar[r - 1])
        break;
  return r;
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define HASHP (P.hash ^ hashxor[flags | 1024 | c << 11])
int search(u64 ch, int c, int d, int ply, int alpha, int beta, int null,
           Move sem) {
  int i, j, n, w = alpha, oc = c ^ 1, pvnode = beta > alpha + 1;

  if (ply)
    pv[ply][ply] = 0;

  // Check for time-out and set abort flag unconditionally
  if (!pondering && getTime() - starttime > maxtime)
    sabort = 1;

  // Perform other periodic checks
  if ((++nodes & CNODES) == 0) {
	  periodic_nnue_check();
    if (bioskey() && !sabort)
      sabort = 1;
    if (node_limit > 0 && (nodes + qnodes) >= node_limit)
      sabort = 1;
  }
  // FIX: Return a special value on abort to indicate unreliability
  if (sabort)
    return -MAXSCORE;

  u64 hp = HASHP;
  if (ply && isDraw(hp, 1))
    return 0;

  if (d <= 0 || ply > 100)
    return quiesce(ch, c, ply, alpha, beta);

  if (alpha < -MAXSCORE + ply)
    alpha = -MAXSCORE + ply;
  if (beta > MAXSCORE - ply - 1)
    beta = MAXSCORE - ply - 1;
  if (alpha >= beta)
    return alpha;

  Move hmove = ply ? 0 : retPVMove(c);

  entry *he = &hashDB[hp & HMASK];
  if (he->key == hp && !sem) {
    if (he->depth >= d) {
      if (he->type <= EXACT && he->value >= beta)
        return beta;
      hash_hits++;
      if (he->type >= EXACT && he->value <= alpha)
        return alpha;
    }
    if (!hmove)
      hmove = he->move;
  }

  int wstat = wstack[COUNT] = ch              ? -MAXSCORE
                              : he->key == hp ? he->value
                                              : eval(c);
  if (!ch && he->key != hp)
    *he = (entry){
        .key = hp, .move = hmove, .value = wstat, .depth = d, .type = LOWER};
  if (!ch && !pvnode) {
    if (d <= 3 && wstat + 400 < beta) {
      w = quiesce(ch, c, ply, alpha, beta);
      if (w < beta)
        return w;
    }
    if (d <= 8 && wstat - 88 * d > beta)
      return beta;
  }

  int raising = !ch && ply >= 2 && wstat >= wstack[COUNT - 2] &&
                wstack[COUNT - 2] != -MAXSCORE;
  hstack[COUNT] = hp;
  // Null Move - pvnode => null == 0
  null = null && !ch && beta > -MAXSCORE + 500 && d > 1 && wstat > alpha &&
         (ply < 2 || (mstack[COUNT - 2] >> 27));
  if (null && bitcnt(P.color[c] & (~P.piece[PAWN]) &
                     (~pinnedPieces(P.king[c], oc))) > 1) {
    int R = (9 + d + nullvariance(wstat - alpha)) / 4 + raising;
    doMove(0, c);
    w = -search(0LL, oc, d - R, ply + 1, -beta, 1 - beta, 0, 0);
    undoMove(0, c);
    if (w >= beta)
      return beta;
  }

  if (d >= 4 && !hmove)
    d--; // Internal Iterative Reduction (IIR)

  Movep mp, mpq;
  mpq.n = 0;
  Pos pos;
  pos.hash = 0;
  int first = NO_MOVE, hismax = -9999;
  for (n = HASH; n <= (ch ? NOISY : QUIET); n++) {
    int nd = d - 1;
    if (n == HASH) {
      if (!hmove)
        continue;
      mp.n = 1;
      if (d >= 8 && ply && he->type == LOWER && he->depth >= d - 3) {
        int bc = he->value - d;
        int ss = search(ch, c, d >> 1, ply, bc - 1, bc, 0,
                        hmove); // Singular extensions
        if (ss < bc)
          nd++;
        else if (ss >= beta)
          return (ss + beta) / 2;
      }
    } else if (n == NOISY) {
      generate(ch, c, &mp, 1, 0);
    } else {
      generate(ch, c, &mp, 0, 1);
    }
    for (i = 0; i < mp.n; i++) {
      Move m = n == HASH    ? hmove
               : n == NOISY ? pick(&mp, i, -1)
                            : pick(&mp, i, ply);
      if ((n != HASH && m == hmove) || m == sem)
        continue;
      if (!ch && n == QUIET && mpq.n > 2 * d * (raising + 1)) {
        n = EXIT;
        break; // LMP
      }
      if (n != HASH && alpha > -MAXSCORE + 500 && d < 8 && swap(m) < -d * 60)
        continue;

      int ext = 0, quiet = !CAP(m) && !PROM(m);
      if (!pos.hash)
        memcpy(&pos, &P, sizeof(Pos));
      doMove(m, c);
      if (quiet)
        mpq.list[mpq.n++] = m;
      u64 nch = attacked(P.king[oc], oc);
      if (nch || pvnode || ch ||
          (PIECE(m) == PAWN &&
           !(pawnfree[c][TO(m)] & P.piece[PAWN] & P.color[oc])))
        ;
      else if (n == NOISY && d >= 2 && swap(m) < 0)
        ext -= (d + 1) / (3 + raising); // Reduce bad exchanges
      else if (n == QUIET &&
               m != killer[ply]) { // LMR, but don't reduce killers
        int his = history[m & 0x1FFF];
        if (his > hismax)
          hismax = his;
        else if (d < 5 && (his < 0 || his * his < hismax)) {
          undoMove(0, c);
          memcpy(&P, &pos, sizeof(Pos));
          continue;
        } else
          ext -= (d + 1) / 3;
      }

      int firstPVNode = first == NO_MOVE && pvnode;
      if (!firstPVNode)
        w = -search(nch, oc, nd + ext, ply + 1, -alpha - 1, -alpha, 1, 0);
      if (ext < 0 && w > alpha)
        w = -search(nch, oc, nd, ply + 1, -alpha - 1, -alpha, 1, 0);
      if (firstPVNode || (w > alpha && w < beta))
        w = -search(nch, oc, nd + ext, ply + 1, -beta, -alpha, 0, 0);

      undoMove(0, c);
      memcpy(&P, &pos, sizeof(Pos));
      // FIX: Check for abort after each recursive call
      if (sabort)
        return -MAXSCORE;

      if (w > alpha) {
        alpha = w, first = GOOD_MOVE;
        pv[ply][ply] = m;
        for (j = ply + 1; (pv[ply][j] = pv[ply + 1][j]); j++)
          ;

        if (w >= beta) {
          if (quiet) {
            int his = MIN(d * d, 512);
            killer[ply] = m;
            history[m & 0x1FFF] += his - history[m & 0x1FFF] * his / 512;

            for (j = 0; j < mpq.n - 1; j++) {
              Move m2 = mpq.list[j];
              history[m2 & 0x1FFF] += -his - history[m2 & 0x1FFF] * his / 512;
            }
          }
          n = EXIT;
          break;
        }
      } else if (first == NO_MOVE)
        first = ANY_MOVE;
    }
  }
  if (first == NO_MOVE)
    alpha = ch || sem ? -MAXSCORE + ply : 0;

  char type = UPPER;
  if (first == GOOD_MOVE)
    type = alpha >= beta ? LOWER : EXACT,
    hmove = pv[ply][ply]; // Found good move

  if (!sem)
    *he = (entry){
        .key = hp, .move = hmove, .value = alpha, .depth = d, .type = type};

  return alpha;
}

#define ISRANK(c) (c >= '1' && c <= '8')
#define ISFILE(c) (c >= 'a' && c <= 'h')
int ismove(Move m, int to, int from, int piece, int prom, int h) {
  if (TO(m) != to)
    return 0;
  if (from < 0 && PIECE(m) != piece)
    return 0;
  if (from >= 0 && FROM(m) != from)
    return 0;
  if (ISFILE(h) && (FROM(m) & 7) != h - 'a')
    return 0;
  if (ISRANK(h) && (FROM(m) & 56) != 8 * (h - '1'))
    return 0;
  if (prom && PROM(m) != prom)
    return 0;
  return 1;
}

int parseMove(char *s, int c, Move p) {
  int i, to, from = -1, piece = PAWN, prom = 0;
  char h = 0, c1, c2;
  if (!strncmp(s, "O-O-O", 5))
    strcpy(s, c ? "Kc8" : "Kc1");
  else if (!strncmp(s, "O-O", 3))
    strcpy(s, c ? "Kg8" : "Kg1");
  if (s[0] >= 'A' && s[0] <= 'Z')
    if ((piece = _getpiece(*s++, &i)) < 1)
      return -1;
  if (s[0] == 'x')
    s++;
  if (ISRANK(s[0])) {
    h = *s++;
    if (s[0] == 'x')
      s++;
  }
  if (!ISFILE(s[0]))
    return -1;
  c1 = *s++;
  if (s[0] == 'x')
    s++;
  if (ISFILE(s[0])) {
    h = c1;
    c1 = *s++;
  }
  c2 = *s++;
  if (!ISRANK(c2))
    return -1;
  if (*s >= '0') {
    if (*s == '=')
      prom = _getpiece(s[1], &i);
    else if (*s == '+')
      ;
    else { // Algebraic Notation
      from = c1 - 'a' + 8 * (c2 - '1');
      c1 = *s++;
      c2 = *s++;
      if (!ISFILE(c1) || !ISRANK(c2))
        return -1;
      prom = _getpiece(*s, &i);
    }
  }
  to = c1 - 'a' + 8 * (c2 - '1');
  if (p)
    return ismove(p, to, from, piece, prom, h) ? p : 0;

  Movep mp;
  GENERATE(c, &mp);
  for (i = 0; i < mp.n; i++)
    if (ismove(mp.list[i], to, from, piece, prom, h))
      return mp.list[i];
  return 0;
}

void displaym(Move m) {
  printf("%c%c%c%c", 'a' + FROM(m) % 8, '1' + FROM(m) / 8, 'a' + TO(m) % 8,
         '1' + TO(m) / 8);
  if (PROM(m))
    printf("%c", pieceChar[PROM(m)] + 32);
}

void displaypv() {
  int i;
  for (i = 0; pv[0][i]; i++) {
    displaym(pv[0][i]);
    printf(" ");
  }
}

void reset_uci_parameters() {
  ttime = 30000;
  inc = 0;
  sd = 64;
  st = 0;
  mps = 0;
  movestogo = 0;
  pondering = 0;
  node_limit = 0;
  analyze = 0;
}

void calc(int ttime) {
  int i, j, w, d, m2go;
  nodes = qnodes = 0LL;
  node_limit = 0;
  

  if (!mps)
    m2go = mps == 0 ? 32 : 1 + mps - ((COUNT / 2) % mps);
  if (movestogo)
    m2go = movestogo; // uci movestogo

  u32 searchtime = 0;
  if (st > 0) {
    searchtime = st;
      } else {
		// Allocate time based on remaining time and moves to go
		searchtime = ttime / m2go + inc;
		// Adjust the search time to not consume all remaining time
		searchtime = (u32)(searchtime * 0.9); // Use 90% of the calculated time
  }
   
   
  maxtime = searchtime;

  if (pondering) {
    // If pondering, use half the remaining time
    maxtime = (ttime + inc * m2go) / 2;
  }

  starttime = getTime();
  u64 ch = attacked(P.king[onmove], onmove);
  memset(history, 0, sizeof(history));
  memset(killer, 0, sizeof(killer));

  sabort = w = d = 0;
  eval1 = qnodes = nodes = 0LL;
  if (book) {
    if (!bkcount[onmove])
      book = 0;
    else {
      hash_hits = 0LL;
      j = (int)(hashxor[starttime & 4095] & 0xFFFFFF) % bkcount[onmove];
      for (i = 0; i < BKSIZE; i++) {
        if (bkflag[i] == onmove && j == d++) {
          pv[0][0] = bkmove[i * 32 + COUNT];
          break;
        }
      }
    }
  }
  if (!book)
    for (d = 1; d <= sd; d++) {
      int alpha = d > 6 ? w - 13 : -MAXSCORE, beta = d > 6 ? w + 13 : MAXSCORE,
          delta = 18;

      for (;;) {
        w = search(ch, onmove, d, 0, alpha, beta, 0, 0);
        if (sabort)
          break;

        if (w <= alpha)
          alpha -= delta, beta = (alpha + beta) / 2;
        else if (w >= beta)
          beta += delta;
        else
          break;
        delta += delta * 2 / 3;
      }

      u32 t1 = (u32)(getTime() - starttime);
      if (pv[0][0] && w > -MAXSCORE && (!pon || pondering)) {
        printf("info depth %d score cp %d time %lu nodes %llu nps %llu "
               "hashhits %llu pv ",
               d, MEVAL(w), t1, nodes + qnodes, nodes * 1000 / (t1 + 1),
               hash_hits);
        displaypv();
        printf("\n");
      } //
      if (sabort)
        break;
      if (d >= sd)
        break;
      if (t1 > maxtime)
        break;
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
    generate(attacked(P.king[onmove ^ 1], onmove ^ 1), onmove ^ 1, &mp, 1, 1);
    undoMove(final_move, onmove);
    memcpy(&P, &temp_pos, sizeof(Pos));
    if (mp.n > 0) {
      ponder_move = mp.list[0];
    } //
  } //

  printf("bestmove ");
  displaym(final_move);
  if (ponder_move) {
    printf(" ponder ");
    displaym(ponder_move);
  }
  printf("\n");
}

// 9. Enhanced position setup function
void do_uci_position(char *line) {
  char *token = strtok(line, " ");

  if (strcmp(token, "startpos") == 0) {
    _parse_fen(sfen, 0);
    onmove = 0;
  } else if (strcmp(token, "fen") == 0) {
    char fen_str[256];
    char *ptr = fen_str;

    token = strtok(NULL, " ");
    while (token != NULL && strcmp(token, "moves") != 0) {
      strcpy(ptr, token);
      ptr += strlen(token);
      *ptr = ' ';
      ptr++;
      token = strtok(NULL, " ");
    }
    *--ptr = '\0';

    _parse_fen(fen_str, 1);
  }

  // CRITICAL FIX: Reset NNUE accumulator after position setup
  if (nnue_available && use_nnue) {
    uint64_t wp[6], bp[6];
    build_nn_arrays(wp, bp);
    nn_update_all_pieces(nn_accumulator, wp, bp);
    verify_position_sync(); // Verify the setup is correct
	#ifdef VALIDATE_NN
	validate_nnue_state();
	#endif
  }

  token = strtok(NULL, " ");
  if (token != NULL && strcmp(token, "moves") == 0) {
    token = strtok(NULL, " ");
    while (token != NULL) {
      Move m = parseMove(token, onmove, 0);
      if (m) {
        doMove(m, onmove);
        onmove ^= 1;
      }
      token = strtok(NULL, " ");
    }
  }

  // Final verification after all moves are played
  if (nnue_available && use_nnue) {
    setup_nnue_for_position();

    // Verify the setup is correct
    verify_position_sync();
  }
}

// ===== DEBUGGING HELPERS =====

void debug_print_bitboards() {
  printf(
      "info string White pieces: P=%llx N=%llx B=%llx R=%llx Q=%llx K=%llx\n",
      P.piece[PAWN] & P.color[0], P.piece[KNIGHT] & P.color[0],
      P.piece[BISHOP] & P.color[0], P.piece[ROOK] & P.color[0],
      P.piece[QUEEN] & P.color[0], P.piece[KING] & P.color[0]);
  printf(
      "info string Black pieces: P=%llx N=%llx B=%llx R=%llx Q=%llx K=%llx\n",
      P.piece[PAWN] & P.color[1], P.piece[KNIGHT] & P.color[1],
      P.piece[BISHOP] & P.color[1], P.piece[ROOK] & P.color[1],
      P.piece[QUEEN] & P.color[1], P.piece[KING] & P.color[1]);
}


void uci_loop() {
  char line[8192];

  while (fgets(line, sizeof(line), stdin)) {
    // Remove newline character
    char *newline = strchr(line, '\n');
    if (newline) *newline = '\0';
    
    if (strncmp(line, "uci", 3) == 0) {
      printf("id name OliThink " VER "\n");
      printf("id author Oliver Brausch\n");
      // Add NNUE options properly formatted for UCI
      printf("option name Use NNUE type check default true\n");
      printf("option name NNUE File type string default %s\n", NN_FILE);
      printf("uciok\n");
    } else if (strncmp(line, "isready", 7) == 0) {
      printf("readyok\n");
	} else if (strncmp(line, "ucinewgame", 10) == 0) {
     _newGame();           // This will now properly load NNUE
      reset_uci_parameters();
    } else if (strncmp(line, "position", 8) == 0) {
      do_uci_position(line + 9);
    } else if (strncmp(line, "bench", 5) == 0) {
      int depth = 10; // Default depth if not specified
      char *token = strtok(line + 6, " ");
      if (token != NULL) {
        depth = atoi(token);
      }
      benchmark(depth);
    } else if (strncmp(line, "setoption", 9) == 0) {
      // Parse setoption command properly
      char option_name[256] = "";
      char option_value[256] = "";
      
      // Look for "name" keyword
      char *name_ptr = strstr(line, "name ");
      if (name_ptr) {
        name_ptr += 5; // Skip "name "
        
        // Find "value" keyword
        char *value_ptr = strstr(name_ptr, " value ");
        if (value_ptr) {
          // Extract option name
          int name_len = value_ptr - name_ptr;
          strncpy(option_name, name_ptr, name_len);
          option_name[name_len] = '\0';
          
          // Extract option value
          value_ptr += 7; // Skip " value "
          strcpy(option_value, value_ptr);
        } else {
          // No value specified, just get the name
          strcpy(option_name, name_ptr);
        }
      }
      
      // Process the options
      if (strcmp(option_name, "Use NNUE") == 0) {
        if (strcmp(option_value, "true") == 0) {
          use_nnue = 1;
          if (nnue_available) {
            printf("info string NNUE enabled\n");
          } else {
            printf("info string NNUE not available, using classical eval\n");
          }
        } else if (strcmp(option_value, "false") == 0) {
          use_nnue = 0;
          printf("info string NNUE disabled, using classical eval\n");
        }
      } else if (strcmp(option_name, "NNUE File") == 0) {
        strncpy(nnue_file_path, option_value, sizeof(nnue_file_path) - 1);
        nnue_file_path[sizeof(nnue_file_path) - 1] = '\0';
        nnue_reload_pending = 1;
        printf("info string NNUE file path set to: %s (will reload on next search)\n", nnue_file_path);
      }
    } else if (strncmp(line, "nnue status", 11) == 0) {
      if (!nnue_available) {
        printf("info string NNUE not loaded (using classical evaluation)\n");
      } else if (use_nnue) {
        printf("info string NNUE is loaded and ENABLED (file: %s)\n", 
               nnue_file_path[0] ? nnue_file_path : NN_FILE);
      } else {
        printf("info string NNUE is loaded but DISABLED by user option\n");
      }
    } else if (strncmp(line, "nnue reload", 11) == 0) {
      char newfile[256] = "";
      if (sscanf(line, "nnue reload %255s", newfile) == 1) {
        strncpy(nnue_filename, newfile, sizeof(nnue_filename) - 1);
        nnue_filename[sizeof(nnue_filename) - 1] = '\0';
      }

      char *file_to_load = (newfile[0] != '\0') ? newfile : 
                           (nnue_file_path[0] != '\0') ? nnue_file_path : NN_FILE;

      if (thinking) {
        nnue_reload_pending = 1;
        printf("info string NNUE reload of '%s' scheduled after search finishes\n", file_to_load);
      } else {
        if (nn_load(file_to_load) < 0) {
          printf("info string Failed to reload NNUE file: %s\n", file_to_load);
          nnue_available = 0;
        } else {
          nnue_available = 1;
          nn_init_accumulator(nn_accumulator);
          setup_nnue_for_position();

          if (use_nnue) {
            printf("info string NNUE reloaded successfully and enabled (file: %s)\n", file_to_load);
          } else {
            printf("info string NNUE reloaded successfully but disabled by user option\n");
          }
        }
      }
    } else if (strncmp(line, "go", 2) == 0) {
      reset_uci_parameters();

      // Handle pending NNUE reload before search
      if (nnue_reload_pending && use_nnue) {
        char* file_to_load = nnue_file_path[0] != '\0' ? nnue_file_path : NN_FILE;
        printf("info string Reloading NNUE file: %s\n", file_to_load);
        
        if (nn_load(file_to_load) < 0) {
          printf("info string Failed to load NNUE file: %s - using classical evaluation\n", file_to_load);
          nnue_available = 0;
        } else {
          nnue_available = 1;
          nn_init_accumulator(nn_accumulator);
          setup_nnue_for_position();
		  #ifdef VALIDATE_NN
		  validate_nnue_state(); 
		  #endif
          printf("info string NNUE loaded successfully: %s\n", file_to_load);
        }
        nnue_reload_pending = 0;
      }

      char *token = strtok(line + 3, " ");

      while (token != NULL) {
        if (strcmp(token, "wtime") == 0) {
          token = strtok(NULL, " ");
          if (onmove == 0)
            ttime = atoi(token);
        } else if (strcmp(token, "btime") == 0) {
          token = strtok(NULL, " ");
          if (onmove == 1)
            ttime = atoi(token);
        } else if (strcmp(token, "winc") == 0) {
          token = strtok(NULL, " ");
          if (onmove == 0)
            inc = atoi(token);
        } else if (strcmp(token, "binc") == 0) {
          token = strtok(NULL, " ");
          if (onmove == 1)
            inc = atoi(token);
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
          movestogo = atoi(token);
        }
        token = strtok(NULL, " ");
      }

      thinking = 1;
      calc(ttime);
      thinking = 0;
    } else if (strncmp(line, "stop", 4) == 0) {
      sabort = 1;
    } else if (strncmp(line, "ponderhit", 9) == 0) {
      pondering = 0;
      sabort = 0;
    } else if (strncmp(line, "quit", 4) == 0) {
      break;
    }
  }
}


int main() {
  int i;
  u64 m, n;

  setbuf(stdout, NULL);
  setbuf(stdin, NULL);
  for (i = 4096, n = 1, m = 6364136223846793005LL; i--;
       hashxor[4095 - i] = n = n * m + 1LL)
    ;
  for (i = 0; i < 64; i++)
    BIT[i] = 1LL << i;
  for (i = 0; i < 128; i++)
    pmoves[0][i] = pawnfree[0][i] = pawnfile[0][i] = pawnhelp[0][i] = 0LL;
  for (i = 0; i < 384; i++)
    pcaps[0][i] = 0LL;
  for (i = 0; i < 64; i++)
    bmask45[i] = _bishop45(i, 0LL, 0), bmask135[i] = _bishop135(i, 0LL, 0);
  for (i = 0; i < 64; i++)
    rmask0[i] = _rook0(i, 0LL, 0);
  for (i = 0; i < 64; i++)
    crevoke[i] = 0x3FF, rankb[i / 8] |= BIT[i], fileb[i & 7] |= BIT[i];
  for (i = 0; i < 64; i++)
    kmoves[i] = nmoves[i] = 0LL, RANK[0][i] = RANK[1][63 - i] = 1 + (i >> 3);
  for (i = 0; i < 64; i++)
    if ((i / 8) % 2 != (i & 7) % 2)
      whitesq |= BIT[i];
  crevoke[7] ^= BIT[6];
  crevoke[63] ^= BIT[7];
  crevoke[0] ^= BIT[8];
  crevoke[56] ^= BIT[9];

  _init_rays((u64 *)raysRank, (u64 *)xrayRank, _rook0, key000, 1);
  _init_rays((u64 *)raysAFile, (u64 *)xrayAFile, _rook90, key090, 8);
  _init_shorts(nmoves, _knight);
  _init_shorts(kmoves, _king);
  _init_pawns(pmoves[0], pcaps[0], pawnfree[0], pawnfile[0], pawnhelp[0],
              pawnprg[0], 0);
  _init_pawns(pmoves[1], pcaps[1], pawnfree[1], pawnfile[1], pawnhelp[1],
              pawnprg[1], 1);

  for (i = 0; i < 64; i++)
    n = bitcnt(nmoves[i]), kmobil[i] = n == 2 ? 33 : n * 10;
  for (i = 0; i < 64; i++)
    n = bitcnt(nmoves[i]), centr |= n >= 4 ? BIT[i] : 0L,
    centr2 |= n >= 8 ? BIT[i] : 0L;
  for (i = 0; i < 32; i++)
    bishcorn[i] = bishcorn[63 - i] = (i & 7) < 4
                                         ? cornbase[(i & 7) + i / 8]
                                         : -cornbase[7 - (i & 7) + i / 8];

  // Check if NN file loads successfully
  if (nn_load(NN_FILE) != 0) {
    printf("info string Could not find NN file, converting from text file.\n");
    if (nn_convert() != 0) {
      printf("info string Failed to create NN file from text. Using "
             "traditional eval. \n");
      nnue_available = 0;
    } else {
      printf("info string NN file created successfully. Attempting to load "
             "again.\n");
      if (nn_load(NN_FILE) != 0) {
        printf("info string Failed to load newly created NN file. Exiting.\n");
        return -1;
      }
    }
  }
  
  // Initialize NNUE-related globals
  nnue_available = 0;
  use_nnue = 1;  // Default to enabled
  nnue_reload_pending = 0;
  nnue_file_path[0] = '\0';  // Start with empty path (use default)
  strcpy(nnue_filename, NN_FILE);  // Set default filename
  

  _newGame();

  uci_loop();

  return 0;
}

void benchmark(int depth) {

  unsigned long long total_nodes = 0;
  unsigned long long total_qnodes = 0;
  u64 total_time = 0;

  printf("Starting benchmark with depth %d\n", depth);
  sd = depth;
  st = 0;
  node_limit = 0;
  maxtime = 10000000; // a very large number for unlimited time
  pondering = 0;
  analyze = 0;
  pondering = 0;

  for (u64 i = 0;
       i < sizeof(benchmark_positions) / sizeof(benchmark_positions[0]); i++) {
    char fen[256];
    char *token = strdup(benchmark_positions[i]);
    char *moves_token = strstr(token, " moves ");
    if (moves_token) {
      *moves_token = '\0';
      strcpy(fen, token);
      do_uci_position(fen);
      char *moves_list = moves_token + 7;
      char *move_token = strtok(moves_list, " ");
      while (move_token) {
        Move m = parseMove(move_token, onmove, 0);
        if (m) {
          doMove(m, onmove);
          onmove ^= 1;
        }
        move_token = strtok(NULL, " ");
      }
    } else {
      strcpy(fen, benchmark_positions[i]);
      do_uci_position(fen);
    }

    u64 start_time = getTime();
    calc(ttime);
    u64 end_time = getTime();
    u64 time_taken = end_time - start_time;

    total_nodes += nodes;
    total_qnodes += qnodes;
    total_time += time_taken;

    printf("Position %llu: FEN '%s', Time: %llu ms, Nodes: %llu, NPS: %llu\n",
           i + 1, fen, time_taken, nodes + qnodes,
           (nodes + qnodes) * 1000 / (time_taken + 1));

    free(token);
  }
  printf("----------------------------------\n");
  printf("Benchmark complete.\n");
  printf("Total Time: %llu ms\n", total_time);
  printf("Total Nodes: %llu\n", total_nodes + total_qnodes);
  printf("Overall NPS: %llu\n\n",
         (total_nodes + total_qnodes) * 1000 / (total_time + 1));
}
