#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "queue.h"
#include "support.h"
#include "output.h"

//////////////////////////////////////////////////////////////////////////////

const char* username = "zimme160";

//////////////////////////////////////////////////////////////////////////////

static int duty_cycle = 0;
static int dir = 0;
static int color = 0;

void display_init_pins();
void display_init_timer();
void display_char_print(const char message[]);
void keypad_init_pins();
void keypad_init_timer();
void init_wavetable(void);
void set_freq(int chan, float f);
extern KeyEvents kev;
void drum_machine();

static float volume_lol = 1;

//////////////////////////////////////////////////////////////////////////////

// When testing static duty-cycle PWM
//#define STEP2
// When testing variable duty-cycle PWM
//#define STEP3
// When testing 8-bit audio synthesis
#define STEP4
// When trying out drum machine
// #define DRUM_MACHINE

//////////////////////////////////////////////////////////////////////////////



//volume from 0 to 1
void pwm_audio_handler() {
    // acknowledge interrupt
    uint32_t slice = pwm_gpio_to_slice_num(36);
    pwm_hw->intr = 1 << slice; 

    offset0 += step0;
    offset1 += step1;

    if (offset0 >= N << 16){
        offset0 -= N << 16;
    }

    if (offset1 >= N << 16){
        offset1 -= N << 16;
    }

    //create samp, divide by 2
    uint32_t samp = wavetable[offset0 >> 16] + wavetable[offset1 >> 16];
    samp /= 2;

    samp = (samp * pwm_hw->slice[slice].top) / (1 << 16);

    //write samp to duty cycle
    pwm_hw->slice[slice].cc = samp * volume_lol; 
}

void init_pwm_audio() {
    //set as pwm out
    gpio_set_function(36, GPIO_FUNC_PWM);
    //get slice num
    uint32_t slice = pwm_gpio_to_slice_num(36);
    //set clock divider
    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv(&pc, 150.f);
    pwm_init(slice, &pc, true);
    
    //set period to from support.c  - 1 
    pwm_hw->slice[slice].top = (1000000/RATE) - 1;
    //set duty cycle
    pwm_hw->slice[slice].cc = 0; //duty cycle of zero for now

    //enable
    pwm_set_enabled(slice, true);

    //setup sine wave
    init_wavetable();

    //setup irq 
    pwm_clear_irq(slice);
    pwm_set_irq0_enabled(slice, true);
    //Interrupt number = 8
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, pwm_audio_handler);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);
}

void init_adc() {
    adc_init();
    adc_gpio_init(ADC_BASE_PIN + 5);
    //Select channel 5 as input
    //adc_hw->cs |= 0x5u << (ADC_CS_AINSEL_LSB);
    adc_select_input(0x5u);

    //setup for singleshot
    adc_hw->cs |= ADC_CS_START_MANY_BITS;
}

uint16_t read_adc() {
    return adc_hw->result;
}

// Frequencies (Hz)
#define NOTE_A4  440
#define NOTE_GS4 415
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880  // Added definition
#define REST     0

// Corrected Durations (ms) for 144 BPM
#define H  800  // Half
#define Q  400  // Quarter
#define E  200  // Eighth
#define S  100  // Sixteenth
#define ED 300  // Dotted Eighth

// Melody Array: {Frequency, Duration}
// This uses the {freq, dur} pair structure from your first request
int melody[][2] = {
    // Phrase 1
    {NOTE_E5, Q},  {NOTE_B4, E},  {NOTE_C5, E},  {NOTE_D5, Q},  {NOTE_C5, E},  {NOTE_B4, E},
    {NOTE_A4, Q},  {NOTE_A4, E},  {NOTE_C5, E},  {NOTE_E5, Q},  {NOTE_D5, E},  {NOTE_C5, E},
    {NOTE_B4, ED}, {NOTE_C5, S},  {NOTE_D5, Q},  {NOTE_E5, Q},  
    {NOTE_C5, Q},  {NOTE_A4, Q},  {NOTE_A4, Q},  {REST,    Q},

    // Phrase 2
    {NOTE_D5, ED}, {NOTE_F5, S},  {NOTE_A5, Q},  {NOTE_G5, E},  {NOTE_F5, E},
    {NOTE_E5, ED}, {NOTE_C5, S},  {NOTE_E5, Q},  {NOTE_D5, E},  {NOTE_C5, E},
    {NOTE_B4, E},  {NOTE_B4, E},  {NOTE_C5, E},  {NOTE_D5, Q},  {NOTE_E5, Q},
    {NOTE_C5, Q},  {NOTE_A4, Q},  {NOTE_A4, Q},  {REST,    Q}
};


int main()
{
    stdio_init_all();

    init_pwm_audio(); 
    init_adc();

    // set_freq(0, 440.0f); // Set initial frequency to 440 Hz (A4 note)
    // set_freq(1, 0.0f); // Turn off channel 1 initially
    // set_freq(0, 261.626f);
    // set_freq(1, 329.628f);

    
    int note_num = 0;
    float note;
    float new_volume = (float)read_adc() / 4096.0f; 
    volume_lol = new_volume;

    while (true){
        if (note_num == 39){
            note_num = 0; 
        }

        note = melody[note_num][0];
        
        new_volume = (float)read_adc() / 4096.0f;
        new_volume = round(new_volume * 20) / 20.0f; 

        if (new_volume - volume_lol > 0.10 || new_volume - volume_lol < -0.10){
            volume_lol = new_volume;
        }

        set_freq(0, note); // Set initial frequency for channel 0
        sleep_ms(melody[note_num++][1]);
    }

    for(;;);
    return 0;
}
