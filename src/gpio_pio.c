#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "gpio6_in.pio.h"

#include <stdlib.h>

PIO pio;
uint sm;
uint offset;

uint32_t pio_outputs;
#define BASE_PIN 27
#define RANGE 6

int init_pio_inputs()
{

    for (int i = 0; i < RANGE; i++) 
    {
        gpio_init(BASE_PIN + i);
        gpio_set_dir(BASE_PIN + i, GPIO_IN);
        gpio_set_function(BASE_PIN + i, GPIO_FUNC_PIO0);
    }

    for (int i = 0; i < RANGE; i++) { gpio_pull_up(BASE_PIN + i); }
    //Remove this line if you want to check actuall pins

    bool valid = pio_claim_free_sm_and_add_program_for_gpio_range(&gpio6_in_program, &pio, &sm, &offset, BASE_PIN, RANGE, true);

    if(!valid)
    {
        return EXIT_FAILURE;
    }

    pio_sm_config config = gpio6_in_program_get_default_config(offset);
    sm_config_set_in_pins(&config, BASE_PIN);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&config, 1.0f);
    sm_config_set_in_shift(&config, false, true, 6);

    pio_sm_init(pio, sm, offset, &config);
    pio_sm_set_enabled(pio, sm, true);

    int chan = dma_claim_unused_channel(true);
    pio_sm_clear_fifos(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(
        chan,          // Channel to be configured
        &c,            // The configuration we just created
        &pio_outputs,           // The initial write address
        &pio->rxf[sm],           // The initial read address
        1024, // Number of transfers; in this case each is 1 byte.
        true           // Start immediately.
    );

    printf("Succesful\n");
    
    return EXIT_SUCCESS;
}

uint32_t get_buffer() //or extern
{
    uint32_t val = pio_sm_get_blocking(pio, sm);
    printf("%08x\n", val);

    // printf("%08x\n", pio_outputs);
    return pio_outputs & 0x003F;
}
