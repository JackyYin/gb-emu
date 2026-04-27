#include "gameboy.h"

/* rate: 
 * = 160     M-cycles / 160 bytes 
 * = 160 * 4 T-cycles / 160 bytes 
 * = 4       T-cycles / byte
 */
#define DMA_CYCLES (4)
#define DMA_CYCLES_SHIFT (2) // 1 << 2 = 4

void oam_tick(Bus *bus, uint32_t cycles) {
    OAM *oam = bus->oam;
    if (!oam->transferring) return;

    uint16_t dma_src = (uint16_t)(bus->mmu->io[0x46] << 8);
    uint32_t steps = cycles >> DMA_CYCLES_SHIFT;
    while (steps-- && oam->dma_offset < 0xA0) {
        bus->mmu->oam[oam->dma_offset] = mmu_read_byte(bus, dma_src + oam->dma_offset);
        oam->dma_offset++;
    }

    if (oam->dma_offset >= 0xA0) {
        oam->transferring = false;
        oam->dma_offset   = 0;
    }
}


void oam_init(OAM *oam) {
    oam->transferring = 0;
    oam->dma_offset = 0;
}
