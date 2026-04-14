/**
 * tetris.c
 *
 * This program is compiled as a standalone binary, loaded into SRAM
 * at 0x20040000 by the loader, and executed on core 1.
 *
 * It uses pico-sdk functions normally (GPIO, sleep, etc.) since
 * the hardware is already initialized by the loader on core 0.
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "lcd.h"
#include "keypad.h"
#include <stdio.h>

//TFT, using SPI1
#define PIN_CS 29
#define PIN_nRESET 32
#define PIN_DC 33
#define PIN_SDI 31
#define PIN_SCK 30
#define STATUS_X 185
#define STATUS_KEY_Y 10
#define STATUS_TICK_Y 30
#define STATUS_TITLE_Y 50

//rendering 
static const uint16_t colors[] = {
    0x0000, // 0 = empty (black)
    0x07FF, // 1 = I (cyan)
    0x001F, // 2 = J (blue)
    0xFD20, // 3 = L (orange)
    0xFFE0, // 4 = O (yellow)
    0x07E0, // 5 = S (green)
    0xF81F, // 6 = T (purple)
    0xF800, // 7 = Z (red)
    0x4208, // 8 = border (dark gray)
};

#define GRID_W 10
#define GRID_H 20
#define TILE_SIZE 12
#define OFFSET_X 60
#define OFFSET_Y 0

//rendering state and updated rendering flags
uint8_t grid[GRID_H][GRID_W];
bool dirty[GRID_H][GRID_W];

//locked cells (pieces that have landed)
uint8_t locked[GRID_H][GRID_W];

//all 7 tetromino shapes with 4 rotations each
//each rotation is 4 blocks of {row, col} offsets
//ordered to match color table: I=1, J=2, L=3, O=4, S=5, T=6, Z=7
#define NUM_PIECES 7
#define NUM_ROTATIONS 4
#define PIECE_BLOCKS 4
//rotations use (r,c)->(c,N-1-r) so 4 rotations returns to start
//I uses 4x4 box, J/L/S/T/Z use 3x3 box, O uses 2x2 box
static const int pieces[NUM_PIECES][NUM_ROTATIONS][PIECE_BLOCKS][2] = {
    // I
    {{{1,0},{1,1},{1,2},{1,3}},
     {{0,2},{1,2},{2,2},{3,2}},
     {{2,0},{2,1},{2,2},{2,3}},
     {{0,1},{1,1},{2,1},{3,1}}},
    // J
    {{{0,0},{1,0},{1,1},{1,2}},
     {{0,1},{0,2},{1,1},{2,1}},
     {{1,0},{1,1},{1,2},{2,2}},
     {{0,1},{1,1},{2,0},{2,1}}},
    // L
    {{{0,2},{1,0},{1,1},{1,2}},
     {{0,1},{1,1},{2,1},{2,2}},
     {{1,0},{1,1},{1,2},{2,0}},
     {{0,0},{0,1},{1,1},{2,1}}},
    // O
    {{{0,0},{0,1},{1,0},{1,1}},
     {{0,0},{0,1},{1,0},{1,1}},
     {{0,0},{0,1},{1,0},{1,1}},
     {{0,0},{0,1},{1,0},{1,1}}},
    // S
    {{{0,1},{0,2},{1,0},{1,1}},
     {{0,1},{1,1},{1,2},{2,2}},
     {{1,1},{1,2},{2,0},{2,1}},
     {{0,0},{1,0},{1,1},{2,1}}},
    // T
    {{{0,1},{1,0},{1,1},{1,2}},
     {{0,1},{1,1},{1,2},{2,1}},
     {{1,0},{1,1},{1,2},{2,1}},
     {{0,1},{1,0},{1,1},{2,1}}},
    // Z
    {{{0,0},{0,1},{1,1},{1,2}},
     {{0,2},{1,1},{1,2},{2,1}},
     {{1,0},{1,1},{2,1},{2,2}},
     {{0,1},{1,0},{1,1},{2,0}}},
};

//simple random number generator
uint32_t rng_state = 1;

static int random_piece(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return ((rng_state >> 16) & 0x7FFF) % NUM_PIECES;
}

//current piece state
int piece_type = 0;
int piece_rot = 0;
int piece_row = 0;
int piece_col = 3;

//game state
bool game_over = false;
uint32_t score = 0;

//variable tick rate
uint32_t GAME_TICK_US = 500000;

uint32_t tick_count = 0;
absolute_time_t next_tick;

//rendering functions
static void draw_tile(int row, int col) {
    uint16_t x = OFFSET_X + col * TILE_SIZE;
    uint16_t y = OFFSET_Y + row * TILE_SIZE;
    uint16_t color = colors[grid[row][col]];
    LCD_DrawFillRectangle(x, y, x + TILE_SIZE - 1, y + TILE_SIZE - 1, color);
}

static void set_tile(int row, int col, uint8_t val) {
    if (grid[row][col] != val) {
        grid[row][col] = val;
        dirty[row][col] = true;
    }
}

//render only what is changed
static void render(void) {
    for (int r = 0; r < GRID_H; r++) {
        for (int c = 0; c < GRID_W; c++) {
            if (dirty[r][c]) {
                draw_tile(r, c);
                dirty[r][c] = false;
            }
        }
    }
}

static void draw_score(void) {
    char buf[20];
    sprintf(buf, "Score:%lu   ", (unsigned long)score);
    uint16_t y = OFFSET_Y + GRID_H * TILE_SIZE + TILE_SIZE + 4;
    LCD_DrawString(OFFSET_X, y, 0xFFFF, 0x0000, buf, 16, 0);
}

static void draw_game_over(void) {
    LCD_DrawString(STATUS_X, STATUS_KEY_Y, 0xF800, 0x0000, "GOON", 16, 0);
    LCD_DrawString(STATUS_X, STATUS_TICK_Y, 0xF800, 0x0000, "BOON", 16, 0);
}

void init_spi_lcd() {
    gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_DC, GPIO_FUNC_SIO);
    gpio_set_function(PIN_nRESET, GPIO_FUNC_SIO);

    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_nRESET, GPIO_OUT);

    gpio_put(PIN_CS, 1);
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_nRESET, 1);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SDI, GPIO_FUNC_SPI);
    spi_init(spi1, 30 * 1000 * 1000);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

static void init_board(void) {
    LCD_Clear(0x0000);

    // Draw left and right borders (1 tile wide each)
    for (int r = 0; r < GRID_H; r++) {
        LCD_DrawFillRectangle(
            OFFSET_X - TILE_SIZE, OFFSET_Y + r * TILE_SIZE,
            OFFSET_X - 1, OFFSET_Y + r * TILE_SIZE + TILE_SIZE - 1,
            colors[8]);
        LCD_DrawFillRectangle(
            OFFSET_X + GRID_W * TILE_SIZE, OFFSET_Y + r * TILE_SIZE,
            OFFSET_X + GRID_W * TILE_SIZE + TILE_SIZE - 1, OFFSET_Y + r * TILE_SIZE + TILE_SIZE - 1,
            colors[8]);
    }

    // Draw bottom border
    LCD_DrawFillRectangle(
        OFFSET_X - TILE_SIZE, OFFSET_Y + GRID_H * TILE_SIZE,
        OFFSET_X + GRID_W * TILE_SIZE + TILE_SIZE - 1, OFFSET_Y + GRID_H * TILE_SIZE + TILE_SIZE - 1,
        colors[8]);

    // Label
    LCD_DrawString(STATUS_X, STATUS_TITLE_Y, 0xFFFF, 0x0000, "TETRIS", 16, 0);

    for (int r = 0; r < GRID_H; r++) {
        for (int c = 0; c < GRID_W; c++) {
            set_tile(r, c, 0);
            dirty[r][c] = true;
        }
    }
    render();
}

static void init_system(void) {
    init_spi_lcd();
    LCD_Setup();
    keypad_init_pins();
    keypad_init_timer();

    next_tick = delayed_by_us(get_absolute_time(), GAME_TICK_US);

    LCD_Clear(BLACK);
    init_board();
}

//remove current piece from grid
static void erase_piece(void) {
    for (int i = 0; i < PIECE_BLOCKS; i++) {
        int r = piece_row + pieces[piece_type][piece_rot][i][0];
        int c = piece_col + pieces[piece_type][piece_rot][i][1];
        if (r >= 0 && r < GRID_H && c >= 0 && c < GRID_W) {
            set_tile(r, c, 0);
        }
    }
}

//place current piece on grid
static void place_piece(void) {
    for (int i = 0; i < PIECE_BLOCKS; i++) {
        int r = piece_row + pieces[piece_type][piece_rot][i][0];
        int c = piece_col + pieces[piece_type][piece_rot][i][1];
        if (r >= 0 && r < GRID_H && c >= 0 && c < GRID_W) {
            set_tile(r, c, (piece_type + 1));
        }
    }
}

//check if piece can exist at given row, col, rotation
static bool piece_fits_at(int row, int col, int rot) {
    for (int i = 0; i < PIECE_BLOCKS; i++) {
        int r = row + pieces[piece_type][rot][i][0];
        int c = col + pieces[piece_type][rot][i][1];
        if (r < 0 || r >= GRID_H || c < 0 || c >= GRID_W) {
            return false;
        }
        if (locked[r][c] != 0) {
            return false;
        }
    }
    return true;
}

static bool piece_fits(int row, int col) {
    return piece_fits_at(row, col, piece_rot);
}

//copy piece into locked grid permanently
static void lock_piece(void) {
    for (int i = 0; i < PIECE_BLOCKS; i++) {
        int r = piece_row + pieces[piece_type][piece_rot][i][0];
        int c = piece_col + pieces[piece_type][piece_rot][i][1];
        if (r >= 0 && r < GRID_H && c >= 0 && c < GRID_W) {
            locked[r][c] = (piece_type + 1);
        }
    }
}

//check if a row is completely full
static bool row_full(int row) {
    for (int c = 0; c < GRID_W; c++) {
        if (locked[row][c] == 0) return false;
    }
    return true;
}

//shift everything above row down by one
static void remove_row(int row) {
    for (int r = row; r > 0; r--) {
        for (int c = 0; c < GRID_W; c++) {
            locked[r][c] = locked[r - 1][c];
            set_tile(r, c, locked[r][c]);
        }
    }
    for (int c = 0; c < GRID_W; c++) {
        locked[0][c] = 0;
        set_tile(0, c, 0);
    }
}

//clear all full rows and return how many were cleared
static int clear_lines(void) {
    int cleared = 0;
    for (int r = GRID_H - 1; r >= 0; r--) {
        if (row_full(r)) {
            remove_row(r);
            cleared++;
            r++; //recheck this row since rows shifted down
        }
    }
    return cleared;
}

//speed up as score increases, minimum 100ms
static void update_speed(void) {
    if (score < 500)       GAME_TICK_US = 500000;
    else if (score < 1000) GAME_TICK_US = 400000;
    else if (score < 2000) GAME_TICK_US = 300000;
    else if (score < 3000) GAME_TICK_US = 200000;
    else                   GAME_TICK_US = 100000;
}

//add score based on lines cleared (classic scoring)
static void add_score(int lines) {
    int points[] = {0, 100, 300, 500, 800};
    if (lines > 0 && lines <= 4) {
        score += points[lines];
        update_speed();
    }
}

//spawn a random piece at top center
static void spawn_piece(void) {
    piece_type = random_piece();
    piece_rot = 0;
    piece_row = 0;
    piece_col = 3;
    if (!piece_fits(piece_row, piece_col)) {
        game_over = true;
        draw_game_over();
        return;
    }
    place_piece();
}

//try to rotate piece clockwise
static void try_rotate(void) {
    int new_rot = (piece_rot + 1) % NUM_ROTATIONS;
    erase_piece();
    if (piece_fits_at(piece_row, piece_col, new_rot)) {
        piece_rot = new_rot;
    }
    place_piece();
    render();
}

//drop piece to lowest valid position and lock it
static void hard_drop(void) {
    erase_piece();
    while (piece_fits(piece_row + 1, piece_col)) {
        piece_row++;
    }
    place_piece();
    lock_piece();
    int lines = clear_lines();
    add_score(lines);
    draw_score();
    spawn_piece();
    render();
}

//try to move piece by dr rows and dc columns
static void try_move(int dr, int dc) {
    erase_piece();
    if (piece_fits(piece_row + dr, piece_col + dc)) {
        piece_row += dr;
        piece_col += dc;
    }
    place_piece();
    render();
}

static void handle_input(char input) {
    if (game_over) return;
    if (input == '4') {
        try_move(0, -1); //left
    } else if (input == '6') {
        try_move(0, 1);  //right
    } else if (input == '5') {
        try_rotate();     //rotate
    } else if (input == '2') {
        hard_drop();      //hard drop
    }
}

static void tick_game(void) {
    if (game_over) return;
    tick_count++;

    //try to move piece down
    erase_piece();
    if (piece_fits(piece_row + 1, piece_col)) {
        piece_row++;
        place_piece();
    } else {
        //can't move down, lock and spawn new piece
        place_piece();
        lock_piece();
        int lines = clear_lines();
        add_score(lines);
        draw_score();
        spawn_piece();
    }

    render();
}

int main() {
    char last_pressed = '-';

    init_system();
    rng_state = time_us_32();
    spawn_piece();
    render();
    draw_score();

    while (true) {
        uint16_t event;

        while (key_try_pop(&event)) {
            if ((event >> 8) & 1u) {
                last_pressed = (char)(event & 0xFF);
                handle_input(last_pressed);
            }
        }

        if (time_reached(next_tick)) {
            next_tick = delayed_by_us(get_absolute_time(), GAME_TICK_US);
            tick_game();
        }

        busy_wait_us_32(500);
    }

    return 0;
}
/* -------------------------------------------------------------
 * CORE 1 OS/SDK BOOTSTRAP
 * ------------------------------------------------------------- */

/* Declare the SDK's internal hardware initialization function */
extern void runtime_init(void);

void core1_startup(void) {
    extern uint32_t __bss_start__[];
    extern uint32_t __bss_end__[];
    
    // 1. Clear BSS 
    for (uint32_t *p = __bss_start__; p < __bss_end__; p++) {
        *p = 0;
    }

    // 2. Initialize the Pico SDK! This sets up hardware timers, 
    //    spinlocks, and the RAM vector table for IRQs.
    runtime_init();

    // 3. Launch the game
    main();
    
    while (true) {
        __asm("wfe");
    }
}

/* 
 * Embed a magic header so the Loader can find our core1_startup 
 * without needing ELF symbols. "GAME" = 0x454D4147 in memory.
 */
struct game_header {
    uint32_t magic;
    void (*entry)(void);
};

__attribute__((section(".embedded_block"), used))
const struct game_header my_header = {
    0x454D4147,
    core1_startup
};
