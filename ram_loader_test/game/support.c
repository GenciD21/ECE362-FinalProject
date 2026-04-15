#include "support.h"

int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;
int volume = 2400;
short int wavetable[N];

void init_wavetable(void){
    for (int i = 0; i < N; i++){
        wavetable[i] = (16383 * sin(2*M_PI*i/N)) + 16384;
    }
}
void set_freq(int chan, int f) {
    // step = (f * N / RATE) * 65536, integer math only
    int s = (f == 0) ? 0 : (int)((int64_t)f * N * 65536 / RATE);
    if (chan == 0) {
        step0 = s;
        if (f == 0) offset0 = 0;
    }
    if (chan == 1) {
        step1 = s;
        if (f == 0) offset1 = 0;
    }
}
