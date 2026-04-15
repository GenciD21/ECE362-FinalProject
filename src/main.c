#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <string.h>
#include "gpio_pio.h"
#include "hardware/pio.h"
#include "lcd.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
//#include <cstring>
#include "core1_bin.h"

/**SPI SD CARD****************************************************************/
#define spi1 ((spi_inst_t *)spi1_hw)
#define SD_MISO 12
#define SD_CS 17
#define SD_SCK 14
#define SD_MOSI 15 
/*******************************************************************/

//#define SDCARD
#define LINKER


void init_spi_sdcard() {

   gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
   gpio_set_function(SD_SCK, GPIO_FUNC_SPI);
   gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);

   gpio_init(SD_CS);
   gpio_set_dir(SD_CS, GPIO_OUT);
   gpio_put(SD_CS, 1); 
   spi_init(spi1, 1000 * 400);
   
   spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

}

void disable_sdcard() {

    
    gpio_put(SD_CS, 1);

    uint8_t data = 0xFF;
    spi_write_blocking(spi1, &data, 1);
    gpio_init(SD_MOSI);
    gpio_set_dir(SD_MOSI, GPIO_OUT);
    gpio_put(SD_MOSI, 1);
}

void enable_sdcard() {

    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    gpio_put(SD_CS, 0);
    
}

void sdcard_io_high_speed() {
    spi_init(spi1, 1000000 * 12);
}

void init_sdcard_io() {
    
    init_spi_sdcard();

    uint8_t dummy = 0xFF;
    for (int i = 0; i < 10; i++) {
        spi_write_blocking(spi1, &dummy, 1);
    }
    
    disable_sdcard();
}

/*******************************************************************/

void init_uart();
void init_uart_irq();
void date(int argc, char *argv[]);
void command_shell();

Picture* load_image(const char* image_data);
void free_image(Picture* pic);


/***************************************************************** */

// Must be RAM-resident and 256-byte aligned (Cortex-M33 VTOR)
static uint8_t __attribute__((aligned(256), section(".uninitialized_data")))
    core1_ram[sizeof(core1_bin)];

void start_core1() {
    memcpy(core1_ram, core1_bin, sizeof(core1_bin));
    __dmb();

    // ARM vector table: word 0 = initial SP, word 1 = reset handler
    uint32_t entry = ((uint32_t *)core1_ram)[1];
    multicore_launch_core1((void (*)(void))entry);
}


int main() {
    // Initialize the standard input/output library
    // init_uart();
    // init_uart_irq();
    stdio_init_all();
    #ifdef SDCARD
    init_pio_inputs();

    run_spi(); //coment this to test the PIO because it does that infinite animation


    // init_sdcard_io();
    
    // SD card functions will initialize everything.
    // command_shell();

    for(;;)
    {
         uint32_t output = get_buffer();
    //    printf("DMA: %08x\n", output);
        sleep_ms(500);
    };
    #endif 

    #ifdef LINKER
        stdio_init_all();
        start_core1();

        // core0 continues here while core1 runs independently
        while (true) {
            tight_loop_contents();
        }

    #endif
}