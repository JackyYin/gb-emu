#include "test_framework.h"
#include "gameboy.h"
#include <string.h>

static MMU mmu;
static Cartridge cart;
static Timer timer;
static uint8_t rom_data[0x8000];
static uint8_t cart_ram[0x2000];

static void setup(void) {
    memset(&mmu, 0, sizeof(mmu));
    memset(&cart, 0, sizeof(cart));
    memset(&timer, 0, sizeof(timer));
    memset(rom_data, 0, sizeof(rom_data));
    memset(cart_ram, 0, sizeof(cart_ram));
    mmu_init(&mmu);
    timer_init(&mmu, &timer);
    cart.rom = rom_data;
    cart.rom_size = sizeof(rom_data);
    cart.rom_bank = 1;
    cart.ram = cart_ram;
    cart.ram_size = sizeof(cart_ram);
    cart.ram_enabled = false;
    cart.boot_rom_enabled = false;
}

/* ========== ROM reads ========== */
static void test_rom_bank0_read(void) {
    setup();
    rom_data[0x0000] = 0x31;
    rom_data[0x0150] = 0xAA;
    ASSERT_EQ(0x31, mmu_read_byte(&mmu, &cart, &timer, 0x0000), "ROM bank 0 addr 0x0000");
    ASSERT_EQ(0xAA, mmu_read_byte(&mmu, &cart, &timer, 0x0150), "ROM bank 0 addr 0x0150");
}

static void test_rom_bank1_read(void) {
    setup();
    cart.rom_bank = 1;
    rom_data[0x4000] = 0xBB;
    rom_data[0x7FFF] = 0xCC;
    ASSERT_EQ(0xBB, mmu_read_byte(&mmu, &cart, &timer, 0x4000), "ROM bank 1 start");
    ASSERT_EQ(0xCC, mmu_read_byte(&mmu, &cart, &timer, 0x7FFF), "ROM bank 1 end");
}

/* ========== Boot ROM overlay ========== */
static void test_boot_rom_overlay(void) {
    setup();
    uint8_t boot[0x100];
    memset(boot, 0, sizeof(boot));
    boot[0x00] = 0x31;
    boot[0xFF] = 0xEE;
    rom_data[0x00] = 0x00;
    cart.boot_rom = boot;
    cart.boot_rom_size = sizeof(boot);
    cart.boot_rom_enabled = true;
    ASSERT_EQ(0x31, mmu_read_byte(&mmu, &cart, &timer, 0x0000), "Boot ROM at 0x0000");
    ASSERT_EQ(0xEE, mmu_read_byte(&mmu, &cart, &timer, 0x00FF), "Boot ROM at 0x00FF");
    /* After boot ROM range, should read game ROM */
    rom_data[0x0100] = 0xDD;
    ASSERT_EQ(0xDD, mmu_read_byte(&mmu, &cart, &timer, 0x0100), "Game ROM after boot ROM range");
}

static void test_boot_rom_disable(void) {
    setup();
    uint8_t boot[0x100];
    memset(boot, 0xAA, sizeof(boot));
    cart.boot_rom = boot;
    cart.boot_rom_size = sizeof(boot);
    cart.boot_rom_enabled = true;
    rom_data[0x00] = 0x55;
    ASSERT_EQ(0xAA, mmu_read_byte(&mmu, &cart, &timer, 0x0000), "Boot ROM active");
    /* Write to 0xFF50 disables boot ROM */
    mmu_write_byte(&mmu, &cart, &timer, 0xFF50, 0x01);
    ASSERT_FALSE(cart.boot_rom_enabled, "Boot ROM disabled");
    ASSERT_EQ(0x55, mmu_read_byte(&mmu, &cart, &timer, 0x0000), "Game ROM after disable");
}

/* ========== VRAM ========== */
static void test_vram_read_write(void) {
    setup();
    mmu_write_byte(&mmu, &cart, &timer, 0x8000, 0x42);
    mmu_write_byte(&mmu, &cart, &timer, 0x9FFF, 0x99);
    ASSERT_EQ(0x42, mmu_read_byte(&mmu, &cart, &timer, 0x8000), "VRAM start");
    ASSERT_EQ(0x99, mmu_read_byte(&mmu, &cart, &timer, 0x9FFF), "VRAM end");
    ASSERT_EQ(0x42, mmu.vram[0], "VRAM direct check start");
    ASSERT_EQ(0x99, mmu.vram[0x1FFF], "VRAM direct check end");
}

/* ========== WRAM ========== */
static void test_wram_read_write(void) {
    setup();
    mmu_write_byte(&mmu, &cart, &timer, 0xC000, 0xAA);
    mmu_write_byte(&mmu, &cart, &timer, 0xDFFF, 0xBB);
    ASSERT_EQ(0xAA, mmu_read_byte(&mmu, &cart, &timer, 0xC000), "WRAM start");
    ASSERT_EQ(0xBB, mmu_read_byte(&mmu, &cart, &timer, 0xDFFF), "WRAM end");
}

/* ========== Echo RAM ========== */
static void test_echo_ram(void) {
    setup();
    mmu_write_byte(&mmu, &cart, &timer, 0xC000, 0x77);
    /* Echo RAM at 0xE000 mirrors 0xC000 */
    ASSERT_EQ(0x77, mmu_read_byte(&mmu, &cart, &timer, 0xE000), "Echo RAM mirrors WRAM");
    /* Write to echo should affect WRAM */
    mmu_write_byte(&mmu, &cart, &timer, 0xE005, 0x88);
    ASSERT_EQ(0x88, mmu_read_byte(&mmu, &cart, &timer, 0xC005), "Echo write mirrors to WRAM");
}

/* ========== OAM ========== */
static void test_oam_read_write(void) {
    setup();
    mmu_write_byte(&mmu, &cart, &timer, 0xFE00, 0x10);
    mmu_write_byte(&mmu, &cart, &timer, 0xFE9F, 0x20);
    ASSERT_EQ(0x10, mmu_read_byte(&mmu, &cart, &timer, 0xFE00), "OAM start");
    ASSERT_EQ(0x20, mmu_read_byte(&mmu, &cart, &timer, 0xFE9F), "OAM end");
}

/* ========== Unusable region ========== */
static void test_unusable_region(void) {
    setup();
    ASSERT_EQ(0xFF, mmu_read_byte(&mmu, &cart, &timer, 0xFEA0), "Unusable region returns 0xFF");
    ASSERT_EQ(0xFF, mmu_read_byte(&mmu, &cart, &timer, 0xFEFF), "Unusable region end returns 0xFF");
}

/* ========== HRAM ========== */
static void test_hram_read_write(void) {
    setup();
    mmu_write_byte(&mmu, &cart, &timer, 0xFF80, 0xDE);
    mmu_write_byte(&mmu, &cart, &timer, 0xFFFE, 0xAD);
    ASSERT_EQ(0xDE, mmu_read_byte(&mmu, &cart, &timer, 0xFF80), "HRAM start");
    ASSERT_EQ(0xAD, mmu_read_byte(&mmu, &cart, &timer, 0xFFFE), "HRAM end");
}

/* ========== IE register (0xFFFF) ========== */
static void test_ie_register(void) {
    setup();
    mmu_write_byte(&mmu, &cart, &timer, 0xFFFF, 0x1F);
    ASSERT_EQ(0x1F, mmu_read_byte(&mmu, &cart, &timer, 0xFFFF), "IE register");
    ASSERT_EQ(0x1F, mmu.io[0x7F], "IE stored in io[0x7F]");
}

/* ========== LY reset on write ========== */
static void test_ly_reset(void) {
    setup();
    mmu.io[0x44] = 0x90; /* LY = 144 */
    mmu_write_byte(&mmu, &cart, &timer, 0xFF44, 0x55);
    ASSERT_EQ(0x00, mmu.io[0x44], "Writing to LY resets to 0");
}

/* ========== Cartridge RAM ========== */
static void test_cart_ram_disabled(void) {
    setup();
    cart.ram_enabled = false;
    ASSERT_EQ(0xFF, mmu_read_byte(&mmu, &cart, &timer, 0xA000), "Cart RAM disabled returns 0xFF");
}

static void test_cart_ram_enabled(void) {
    setup();
    cart.ram_enabled = true;
    cart_ram[0] = 0x42;
    cart_ram[0x1FFF] = 0x99;
    ASSERT_EQ(0x42, mmu_read_byte(&mmu, &cart, &timer, 0xA000), "Cart RAM start");
    ASSERT_EQ(0x99, mmu_read_byte(&mmu, &cart, &timer, 0xBFFF), "Cart RAM end");
}

/* ========== RAM enable register (MBC1) ========== */
static void test_mbc1_ram_enable(void) {
    setup();
    cart.type = 0x01; /* MBC1 */
    cart.ram_enabled = false;
    mmu_write_byte(&mmu, &cart, &timer, 0x0000, 0x0A); /* Enable */
    ASSERT_TRUE(cart.ram_enabled, "RAM enabled by 0x0A");
    mmu_write_byte(&mmu, &cart, &timer, 0x0000, 0x00); /* Disable */
    ASSERT_FALSE(cart.ram_enabled, "RAM disabled by 0x00");
}

/* ========== ROM bank switching (MBC1) ========== */
static void test_mbc1_rom_bank(void) {
    setup();
    cart.type = 0x01; /* MBC1 */
    mmu_write_byte(&mmu, &cart, &timer, 0x2000, 0x05);
    ASSERT_EQ(5, cart.rom_bank, "MBC1 ROM bank 5");
    mmu_write_byte(&mmu, &cart, &timer, 0x2000, 0x00);
    ASSERT_EQ(1, cart.rom_bank, "MBC1 ROM bank 0 maps to 1");
}

/* ========== Joypad register ========== */
static void test_joypad_no_selection(void) {
    setup();
    mmu.io[0x00] = 0x30; /* Both P14/P15 high = no selection */
    mmu.joypad = 0x00;    /* All pressed */
    uint8_t val = mmu_read_byte(&mmu, &cart, &timer, 0xFF00);
    /* No selection -> nibble = 0x0F (no buttons) */
    ASSERT_EQ(0xFF, val, "Joypad no selection returns 0x3F");
}

static void test_joypad_dpad(void) {
    setup();
    mmu.io[0x00] = 0x20; /* P14 low (d-pad selected), P15 high */
    mmu.joypad = 0xFB;   /* Up pressed (bit 2 clear) = 0b11111011 */
    uint8_t val = mmu_read_byte(&mmu, &cart, &timer, 0xFF00);
    uint8_t dpad = val & 0x0F;
    /* Active-low: bit 2 = 0 (pressed) */
    ASSERT_EQ(0x0B, dpad, "D-pad: Up pressed (bit 2 = 0)");
}

static void test_joypad_buttons(void) {
    setup();
    mmu.io[0x00] = 0x10; /* P15 low (buttons selected), P14 high */
    mmu.joypad = 0xEF;   /* A pressed (bit 4 clear) = 0b11101111 */
    uint8_t val = mmu_read_byte(&mmu, &cart, &timer, 0xFF00);
    uint8_t btns = val & 0x0F;
    /* A is bit 4 of joypad, maps to bit 0 of returned nibble */
    ASSERT_EQ(0x0E, btns, "Buttons: A pressed (bit 0 = 0)");
}

/* ========== MMU init ========== */
static void test_mmu_init_zeroed(void) {
    setup();
    int all_zero = 1;
    for (int i = 0; i < 0x2000; i++) {
        if (mmu.vram[i] != 0) { all_zero = 0; break; }
    }
    ASSERT_TRUE(all_zero, "VRAM zeroed after init");

    all_zero = 1;
    for (int i = 0; i < 0x2000; i++) {
        if (mmu.wram[i] != 0) { all_zero = 0; break; }
    }
    ASSERT_TRUE(all_zero, "WRAM zeroed after init");
    ASSERT_EQ(0xFF, mmu.joypad, "Joypad init to 0xFF (all released)");
}

void run_mmu_tests(void) {
    TEST_SUITE("MMU Tests");

    RUN_TEST(test_rom_bank0_read);
    RUN_TEST(test_rom_bank1_read);
    RUN_TEST(test_boot_rom_overlay);
    RUN_TEST(test_boot_rom_disable);
    RUN_TEST(test_vram_read_write);
    RUN_TEST(test_wram_read_write);
    RUN_TEST(test_echo_ram);
    RUN_TEST(test_oam_read_write);
    RUN_TEST(test_unusable_region);
    RUN_TEST(test_hram_read_write);
    RUN_TEST(test_ie_register);
    RUN_TEST(test_ly_reset);
    RUN_TEST(test_cart_ram_disabled);
    RUN_TEST(test_cart_ram_enabled);
    RUN_TEST(test_mbc1_ram_enable);
    RUN_TEST(test_mbc1_rom_bank);
    RUN_TEST(test_joypad_no_selection);
    RUN_TEST(test_joypad_dpad);
    RUN_TEST(test_joypad_buttons);
    RUN_TEST(test_mmu_init_zeroed);
}
