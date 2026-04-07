#include "test_framework.h"
#include "gameboy.h"
#include <string.h>

extern void cart_init(Cartridge *cart);
extern uint8_t cart_read_rom(Cartridge *cart, uint16_t addr);
extern void cart_write_ram(Cartridge *cart, uint16_t addr, uint8_t value);
extern uint8_t cart_read_ram(Cartridge *cart, uint16_t addr);
extern void timer_tick(MMU *mmu, Timer *timer, uint32_t cycles);
extern void timer_init(MMU *mmu, Timer *timer);

static Cartridge cart;
static uint8_t rom_data[0x10000]; /* 64KB = 4 banks */
static uint8_t ram_data[0x2000];

static void setup(void) {
    memset(&cart, 0, sizeof(cart));
    memset(rom_data, 0, sizeof(rom_data));
    memset(ram_data, 0, sizeof(ram_data));
    cart_init(&cart);
    cart.rom = rom_data;
    cart.rom_size = sizeof(rom_data);
    cart.ram = ram_data;
    cart.ram_size = sizeof(ram_data);
}

/* ========== cart_init ========== */
static void test_cart_init(void) {
    Cartridge c;
    memset(&c, 0xFF, sizeof(c));
    cart_init(&c);
    ASSERT_EQ(1, c.rom_bank, "cart_init rom_bank = 1");
    ASSERT_EQ(0, c.ram_bank, "cart_init ram_bank = 0");
    ASSERT_FALSE(c.ram_enabled, "cart_init ram disabled");
    ASSERT_TRUE(c.rom == NULL, "cart_init rom = NULL");
    ASSERT_TRUE(c.ram == NULL, "cart_init ram = NULL");
}

/* ========== ROM bank 0 read ========== */
static void test_cart_read_rom_bank0(void) {
    setup();
    rom_data[0x0000] = 0x31;
    rom_data[0x3FFF] = 0xAA;
    ASSERT_EQ(0x31, cart_read_rom(&cart, 0x0000), "ROM bank 0 start");
    ASSERT_EQ(0xAA, cart_read_rom(&cart, 0x3FFF), "ROM bank 0 end");
}

/* ========== ROM bank N read ========== */
static void test_cart_read_rom_bank_switch(void) {
    setup();
    cart.rom_bank = 2;
    rom_data[0x8000] = 0xBB; /* Bank 2 at offset 0x8000 */
    ASSERT_EQ(0xBB, cart_read_rom(&cart, 0x4000), "ROM bank 2 read");
}

static void test_cart_read_rom_out_of_bounds(void) {
    setup();
    cart.rom_size = 0x8000; /* Only 2 banks */
    cart.rom_bank = 3;      /* Bank 3 doesn't exist */
    ASSERT_EQ(0xFF, cart_read_rom(&cart, 0x4000), "ROM out of bounds returns 0xFF");
}

/* ========== Cart RAM read/write ========== */
static void test_cart_ram_write_read(void) {
    setup();
    cart_write_ram(&cart, 0xA000, 0x42);
    ASSERT_EQ(0x42, ram_data[0], "Cart RAM write stored");
    ASSERT_EQ(0x42, cart_read_ram(&cart, 0xA000), "Cart RAM read back");
}

static void test_cart_ram_out_of_range(void) {
    setup();
    /* Address below 0xA000 should not write */
    cart_write_ram(&cart, 0x9FFF, 0xFF);
    ASSERT_EQ(0xFF, cart_read_ram(&cart, 0x9FFF), "RAM read out of range returns 0xFF");
}

static void test_cart_ram_no_ram(void) {
    setup();
    cart.ram = NULL;
    ASSERT_EQ(0xFF, cart_read_ram(&cart, 0xA000), "No RAM returns 0xFF");
}

/* ========== Timer init ========== */
static void test_timer_init(void) {
    MMU mmu;
    Timer timer;
    memset(&mmu, 0xFF, sizeof(mmu));
    memset(&timer, 0xFF, sizeof(timer));
    timer_init(&mmu, &timer);
    ASSERT_EQ(0, mmu.io[0x04], "Timer DIV init 0");
    ASSERT_EQ(0, mmu.io[0x05], "Timer TIMA init 0");
    ASSERT_EQ(0, mmu.io[0x06], "Timer TMA init 0");
    ASSERT_EQ(0, mmu.io[0x07], "Timer TAC init 0");
    ASSERT_EQ(0, timer.div_counter, "Timer div_counter init 0");
    ASSERT_EQ(0, timer.tima_counter, "Timer tima_counter init 0");
}

static void test_timer_tick_div(void) {
    Timer timer;
    MMU mmu;
    memset(&timer, 0, sizeof(timer));
    memset(&mmu, 0, sizeof(mmu));
    timer_init(&mmu, &timer);

    timer_tick(&mmu, &timer, 256);
    ASSERT_EQ(1, mmu.io[0x04], "DIV increments after 256 cycles");

    timer_tick(&mmu, &timer, 256);
    ASSERT_EQ(2, mmu.io[0x04], "DIV increments again after 256 cycles");

    Timer timer2;
    MMU mmu2;
    memset(&timer2, 0, sizeof(timer2));
    memset(&mmu2, 0, sizeof(mmu2));
    timer_init(&mmu2, &timer2);
    timer_tick(&mmu2, &timer2, 512);
    ASSERT_EQ(2, mmu2.io[0x04], "DIV increments 2 times after 512 cycles");
}

static void test_timer_tick_tima(void) {
    Timer timer;
    MMU mmu;
    memset(&timer, 0, sizeof(timer));
    memset(&mmu, 0, sizeof(mmu));
    timer_init(&mmu, &timer);

    mmu.io[0x07] = 0x04;
    mmu.io[0x06] = 0x00;
    timer_tick(&mmu, &timer, 256);
    ASSERT_EQ(1, mmu.io[0x05], "TIMA increments after 256 cycles with TAC=0x04");

    mmu.io[0x05] = 0xFF;
    timer.tima_counter = 0;
    mmu.io[0x0F] = 0;
    timer_tick(&mmu, &timer, 256);
    ASSERT_EQ(mmu.io[0x06], mmu.io[0x05], "TIMA resets to TMA on overflow");
    ASSERT_EQ(0x04, mmu.io[0x0F], "Timer interrupt requested on overflow");
}

static void test_timer_div_reset(void) {
    Timer timer;
    MMU mmu;
    memset(&timer, 0, sizeof(timer));
    memset(&mmu, 0, sizeof(mmu));
    timer_init(&mmu, &timer);

    timer_tick(&mmu, &timer, 256);
    ASSERT_EQ(1, mmu.io[0x04], "DIV incremented");

    mmu_write_byte(&mmu, NULL, &timer, 0xFF04, 0x00);
    ASSERT_EQ(0, mmu.io[0x04], "DIV reset after write to 0xFF04");
    ASSERT_EQ(0, timer.div_counter, "DIV counter reset after write to 0xFF04");
}

static void test_timer_tma_write(void) {
    Timer timer;
    MMU mmu;
    memset(&timer, 0, sizeof(timer));
    memset(&mmu, 0, sizeof(mmu));
    timer_init(&mmu, &timer);

    mmu_write_byte(&mmu, NULL, &timer, 0xFF06, 0xAB);
    ASSERT_EQ(0xAB, mmu.io[0x06], "Write TMA");

    mmu_write_byte(&mmu, NULL, &timer, 0xFF07, 0x05);
    ASSERT_EQ(0x05, mmu.io[0x07], "Write TAC");
}

void run_cartridge_tests(void) {
    TEST_SUITE("Cartridge & Timer Tests");

    RUN_TEST(test_cart_init);
    RUN_TEST(test_cart_read_rom_bank0);
    RUN_TEST(test_cart_read_rom_bank_switch);
    RUN_TEST(test_cart_read_rom_out_of_bounds);
    RUN_TEST(test_cart_ram_write_read);
    RUN_TEST(test_cart_ram_out_of_range);
    RUN_TEST(test_cart_ram_no_ram);
    RUN_TEST(test_timer_init);
    RUN_TEST(test_timer_tick_div);
    RUN_TEST(test_timer_tick_tima);
    RUN_TEST(test_timer_div_reset);
    RUN_TEST(test_timer_tma_write);
}
