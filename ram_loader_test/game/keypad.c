#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <stdio.h>
#include "queue.h"

// Global column variable
int col = -1;

// Global key state
static bool state[16]; // Are keys pressed/released

// Keymap for the keypad
const char keymap[17] = "DCBA#9630852*741";

// Defined here to avoid circular dependency issues with autotest
// You can see the struct definition in queue.h
KeyEvents kev = { 
    .head = 0, 
    .tail = 0 
};

char key = '\0';

void keypad_drive_column();                                                                                                                                                                             
                                                                                                                                                                                                      
void keypad_isr();                                                                                                                                                                                
/********************************************************* */
// Implement the functions below.

uint16_t key_pop(void) {
    while (kev.head == kev.tail) {
        sleep_ms(10);
    }
    uint16_t value = kev.q[kev.tail];
    kev.tail = (kev.tail + 1) % 32;
    return value;
}

bool key_try_pop(uint16_t *value) {
    if (kev.head == kev.tail) {
        return false;
    }
    *value = kev.q[kev.tail];
    kev.tail = (kev.tail + 1) % 32;
    return true;
}

void key_push(uint16_t value) {
    if ((kev.head + 1) % 32 == kev.tail) {
        return;
    }
    kev.q[kev.head] = value;
    kev.head = (kev.head + 1) % 32;
}

void keypad_init_pins() {
    for (int i = 6 ; i < 10; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put(i, 0);
    }
    for (int i = 2; i < 6; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
    }
}

void keypad_init_timer() {
    timer_hw->inte |= (1u << 0);
    timer_hw->inte |= (1u << 1);

    irq_set_exclusive_handler(TIMER0_IRQ_0, keypad_drive_column);
    irq_set_exclusive_handler(TIMER0_IRQ_1, keypad_isr);
   
    // timer_hardware_alarm_set_callback(timer_hw, 0, keypad_drive_column);
    // timer_hardware_alarm_set_callback(timer_hw, 1, keypad_isr);

    timer_hardware_alarm_set_target(timer_hw, 0, time_us_64() + 1000000);
    timer_hardware_alarm_set_target(timer_hw, 1, time_us_64() + 1100000);

    irq_set_enabled(TIMER0_IRQ_0, 1);
    irq_set_enabled(TIMER0_IRQ_1, 1);

}

void keypad_drive_column() {
    //ack
    timer_hw->intr |= (1u << 0);
    col++;
    col = col % 4;
    sio_hw->gpio_clr = (1u << 6) | (1u << 7) | (1u << 8) | (1u << 9);
    sio_hw->gpio_set = (1u << (col + 6));
    timer_hardware_alarm_set_target(timer_hw, 0, time_us_64() + 25000);
}

uint8_t keypad_read_rows() {
    return ((gpio_get(5)) | (gpio_get(4) << 1) | (gpio_get(3) << 2) | (gpio_get(2) << 3));
}

void keypad_isr() {
   timer_hw->intr |= (1u << 1); 
   uint8_t rows = keypad_read_rows();
   for (int i = 2; i < 6; i++) {
        uint8_t index = ((col) % 4) * 4 + (5 - i);
        if ((rows & (1u << (i - 2))) && state[index] == 0) {
            state[index] = 1;
            key_push((uint16_t)((1u << 8) | keymap[index]));
        } else {
            if ((~rows & (1u << (i - 2)) && state[index] == 1)) {
                state[index] = 0;
                key_push((uint16_t)(keymap[index]));
            }
        }
   }
   timer_hardware_alarm_set_target(timer_hw, 1, time_us_64() + 25000);
}
