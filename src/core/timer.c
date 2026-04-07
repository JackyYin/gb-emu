#include "gameboy.h"

static const uint32_t tima_thresholds[4] = {
    256,  /* 00: 4096 Hz  */
    4,    /* 01: 262144 Hz */
    16,   /* 10: 65536 Hz  */
    64,   /* 11: 16384 Hz  */
};

void timer_tick(MMU *mmu, Timer *timer, uint32_t cycles) {
    timer->div_counter += cycles;
    while (timer->div_counter >= 256) {
        timer->div_counter -= 256;
        mmu->io[0x04]++;
    }

    if (mmu->io[0x07] & 0x04) {
        uint32_t threshold = tima_thresholds[mmu->io[0x07] & 0x03];
        timer->tima_counter += cycles;
        while (timer->tima_counter >= threshold) {
            timer->tima_counter -= threshold;
            mmu->io[0x05]++;
            if (mmu->io[0x05] == 0) {
                mmu->io[0x05] = mmu->io[0x06];
                mmu->io[0x0F] |= 0x04;
            }
        }
    }
}

void timer_init(MMU *mmu, Timer *timer) {
    mmu->io[0x04] = 0;
    mmu->io[0x05] = 0;
    mmu->io[0x06] = 0;
    mmu->io[0x07] = 0;
    timer->div_counter = 0;
    timer->tima_counter = 0;
}
