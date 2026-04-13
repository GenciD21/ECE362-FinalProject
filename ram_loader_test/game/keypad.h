#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

extern int col;
extern const char keymap[17];
extern char key;

void keypad_init_pins(void);
void keypad_init_timer(void);
void keypad_drive_column(void);
uint8_t keypad_read_rows(void);
void keypad_isr(void);

void key_push(uint16_t event);
uint16_t key_pop(void);
bool key_try_pop(uint16_t *event);

#endif /* KEYPAD_H */
