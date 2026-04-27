#ifndef GAMEBOY_H
#define GAMEBOY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SCREEN_W 160
#define SCREEN_H 144
#define CPU_FREQ 4194304
#define FRAME_CYCLES 70224

typedef struct {
    uint8_t a, b, c, d, e, f, h, l;
    uint16_t sp, pc;
    bool ime;
    bool ime_delay;
    bool halted;
    bool halt_bug;
} CPU;

typedef struct {
    uint8_t vram[0x2000];
    uint8_t wram[0x2000];
    uint8_t hram[0x80];
    uint8_t oam[0xA0];
    uint8_t io[0x80];
    uint32_t framebuffer[SCREEN_W * SCREEN_H];
    /*  bit 0 = Right (d-pad) / A (buttons) */
    /*  bit 1 = Left (d-pad) / B (buttons) */
    /*  bit 2 = Up (d-pad) / Select (buttons) */
    /*  bit 3 = Down (d-pad) / Start (buttons) */
    uint8_t joypad;
} MMU;

typedef struct {
    bool boot_rom_enabled;
    bool ram_enabled;
    bool is_mbc1m; // 1 MiB Multi-Game Compilation Carts
    bool rtc_mode;
    const uint8_t *rom;
    uint8_t *ram;
    uint8_t type;
    uint8_t rom_bank;
    uint8_t ram_bank;
    uint8_t bank_mode;
    uint8_t rom_nr_bits;
    const uint8_t *boot_rom;
    uint32_t boot_rom_size;
    uint32_t rom_size;
    uint32_t ram_size;
} Cartridge;

typedef struct {
    uint8_t nr10, nr11, nr12, nr13, nr14;
    float samples[4096];
    uint32_t sample_count;
} APU;

typedef struct {
    uint32_t div_counter;
    uint32_t tima_counter;
} Timer;

typedef struct {
    uint32_t counter;
    bool transferring;
} Serial;

typedef struct {
    uint8_t dma_offset;
    bool transferring;
} OAM;

typedef struct {
    MMU      *mmu;
    Cartridge *cart;
    Timer     *timer;
    OAM       *oam;
} Bus;

typedef struct {
    CPU      cpu;
    MMU      mmu;
    Cartridge cart;
    APU      apu;
    Timer    timer;
    Serial   serial;
    OAM      oam;
    Bus      bus;
    uint32_t cycles;
    bool     running;
} GameBoy;

void gb_init(void);
void gb_load_boot_rom(const uint8_t *rom, uint32_t size);
void gb_load_rom(const uint8_t *rom, uint32_t size);
void gb_run_frame(void);
uint32_t *gb_get_framebuffer(void);
void gb_set_joypad(uint8_t state);
float *gb_get_audio_samples(void);
uint32_t gb_get_audio_sample_count(void);

int cpu_step(CPU *cpu, Bus *bus);
void cpu_init(CPU *cpu);
void ppu_render_scanline(MMU *mmu, uint8_t line);
void ppu_init(MMU *mmu);
void mmu_init(MMU *mmu);
uint8_t mmu_read_byte(Bus *bus, uint16_t addr);
void mmu_write_byte(Bus *bus, uint16_t addr, uint8_t value);
void timer_tick(Bus *bus, uint32_t cycles);
void timer_init(MMU *mmu, Timer *timer);
void serial_tick(Bus *bus, Serial *serial, uint32_t cycles);
void serial_init(Serial *serial);
void cart_init(Cartridge *cart);
void cart_load_rom(Cartridge *cart, const uint8_t *rom, uint32_t size);
void oam_init(OAM *oam);
void oam_tick(Bus *bus, uint32_t cycles);

/* Test harness accessors */
CPU *gb_get_cpu(void);
MMU *gb_get_mmu(void);
bool gb_is_test_done(void);
bool gb_check_mooneye_pass(void);
char *gb_get_serial_output(void);
int gb_get_serial_output_len(void);

#endif
