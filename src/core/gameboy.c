#include "gameboy.h"
#include <stdio.h>

static GameBoy gb;
static int frame_count = 0;

void gb_init(void) {
    gb.cycles = 0;
    gb.running = false;

    cpu_init(&gb.cpu);
    mmu_init(&gb.mmu);
    ppu_init(&gb.mmu);

    gb.cart.rom = NULL;
    gb.cart.rom_size = 0;
    gb.cart.ram = NULL;
    gb.cart.ram_size = 0;
    gb.cart.rom_bank = 1;
    gb.cart.ram_bank = 0;
    gb.cart.ram_enabled = false;
    gb.cart.type = 0;
    gb.cart.rtc_mode = false;
    gb.cart.boot_rom = NULL;
    gb.cart.boot_rom_size = 0;
    gb.cart.boot_rom_enabled = false;

    gb.apu.sample_count = 0;
    timer_init(&gb.mmu, &gb.timer);
    serial_init(&gb.serial);
}

void gb_load_boot_rom(const uint8_t *rom, uint32_t size) {
    if (size > 0x100) size = 0x100;
    gb.cart.boot_rom = rom;
    gb.cart.boot_rom_size = size;
    gb.cart.boot_rom_enabled = true;
    gb.cpu.pc = 0x0000;
}

void gb_load_rom(const uint8_t *rom, uint32_t size) {
    gb.cart.rom_size = size;
    gb.cart.rom = rom;
    gb.cart.type = rom[0x147];

    uint8_t ram_size_byte = rom[0x149];
    switch (ram_size_byte) {
        case 0x02: gb.cart.ram_size = 8 * 1024; break;
        case 0x03: gb.cart.ram_size = 32 * 1024; break;
        case 0x04: gb.cart.ram_size = 128 * 1024; break;
        case 0x05: gb.cart.ram_size = 64 * 1024; break;
        default:   gb.cart.ram_size = 0; break;
    }

    gb.cart.rtc_mode = false;

    gb.running = true;
}

void gb_run_frame(void) {
    if (!gb.running) return;

    uint32_t frame_cycles = 0;
    uint32_t scanline_cycles = 0;
    uint8_t ly = 0;
    static int first_frames = 0;

    while (frame_cycles < FRAME_CYCLES) {
        int cycles = cpu_step(&gb.cpu, &gb.mmu, &gb.cart, &gb.timer);
        frame_cycles += cycles;
        gb.cycles += cycles;
        scanline_cycles += cycles;

        timer_tick(&gb.mmu, &gb.timer, cycles);
        serial_tick(&gb.mmu, &gb.serial, cycles);

        if (scanline_cycles >= 456) {
            scanline_cycles -= 456;

            if (ly < 144) {
                ppu_render_scanline(&gb.mmu, ly);
            }

            ly++;
            if (ly >= 154) ly = 0;
            gb.mmu.io[0x44] = ly;
            if (ly == 144) {
                gb.mmu.io[0x0F] |= 0x01;
            }
        }

        uint8_t old_stat = gb.mmu.io[0x41];
        uint8_t old_mode = old_stat & 0x03;
        uint8_t stat = old_stat & 0xF8;
        uint8_t new_mode;
        if (ly >= 144) {
            new_mode = 1;
        } else if (scanline_cycles < 80) {
            new_mode = 2;
        } else if (scanline_cycles < 252) {
            new_mode = 3;
        } else {
            new_mode = 0;
        }
        stat |= new_mode;

        bool lyc_match = (gb.mmu.io[0x45] == ly);
        if (lyc_match) stat |= 0x04;

        gb.mmu.io[0x41] = stat;

        /* Fire STAT interrupt on rising edge */
        bool old_stat_irq = false;
        bool new_stat_irq = false;

        if ((stat & 0x40) && lyc_match) new_stat_irq = true;
        if ((stat & 0x20) && new_mode == 2) new_stat_irq = true;
        if ((stat & 0x10) && new_mode == 1) new_stat_irq = true;
        if ((stat & 0x08) && new_mode == 0) new_stat_irq = true;

        if ((old_stat & 0x40) && (old_stat & 0x04)) old_stat_irq = true;
        if ((old_stat & 0x20) && old_mode == 2) old_stat_irq = true;
        if ((old_stat & 0x10) && old_mode == 1) old_stat_irq = true;
        if ((old_stat & 0x08) && old_mode == 0) old_stat_irq = true;

        if (new_stat_irq && !old_stat_irq) {
            gb.mmu.io[0x0F] |= 0x02;
        }
    }

    gb.mmu.io[0x44] = 0;
}

uint32_t *gb_get_framebuffer(void) {
    return gb.mmu.framebuffer;
}

void gb_set_joypad(uint8_t state) {
    gb.mmu.joypad = state;
}

float *gb_get_audio_samples(void) {
    return gb.apu.samples;
}

uint32_t gb_get_audio_sample_count(void) {
    return gb.apu.sample_count;
}
