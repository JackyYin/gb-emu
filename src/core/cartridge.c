#include "gameboy.h"

void cart_init(Cartridge *cart) {
    cart->rom = NULL;
    cart->rom_size = 0;
    cart->ram = NULL;
    cart->ram_size = 0;
    cart->rom_bank = 1;
    cart->ram_bank = 0;
    cart->ram_enabled = false;
}

uint8_t cart_read_rom(Cartridge *cart, uint16_t addr) {
    if (addr < 0x4000) {
        return cart->rom[addr];
    } else {
        uint32_t bank_addr = ((uint32_t)cart->rom_bank << 14) + (addr & 0x3FFF);
        if (bank_addr < cart->rom_size) {
            return cart->rom[bank_addr];
        }
    }
    return 0xFF;
}

void cart_write_ram(Cartridge *cart, uint16_t addr, uint8_t value) {
    if (addr >= 0xA000 && addr < 0xC000) {
        uint16_t ram_addr = addr - 0xA000;
        if (cart->ram && ram_addr < cart->ram_size) {
            cart->ram[ram_addr] = value;
        }
    }
}

uint8_t cart_read_ram(Cartridge *cart, uint16_t addr) {
    if (addr >= 0xA000 && addr < 0xC000) {
        uint16_t ram_addr = addr - 0xA000;
        if (cart->ram && ram_addr < cart->ram_size) {
            return cart->ram[ram_addr];
        }
    }
    return 0xFF;
}
