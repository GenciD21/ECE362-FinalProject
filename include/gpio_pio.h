#ifndef GPIO6_IN_H
#define GPIO6_IN_H

#include "pico/stdlib.h"
#include "hardware/pio.h"


#define GPIO6_BASE_PIN 26
#define GPIO6_RANGE 6


int init_pio_inputs();
uint32_t get_buffer();

#endif