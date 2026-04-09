/**
 * demo_game.c - A simple demo "game" that runs on core 1.
 *
 * This program is compiled as a standalone binary, loaded into SRAM
 * at 0x20040000 by the loader, and executed on core 1.
 *
 * It uses pico-sdk functions normally (GPIO, sleep, etc.) since
 * the hardware is already initialized by the loader on core 0.
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"

/* Pins */
#define LED_PIN 23
#define BUTTON_PIN 21

/* Shared state between the interrupt handler and main loop */
/* Shared state between the interrupt handler and main loop */
volatile bool is_blinking = true;
volatile uint64_t last_button_press_time = 0;

/* Standard Pico SDK GPIO interrupt callback */
void button_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN) {
        // Simple 100ms debounce
        uint64_t current_time = time_us_64();
        if (current_time - last_button_press_time > 100000) {
            // Toggle the blink state! 
            is_blinking = !is_blinking;
            last_button_press_time = current_time;
        }
    }
}

int main() {
    /* Initialize the LED */
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /* Initialize the Button as Active High */
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_PIN); // Pull down so it sits at 0, goes to 1 when pressed

    /* Enable SDK Interrupts for the button (trigger on rise to 3.3V) */
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_RISE, true, &button_callback);

    /* Main Game Loop */
    while (true) {
        if (is_blinking) {
            gpio_put(LED_PIN, 1);
            sleep_ms(250);
            gpio_put(LED_PIN, 0);
            sleep_ms(250);
        } else {
            // Ensure LED is off and wait a tiny bit to check again
            gpio_put(LED_PIN, 0);
            sleep_ms(50);
        }
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
