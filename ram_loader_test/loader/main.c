#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <string.h>
#include "pico/multicore.h"
#include "sdcard.h"

/**SPI SD CARD****************************************************************/
#define spi0 ((spi_inst_t *)spi0_hw)
#define SD_MISO 4 
#define SD_CS 5
#define SD_SCK 6
#define SD_MOSI 3 
/*******************************************************************/

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

void init_spi_sdcard() {

   gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
   gpio_set_function(SD_SCK, GPIO_FUNC_SPI);
   gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);

   gpio_init(SD_CS);
   gpio_set_dir(SD_CS, GPIO_OUT);
   gpio_put(SD_CS, 1); 
   spi_init(spi0, 1000 * 400);
   
   spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

}

void disable_sdcard() {

    
    gpio_put(SD_CS, 1);

    uint8_t data = 0xFF;
    spi_write_blocking(spi0, &data, 1);
    gpio_init(SD_MOSI);
    gpio_set_dir(SD_MOSI, GPIO_OUT);
    gpio_put(SD_MOSI, 1);
}

void enable_sdcard() {

    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    gpio_put(SD_CS, 0);
    
}

void sdcard_io_high_speed() {
    spi_init(spi0, 1000000 * 12);
}

void init_sdcard_io() {
    
    init_spi_sdcard();

    uint8_t dummy = 0xFF;
    for (int i = 0; i < 10; i++) {
        spi_write_blocking(spi0, &dummy, 1);
    }
    
    disable_sdcard();
}

/*******************************************************************/


void date(int argc, char *argv[]);
void command_shell();

// Picture* load_image(const char* image_data);
// void free_image(Picture* pic);
#define GAME_BIN_FILE "DEMO_G~1.BIN"


int main() {
    // Initialize the standard input/output library

    stdio_init_all();
    // init_pio_inputs();

    // run_spi(); //coment this to test the PIO because it does that infinite animation


    init_sdcard_io();
    
    // SD card functions will initialize everything.


    // char *args_ls[] = {NULL, '/'};
    // ls(2, args_ls);

     /* Let's PROVE Core 0 is alive by blinking the LED explicitly */
      char *args[] = {NULL, GAME_BIN_FILE};
    mount(2, args);
    const uint LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    for (int i = 0; i < 5; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }

    sleep_ms(1000);

    printf("\n");
    printf("===========================================\n");
    printf("  RP2350 Game Loader v0.1\n");
    printf("===========================================\n");
    printf("[LOADER] Running on core %d\n", get_core_num());

    //uint8_t *get_point(const char *path, uint32_t size) 

    uint32_t file_size_bytes = byte_size(2, args);
    uint8_t *  game_pointer = get_point(GAME_BIN_FILE, file_size_bytes);

    printf("Pointer: %p\n", game_pointer);
    printf("File size: %lu\n", (unsigned long)file_size_bytes);
    printf("[LOADER] Game binary size: %lu bytes\n", file_size_bytes);


    /* Load and launch the embedded demo game */
    if (!load_and_launch_game(game_pointer, file_size_bytes)) {
        printf("[LOADER] Failed to launch game!\n");
        while (true) {
            gpio_put(LED_PIN, 1);
            sleep_ms(2000);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
        }
    }

    // command_shell();

    printf("[LOADER] Entering monitor loop...\n");
    while (true) {
        sleep_ms(1000);
    }
}