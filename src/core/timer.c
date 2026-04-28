#include "gameboy.h"

/*
 * TIMA is clocked by a specific bit of the internal 16-bit sys_counter.
 * An increment fires on the falling edge (1→0) of that bit.
 *
 * The AND gate: TAC_enable & sys_counter_bit. Falling edge of the AND
 * output triggers TIMA, so disabling TAC or writing DIV can also fire it.
 *
 * bit positions (of sys_counter):
 *   mode 00 (4096 Hz,   1024 T/tick): bit 9
 *   mode 01 (262144 Hz,   16 T/tick): bit 3
 *   mode 10 (65536 Hz,    64 T/tick): bit 5
 *   mode 11 (16384 Hz,   256 T/tick): bit 7
 */
static const uint8_t tima_bit_pos[4] = {9, 3, 5, 7};

#define BITS_SET(x, b) \
   ( ((x) & (1 << (b))) != 0)

#define BITS_NOT_SET(x, b) \
   ( ((x) & (1 << (b))) == 0)

static void tima_increment(Bus *bus) {
    MMU *mmu = bus->mmu;
    Timer *timer = bus->timer;
    mmu->io[0x05]++;
    if (mmu->io[0x05] == 0) {
        /* Overflow: TIMA stays 0 for 4 T-cycles, then reloads from TMA */
        timer->reload_fired = 1;
    }
}

void timer_div_write(Bus *bus) {
    MMU *mmu = bus->mmu;
    Timer *timer = bus->timer;
    uint8_t tac = mmu->io[0x07];
    if (tac & 0x04) {
        if (BITS_SET(timer->sys_counter, tima_bit_pos[tac & 0x03])) {
            tima_increment(bus);
        }
    }
    timer->sys_counter = 0;
    mmu->io[0x04] = 0;
}

void timer_tima_write(Bus *bus, uint8_t value) {
    MMU *mmu = bus->mmu;
    Timer *timer = bus->timer;
    /* Writing TIMA during the 4T reload window cancels the pending reload */
    timer->reload_fired = 0;
    mmu->io[0x05] = value;
}

void timer_tac_write(Bus *bus, uint8_t value) {
    MMU *mmu = bus->mmu;
    Timer *timer = bus->timer;
    uint8_t old_tac = mmu->io[0x07];
    bool old_and = (old_tac & 0x04) && BITS_SET(timer->sys_counter, tima_bit_pos[old_tac & 0x03]);
    mmu->io[0x07] = value & 0x07;
    bool new_and = (mmu->io[0x07] & 0x04) && BITS_SET(timer->sys_counter, tima_bit_pos[mmu->io[0x07] & 0x03]);
    if (old_and && !new_and) {
        tima_increment(bus);
    }
}

void timer_tick(Bus *bus, uint32_t cycles) {
    MMU *mmu = bus->mmu;
    Timer *timer = bus->timer;

    uint8_t tac = mmu->io[0x07];
    bool timer_enabled = (tac & 0x04) != 0;
    uint8_t bit = tima_bit_pos[tac & 0x03];

    uint32_t remaining = cycles;
    while (remaining > 0) {
        uint32_t step = (remaining >= 4) ? 4 : remaining;
        remaining -= step;

        /*
         * Process reload countdown BEFORE the falling-edge check.
         * This ensures a reload set during this step's overflow won't
         * fire until the next step (correct 4T delay).
         */
        if (timer->reload_fired) {
            mmu->io[0x05] = mmu->io[0x06];
            mmu->io[0x0F] |= 0x04;
            timer->reload_fired = 0;
        }

        uint16_t old = timer->sys_counter;
        timer->sys_counter = (uint16_t)(timer->sys_counter + step);

        /* TIMA: falling edge of selected bit */
        if (timer_enabled) {
            if (BITS_SET(old, bit) && BITS_NOT_SET(timer->sys_counter, bit)) {
                tima_increment(bus);
            }
        }
    }

    mmu->io[0x04] = (uint8_t)(timer->sys_counter >> 8);
}

void timer_init(MMU *mmu, Timer *timer) {
    mmu->io[0x04] = 0;
    mmu->io[0x05] = 0;
    mmu->io[0x06] = 0;
    mmu->io[0x07] = 0;
    timer->sys_counter = 0;
    timer->reload_fired = 0;
}
