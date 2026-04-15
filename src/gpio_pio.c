#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "gpio6_in.pio.h"
#include <stdlib.h>
#include <stdio.h>

PIO pio;
uint sm;
uint offset;

volatile uint32_t pio_outputs;
static uint32_t single_transfer = 1;

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

    // for (int i = 0; i < RANGE; i++) { gpio_pull_up(BASE_PIN + i); }
    // Remove this line if you want to check actuall pins

    bool valid = pio_claim_free_sm_and_add_program_for_gpio_range(&gpio6_in_program, &pio, &sm, &offset, BASE_PIN, RANGE, true);

    if (!valid)
    {
        printf("Failed to load PIO program\n");
        return EXIT_FAILURE;
    }

    pio_sm_config config = gpio6_in_program_get_default_config(offset);
    sm_config_set_in_pins(&config, BASE_PIN);
    // sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_in_shift(&config, false, false, 6);
    sm_config_set_clkdiv(&config, 1.0f);
    

    pio_sm_init(pio, sm, offset, &config);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_set_enabled(pio, sm, true);

    int data_chan = dma_claim_unused_channel(true);
    int ctrl_chan = dma_claim_unused_channel(true);

    dma_channel_config c_ctrl = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c_ctrl, DMA_SIZE_32);
    channel_config_set_read_increment(&c_ctrl, false);
    channel_config_set_write_increment(&c_ctrl, false);
    // channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(
        ctrl_chan,                                 // Channel to be configured
        &c_ctrl,                                   // The configuration we just created
        &dma_hw->ch[data_chan].al3_transfer_count, // Write to the data channel's trigger register
        &single_transfer,                          // Read from our static 1 var
        1,                                         // Do this 1 time only
        false                                      // Start start immediately
    );

    dma_channel_config c_data = dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&c_data, DMA_SIZE_32);
    channel_config_set_read_increment(&c_data, false);
    channel_config_set_write_increment(&c_data, false);
    channel_config_set_dreq(&c_data, pio_get_dreq(pio, sm, false)); // Wait for PIO DREQ
    channel_config_set_chain_to(&c_data, ctrl_chan);                // Trigger Control Channel when done

    dma_channel_configure(
        data_chan,
        &c_data,
        (void*)&pio_outputs, // Write to volatile RAM variable
        &pio->rxf[sm],       // Read from PIO RX FIFO
        1,                   // Transfer one word, then tridder the control channel
        true                 // Start immediately
    );

    printf("Succesful\n");
    return EXIT_SUCCESS;
}

uint32_t get_buffer() //or extern
{
    // uint32_t val = pio_sm_get_blocking(pio, sm);
    // printf("%08x\n", val);

    // printf("%08x\n", pio_outputs);
    return pio_outputs & 0x003F;
}
