#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/regs/m33_iac.h"


int main() {
    // Tell core1's NVIC where the vector table actually lives
    // core1_ram address must match what core0 used
    extern char __vector_table;  // pico-sdk provides this symbol
    // But since we're relocated, set it manually:
    *((volatile uint32_t *)0xe000ed08) = (uint32_t)0x20010000; // VTOR
    // core1 entry — pico-sdk startup runs normally,
    // then lands here. Never return.


    const uint LED = 23;  // use a different pin than core0
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    while (true) {
        gpio_put(LED, 1);
        sleep_ms(200);
        gpio_put(LED, 0);
        sleep_ms(200);
    }
}