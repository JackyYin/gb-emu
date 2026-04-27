#include "gameboy.h"
#include <stdio.h>
#include <stdlib.h>

static GameBoy gb;
static int frame_count = 0;
static char serial_buf[4096];
static int serial_buf_len = 0;
static bool test_done = false;

void gb_init(void) {
    gb.cycles = 0;
    gb.running = false;

    cpu_init(&gb.cpu);
    mmu_init(&gb.mmu);
    ppu_init(&gb.mmu);
    cart_init(&gb.cart);

    gb.apu.sample_count = 0;
    timer_init(&gb.mmu, &gb.timer);
    serial_init(&gb.serial);
    serial_buf_len = 0;
    test_done = false;
    oam_init(&gb.oam);

    gb.bus.mmu   = &gb.mmu;
    gb.bus.cart  = &gb.cart;
    gb.bus.timer = &gb.timer;
    gb.bus.oam   = &gb.oam;
}

void gb_load_boot_rom(const uint8_t *rom, uint32_t size) {
    if (size > 0x100) size = 0x100;
    gb.cart.boot_rom = rom;
    gb.cart.boot_rom_size = size;
    gb.cart.boot_rom_enabled = true;
    gb.cpu.pc = 0x0000;
}

void gb_load_rom(const uint8_t *rom, uint32_t size) {
    cart_load_rom(&gb.cart, rom, size);
    printf("is_mbc1m: %d\n", gb.cart.is_mbc1m);

    gb.running = true;
}

void gb_run_frame(void) {
    if (!gb.running) return;

    uint32_t frame_cycles = 0;
    uint32_t scanline_cycles = 0;
    uint8_t ly = 0;
    static int first_frames = 0;

    while (frame_cycles < FRAME_CYCLES) {
        /* Detect infinite loop: JR -2 (0x18 0xFE) or HALT loop */
        if (!test_done) {
            uint16_t pc = gb.cpu.pc;
            uint8_t b0 = mmu_read_byte(&gb.bus, pc);
            uint8_t b1 = mmu_read_byte(&gb.bus, pc + 1);
            if (b0 == 0x18 && b1 == 0xFE) {
                test_done = true;
            }
        }

        int cycles = cpu_step(&gb.cpu, &gb.bus);
        frame_cycles += cycles;
        gb.cycles += cycles;
        scanline_cycles += cycles;

        timer_tick(&gb.bus, cycles);
        oam_tick(&gb.bus, cycles);

        /* Capture serial byte before transfer overwrites SB */
        uint8_t old_sc = gb.mmu.io[0x02];
        uint8_t old_sb = gb.mmu.io[0x01];
        serial_tick(&gb.bus, &gb.serial, cycles);
        if ((old_sc & 0x80) && !(gb.mmu.io[0x02] & 0x80)) {
            if (serial_buf_len < (int)sizeof(serial_buf) - 1) {
                serial_buf[serial_buf_len] = (char)old_sb;
                serial_buf_len++;
            }
        }

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

CPU *gb_get_cpu(void) {
    return &gb.cpu;
}

MMU *gb_get_mmu(void) {
    return &gb.mmu;
}

bool gb_is_test_done(void) {
    return test_done;
}

bool gb_check_mooneye_pass(void) {
    return gb.cpu.b == 3 && gb.cpu.c == 5 &&
           gb.cpu.d == 8 && gb.cpu.e == 13 &&
           gb.cpu.h == 21 && gb.cpu.l == 34;
}

char *gb_get_serial_output(void) {
    serial_buf[serial_buf_len] = '\0';
    return serial_buf;
}

int gb_get_serial_output_len(void) {
    return serial_buf_len;
}
