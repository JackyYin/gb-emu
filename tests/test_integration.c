#include "test_framework.h"
#include "gameboy.h"
#include <string.h>

/* gb_init uses a static GameBoy, so we test through the public API */

/* ========== gb_init ========== */
static void test_gb_init(void) {
    gb_init();
    uint32_t *fb = gb_get_framebuffer();
    ASSERT_TRUE(fb != NULL, "Framebuffer not NULL after init");

    float *samples = gb_get_audio_samples();
    ASSERT_TRUE(samples != NULL, "Audio samples not NULL after init");

    uint32_t count = gb_get_audio_sample_count();
    ASSERT_EQ(0, count, "Audio sample count is 0 after init");
}

/* ========== ROM loading ========== */
static void test_load_rom(void) {
    gb_init();
    /* Create a minimal valid ROM (32KB) */
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));
    /* Entry point: NOP; JP 0x0150 */
    rom[0x0100] = 0x00;
    rom[0x0101] = 0xC3;
    rom[0x0102] = 0x50;
    rom[0x0103] = 0x01;
    /* Cartridge type at 0x147 */
    rom[0x0147] = 0x00; /* ROM only */
    /* ROM size at 0x148 */
    rom[0x0148] = 0x00;
    /* Place a HALT + infinite loop at 0x0150 */
    rom[0x0150] = 0x76; /* HALT */

    gb_load_rom(rom, sizeof(rom));
    /* Should be running now */
    /* Run one frame without crash */
    gb_run_frame();
    ASSERT_TRUE(1, "ROM loaded and frame executed without crash");
}

/* ========== Boot ROM ========== */
static void test_boot_rom_load(void) {
    gb_init();
    static uint8_t boot[0x100];
    memset(boot, 0, sizeof(boot));
    /* Simple boot ROM: LD SP, 0xFFFE; then a bunch of NOPs */
    boot[0x00] = 0x31;
    boot[0x01] = 0xFE;
    boot[0x02] = 0xFF;
    /* Fill rest with NOPs */

    gb_load_boot_rom(boot, sizeof(boot));
    /* After loading boot ROM, PC should be 0x0000 */
    /* We can't directly check CPU state, but we can run a frame */
    ASSERT_TRUE(1, "Boot ROM loaded without crash");
}

/* ========== Joypad ========== */
static void test_joypad_set(void) {
    gb_init();
    gb_set_joypad(0xEF); /* Start pressed */
    /* Can't directly verify without accessing internal state,
       but verify it doesn't crash */
    ASSERT_TRUE(1, "Joypad set without crash");
}

/* ========== Frame execution with simple program ========== */
static void test_frame_execution(void) {
    gb_init();
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));

    /* Simple program at 0x0100: infinite loop */
    rom[0x0100] = 0x18; /* JR -2 (infinite loop) */
    rom[0x0101] = 0xFE;
    rom[0x0147] = 0x00;
    rom[0x0148] = 0x00;

    gb_load_rom(rom, sizeof(rom));
    gb_run_frame();

    uint32_t *fb = gb_get_framebuffer();
    ASSERT_TRUE(fb != NULL, "Framebuffer valid after frame");
}

/* ========== Multiple frames ========== */
static void test_multiple_frames(void) {
    gb_init();
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));

    rom[0x0100] = 0x18;
    rom[0x0101] = 0xFE;
    rom[0x0147] = 0x00;
    rom[0x0148] = 0x00;

    gb_load_rom(rom, sizeof(rom));

    for (int i = 0; i < 10; i++) {
        gb_run_frame();
    }
    ASSERT_TRUE(1, "10 frames executed without crash");
}

/* ========== Program that writes to VRAM ========== */
static void test_vram_write_program(void) {
    gb_init();
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));

    /* Program at 0x0100:
       LD HL, 0x8000
       LD A, 0xFF
       LD (HL), A
       HALT
    */
    rom[0x0100] = 0x21; /* LD HL, 0x8000 */
    rom[0x0101] = 0x00;
    rom[0x0102] = 0x80;
    rom[0x0103] = 0x3E; /* LD A, 0xFF */
    rom[0x0104] = 0xFF;
    rom[0x0105] = 0x77; /* LD (HL), A */
    rom[0x0106] = 0x76; /* HALT */
    rom[0x0147] = 0x00;
    rom[0x0148] = 0x00;

    gb_load_rom(rom, sizeof(rom));
    gb_run_frame();
    ASSERT_TRUE(1, "VRAM write program executed without crash");
}

/* ========== Program with subroutine ========== */
static void test_subroutine_program(void) {
    gb_init();
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));

    /* Main at 0x0100:
       LD SP, 0xFFFE
       CALL 0x0200
       HALT
    */
    rom[0x0100] = 0x31; /* LD SP, 0xFFFE */
    rom[0x0101] = 0xFE;
    rom[0x0102] = 0xFF;
    rom[0x0103] = 0xCD; /* CALL 0x0200 */
    rom[0x0104] = 0x00;
    rom[0x0105] = 0x02;
    rom[0x0106] = 0x76; /* HALT */

    /* Subroutine at 0x0200:
       LD A, 0x42
       RET
    */
    rom[0x0200] = 0x3E; /* LD A, 0x42 */
    rom[0x0201] = 0x42;
    rom[0x0202] = 0xC9; /* RET */

    rom[0x0147] = 0x00;
    rom[0x0148] = 0x00;

    gb_load_rom(rom, sizeof(rom));
    gb_run_frame();
    ASSERT_TRUE(1, "Subroutine program executed without crash");
}

/* ========== Interrupt handling in frame ========== */
static void test_interrupt_frame(void) {
    gb_init();
    static uint8_t rom[0x8000];
    memset(rom, 0, sizeof(rom));

    /* V-Blank handler at 0x0040: RETI */
    rom[0x0040] = 0xD9;

    /* Main: enable interrupts, then loop */
    rom[0x0100] = 0x31; /* LD SP, 0xFFFE */
    rom[0x0101] = 0xFE;
    rom[0x0102] = 0xFF;
    rom[0x0103] = 0xFB; /* EI */
    rom[0x0104] = 0x3E; /* LD A, 0x01 */
    rom[0x0105] = 0x01;
    rom[0x0106] = 0xE0; /* LDH (0xFF), A  -> IE = 0x01 (V-Blank) */
    rom[0x0107] = 0xFF;
    rom[0x0108] = 0x18; /* JR -2 (loop) */
    rom[0x0109] = 0xFE;

    rom[0x0147] = 0x00;
    rom[0x0148] = 0x00;

    gb_load_rom(rom, sizeof(rom));
    gb_run_frame();
    ASSERT_TRUE(1, "Interrupt-enabled frame executed without crash");
}

void run_integration_tests(void) {
    TEST_SUITE("Integration Tests");

    RUN_TEST(test_gb_init);
    RUN_TEST(test_load_rom);
    RUN_TEST(test_boot_rom_load);
    RUN_TEST(test_joypad_set);
    RUN_TEST(test_frame_execution);
    RUN_TEST(test_multiple_frames);
    RUN_TEST(test_vram_write_program);
    RUN_TEST(test_subroutine_program);
    RUN_TEST(test_interrupt_frame);
}
