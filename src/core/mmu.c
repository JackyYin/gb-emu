#include "gameboy.h"
#include <stdio.h>

uint8_t mmu_read_byte(MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr) {
    if (addr < 0x8000) {
        if (addr < 0x100 && cart->boot_rom_enabled && cart->boot_rom) {
            return cart->boot_rom[addr];
        }
        if (addr < 0x4000) {
            return cart->rom ? cart->rom[addr] : 0;
        } else {
            uint32_t rom_addr = (uint32_t)cart->rom_bank * 0x4000 + (addr - 0x4000);
            if (rom_addr < cart->rom_size) {
                return cart->rom[rom_addr];
            }
            return 0;
        }
    }
    if (addr < 0xA000) {
        return mmu->vram[addr - 0x8000];
    }
    if (addr < 0xC000) {
        if (cart->ram_enabled && cart->ram) {
            uint32_t ram_addr = (uint32_t)cart->ram_bank * 0x2000 + (addr - 0xA000);
            if (ram_addr < cart->ram_size) {
                return cart->ram[ram_addr];
            }
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
    if (addr == 0xFF00) {
        uint8_t p1 = mmu->io[0x00];
        uint8_t nibble = 0x0F;  /* default: no buttons pressed */
        if (!(p1 & 0x10)) {     /* P14 selected: direction pad */
            nibble = mmu->joypad & 0x0F;
        }
        if (!(p1 & 0x20)) {     /* P15 selected: action buttons */
            uint8_t act = 0;
            if (mmu->joypad & 0x20) act |= 0x01;  /* A    */
            if (mmu->joypad & 0x10) act |= 0x02;  /* B    */
            if (mmu->joypad & 0x40) act |= 0x04;  /* Sel  */
            if (mmu->joypad & 0x80) act |= 0x08;  /* Start*/
            nibble = act;
        }
        return (p1 & 0xF0) | nibble;
    }
    /* Serial transfer Data*/
    /* Serial transfer Control*/
    if (addr >= 0xFF01 && addr <= 0xFF02) {
        return mmu->io[addr - 0xFF00];
    }
    if (addr >= 0xFF04 && addr <= 0xFF07) {
        return mmu->io[addr - 0xFF00];
    }
    if (addr < 0xFF80) {
        return mmu->io[addr - 0xFF00];
    }
    if (addr < 0xFFFF) {
        return mmu->hram[addr - 0xFF80];
    }
    return mmu->io[0x7F];
}

void mmu_write_byte(MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr, uint8_t value) {
    if (addr < 0x8000) {
        if (addr < 0x2000) {
            cart->ram_enabled = (value & 0x0F) == 0x0A;
        } else if (addr < 0x4000) {
            if (cart->type == 0x13) {
                uint8_t bank = value & 0x7F;
                if (bank == 0) bank = 1;
                cart->rom_bank = bank;
            } else {
                uint8_t bank = value & 0x1F;
                if (bank == 0) bank = 1;
                cart->rom_bank = bank;
            }
        } else if (addr < 0x6000) {
            if (cart->type == 0x13) {
                if (value <= 0x03) {
                    cart->ram_bank = value;
                    cart->rtc_mode = false;
                } else if (value >= 0x08 && value <= 0x0C) {
                    cart->ram_bank = value;
                    cart->rtc_mode = true;
                }
            } else {
                cart->rom_bank = (cart->rom_bank & 0x1F) | ((value & 0x03) << 5);
                if (cart->rom_bank == 0) cart->rom_bank = 1;
            }
        } else if (addr < 0x8000) {
            if (cart->type == 0x13 && value == 0x01) {
                /* RTC latch - ignore for now */
            }
        }
        return;
    }
    if (addr < 0xA000) {
        mmu->vram[addr - 0x8000] = value;
        return;
    }
    if (addr < 0xC000) {
        if (cart->ram_enabled && cart->ram) {
            uint32_t ram_addr = (uint32_t)cart->ram_bank * 0x2000 + (addr - 0xA000);
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
    if (addr == 0xFF00) {
        mmu->io[0x00] |= (value & 0xC0);
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
        if (addr == 0xFF40) {
            /* LCDC write - sync with PPU */
            /* printf("LCDC write: 0x%02X (LCD %s)\n", value, (value & 0x80) ? "ON" : "OFF"); */
        }
        if (addr == 0xFF44) {
            /* printf("trying to reset LY...\n"); */
            mmu->io[0x44] = 0;
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
