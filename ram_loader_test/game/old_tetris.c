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
#include "hardware/irq.h"
#include "lcd.h"
#include "keypad.h"
#include <stdio.h>

//TFT, using SPI1
#define PIN_CS 29
#define PIN_nRESET 32
#define PIN_DC 33
#define PIN_SDI 31
#define PIN_SCK 30


//game specific rendering values
#define GRID_W 10
#define GRID_H 20
#define TILE_SIZE 12  // 12px tiles -> 120x240 play area, fits nicely
#define OFFSET_X 60   // center horizontally: (240 - 120) / 2
#define OFFSET_Y 0    // top of screen

// Board state: each cell holds a color index (0 = empty)
uint8_t grid[GRID_H][GRID_W];
bool dirty[GRID_H][GRID_W];

// Color lookup table
const uint16_t piece_colors[] = {
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

// Game tick rate in microseconds
uint32_t GAME_TICK_US = 500000;

static const uint GAME_TICK_ALARM = 2;

static volatile uint32_t tick_count = 0;
static volatile bool tick_pending = false;
static volatile char key_pending = 0;

// Test piece position
static int test_row = 0;

/***********************************************
 * Rendering
 ***********************************************/
void draw_cell(int row, int col) {
    uint16_t x = OFFSET_X + col * TILE_SIZE;
    uint16_t y = OFFSET_Y + row * TILE_SIZE;
    uint16_t color = piece_colors[grid[row][col]];
    LCD_DrawFillRectangle(x, y, x + TILE_SIZE - 1, y + TILE_SIZE - 1, color);
}

void render() {
    for (int r = 0; r < GRID_H; r++) {
        for (int c = 0; c < GRID_W; c++) {
            if (dirty[r][c]) {
                draw_cell(r, c);
                dirty[r][c] = false;
            }
        }
    }
}

void set_cell(int row, int col, uint8_t val) {
    if (grid[row][col] != val) {
        grid[row][col] = val;
        dirty[row][col] = true;
    }
}

/***********************************************
 * Input handler
 ***********************************************/
void on_input(char key, bool pressed) {
    if (!pressed) return; // only act on key down
    key_pending = key;
}

/***********************************************
 * Game tick callback — just sets the flag and rearms alarm 2
 ***********************************************/
static void game_tick_callback(uint alarm_num) {
    (void)alarm_num;
    tick_pending = true;
    hardware_alarm_set_target(
        GAME_TICK_ALARM,
        delayed_by_us(get_absolute_time(), GAME_TICK_US)
    );
}

/***********************************************
 * Game update — move test piece down each tick
 ***********************************************/
void game_update() {
    //calcaualte game logic

    // Render only dirty cells
    render();

    // Update tick counter on sidebar
    char buf[20];
    sprintf(buf, "T:%lu   ", (unsigned long)tick_count);
    LCD_DrawString(185, 30, 0xFFFF, 0x0000, buf, 16, 0);
    tick_count++;
}

/***********************************************
 * Draw the initial scene: border + empty grid
 ***********************************************/
void draw_initial_scene() {
    LCD_Clear(0x0000);

    // Draw left and right borders (1 tile wide each)
    for (int r = 0; r < GRID_H; r++) {
        LCD_DrawFillRectangle(
            OFFSET_X - TILE_SIZE, OFFSET_Y + r * TILE_SIZE,
            OFFSET_X - 1, OFFSET_Y + r * TILE_SIZE + TILE_SIZE - 1,
            piece_colors[8]);
        LCD_DrawFillRectangle(
            OFFSET_X + GRID_W * TILE_SIZE, OFFSET_Y + r * TILE_SIZE,
            OFFSET_X + GRID_W * TILE_SIZE + TILE_SIZE - 1, OFFSET_Y + r * TILE_SIZE + TILE_SIZE - 1,
            piece_colors[8]);
    }

    // Draw bottom border
    LCD_DrawFillRectangle(
        OFFSET_X - TILE_SIZE, OFFSET_Y + GRID_H * TILE_SIZE,
        OFFSET_X + GRID_W * TILE_SIZE + TILE_SIZE - 1, OFFSET_Y + GRID_H * TILE_SIZE + TILE_SIZE - 1,
        piece_colors[8]);

    // Label
    LCD_DrawString(185, 50, 0xFFFF, 0x0000, "TETRIS", 16, 0);
    LCD_DrawString(185, 30, 0xFFFF, 0x0000, "T:0   ", 16, 0);

    // Mark entire grid dirty so first render paints it
    for (int r = 0; r < GRID_H; r++)
        for (int c = 0; c < GRID_W; c++)
            dirty[r][c] = true;
    render();
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

void init_game_timer() {
    // Use hardware alarm 2 so the SDK owns the IRQ plumbing on core 1.
    hardware_alarm_claim(GAME_TICK_ALARM);
    hardware_alarm_set_callback(GAME_TICK_ALARM, game_tick_callback);
    hardware_alarm_set_target(
        GAME_TICK_ALARM,
        delayed_by_us(get_absolute_time(), GAME_TICK_US)
    );
}

int main() {
    init_spi_lcd();
    LCD_Setup();

    keypad_init_pins();
    keypad_init_timer();

    // Draw the playfield
    draw_initial_scene();

    // Start the game tick timer
    init_game_timer();

    while (true) {
        uint16_t key;
        while ((key = key_pop()) != 0) {
            bool is_pressed = (key >> 8) & 1;
            char k_char = (char)(key & 0xFF);
            on_input(k_char, is_pressed);
        }

        if (key_pending) {
            char debug_str[30];
            sprintf(debug_str, "Key: %c  ", key_pending);
            LCD_DrawString(185, 10, 0x07E0, 0x0000, debug_str, 16, 0);
            key_pending = 0;
        }

        if (tick_pending) {
            LCD_Clear(0x0000);
            tick_pending = false;
            game_update();
        }

        __asm volatile ("wfe");
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
