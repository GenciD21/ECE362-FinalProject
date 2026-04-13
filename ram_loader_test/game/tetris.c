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
#define STATUS_X 20
#define STATUS_TITLE_Y 20
#define STATUS_LAST_KEY_Y 70
#define STATUS_TICK_Y 120
#define TICK_ALARM_NUM 2
#define TICK_INTERVAL_US 250000

static volatile uint32_t pending_ticks = 0;

static void tick_alarm_callback(uint alarm_num) {
    (void)alarm_num;
    pending_ticks++;
    hardware_alarm_set_target(
        TICK_ALARM_NUM,
        delayed_by_us(get_absolute_time(), TICK_INTERVAL_US)
    );
}

static void init_tick_timer(void) {
    hardware_alarm_claim(TICK_ALARM_NUM);
    hardware_alarm_set_callback(TICK_ALARM_NUM, tick_alarm_callback);
    hardware_alarm_set_target(
        TICK_ALARM_NUM,
        delayed_by_us(get_absolute_time(), TICK_INTERVAL_US)
    );
}

static void draw_static_ui(void) {
    LCD_Clear(BLACK);
    LCD_DrawString(STATUS_X, STATUS_TITLE_Y, WHITE, BLACK, "Keypad Test", 16, 0);
    LCD_DrawString(STATUS_X, 45, WHITE, BLACK, "Last pressed:", 16, 0);
    LCD_DrawString(STATUS_X, 95, WHITE, BLACK, "Tick count:", 16, 0);
}

static void draw_last_key(char key_char) {
    char buf[32];
    sprintf(buf, "Key: %c  ", key_char);
    LCD_DrawString(STATUS_X, STATUS_LAST_KEY_Y, GREEN, BLACK, buf, 16, 0);
}

static void draw_tick_count(uint32_t tick_count) {
    char buf[32];
    sprintf(buf, "Tick: %lu    ", (unsigned long)tick_count);
    LCD_DrawString(STATUS_X, STATUS_TICK_Y, CYAN, BLACK, buf, 16, 0);
}

static bool poll_keypad(char *last_pressed) {
    uint16_t event;
    bool updated = false;

    while (key_try_pop(&event)) {
        bool pressed = ((event >> 8) & 1u) != 0;
        char key_char = (char)(event & 0xFF);

        if (pressed) {
            *last_pressed = key_char;
            updated = true;
        }
    }

    return updated;
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

int main() {
    char last_pressed = '-';
    uint32_t tick_count = 0;

    init_spi_lcd();
    LCD_Setup();
    keypad_init_pins();
    keypad_init_timer();
    init_tick_timer();

    draw_static_ui();
    draw_last_key(last_pressed);
    draw_tick_count(tick_count);

    while (true) {
        if (poll_keypad(&last_pressed)) {
            draw_last_key(last_pressed);
        }

        if (pending_ticks != 0) {
            uint32_t ticks_to_process = pending_ticks;
            pending_ticks = 0;
            tick_count += ticks_to_process;
            draw_tick_count(tick_count);
        }

        tight_loop_contents();
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
