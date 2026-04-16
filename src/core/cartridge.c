#include "gameboy.h"

#include <stdio.h>
#include <stdlib.h>

static uint8_t nintendo_logo[0x30] = {
0xCE,0xED,0x66,0x66,0xCC,0x0D,0x00,0x0B,0x03,0x73,0x00,0x83,0x00,0x0C,0x00,0x0D,
0x00,0x08,0x11,0x1F,0x88,0x89,0x00,0x0E,0xDC,0xCC,0x6E,0xE6,0xDD,0xDD,0xD9,0x99,
0xBB,0xBB,0x67,0x63,0x6E,0x0E,0xEC,0xCC,0xDD,0xDC,0x99,0x9F,0xBB,0xB9,0x33,0x3E,
};

static bool check_is_mbc1m(Cartridge *cart) {
    uint32_t addr = 0x104;
    for (uint8_t i = 0; i < 0x30; i++) {
        uint8_t bytes = cart->rom[addr+i];
        if (bytes != nintendo_logo[i])
            return false;
    }
    addr = 0x40104;
    if (addr > cart->rom_size)
        return false;
    for (uint8_t i = 0; i < 0x30; i++) {
        uint8_t bytes = cart->rom[addr+i];
        if (bytes != nintendo_logo[i])
            return false;
    }
    return true;
}

void cart_load_rom(Cartridge *cart, const uint8_t *rom, uint32_t size) {
    cart->rom_size = size;
    cart->rom = rom;
    cart->type = rom[0x147];
    cart->rom_nr_bits = rom[0x148];
    uint8_t ram_size_byte = rom[0x149];
    switch (ram_size_byte) {
        case 0x02: cart->ram_size = 8 * 1024; break;
        case 0x03: cart->ram_size = 32 * 1024; break;
        case 0x04: cart->ram_size = 128 * 1024; break;
        case 0x05: cart->ram_size = 64 * 1024; break;
        default:   cart->ram_size = 0; break;
    }
    printf("cart type: 0x%x, rom size: 0x%x ram size: 0x%x, CGB flag: 0x%x\n", rom[0x147], rom[0x148], rom[0x149], rom[0x143]);

    if (cart->ram_size > 0) {
        cart->ram = malloc(cart->ram_size);
    }

    if (cart->type >= 1 && cart->type <= 3)
        cart->is_mbc1m = check_is_mbc1m(cart);

    cart->rtc_mode = false;
}

void cart_init(Cartridge *cart) {
    cart->rom = NULL;
    cart->rom_size = 0;
    cart->rom_nr_bits = 0;
    cart->rom_bank = 1;
    cart->ram = NULL;
    cart->ram_size = 0;
    cart->ram_bank = 0;
    cart->ram_enabled = false;
    cart->type = 0;
    cart->rtc_mode = false;
    cart->is_mbc1m = false;
    cart->boot_rom = NULL;
    cart->boot_rom_size = 0;
    cart->boot_rom_enabled = false;
}
