#include "gameboy.h"
#include <stdio.h>

/*
 * IO register read masks (0xFF00-0xFF7F):
 * 0xFF = unmapped (reads as 0xFF)
 * 0x00 = all bits readable
 * other = unused bits ORed in on read
 */
static const uint8_t io_read_mask[0x80] = {
    /* 0xFF00 */ 0x00, /* P1: handled above */
    /* 0xFF01 */ 0x00, /* SB: Serial data */
    /* 0xFF02 */ 0x7E, /* SC: bits 1-6 unused */
    /* 0xFF03 */ 0xFF, /* unmapped */
    /* 0xFF04 */ 0x00, /* DIV */
    /* 0xFF05 */ 0x00, /* TIMA */
    /* 0xFF06 */ 0x00, /* TMA */
    /* 0xFF07 */ 0xF8, /* TAC: bits 3-7 unused */
    /* 0xFF08 */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* 0x08-0x0E unmapped */
    /* 0xFF0F */ 0xE0, /* IF: bits 5-7 unused */
    /* 0xFF10 */ 0x80, /* NR10: bit 7 unused */
    /* 0xFF11 */ 0x00, /* NR11 */
    /* 0xFF12 */ 0x00, /* NR12 */
    /* 0xFF13 */ 0x00, /* NR13 */
    /* 0xFF14 */ 0x00, /* NR14 */
    /* 0xFF15 */ 0xFF, /* unmapped */
    /* 0xFF16 */ 0x00, /* NR21 */
    /* 0xFF17 */ 0x00, /* NR22 */
    /* 0xFF18 */ 0x00, /* NR23 */
    /* 0xFF19 */ 0x00, /* NR24 */
    /* 0xFF1A */ 0x7F, /* NR30: bits 0-6 unused */
    /* 0xFF1B */ 0x00, /* NR31 */
    /* 0xFF1C */ 0x9F, /* NR32: bits 0-4,7 unused */
    /* 0xFF1D */ 0x00, /* NR33 */
    /* 0xFF1E */ 0x00, /* NR34 */
    /* 0xFF1F */ 0xFF, /* unmapped */
    /* 0xFF20 */ 0xC0, /* NR41: bits 6-7 unused */
    /* 0xFF21 */ 0x00, /* NR42 */
    /* 0xFF22 */ 0x00, /* NR43 */
    /* 0xFF23 */ 0x3F, /* NR44: bits 0-5 unused */
    /* 0xFF24 */ 0x00, /* NR50 */
    /* 0xFF25 */ 0x00, /* NR51 */
    /* 0xFF26 */ 0x70, /* NR52: bits 4-6 unused */
    /* 0xFF27 */ 0xFF, 0xFF, 0xFF, /* 0x27-0x29 unmapped */
    /* 0xFF2A-0xFF2F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* unmapped */
    /* 0xFF30-0xFF3F: Wave RAM, all bits readable */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 0xFF40 */ 0x00, /* LCDC */
    /* 0xFF41 */ 0x80, /* STAT: bit 7 unused */
    /* 0xFF42 */ 0x00, /* SCY */
    /* 0xFF43 */ 0x00, /* SCX */
    /* 0xFF44 */ 0x00, /* LY */
    /* 0xFF45 */ 0x00, /* LYC */
    /* 0xFF46 */ 0x00, /* DMA */
    /* 0xFF47 */ 0x00, /* BGP */
    /* 0xFF48 */ 0x00, /* OBP0 */
    /* 0xFF49 */ 0x00, /* OBP1 */
    /* 0xFF4A */ 0x00, /* WY */
    /* 0xFF4B */ 0x00, /* WX */
    /* 0xFF4C-0xFF4F */ 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0xFF50 */ 0xFF, /* Boot ROM disable: write-only, reads as 0xFF */
    /* 0xFF51-0xFF7F: unmapped on DMG */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
static uint8_t io_reg_read_byte(MMU *mmu, uint16_t addr) {
    uint8_t reg = addr - 0xFF00;
    uint8_t mask = io_read_mask[reg];
    if (mask == 0xFF) return 0xFF;
    return mmu->io[reg] | mask;
}

static uint8_t mbc1_read_byte(Cartridge *cart, uint16_t addr) {
     // 0x0000–0x3FFF (fixed bank)
    if (addr < 0x4000) {
        uint8_t bank_mask = (1 << (cart->rom_nr_bits + 1)) - 1;
        uint8_t bank = cart->is_mbc1m ?
            (cart->bank_mode ? (cart->ram_bank << 4) : 0) & bank_mask
            : (cart->bank_mode ? (cart->ram_bank << 5) : 0) & bank_mask;
        uint32_t rom_addr = (uint32_t)bank * 0x4000 + addr;
        return cart->rom[rom_addr];
    }
    // 0x4000–0x7FFF (switchable bank)
    else {
        uint8_t bank_mask = (1 << (cart->rom_nr_bits + 1)) - 1;
        uint8_t bank = cart->is_mbc1m ?
            ((cart->ram_bank << 4) | cart->rom_bank & 0x0F) & bank_mask
            : ((cart->ram_bank << 5) | cart->rom_bank & 0x1F) & bank_mask;
        uint32_t rom_addr = (uint32_t)bank * 0x4000 + (addr - 0x4000);
        return cart->rom[rom_addr];
    }
}
uint8_t mmu_read_byte(MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr) {
    if (addr < 0x100 && cart->boot_rom_enabled && cart->boot_rom) {
        return cart->boot_rom[addr];
    }
    if (addr < 0x8000) {
        // MBC 1
        if (cart->type >= 0x01 && cart->type <= 0x03) {
            return mbc1_read_byte(cart, addr);
        }
    }
    if (addr < 0xA000) {
        return mmu->vram[addr - 0x8000];
    }
    if (addr < 0xC000) {
        if (cart->ram_enabled && cart->ram) {
            uint32_t ram_addr = (uint32_t)(cart->bank_mode ? (cart->ram_bank * 0x2000) : 0) + (addr - 0xA000);
            return cart->ram[ram_addr];
        }
        return 0xFF;
    }
    if (addr < 0xE000) {
        return mmu->wram[addr - 0xC000];
    }
    if (addr < 0xFE00) {
        return mmu->wram[addr - 0xE000];
    }
    if (addr < 0xFEA0) {
        return mmu->oam[addr - 0xFE00];
    }
    if (addr < 0xFF00) {
        return 0xFF;
    }
    /* Joypad */
    if (addr == 0xFF00) {
        uint8_t p1 = mmu->io[0x00];
        uint8_t nibble = 0x0F;  /* default: no buttons pressed */
        if (!(p1 & 0x10)) {     /* P14 selected: direction pad */
            nibble = mmu->joypad & 0x0F;
        }
        if (!(p1 & 0x20)) {     /* P15 selected: action buttons */
            nibble = (mmu->joypad >> 4) & 0x0F;
        }
        return 0xC0 | (p1 & 0x30) | nibble;
    }
    if (addr < 0xFF80) {
        return io_reg_read_byte(mmu, addr);
    }
    if (addr < 0xFFFF) {
        return mmu->hram[addr - 0xFF80];
    }
    return mmu->io[0x7F];  /* IE register */
}


static void mbc1_write_byte(Cartridge *cart, uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        cart->ram_enabled = (value & 0x0F) == 0x0A;
    } else if (addr < 0x4000) {
        uint8_t bank = value & 0x1F;
        if (bank == 0) bank = 1;
        cart->rom_bank = bank;
    } else if (addr < 0x6000) {
        /*
         * Use 2 bits to select RAM Bank
         * */
        cart->ram_bank = value & 0x03;
    } else if (addr < 0x8000) {
        cart->bank_mode = value & 0x01;
    }
}

void mmu_write_byte(MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        // MBC 1
        if (cart->type >= 0x01 && cart->type <= 0x03) {
            mbc1_write_byte(cart, addr, value);
        }
        return;
    }
    if (addr < 0xA000) {
        mmu->vram[addr - 0x8000] = value;
        return;
    }
    if (addr < 0xC000) {
        if (cart->ram_enabled && cart->ram) {
            uint32_t ram_addr = (uint32_t)(cart->bank_mode ? (cart->ram_bank * 0x2000):0) + (addr - 0xA000);
            if (ram_addr < cart->ram_size) {
                cart->ram[ram_addr] = value;
            }
        }
        return;
    }
    if (addr < 0xE000) {
        mmu->wram[addr - 0xC000] = value;
        return;
    }
    if (addr < 0xFE00) {
        mmu->wram[addr - 0xE000] = value;
        return;
    }
    if (addr < 0xFEA0) {
        mmu->oam[addr - 0xFE00] = value;
        return;
    }
    if (addr < 0xFF00) {
        return;
    }
    /* Joypad */
    if (addr == 0xFF00) {
        /*
         * bits 6-7: unused
         * bits 4: Select buttons
         * bits 3: Select d-pad
         * */
        mmu->io[0x00] = (value & 0x30);
        return;
    }
    /* Serial transfer Data */
    if (addr == 0xFF01) {
        mmu->io[0x01] = value;
        return;
    }
    /* Serial transfer Control */
    if (addr == 0xFF02) {
        mmu->io[0x02] = value;
        return;
    }
    if (addr >= 0xFF04 && addr <= 0xFF07) {
        if (addr == 0xFF04) {
            /* Writing any value resets DIV to 0 */
            mmu->io[0x04] = 0;
            timer->div_counter = 0;
        } else if (addr == 0xFF05) {
            mmu->io[0x05] = value;
        } else if (addr == 0xFF06) {
            mmu->io[0x06] = value;
            /* If TIMA has overflowed, reload immediately */
            if (mmu->io[0x05] == 0) {
                mmu->io[0x05] = value;
            }
        } else if (addr == 0xFF07) {
            mmu->io[0x07] = value & 0x07;
            timer->tima_counter = 0;
        }
        return;
    }
    if (addr < 0xFF80) {
        if (addr == 0xFF44) {
            mmu->io[0x44] = 0;
            return;
        }
        if (addr == 0xFF46) {
            mmu->io[0x46] = value;
            uint16_t src = ((uint16_t)value) << 8;
            for (int i = 0; i < 0xA0; i++) {
                mmu->oam[i] = mmu_read_byte(mmu, cart, timer, src + i);
            }
            return;
        }
        if (addr == 0xFF50) {
            cart->boot_rom_enabled = false;
            return;
        }
        mmu->io[addr - 0xFF00] = value;
        return;
    }
    if (addr < 0xFFFF) {
        mmu->hram[addr - 0xFF80] = value;
        return;
    }
    mmu->io[0x7F] = value;
}

void mmu_init(MMU *mmu) {
    for (int i = 0; i < sizeof(mmu->vram); i++) mmu->vram[i] = 0;
    for (int i = 0; i < sizeof(mmu->wram); i++) mmu->wram[i] = 0;
    for (int i = 0; i < sizeof(mmu->hram); i++) mmu->hram[i] = 0;
    for (int i = 0; i < sizeof(mmu->oam); i++) mmu->oam[i] = 0;
    for (int i = 0; i < sizeof(mmu->io); i++) mmu->io[i] = 0;
    mmu->joypad = 0xFF;  /* all buttons released */
}
