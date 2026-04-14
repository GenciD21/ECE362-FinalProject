#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <string.h>
#include "gpio_pio.h"
#include "hardware/pio.h"
#include "pico/multicore.h"
#include "sdcard.h"
#include "lcd.h"


/**SPI SD CARD****************************************************************/
#define spi0 ((spi_inst_t *)spi0_hw)
#define SD_MISO 4 
#define SD_CS 5
#define SD_SCK 6
#define SD_MOSI 3 
/*******************************************************************/


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

void init_uart();
void init_uart_irq();
void date(int argc, char *argv[]);
void command_shell();

Picture* load_image(const char* image_data);
void free_image(Picture* pic);
#define GAME_BIN_FILE "DEMO_G~5.BIN"


int main() {
    // Initialize the standard input/output library
    
    stdio_init_all();
    // init_pio_inputs();

    // run_spi(); //coment this to test the PIO because it does that infinite animation


    init_sdcard_io();
    
    // SD card functions will initialize everything.

    char *args[] = {NULL, GAME_BIN_FILE};

    mount(2, args);
    //uint8_t *get_point(const char *path, uint32_t size) 

    uint32_t file_size_bytes = byte_size(2, args);
    uint8_t *  game_pointer = get_point(GAME_BIN_FILE, file_size_bytes);

    printf("Pointer: %p\n", game_pointer);
    printf("File size: %lu\n", (unsigned long)file_size_bytes);

    for(;;)
    {
         uint32_t output = get_buffer();
    //    printf("DMA: %08x\n", output);
        sleep_ms(500);
    };
}