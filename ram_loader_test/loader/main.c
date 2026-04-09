/**
 * loader/main.c - Core 0 loader program for RP2350
 *
 * This program runs on core 0 and:
 * 1. Copies a game binary into the upper SRAM region (0x20040000)
 * 2. Launches core 1 to execute the loaded game
 * 3. Monitors core 1 via the inter-core FIFO
 *
 * In the future, the embedded game binary will be replaced with
 * SD card reads to dynamically load different games.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

/* Include the game binary as a C array (generated at build time) */
#include "demo_game_bin.h"

/* Memory layout constants */
#define GAME_LOAD_ADDR    ((void *)0x20040000)
#define CORE1_STACK_TOP   ((uint32_t *)0x20080000)  /* Top of 16KB stack region */
#define CORE1_VTOR_ADDR   0x20040000                /* Vector table at start of game binary */

/**
 * Load a game binary into SRAM and launch it on core 1.
 *
 * @param bin      Pointer to the game binary data
 * @param bin_len  Length of the binary in bytes
 * @return true if launched successfully
 */
static bool load_and_launch_game(const uint8_t *bin, uint32_t bin_len) {
    /* Validate binary size */
    if (bin_len > (240 * 1024)) {
        printf("[LOADER] ERROR: Game binary too large (%lu bytes, max 245760)\n",
               (unsigned long)bin_len);
        return false;
    }

    /* Validate binary has at least a vector table (SP + Reset) */
    if (bin_len < 8) {
        printf("[LOADER] ERROR: Game binary too small (%lu bytes)\n",
               (unsigned long)bin_len);
        return false;
    }

    printf("[LOADER] Copying %lu bytes to 0x%08X...\n",
           (unsigned long)bin_len, (unsigned int)(uintptr_t)GAME_LOAD_ADDR);

    /* Copy game binary to the target SRAM region */
    memcpy(GAME_LOAD_ADDR, bin, bin_len);

    /* Memory barriers to ensure all writes are visible before core 1 starts */
    __dmb();
    __isb();

    /* Read the initial Stack Pointer from the vector table */
    uint32_t *vtor = (uint32_t *)GAME_LOAD_ADDR;

    /* Scan the first 4KB for our magic header (0x454D4147 = 'GAME') to find 
     * the actual custom startup function. We do this to bypass the pico-sdk 
     * crt0.S which forces Core 1 to sleep! */
    void (*game_entry)(void) = NULL;
    for (uint32_t i = 0; i < 1024; i++) {
        if (vtor[i] == 0x454D4147) {
            game_entry = (void (*)(void))vtor[i + 1];
            break;
        }
    }

    if (!game_entry) {
        printf("[LOADER] ERROR: Could not find 'GAME' magic header!\n");
        return false;
    }

    printf("[LOADER] Game vector table at 0x%08X\n", CORE1_VTOR_ADDR);
    printf("[LOADER] Game initial SP:    0x%08X\n", vtor[0]);
    printf("[LOADER] Game custom entry:  0x%08X\n", (unsigned int)(uintptr_t)game_entry);
    printf("[LOADER] Core 1 stack top:   0x%08X\n", (unsigned int)(uintptr_t)CORE1_STACK_TOP);

    /* Reset core 1 to a known state before launching */
    multicore_reset_core1();
    sleep_ms(10);  /* Brief delay to let reset complete */

    /* Launch core 1 with:
     *   - entry: our custom game_entry that bypasses crt0.S CPUID blocks
     *   - sp:    top of the dedicated core 1 stack
     *   - vtor:  the game's vector table for interrupts */
    multicore_launch_core1_raw(game_entry, CORE1_STACK_TOP, CORE1_VTOR_ADDR);

    printf("[LOADER] Core 1 launched!\n");
    return true;
}

int main() {
    /* Initialize stdio for debug output over USB serial */
    stdio_init_all();

    /* Let's PROVE Core 0 is alive by blinking the LED explicitly */
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    for (int i = 0; i < 5; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }

    /* Wait for USB serial connection (optional, but helpful for debugging) */
    sleep_ms(1000);

    printf("\n");
    printf("===========================================\n");
    printf("  RP2350 Game Loader v0.1\n");
    printf("===========================================\n");
    printf("[LOADER] Running on core %d\n", get_core_num());
    printf("[LOADER] Game binary size: %u bytes\n", demo_game_bin_len);

    /* Load and launch the embedded demo game */
    if (!load_and_launch_game(demo_game_bin, demo_game_bin_len)) {
        printf("[LOADER] Failed to launch game!\n");
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(2000);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
        }
    }

    /* Main loop - Core 0 stops touching the LED so Core 1 can have it! */
    printf("[LOADER] Entering monitor loop...\n");
    while (true) {
        sleep_ms(1000);
    }

    return 0;
}
