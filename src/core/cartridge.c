#include "gameboy.h"

void cart_init(Cartridge *cart) {
    cart->rom = NULL;
    cart->rom_size = 0;
    cart->ram = NULL;
    cart->ram_size = 0;
    cart->rom_bank = 1;
    cart->ram_bank = 0;
    cart->ram_enabled = false;
    cart->bank_mode = 0;
}
