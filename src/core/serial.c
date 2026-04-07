#include "gameboy.h"

#define SERIAL_TRANSFER_CYCLES 1024  /* 8 bits x 128 T-cycles each */

void serial_tick(MMU *mmu, Serial *serial, uint32_t cycles) {
    if (!serial->transferring) {
        /* Detect new transfer: SC bit 7 (start) + bit 0 (internal clock) */
        if ((mmu->io[0x02] & 0x81) == 0x81) {
            serial->transferring = true;
            serial->counter = 0;
        } else {
            return;
        }
    }

    serial->counter += cycles;
    if (serial->counter >= SERIAL_TRANSFER_CYCLES) {
        mmu->io[0x01] = 0xFF;      /* SB: no device connected */
        mmu->io[0x02] &= 0x7F;     /* clear transfer start flag */
        mmu->io[0x0F] |= 0x08;     /* request serial interrupt */
        serial->transferring = false;
    }
}

void serial_init(Serial *serial) {
    serial->counter = 0;
    serial->transferring = false;
}
