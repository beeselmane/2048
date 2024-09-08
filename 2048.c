#include <curses.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Add debug messaged to log
#define DEBUG_LOG       0

// Dimension of game board
#define BOARD_SIZE      4

// Maximum number of digits per entry
#define ENTRY_DIGITS    6

// Check if a character corresponds to a valid move.
#define IS_MOVE(c)      (MOVE_MAP[(c)] != 0)

// Probability (/ 100) to spawn 4 instead of 2
#define SPAWN_RATE      10

// Game state
struct game {
    // Game board
    uint32_t board[BOARD_SIZE][BOARD_SIZE];

    // Last move made
    enum move {
        MOVE_LEFT   = 1,
        MOVE_RIGHT  = 2,
        MOVE_DOWN   = 3,
        MOVE_UP     = 4
    } move;

    // Number of slots remaining
    uint32_t slots;

    // Last selected random number
    uint32_t rand;

    // Log file for this game
    FILE *log;
};

// Color map. Pretty arbitrary.
uint8_t COLOR_MAP[20] = {
    /* 0, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 1 */
       8, 7, 6, 1,  4,  5,  3,   7,   6,   1,    4,    2,    5,    3,     2,     2,     2,      7, 8
};

// Map between characters and moves.
int MOVE_MAP[0x100] = {
    /* 0x00 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x10 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x20 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x30 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    /* 0x40 */ 0, 0,         0, 0,         0,          0, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x50 */ 0, 0,         0, 0,         0,          0, 0, 0, 0, 0, 0,       0, 0, 0, 0, 0,
    /* 0x60 */ 0, MOVE_LEFT, 0, 0,         MOVE_RIGHT, 0, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x70 */ 0, 0,         0, MOVE_DOWN, 0,          0, 0, MOVE_UP, 0, 0, 0, 0, 0, 0, 0, 0,

};

static void spawn_tile(struct game *state);

// Initialize curses state
int init_curses(void)
{
    initscr();
    keypad(stdscr, TRUE);
    nonl();
    cbreak();
    noecho();

    if (has_colors())
    {
        start_color();

        init_pair(1, COLOR_RED,     COLOR_BLACK);
        init_pair(2, COLOR_GREEN,   COLOR_BLACK);
        init_pair(3, COLOR_BLUE,    COLOR_BLACK);
        init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(5, COLOR_CYAN,    COLOR_BLACK);
        init_pair(6, COLOR_YELLOW,  COLOR_BLACK);
        init_pair(7, COLOR_WHITE,   COLOR_BLACK);
        init_pair(8, COLOR_BLACK,   COLOR_BLACK);
    }

    return 0;
}

// Generate new game board
int init_game(struct game *state, FILE *log)
{
    state->log = log;

    for (int i = 0; i < (BOARD_SIZE * BOARD_SIZE); i++) {
        state->board[i / BOARD_SIZE][i % BOARD_SIZE] = 0;
    }

    // Log how large of a board this game is played on.
    if (state->log) { fprintf(state->log, "%u,%u\n", BOARD_SIZE, BOARD_SIZE); }

    state->slots = (BOARD_SIZE * BOARD_SIZE);
    spawn_tile(state);

    return 0;
}

// `val` has to be a power of 2.
void write_entry(uint32_t val)
{
    // Don't write zeros.
    if (!val)
    {
        printw("%*s", ENTRY_DIGITS, "");
        return;
    }

    // Otherwise, compute log base 2.
    uint32_t log2 = 1;
    uint32_t ctr = 2;

    // This is probably a bad algorithm but oh well.
    while (ctr != val && ctr != 0x40000)
    {
        ctr <<= 1;
        log2++;
    }

    // And print in the right color.
    attrset(COLOR_PAIR(COLOR_MAP[log2]));
    printw("%*u", ENTRY_DIGITS, val);
    attrset(COLOR_PAIR(0));
}

// Display game board at a given point
int display_board(struct game *state, int x, int y)
{
    // Line of grid cell walls
    static char *grid = NULL;

    // Separator line between grid cells
    static char *sep = NULL;

    if (!sep || !grid)
    {
        if (!grid)
        {
            grid = malloc(BOARD_SIZE * (ENTRY_DIGITS + 3) + 1);
            if (!grid) { perror("malloc"); exit(1); }
        }

        if (!sep)
        {
            sep = malloc(BOARD_SIZE * (ENTRY_DIGITS + 3) + 1);
            if (!sep) { perror("malloc"); exit(1); }
        }

        off_t off = 1;
        grid[0] = '|';
        sep[0]  = '+';

        for (int i = 0; i < BOARD_SIZE; i++)
        {
            memset(&grid[off], ' ', ENTRY_DIGITS + 2);
            memset(&sep[off],  '-', ENTRY_DIGITS + 2);

            off += ENTRY_DIGITS + 2;
            grid[off] = '|';
            sep[off]  = '+';
            off++;
        }
    }

    for (int i = 0; i < BOARD_SIZE; i++)
    {
        move(y + (i * 4) + 0, x);
        addstr(sep);
        move(y + (i * 4) + 1, x);
        addstr(grid);
        move(y + (i * 4) + 2, x);

        for (int j = 0; j < BOARD_SIZE; j++)
        {
            addstr("| ");
            write_entry(state->board[i][j]);
            addch(' ');
        }

        addch('|');

        move(y + (i * 4) + 3, x);
        addstr(grid);
    }

    move(y + (BOARD_SIZE * 4), x);
    addstr(sep);

    return 0;
}

// Spawn a new tile into the gameboard depending on how many slots are left.
void spawn_tile(struct game *state)
{
    state->rand = arc4random_uniform(state->slots * 100);

    uint32_t slot = state->rand % state->slots;
    uint32_t tile = state->rand / state->slots;

    // Right now, there are only 2 and 4.
    uint32_t tile_val = (tile < SPAWN_RATE) ? 4 : 2;

    // Insert new tile into board. This is linear in board size.
    for (int i = 0, j = 0; i < (BOARD_SIZE * BOARD_SIZE); i++)
    {
        if (!state->board[i / BOARD_SIZE][i % BOARD_SIZE])
        {
            if (j == slot)
            {
                state->board[i / BOARD_SIZE][i % BOARD_SIZE] = tile_val;
                state->slots--;

                if (DEBUG_LOG)
                {
                    fprintf(state->log, "Insert %u to (%ux%u) [slots=%u]\n", tile_val, i / BOARD_SIZE, i % BOARD_SIZE, state->slots);
                    fflush(state->log);
                }

                break;
            }

            j++;
        }
    }

    // Log randomness for replication.
    if (state->log)
    {
        fprintf(state->log, "%u,%u\n", state->slots, state->rand);
        fflush(state->log);
    }
}

// Do a single "move" on the game board.
int do_move(struct game *state, enum move move)
{
    // Move Part 1: Eliminate tiles in the move direction.
    bool changed = false;

    if (DEBUG_LOG)
    {
        fprintf(state->log, "Move %u...\n", move);
        fflush(state->log);
    }

    for (int i = 0; i < BOARD_SIZE; i++)
    {
        for (int j = 0; j < (BOARD_SIZE - 1); j++)
        {
            // We want a 'plain' pointer here, since we need to flip indicies in some cases.
            uint32_t *board = (uint32_t *)state->board;

            // Moving down and right will need to access the board in the opposite direction.
            int flip = (move == MOVE_DOWN || move == MOVE_RIGHT) ? 1 : 0;
            int mod = (flip ? ((BOARD_SIZE - 1) - j) : j); // Modified inner index

            // Based on the move being made, we either move horizontally or vertically through the board.
            int dir = (move == MOVE_UP || move == MOVE_DOWN) ? 1 : 0;

            // Compute the tile we're operating on as follows:
            // MOVE_UP:    board[j    ][i    ]
            // MOVE_DOWN:  board[N - j][i    ]
            // MOVE_LEFT:  board[i    ][j    ]
            // MOVE_RIGHT: board[i    ][N - j]
            uint32_t idx = ((BOARD_SIZE * (dir ? mod : i)) + (dir ? i : mod));
            uint32_t val = board[idx];

            // We're going to loop (u/d/l/r) over tiles from the initial one.
            int k = mod + (flip ? (-1) : (+1));

            while (true)
            {
                if (flip ? (k < 0) : (k >= BOARD_SIZE))
                {
                    // Always stop when we reach the edge of the board.
                    break;
                }

                // Similar to picking `idx`, start at the next index in the proper direction.
                uint32_t nidx = (BOARD_SIZE * (dir ? k : i)) + (dir ? i : k);
                uint32_t next = board[nidx];

                if (next)
                {
                    if (val && (next == val)) {
                        // Matched value. Double.
                        board[idx] <<= 1;
                        board[nidx] = 0;

                        changed = true;
                        state->slots++;

                        break;
                    } else if (!val) {
                        // Nothing is currently, here, move.
                        board[idx] = next;
                        board[nidx] = 0;

                        // We need to try again since merges can happen 'accross' empty tile moves.
                        changed = true;
                        val = next;
                        continue;
                    } else {
                        // We hit a thing we can't change.
                        break;
                    }
                }

                // Move in the right direction.
                k += (flip ? (-1) : (+1));
            }
        }
    }

    if (DEBUG_LOG)
    {
        fprintf(state->log, "Changed: %c; slots: %u\n", changed ? 'y' : 'n', state->slots);
        fflush(state->log);
    }

    // If nothing changed, this is not a valid move.
    if (!changed) { return 1; }
    state->move = move;

    // Log the last move made.
    if (state->log) { fprintf(state->log, "%u\n", state->move); }

    // Move Part 2: Pick a new tile and location to spawn.
    spawn_tile(state);

    return 0;
}

int main(int argc, const char *const *argv)
{
    FILE *log = fopen("log.txt", "wb");

    if (init_curses())
    {
        fprintf(stderr, "Failed to initialize curses library!\n");
        return 1;
    }

    // Shared game state
    struct game game_state;
    init_game(&game_state, log);

    // Does the board need redrawn?
    bool dirty = true;

    // Input character.
    char c = 0;

    do {
        // Accept characters in any case.
        c = c | 0x20;

        if (DEBUG_LOG)
        {
            fprintf(game_state.log, "read '%c', move '%d' (%c)\n", c, MOVE_MAP[c], IS_MOVE(c) ? 'y' : 'n');
            fflush(game_state.log);
        }

        // Only take action if c is a valid move.
        if (IS_MOVE(c))
        {
            if (do_move(&game_state, MOVE_MAP[c])) {
                // Invalid move.
            } else {
                // Valid move.
                dirty = true;
            }
        }

        // Redraw if necessary
        if (dirty)
        {
            clear();
            display_board(&game_state, 5, 3);

            dirty = false;
        }

        if (c == 'q') { break; }
    } while ((c = getch()) != ERR);

    fclose(log);
    endwin();

    return 0;
}
