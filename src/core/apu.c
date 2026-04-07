#include "gameboy.h"

void apu_process(APU *apu, uint32_t cycles) {
    (void)apu;
    (void)cycles;
}

void apu_init(APU *apu) {
    apu->nr10 = 0x80;
    apu->nr11 = 0xBF;
    apu->nr12 = 0xF3;
    apu->nr13 = 0xFF;
    apu->nr14 = 0xBF;
    apu->sample_count = 0;
}
