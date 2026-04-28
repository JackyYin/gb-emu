#include "gameboy.h"
#include <stdio.h>

/* rate:
 * = 160     M-cycles / 160 bytes
 * = 160 * 4 T-cycles / 160 bytes
 * = 4       T-cycles / byte
 */
#define DMA_CYCLES_SHIFT (2) /* 1 << 2 = 4 T-cycles per byte */

void oam_tick(Bus *bus, uint32_t cycles) {
    OAM *oam = bus->oam;
    if ((oam->status & 0x01) == 0) return;

    /*
     * Fresh DMA has a 1 M-cycle (4 T-cycle) startup grace period.
     * We skip the very first tick (the one that contains the trigger
     * instruction) so the grace period starts counting from the
     * instruction AFTER the FF46 write.
     */
    if ((oam->status & 0x02) != 0) {
        oam->status ^= 0x02;
        return;
    }

    /* Clear the grace flag — it only gates OAM read accessibility, not byte copying. */
    if (oam->status & 0x04) {
        oam->status ^= 0x04;
    }

    uint16_t dma_src = (uint16_t)(bus->mmu->io[0x46] << 8);
    uint32_t steps = cycles >> DMA_CYCLES_SHIFT;
    while (steps-- && oam->offset < 0xA0) {
        bus->mmu->oam[oam->offset] = mmu_read_byte(bus, dma_src + oam->offset);
        oam->offset++;
    }

    if (oam->offset >= 0xA0) {
        oam->status   = 0;
        oam->offset   = 0;
    }
}

void oam_init(OAM *oam) {
    oam->status         = 0;
    oam->offset         = 0;
}
