#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#define MATE 32000
#define MAX_PLY 64
#define MAX_MOVE 256
#define KEYS_COUNT 896
#define U8 unsigned __int8
#define S16 signed __int16
#define U16 unsigned __int16
#define S32 signed __int32
#define U32 unsigned __int32
#define S64 signed __int64
#define U64 unsigned __int64
#define FALSE 0
#define TRUE 1
#define NAME "Unicorn"
#define VERSION "2026-03-24"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define FLIP_SQ(sq) ((sq)^0x38)

enum Color { EMPTY = 0, WHITE = 8, BLACK = 16, COLOR_MASK = WHITE | BLACK };
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PT_NB };
enum File { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };
enum Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8 };
enum Piece {
	WHITE_PAWN = WHITE, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
	BLACK_PAWN = BLACK, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING
};
enum Castling { CWK = 1, CWQ = 2, CBK = 4, CBQ = 8 };
enum Bound { TT_LOWER, TT_UPPER, TT_EXACT };
enum Squares {
	SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
	SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
	SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
	SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
	SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
	SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
	SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
	SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
	SQ_NB
};

typedef struct {
	int x;
	int y;
} D2;

typedef struct {
	U8 kingSq[2];
	U8 board[64];
	U8 color;
	U8 ep;
	U8 castle;
	U8 move50;
}Position;

typedef union {
	U32 move;
	struct {
		U8 from;
		U8 to;
		U8 promo;
		U8 flag;
	};
}Move;

typedef struct {
	U64 hash;
	Move move;
	S16 score;
	U8 depth;
	U8 flag;
}TTEntry;

typedef struct {
	U8 post;
	U8 stop;
	U8 depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}SearchInfo;

int notOnlyPawns = 0;
int historyCount = 0;
U64 historyHash[1024];
D2 dirOffset[16] = { {1,1},{-1,1},{-1,-1},{1,-1},{1,0},{-1,0},{0,1},{0,-1},{1,2},{2,1},{-1,2},{-2,1},{-1,-2},{-2,-1},{1,-2},{2,-1} };
int dirStart[PT_NB] = { 0,8,0,4,0,0 };
int dirCount[PT_NB] = { 0,8,4,4,8,8 };
int dirSlide[PT_NB] = { 0,0,1,1,1,0 };
int phaseVal[PT_NB] = { 0,1,1,2,4,0 };
int insufVal[PT_NB] = { 5,2,3,5,5,0 };
int mg_material[PT_NB] = { 82, 337, 365, 477, 1025, 0 };
int eg_material[PT_NB] = { 94, 281, 297, 512,  936, 0 };
const U64 tt_count = 64ULL << 15;
U64 keys[KEYS_COUNT];
int hh[2][64][64];
int mg_pst[16][SQ_NB];
int eg_pst[16][SQ_NB];
int mx_pst[16][SQ_NB];
Move stack[MAX_PLY];
TTEntry tt[64ULL << 15];
SearchInfo info;
Position pos;

int boardCastle[64] = {
	 7, 15, 15, 15,  3, 15, 15, 11,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	13, 15, 15, 15, 12, 15, 15, 14
};

void UciCommand(char* line);

static inline void SSClear() { memset(stack, 0, sizeof(stack)); }
static inline void TTClear() { memset(tt, 0, sizeof(tt)); }
static inline U64 GetTimeMs() { return (U64)GetTickCount64(); }
static inline int MakePiece(int color, int pt) { return color | pt; }
static inline int GetPieceColor(int piece) { return piece & COLOR_MASK; }
static inline int GetPieceType(int piece) { return piece == EMPTY ? PT_NB : piece & 7; }
static inline int FileOf(int sq) { return sq % 8; }
static inline int RankOf(int sq) { return sq / 8; }
static inline char CFileOf(int sq) { return 'a' + FileOf(sq); }
static inline char CRankOf(int sq) { return '1' + (7 - RankOf(sq)); }
static inline int PieceTypeOn(const Position* pos, int sq) { return GetPieceType(pos->board[sq]); }
static inline int Center(int rank, int file) { return -abs(rank * 2 - 7) / 2 - abs(file * 2 - 7) / 2; }

static U64 Rand64() {
	static U64 next = 1;
	next = next * 12345104729 + 104723;
	return next;
}

static int Distance(int sq1, int sq2) {
	int x1 = FileOf(sq1);
	int y1 = RankOf(sq1);
	int x2 = FileOf(sq2);
	int y2 = RankOf(sq2);
	return max(abs(x1 - x2), abs(y1 - y2));
}

static int InputAvailable(void) {
	static int init = 0, pipe;
	static HANDLE inh;
	DWORD dw;
	if (!init) {
		init = 1;
		inh = GetStdHandle(STD_INPUT_HANDLE);
		pipe = !GetConsoleMode(inh, &dw);
		if (!pipe) {
			SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(inh);
		}
	}
	if (pipe) {
		if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL))
			return 1;
		return dw > 0;
	}
	else {
		GetNumberOfConsoleInputEvents(inh, &dw);
		return dw > 1;
	}
}

static int CheckUp() {
	if ((++info.nodes & 0xffff) == 0) {
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit)
			info.stop = 1;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = 1;
		if (InputAvailable()) {
			char line[4000];
			fgets(line, sizeof(line), stdin);
			UciCommand(line);
		}
	}
	return info.stop;
}

static char* ParseToken(char* string, char* token) {
	while (*string == ' ')
		string++;
	while (*string != ' ' && *string != '\0')
		*token++ = *string++;
	*token = '\0';
	return string;
}

static char* MoveToUci(Move move) {
	static char str[6] = { 0 };
	str[0] = CFileOf(move.from);
	str[1] = CRankOf(move.from);
	str[2] = CFileOf(move.to);
	str[3] = CRankOf(move.to);
	str[4] = "\0nbrq\0\0"[move.promo];
	return str;
}

static Move UciToMove(char* s) {
	Move m;
	m.from = (s[0] - 'a');
	int f = (s[1] - '1');
	m.from += 8 * (7 - f);
	m.to = (s[2] - 'a');
	f = (s[3] - '1');
	m.to += 8 * (7 - f);
	m.promo = PT_NB;
	switch (s[4]) {
	case 'N':
	case 'n':
		m.promo = KNIGHT;
		break;
	case 'B':
	case 'b':
		m.promo = BISHOP;
		break;
	case 'R':
	case 'r':
		m.promo = ROOK;
		break;
	case 'Q':
	case 'q':
		m.promo = QUEEN;
		break;
	}
	return m;
}

static U64 GetHash(const Position* pos) {
	U64 hash = pos->color;
	for (int sq = 0; sq < 64; sq++) {
		int piece = pos->board[sq];
		if (piece)
			hash ^= keys[(piece & 0xf) * 64 + sq];
	}
	if (pos->ep < SQ_NB)
		hash ^= keys[6 * 64 + pos->ep];
	if (pos->castle)
		hash ^= keys[7 * 64 + pos->castle];
	return hash;
}

static int IsRepetition(Position* pos, U64 hash) {
	int limit = max(0, historyCount - pos->move50);
	for (int n = historyCount - 4; n >= limit; n -= 2)
		if (historyHash[n] == hash)
			return TRUE;
	return FALSE;
}

static int IsPseudolegalMove(const Position* pos, const Move move) {
	Move moves[256];
	const int inCheck = IsAttacked(pos, pos->kingSq[pos->color == BLACK], pos->color ^ COLOR_MASK);
	const int num_moves = GenerateMoves(pos, moves, 0, inCheck);
	for (int i = 0; i < num_moves; ++i)
		if (moves[i].from == move.from && moves[i].to == move.to)
			return 1;
	return 0;
}

static void PrintPv(const Position* pos, const Move move) {
	if (!IsPseudolegalMove(pos, move))
		return;
	const Position npos = *pos;
	if (!MakeMove(&npos, &move))
		return;
	printf(" %s", MoveToUci(move));
	const U64 hash = GetHash(&npos);
	TTEntry* tt_entry = tt + (hash % tt_count);
	if (tt_entry->hash != hash || IsRepetition(pos, hash))
		return;
	historyHash[historyCount++] = hash;
	PrintPv(&npos, tt_entry->move);
	historyCount--;
}

static int Permill() {
	int pm = 0;
	for (int n = 0; n < 1000; n++)
		if (tt[n].hash)
			pm++;
	return pm;
}

static int EvalPosition(Position* pos) {
	notOnlyPawns = 0;
	int scoreMg = 0;
	int scoreEg = 0;
	int phase = 0;
	int insufficient[2] = { 0 };
	for (int sq = 0; sq < SQ_NB; ++sq) {
		int piece = pos->board[sq];
		if (piece == EMPTY)continue;
		int pt = GetPieceType(piece);
		int color = GetPieceColor(piece);
		int pc = piece & 0xf;
		insufficient[color == BLACK] += insufVal[pt];
		if (color == pos->color)
			notOnlyPawns += phaseVal[pt];
		phase += phaseVal[pt];
		scoreMg += mg_pst[pc][sq];
		scoreEg += eg_pst[pc][sq];
	}
	phase = min(24, phase);
	int score = (scoreMg * phase + scoreEg * (24 - phase)) / 24;
	if (max(insufficient[0], insufficient[1]) < 5)return 0;
	if (insufficient[score < 0] < 4)return 0;
	score = (score * (100 - pos->move50)) / 100;
	return pos->color == WHITE ? score : -score;
}

static void SetFen(Position* pos, char* fen) {
	memset(pos, 0, sizeof(Position));
	pos->ep = SQ_NB;
	int sq = 0;
	while (*fen && *fen != ' ') {
		switch (*fen) {
		case '1': sq += 1; break;
		case '2': sq += 2; break;
		case '3': sq += 3; break;
		case '4': sq += 4; break;
		case '5': sq += 5; break;
		case '6': sq += 6; break;
		case '7': sq += 7; break;
		case '8': sq += 8; break;
		case 'P': pos->board[sq++] = WHITE_PAWN; break;
		case 'N': pos->board[sq++] = WHITE_KNIGHT; break;
		case 'B': pos->board[sq++] = WHITE_BISHOP; break;
		case 'R': pos->board[sq++] = WHITE_ROOK; break;
		case 'Q': pos->board[sq++] = WHITE_QUEEN; break;
		case 'K': pos->kingSq[0] = sq; pos->board[sq++] = WHITE_KING; break;
		case 'p': pos->board[sq++] = BLACK_PAWN; break;
		case 'n': pos->board[sq++] = BLACK_KNIGHT; break;
		case 'b': pos->board[sq++] = BLACK_BISHOP; break;
		case 'r': pos->board[sq++] = BLACK_ROOK; break;
		case 'q': pos->board[sq++] = BLACK_QUEEN; break;
		case 'k': pos->kingSq[1] = sq; pos->board[sq++] = BLACK_KING; break;
		}
		fen++;
	}
	fen++;
	pos->color = *fen == 'w' ? WHITE : BLACK;
	while (*fen && *fen != ' ') fen++; fen++;
	while (*fen && *fen != ' ') {
		switch (*fen) {
		case 'K': pos->castle |= CWK; break;
		case 'Q': pos->castle |= CWQ; break;
		case 'k': pos->castle |= CBK; break;
		case 'q': pos->castle |= CBQ; break;
		case '-': break;
		}
		fen++;
	}
	fen++;
	if (*fen != '-')
		pos->ep = fen[0] - 'a' + 8 * (7 - (fen[1] - '1'));
	while (*fen && *fen != ' ') fen++; fen++;
	pos->move50 = atoi(fen);
}

static void AddMove(Move* const moveList, int* num_moves, const int from, const int to, const int promo) {
	Move* m = &moveList[(*num_moves)++];
	m->from = from;
	m->to = to;
	m->promo = promo;
}

static void AddPawnMove(Position* pos, Move* const moveList, int* num_moves, const int from, const int to, const int rank) {
	if (rank == 6) {
		for (int pt = KNIGHT; pt < KING; pt++)
			AddMove(moveList, num_moves, from, to, pt);
	}
	else
		AddMove(moveList, num_moves, from, to, PT_NB);
}

static int IsLegalMove(int fx, int fy, int dx, int dy, int* sq) {
	int tx = fx + dx;
	int ty = fy + dy;
	*sq = ty * 8 + tx;
	return (tx >= 0) && (tx < 8) && (ty >= 0) && (ty < 8);
}

static void GeneratePawnMoves(Position* pos, Move* const moveList, int* num_moves, int x, int y, int dy, int onlyCaptures) {
	int from = y * 8 + x;
	int to = (y + dy) * 8 + x;
	int rank = (pos->color == WHITE) ? (7 - y) : y;
	int enColor = (pos->color == WHITE) ? BLACK : WHITE;
	if (!onlyCaptures && pos->board[to] == EMPTY) {
		AddPawnMove(pos, moveList, num_moves, from, to, rank);
		if (rank == 1) {
			to = (y + 2 * dy) * 8 + x;
			if (pos->board[to] == EMPTY)
				AddMove(moveList, num_moves, from, to, PT_NB);
		}
	}
	int sq;
	if (IsLegalMove(x, y, 1, dy, &sq))
		if ((GetPieceColor(pos->board[sq]) == enColor) || (sq == pos->ep))
			AddPawnMove(pos, moveList, num_moves, from, sq, rank);
	if (IsLegalMove(x, y, -1, dy, &sq))
		if ((GetPieceColor(pos->board[sq]) == enColor) || (sq == pos->ep))
			AddPawnMove(pos, moveList, num_moves, from, sq, rank);

}

static void GeneratePieceMoves(Position* pos, Move* const moveList, int* num_moves, const int x, const int y, const D2 dir, int slider, int onlyCaptures) {
	int sq;
	int nx = dir.x;
	int ny = dir.y;
	if (slider > 1)
		nx = dir.x * slider, ny = dir.y * slider;
	if (IsLegalMove(x, y, nx, ny, &sq)) {
		int piece = pos->board[sq];
		if (GetPieceColor(piece) == pos->color)
			return;
		if (piece || !onlyCaptures)
			AddMove(moveList, num_moves, y * 8 + x, sq, PT_NB);
		if (piece)
			return;
		if (slider)
			GeneratePieceMoves(pos, moveList, num_moves, x, y, dir, ++slider, onlyCaptures);
	}
}

static int GenerateMoves(Position* pos, Move* const moveList, int onlyCaptures, int inCheck) {
	int num_moves = 0;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++) {
			int sq = y * 8 + x;
			int piece = pos->board[sq];
			if ((piece & COLOR_MASK) != pos->color)
				continue;
			int pt = GetPieceType(piece);
			int start = dirStart[pt];
			for (int n = 0; n < dirCount[pt]; n++)
				GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirOffset[start + n], dirSlide[pt], onlyCaptures);
			switch (piece) {
			case WHITE_PAWN:
				GeneratePawnMoves(pos, moveList, &num_moves, x, y, -1, onlyCaptures);
				break;
			case BLACK_PAWN:
				GeneratePawnMoves(pos, moveList, &num_moves, x, y, 1, onlyCaptures);
				break;
			case WHITE_KING:
				if (!onlyCaptures && !inCheck) {
					if (pos->castle & CWK)
						if (pos->board[SQ_F1] == EMPTY && pos->board[SQ_G1] == EMPTY && !IsAttacked(pos, SQ_F1, BLACK))
							AddMove(moveList, &num_moves, SQ_E1, SQ_G1, PT_NB);
					if (pos->castle & CWQ)
						if (pos->board[SQ_D1] == EMPTY && pos->board[SQ_B1] == EMPTY && pos->board[SQ_C1] == EMPTY && !IsAttacked(pos, SQ_D1, BLACK))
							AddMove(moveList, &num_moves, SQ_E1, SQ_C1, PT_NB);
				}
				break;
			case BLACK_KING:
				if (!onlyCaptures && !inCheck) {
					if (pos->castle & CBK)
						if (pos->board[SQ_F8] == EMPTY && pos->board[SQ_G8] == EMPTY && !IsAttacked(pos, SQ_F8, WHITE))
							AddMove(moveList, &num_moves, SQ_E8, SQ_G8, PT_NB);
					if (pos->castle & CBQ)
						if (pos->board[SQ_D8] == EMPTY && pos->board[SQ_B8] == EMPTY && pos->board[SQ_C8] == EMPTY && !IsAttacked(pos, SQ_D8, WHITE))
							AddMove(moveList, &num_moves, SQ_E8, SQ_C8, PT_NB);
				}
				break;
			}//switch
		}
	return num_moves;
}

static void PrintBoard(Position* pos) {
	const char* s = "   +---+---+---+---+---+---+---+---+\n";
	const char* t = "     A   B   C   D   E   F   G   H\n";
	printf(t);
	for (int r = 0; r < 8; r++) {
		printf(s);
		printf(" %d |", 8 - r);
		for (int f = 0; f < 8; f++) {
			int sq = r * 8 + f;
			int piece = pos->board[sq];
			int pt = GetPieceType(piece);
			int color = GetPieceColor(piece);
			if (color == WHITE)
				printf(" %c |", "ANBRQK "[pt]);
			else
				printf(" %c |", "anbrqk "[pt]);
		}
		printf(" %d \n", 8 - r);
	}
	printf(s);
	printf(t);
	char castling[5] = "KQkq";
	for (int n = 0; n < 4; n++)
		if (!pos->castle & 1 << n)
			castling[n] = '-';
	printf("side     : %16s\n", pos->color == WHITE ? "white" : "black");
	printf("castling : %16s\n", castling);
	printf("hash     : %16llx\n", GetHash(pos));
	printf("score    : %16d\n", EvalPosition(pos));
}

static void PrintInfo(Position* pos, int depth, int score) {
	printf("info depth %d score ", depth);
	if (abs(score) < MATE - MAX_PLY)
		printf("cp %d", score);
	else
		printf("mate %d", (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1));
	printf(" time %lld", GetTimeMs() - info.timeStart);
	printf(" nodes %lld", info.nodes);
	printf(" hashfull %d pv", Permill());
	PrintPv(pos, stack[0]);
	printf("\n");
}

static int SearchAlpha(Position* pos, int alpha, int beta, int depth, int ply) {
	if (CheckUp())
		return 0;
	int  mate_value = MATE - ply;
	if (alpha < -mate_value)
		alpha = -mate_value;
	if (beta > mate_value - 1)
		beta = mate_value - 1;
	if (alpha >= beta)
		return alpha;
	int staticEval = EvalPosition(pos);
	if (ply >= MAX_PLY)
		return staticEval;
	int color = pos->color == BLACK;
	const int inCheck = IsAttacked(pos, pos->kingSq[color], pos->color ^ COLOR_MASK);
	if (inCheck)
		depth = max(1, depth + 1);
	int inQuiescence = depth < 1;
	if (inQuiescence&& alpha < staticEval) {
		alpha = staticEval;
		if (alpha >= beta)
			return beta;
	}
	const U64 hash = GetHash(pos);
	if (ply && !inQuiescence)
		if (pos->move50 >= 100 || IsRepetition(pos, hash))
			return 0;
	TTEntry* ttEntry = tt + (hash % tt_count);
	Move tt_move = { 0 };
	int inPv = beta - alpha > 1;
	if (ttEntry->hash == hash) {
		tt_move = ttEntry->move;
		if (!inPv && ttEntry->depth >= depth) {
			if (ttEntry->flag == TT_EXACT)return ttEntry->score;
			if (ttEntry->flag == TT_UPPER && ttEntry->score <= alpha)return ttEntry->score;
			if (ttEntry->flag == TT_LOWER && ttEntry->score >= beta)return ttEntry->score;
		}
	}
	else
		depth -= depth > 3;
	historyHash[historyCount++] = hash;
	U8 tt_flag = TT_UPPER;
	int score;
	int legalCount = 0;
	int quietCount = 0;
	Move movesList[MAX_MOVE];
	Move quietList[MAX_MOVE];
	S64 scoreList[MAX_MOVE];
	const int movesCount = GenerateMoves(pos, movesList, inQuiescence, inCheck);
	for (int j = 0; j < movesCount; ++j) {
		Move m = movesList[j];
		int piece = pos->board[m.from];
		const int ptSou = PieceTypeOn(pos, m.from);
		int ptDes = m.promo == PT_NB ? PieceTypeOn(pos, m.to) : m.promo;
		if (m.move == tt_move.move)
			scoreList[j] = 1LL << 62;
		else if (ptDes != PT_NB)
			scoreList[j] = (1LL << 54) + ptDes * 10 - ptSou;
		else {
			int pc = piece & 0xf;
			scoreList[j] = mx_pst[pc][m.to]-mx_pst[pc][m.from];
		}
	}
	for (int n = 0; n < movesCount; n++) {
		int bstIdx = n;
		for (int j = n + 1; j < movesCount; ++j)
			if (scoreList[j] > scoreList[bstIdx])
				bstIdx = j;
		Move move = movesList[bstIdx];
		scoreList[bstIdx] = scoreList[n];
		movesList[bstIdx] = movesList[n];
		Position npos = *pos;
		if (!MakeMove(&npos, &move))
			continue;
		if (!legalCount || depth < 4)
			score = -SearchAlpha(&npos, -beta, -alpha, depth - 1, ply + 1);
		else {
			int r = !inPv;
			score = -SearchAlpha(&npos, -alpha - 1, -alpha, depth - 1 - r, ply + 1);
			if (r && score > alpha)
				score = -SearchAlpha(&npos, -alpha - 1, -alpha, depth - 1, ply + 1);
			if (score > alpha && score < beta)
				score = -SearchAlpha(&npos, -beta, -alpha, depth - 1, ply + 1);
		}
		legalCount++;
		if (info.stop)
			break;
		int isQuiet = move.promo == PT_NB && PieceTypeOn(pos, move.to) == PT_NB && (PieceTypeOn(pos, move.from) != PAWN || move.to != pos->ep);
		if (isQuiet)
			quietList[quietCount++] = move;
		if (alpha < score) {
			alpha = score;
			stack[ply] = move;
			tt_flag = TT_EXACT;
			if (!ply && info.post)
				PrintInfo(pos, depth, score);
			if (alpha >= beta) {
				tt_flag = TT_LOWER;
				break;
			}
		}
	}
	historyCount--;
	if (info.stop)
		return 0;
	ttEntry->hash = hash;
	if(stack[ply].move)
	ttEntry->move = stack[ply];
	ttEntry->depth = max(0, depth);
	ttEntry->score = alpha;
	ttEntry->flag = tt_flag;
	if (!legalCount)
		return inQuiescence ? alpha : inCheck ? ply - MATE : 0;
	return alpha;
}

static void SearchIteratively(Position* pos) {
	SSClear();
	TTClear();
	int score = 0;
	int alpha = -MATE;
	int beta = MATE;
	for (int depth = 1; depth <= info.depthLimit; ++depth) {
		int aspH = 16, aspL = 16;
		do {
			if (depth > 4) {
				alpha = score - aspL;
				beta = score + aspH;
			}
			score = SearchAlpha(pos, alpha, beta, depth, 0);
			if (score <= alpha) {
				alpha -= aspL;
				aspL *= 2;
			}
			else if (score >= beta) {
				beta += aspH;
				aspH *= 2;
			}
			else
				break;
		} while (!info.stop);
		if (info.stop)
			break;
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit / 2)
			break;
	}
	if (info.post)
		printf("bestmove %s\n", MoveToUci(stack[0]));
}

static int GetSliderPiece(Position* pos, int x, int y, D2 dir) {
	int sq;
	if (IsLegalMove(x, y, dir.x, dir.y, &sq)) {
		if (pos->board[sq])
			return pos->board[sq];
		return GetSliderPiece(pos, x + dir.x, y + dir.y, dir);
	}
	return EMPTY;
}

static int IsAttacked(Position* pos, int sq, int byColor) {
	int sq2;
	int fx = FileOf(sq);
	int fy = RankOf(sq);
	int dy = byColor == WHITE ? 1 : -1;
	for (int dx = -1; dx <= 1; dx += 2)
		if (IsLegalMove(fx, fy, dx, dy, &sq2) && (pos->board[sq2] == MakePiece(byColor, PAWN)))
			return TRUE;
	for (int n = 0; n < 8; n++)
		if (IsLegalMove(fx, fy, dirOffset[n + 8].x, dirOffset[n + 8].y, &sq2) && (pos->board[sq2] == MakePiece(byColor, KNIGHT)))
			return TRUE;
	if (Distance(sq, pos->kingSq[byColor == BLACK]) == 1)
		return TRUE;
	int bishop = MakePiece(byColor, BISHOP);
	int rook = MakePiece(byColor, ROOK);
	int queen = MakePiece(byColor, QUEEN);
	for (int n = 0; n < 4; n++) {
		int piece = GetSliderPiece(pos, fx, fy, dirOffset[n]);
		if ((piece == bishop) || (piece == queen))
			return TRUE;
		piece = GetSliderPiece(pos, fx, fy, dirOffset[n + 4]);
		if ((piece == rook) || (piece == queen))
			return TRUE;
	}
	return FALSE;
}

static void MovePiece(Position* pos, int from, int to) {
	pos->board[to] = pos->board[from];
	pos->board[from] = EMPTY;
}

static int MakeMove(Position* pos, const Move* move) {
	int ep = pos->ep;
	pos->ep = SQ_NB;
	if (pos->board[move->to])
		pos->move50 = 0;
	else
		pos->move50++;
	int piece = pos->board[move->from];
	if (piece == WHITE_KING) {
		pos->kingSq[0] = move->to;
		if (move->from == SQ_E1) {
			if (move->to == SQ_G1)
				MovePiece(pos, SQ_H1, SQ_F1);
			else if (move->to == SQ_C1)
				MovePiece(pos, SQ_A1, SQ_D1);
		}
	}
	else if (piece == BLACK_KING) {
		pos->kingSq[1] = move->to;
		if (move->from == SQ_E8) {
			if (move->to == SQ_G8)
				MovePiece(pos, SQ_H8, SQ_F8);
			else if (move->to == SQ_C8)
				MovePiece(pos, SQ_A8, SQ_D8);
		}
	}
	int pt = GetPieceType(piece);
	if (pt == PAWN) {
		if (move->to == ep)
			pos->board[move->to + (pos->color == WHITE ? 8 : -8)] = EMPTY;
		if (abs(move->from - move->to) == 16)
			pos->ep = (move->from + move->to) / 2;
		pos->move50 = 0;
	}
	MovePiece(pos, move->from, move->to);
	if (move->promo < PT_NB)
		pos->board[move->to] = MakePiece(pos->color, move->promo);
	pos->castle &= boardCastle[move->from] & boardCastle[move->to];
	pos->color ^= COLOR_MASK;
	return !IsAttacked(pos, pos->kingSq[pos->color == WHITE], pos->color);
}

static int ShrinkNumber(U64 n) {
	if (n < 10000)
		return 0;
	if (n < 10000000)
		return 1;
	if (n < 10000000000)
		return 2;
	return 3;
}

static void PrintSummary(U64 time, U64 nodes) {
	U64 nps = (nodes * 1000) / max(time, 1);
	const char* units[] = { "", "k", "m", "g" };
	int sn = ShrinkNumber(nps);
	int p = pow(10, sn * 3);
	printf("-----------------------------\n");
	printf("Time        : %llu\n", time);
	printf("Nodes       : %llu\n", nodes);
	printf("Nps         : %llu (%llu%s/s)\n", nps, nps / p, units[sn]);
	printf("-----------------------------\n");
}

void PrintPerformanceHeader() {
	printf("-----------------------------\n");
	printf("ply      time        nodes\n");
	printf("-----------------------------\n");
}

static void ResetInfo() {
	info.timeStart = GetTimeMs();
	info.timeLimit = 0;
	info.depthLimit = MAX_PLY;
	info.nodesLimit = 0;
	info.nodes = 0;
	info.stop = FALSE;
	info.post = TRUE;
}

static inline void PerftDriver(Position* pos, int depth) {
	Move moves[256];
	const int inCheck = IsAttacked(pos, pos->kingSq[pos->color == BLACK], pos->color ^ COLOR_MASK);
	const int num_moves = GenerateMoves(pos, moves, 0, inCheck);
	for (int n = 0; n < num_moves; n++) {
		Position npos = *pos;
		if (!MakeMove(&npos, &moves[n]))
			continue;
		if (depth)
			PerftDriver(&npos, depth - 1);
		else
			info.nodes++;
	}
}

//performance test
static void UciPerformance(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	U64 elapsed = 0;
	while (elapsed < 4000) {
		info.nodes = 0;
		info.timeStart = GetTimeMs();
		PerftDriver(pos, info.depthLimit++);
		elapsed = GetTimeMs() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

//start benchmark
static void UciBench(Position* pos) {
	ResetInfo();
	PrintPerformanceHeader();
	info.depthLimit = 0;
	info.post = FALSE;
	U64 elapsed = 0;
	while (elapsed < 8000) {
		++info.depthLimit;
		SearchIteratively(pos);
		elapsed = GetTimeMs() - info.timeStart;
		printf(" %2d. %8llu %12llu\n", info.depthLimit, elapsed, info.nodes);
	}
	PrintSummary(elapsed, info.nodes);
}

static void ParsePosition(Position* pos, char* ptr) {
	char token[80], fen[80];
	ptr = ParseToken(ptr, token);
	if (strcmp(token, "fen") == 0) {
		fen[0] = '\0';
		while (1) {
			ptr = ParseToken(ptr, token);
			if (*token == '\0' || strcmp(token, "moves") == 0)
				break;
			strcat(fen, token);
			strcat(fen, " ");
		}
		SetFen(pos, fen);
	}
	else {
		ptr = ParseToken(ptr, token);
		SetFen(pos, START_FEN);
	}
	historyCount = 0;
	if (strcmp(token, "moves") == 0)
		while (1) {
			ptr = ParseToken(ptr, token);
			if (*token == '\0')
				break;
			Move m = UciToMove(token);
			historyHash[historyCount++] = GetHash(pos);
			MakeMove(pos, &m);
			if (!pos->move50)
				historyCount = 0;
		}
}

static void ParseGo(Position* pos, char* command) {
	ResetInfo();
	int wtime = 0;
	int btime = 0;
	int winc = 0;
	int binc = 0;
	int movestogo = 32;
	char* argument = NULL;
	if (argument = strstr(command, "binc"))
		binc = atoi(argument + 5);
	if (argument = strstr(command, "winc"))
		winc = atoi(argument + 5);
	if (argument = strstr(command, "wtime"))
		wtime = max(1, atoi(argument + 6));
	if (argument = strstr(command, "btime"))
		btime = max(1, atoi(argument + 6));
	if ((argument = strstr(command, "movestogo")))
		movestogo = atoi(argument + 10);
	if ((argument = strstr(command, "movetime")))
		info.timeLimit = atoi(argument + 9);
	if ((argument = strstr(command, "depth")))
		info.depthLimit = atoi(argument + 6);
	if (argument = strstr(command, "nodes"))
		info.nodesLimit = atoi(argument + 5);
	int time = pos->color == WHITE ? wtime : btime;
	int inc = pos->color == WHITE ? winc : binc;
	if (time)
		info.timeLimit = max(1, min(time / movestogo + inc, time / 2));
	SearchIteratively(pos);
}

void UciCommand(char* line) {
	char command[0xff] = { 0 };
	sscanf(line, "%s", command);
	if (!strcmp(command, "uci"))printf("id name %s\nuciok\n", NAME);
	else if (!strcmp(command, "isready"))printf("readyok\n");
	else if (!strcmp(command, "go"))ParseGo(&pos, line + 2);
	else if (!strcmp(command, "position"))ParsePosition(&pos, line + 8);
	else if (!strcmp(command, "print"))PrintBoard(&pos);
	else if (!strcmp(command, "perft"))UciPerformance(&pos);
	else if (!strcmp(command, "bench"))UciBench(&pos);
	else if (!strcmp(command, "stop"))info.stop = TRUE;
	else if (!strcmp(command, "quit"))exit(0);
}

static void InitHash() {
	for (int i = 0; i < KEYS_COUNT; ++i)
		keys[i] = Rand64();
}

static void InitPst() {
	for (int pt = PAWN; pt <= KING; pt++)
		for (int sq = 0; sq < SQ_NB; sq++) {
			int mg = mg_material[pt];
			int eg = eg_material[pt];
			int file = FileOf(sq);
			int rank = RankOf(sq);
			int center = Center(rank, file);
			switch (pt) {
			case PAWN:
				mg += file == FILE_D || file == FILE_E ? rank : 0;
				eg += rank * 2;
				break;
			case KNIGHT:
			case BISHOP:
			case ROOK:
			case QUEEN:
				mg += center;
				eg += center;
				break;
			case KING:
				mg -= center;
				eg += center;
				break;
			}
			mg_pst[MakePiece(WHITE, pt) & 0xf][FLIP_SQ(sq)] = mg;
			eg_pst[MakePiece(WHITE, pt) & 0xf][FLIP_SQ(sq)] = eg;
			mx_pst[MakePiece(WHITE, pt) & 0xf][FLIP_SQ(sq)] = max(mg,eg);
			mg_pst[MakePiece(BLACK, pt) & 0xf][sq] = -mg;
			eg_pst[MakePiece(BLACK, pt) & 0xf][sq] = -eg;
			mx_pst[MakePiece(BLACK, pt) & 0xf][sq] = max(mg,eg);
		}
}

int main(const int argc, const char** argv) {
	InitHash();
	InitPst();
	setbuf(stdout, NULL);
	printf("%s %s\n", NAME, VERSION);
	SetFen(&pos, START_FEN);
	char line[4000];
	while (fgets(line, sizeof(line), stdin))
		UciCommand(line);
}
