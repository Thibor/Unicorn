#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INF 32001
#define MATE 32000
#define MAX_PLY 64
#define U16 unsigned __int16
#define S32 signed __int32
#define S64 signed __int64
#define U64 unsigned __int64
#define FALSE 0
#define TRUE 1
#define NAME "Unicorn"
#define VERSION "2026-03-24"
#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

enum Color { WHITE = 8, BLACK = 16, COLOR_MASK = WHITE | BLACK };
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PT_NB };
enum Piece {
	EMPTY,
	WHITE_PAWN = 8, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
	BLACK_PAWN = 16, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING
};

enum Squares {
	a8, b8, c8, d8, e8, f8, g8, h8,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};

typedef struct {
	int x;
	int y;
} D2;

typedef struct {
	int kingSq[2];
	int board[64];
	int color;
	int ep;
	int castle;
}Position;

Position pos;

typedef struct {
	int from;
	int to;
	int promo;
}Move;

typedef struct {
	Move move;
} Stack;

typedef struct {
	U64 key;
	Move move;
	int score;
	int depth;
	U16 flag;
}TT_Entry;

typedef struct {
	int stop;
	int depthLimit;
	U64 timeStart;
	U64 timeLimit;
	U64 nodes;
	U64 nodesLimit;
}SearchInfo;

SearchInfo info;

const int W_QS = 1, W_KS = 2, B_QS = 4, B_KS = 8;
D2 dirKnight[8] = { {1,2},{2,1},{-1,2},{-2,1},{-1,-2},{-2,-1},{1,-2},{2,-1} };
D2 dirBishop[4] = { {1,1},{-1,1},{-1,-1},{1,-1} };
D2 dirRook[4] = { {1,0},{-1,0},{0,1},{0,-1} };
int boardCastle[64] = { 0 };

int material[PT_NB] = { 100,320,330,500,900,0 };
Stack stack[MAX_PLY];

static U64 GetTimeMs() {
	return (U64)GetTickCount64();
}

static int MakePiece(int color, int pt) {
	return color | pt;
}

static int GetPieceColor(int piece) {
	return piece & COLOR_MASK;
}

static int GetPieceType(int piece) {
	return piece & 7;
}

static int Distance(int sq1, int sq2) {
	int x1 = sq1 % 8;
	int y1 = sq1 / 8;
	int x2 = sq2 % 8;
	int y2 = sq2 / 8;
	return max(abs(x1 - x2), abs(y1 - y2));
}

static int CheckUp() {
	if ((++info.nodes & 0xffff) == 0) {
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit)
			info.stop = 1;
		if (info.nodesLimit && info.nodes > info.nodesLimit)
			info.stop = 1;
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
	str[0] = 'a' + (move.from % 8);
	str[1] = '1' + (7 - move.from / 8);
	str[2] = 'a' + (move.to % 8);
	str[3] = '1' + (7 - move.to / 8);
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

static int EvalPosition(Position* pos) {
	int score = 0;
	for (int sq = 0; sq < 64; ++sq) {
		int piece = pos->board[sq];
		if (piece != EMPTY) {
			int pt = piece & 7;
			int color = piece & COLOR_MASK;
			if (color == WHITE)
				score += material[pt];
			else
				score -= material[pt];
		}
	}
	return pos->color == WHITE ? score : -score;
}

static void SetFen(Position* pos, char* fen) {
	memset(pos, 0, sizeof(Position));
	pos->ep = no_sq;
	int i = 0;
	int z = 0;
	int sq = 0;
	int n = (int)strlen(fen);
	for (i = 0; i < n && !z; ++i) {
		switch (fen[i]) {
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
		case 'k':  pos->kingSq[1] = sq; pos->board[sq++] = BLACK_KING; break;
		case '/': break;
		default: z = 1; break;
		}
	}
	pos->color = fen[i++] == 'w' ? WHITE : BLACK;
	i++;
	for (z = 0; i < n && !z; ++i) {
		switch (fen[i]) {
		case 'K': pos->castle |= W_KS; break;
		case 'Q': pos->castle |= W_QS; break;
		case 'k': pos->castle |= B_KS; break;
		case 'q': pos->castle |= B_QS; break;
		case '-': break;
		default: z = 1; break;
		}
	}
	if (fen[i] != '-')
		pos->ep = fen[i] - 'a' + 8 * (7 - (fen[i + 1] - '1'));
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

static int RelativeRank(Position* pos, int y) {
	return (pos->color == WHITE) ? (7 - y) : y;
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
	int rank = RelativeRank(pos, y);
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
		if (((pos->board[sq] & COLOR_MASK) == enColor) || (sq == pos->ep))
			AddPawnMove(pos, moveList, num_moves, from, sq, rank);
	if (IsLegalMove(x, y, -1, dy, &sq))
		if (((pos->board[sq] & COLOR_MASK) == enColor) || (sq == pos->ep))
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

static int MoveGen(const Position* pos, Move* const moveList, int onlyCaptures) {
	int num_moves = 0;
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++) {
			int sq = y * 8 + x;
			int piece = pos->board[sq];
			if ((piece & COLOR_MASK) == pos->color)
				switch (piece) {
				case WHITE_PAWN:
					GeneratePawnMoves(pos, moveList, &num_moves, x, y, -1, onlyCaptures);
					break;
				case BLACK_PAWN:
					GeneratePawnMoves(pos, moveList, &num_moves, x, y, 1, onlyCaptures);
					break;
				case WHITE_KNIGHT:
				case BLACK_KNIGHT:
					for (int n = 0; n < 8; n++)
						GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirKnight[n], 0, onlyCaptures);
					break;
				case WHITE_BISHOP:
				case BLACK_BISHOP:
					for (int n = 0; n < 4; n++)
						GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirBishop[n], 1, onlyCaptures);
					break;
				case WHITE_ROOK:
				case BLACK_ROOK:
					for (int n = 0; n < 4; n++)
						GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirRook[n], 1, onlyCaptures);
					break;
				case WHITE_QUEEN:
				case BLACK_QUEEN:
					for (int n = 0; n < 4; n++) {
						GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirBishop[n], 1, onlyCaptures);
						GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirRook[n], 1, onlyCaptures);
					}
					break;
				case WHITE_KING:
					if (!onlyCaptures) {
						if (pos->castle & W_KS)
							if (pos->board[f1] == EMPTY && pos->board[g1] == EMPTY)
								if (!IsSquareAttacked(pos, e1, BLACK) && !IsSquareAttacked(pos, f1, BLACK))
									AddMove(moveList, &num_moves, e1, g1, PT_NB);
						if (pos->castle & W_QS)
							if (pos->board[d1] == EMPTY && pos->board[b1] == EMPTY && pos->board[c1] == EMPTY)
								if (!IsSquareAttacked(pos, e1, BLACK) && !IsSquareAttacked(pos, d1, BLACK))
									AddMove(moveList, &num_moves, e1, c1, PT_NB);
						for (int n = 0; n < 4; n++) {
							GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirBishop[n], 0, onlyCaptures);
							GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirRook[n], 0, onlyCaptures);
						}
					}
					break;
				case BLACK_KING:
					if (!onlyCaptures) {
						if (pos->castle & B_KS)
							if (pos->board[f8] == EMPTY && pos->board[g8] == EMPTY)
								if (!IsSquareAttacked(pos, e8, WHITE) && !IsSquareAttacked(pos, f8, WHITE))
									AddMove(moveList, &num_moves, e8, g8, PT_NB);
						if (pos->castle & B_QS)
							if (pos->board[d8] == EMPTY && pos->board[b8] == EMPTY && pos->board[c8] == EMPTY)
								if (!IsSquareAttacked(pos, e8, WHITE) && !IsSquareAttacked(pos, d8, WHITE))
									AddMove(moveList, &num_moves, e8, c8, PT_NB);
					}
					for (int n = 0; n < 4; n++) {
						GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirBishop[n], 0, onlyCaptures);
						GeneratePieceMoves(pos, moveList, &num_moves, x, y, dirRook[n], 0, onlyCaptures);
					}
					break;
				}
		}
	return num_moves;
}

static void PrintInfo(int depth, int score) {
	printf("info depth %d score ", depth);
	if (abs(score) < MATE - MAX_PLY)
		printf("cp %d", score);
	else
		printf("mate %d", (score > 0 ? (MATE - score + 1) >> 1 : -(MATE + score) >> 1));
	printf(" time %lld", GetTimeMs() - info.timeStart);
	printf(" nodes %lld pv %s", info.nodes, MoveToUci(stack[0].move));
	printf("\n");
	fflush(stdout);
}

static int Center(int rank, int file) {
	return -abs(rank * 2 - 7) / 2 - abs(file * 2 - 7) / 2;
}

static int CenterSq(int sq) {
	int rank = sq / 8;
	int file = sq % 8;
	return Center(rank, file);
}

static int EvalMove(Position* pos, Move* bst, Move* m) {
	int score = CenterSq(m->to) - CenterSq(m->from);
	int pSou = pos->board[m->from];
	int pDes = pos->board[m->to];
	if ((m->from == bst->from) && (m->to == bst->to))
		score += 10000;
	if (m->promo<PT_NB)
		score += material[m->promo] - material[PAWN];
	if (pDes)
		score += 10 * material[pDes & 7] - material[pSou & 7];
	return score;
}

static Move PickMove(Position* pos, Move* moveList, int* scoreList, int num_moves, int from) {
	int bestIndex = from;
	int bestScore = scoreList[from];
	Move m = moveList[from];
	for (int i = from + 1; i < num_moves; i++) {
		if (bestScore < scoreList[i]) {
			bestIndex = i;
			bestScore = scoreList[i];
			m = moveList[i];
		}
	}
	moveList[bestIndex] = moveList[from];
	scoreList[bestIndex] = scoreList[from];
	return m;
}

static int SearchAlpha(Position* pos, int alpha, int beta, int depth, int ply) {
	if (CheckUp())
		return 0;
	const int static_eval = EvalPosition(pos);
	if (ply >= MAX_PLY)
		return static_eval;
	const int inCheck = IsSquareAttacked(pos, pos->kingSq[pos->color == BLACK], pos->color ^ COLOR_MASK);
	if (inCheck)depth = max(1, depth + 1);
	int in_qsearch = depth < 1;
	if (in_qsearch && alpha < static_eval) {
		alpha = static_eval;
		if (alpha >= beta)
			return beta;
	}
	int legalMoves = 0;
	Move moves[256];
	const int num_moves = MoveGen(pos, moves, in_qsearch);
	int scoreList[256];
	for (int n = 0; n < num_moves; n++)
		scoreList[n] = EvalMove(pos, &stack[ply].move, &moves[n]);
	for (int n = 0; n < num_moves; n++) {
		Move move = PickMove(pos, moves, scoreList, num_moves, n);
		Position npos = *pos;
		if (!MakeMove(&npos, &move))
			continue;
		legalMoves++;
		int score = -SearchAlpha(&npos, -beta, -alpha, depth - 1, ply + 1);
		if (info.stop)
			return 0;
		if (alpha < score) {
			alpha = score;
			stack[ply].move = move;
			if (!ply)
				PrintInfo(depth, score);
			if (alpha >= beta)
				return beta;
		}
	}
	if (!legalMoves && !in_qsearch)
		return inCheck ? ply - MATE : 0;
	return alpha;
}

static void SearchIteratively(Position* pos) {
	for (int depth = 1; depth <= info.depthLimit; ++depth) {
		SearchAlpha(pos, -MATE, MATE, depth, 0);
		if (info.stop)
			break;
		if (info.timeLimit && GetTimeMs() - info.timeStart > info.timeLimit / 2)
			break;
	}
	char* uci = MoveToUci(stack[0].move);
	printf("bestmove %s\n", uci);
	fflush(stdout);
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

static int IsSquareAttacked(Position* pos, int sq, int byColor) {
	int sq2;
	int fx = sq % 8;
	int fy = sq / 8;
	int dy = byColor == WHITE ? 1 : -1;
	for (int dx = -1; dx <= 1; dx += 2)
		if (IsLegalMove(fx, fy, dx, dy, &sq2) && (pos->board[sq2] == MakePiece(byColor, PAWN)))
			return TRUE;
	for (int n = 0; n < 8; n++)
		if (IsLegalMove(fx, fy, dirKnight[n].x, dirKnight[n].y, &sq2) && (pos->board[sq2] == MakePiece(byColor, KNIGHT)))
			return TRUE;
	int c = byColor == BLACK;
	if (Distance(sq, pos->kingSq[c]) == 1)
		return TRUE;
	int bishop = MakePiece(byColor, BISHOP);
	int rook = MakePiece(byColor, ROOK);
	int queen = MakePiece(byColor, QUEEN);
	for (int n = 0; n < 4; n++) {
		int piece = GetSliderPiece(pos, fx, fy, dirBishop[n]);
		if ((piece == bishop) || (piece == queen))
			return TRUE;
		piece = GetSliderPiece(pos, fx, fy, dirRook[n]);
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
	pos->ep = no_sq;
	int piece = pos->board[move->from];
	if (piece == WHITE_KING) {
		pos->kingSq[0] = move->to;
		if (move->from == e1) {
			if (move->to == g1)
				MovePiece(pos, h1, f1);
			else if (move->to == c1)
				MovePiece(pos, a1, d1);
		}
	}
	else if (piece == BLACK_KING) {
		pos->kingSq[1] = move->to;
		if (move->from == e8) {
			if (move->to == g8)
				MovePiece(pos, h8, f8);
			else if (move->to == c8)
				MovePiece(pos, a8, d8);
		}
	}
	int pt = piece & 7;
	if (pt == PAWN) {
		if (move->to == ep)
			if (pos->color == WHITE)
				pos->board[move->to + 8] = EMPTY;
			else
				pos->board[move->to - 8] = EMPTY;
		if (abs(move->from - move->to) == 16)
			pos->ep = (move->from + move->to) / 2;
	}
	MovePiece(pos, move->from, move->to);
	if (move->promo < PT_NB)
		pos->board[move->to] = MakePiece(pos->color, move->promo);
	pos->castle &= ~boardCastle[move->from] & ~boardCastle[move->to];
	pos->color ^= COLOR_MASK;
	return !IsSquareAttacked(pos, pos->kingSq[pos->color == WHITE], pos->color);
}

static void ParsePosition(char* ptr) {
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
		SetFen(&pos, fen);
	}
	else {
		ptr = ParseToken(ptr, token);
		SetFen(&pos, START_FEN);
	}
	if (strcmp(token, "moves") == 0)
		while (1) {
			ptr = ParseToken(ptr, token);
			if (*token == '\0')
				break;
			Move m = UciToMove(token);
			MakeMove(&pos, &m);
		}
}

static void ParseGo(char* command) {
	info.stop = FALSE;
	info.nodes = 0;
	info.depthLimit = MAX_PLY;
	info.nodesLimit = 0;
	info.timeLimit = 0;
	info.timeStart = GetTimeMs();
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
		wtime = atoi(argument + 6);
	if (argument = strstr(command, "btime"))
		btime = atoi(argument + 6);
	if ((argument = strstr(command, "movestogo")))
		movestogo = atoi(argument + 10);
	if ((argument = strstr(command, "movetime")))
		info.timeLimit = atoi(argument + 9);
	if ((argument = strstr(command, "depth")))
		info.depthLimit = atoi(argument + 6);
	if (argument = strstr(command, "nodes"))
		info.nodesLimit = atoi(argument + 5);
	int time = pos.color == WHITE ? wtime : btime;
	int inc = pos.color == WHITE ? winc : binc;
	if (time)
		info.timeLimit = min(time / movestogo + inc, time / 2);
	SearchIteratively(&pos);
}

static void UciCommand(char* line) {
	if (strncmp(line, "ucinewgame", 10) == 0) {}
	else if (!strncmp(line, "uci", 3)) {
		printf("id name %s\nuciok\n", NAME);
		fflush(stdout);
	}
	else if (!strncmp(line, "isready", 7)) {
		printf("readyok\n");
		fflush(stdout);
	}
	else if (!strncmp(line, "go", 2))
		ParseGo(line + 2);
	else if (!strncmp(line, "position", 8))
		ParsePosition(line + 8);
	else if (!strncmp(line, "exit", 4))
		exit(0);
}

static void UciLoop() {
	char line[4000];
	while (fgets(line, sizeof(line), stdin))
		UciCommand(line);
}

int main(const int argc, const char** argv) {
	boardCastle[a8] = B_QS;
	boardCastle[e8] = B_QS | B_KS;
	boardCastle[h8] = B_KS;
	boardCastle[a1] = W_QS;
	boardCastle[e1] = W_QS | W_KS;
	boardCastle[h1] = W_KS;
	printf("%s %s\n", NAME, VERSION);
	SetFen(&pos, START_FEN);
	UciLoop();
}
