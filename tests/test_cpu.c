#include "test_framework.h"
#include "gameboy.h"
#include <string.h>

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

/* Helper: set up CPU/MMU/Cart with a small program in WRAM mapped as ROM */
static CPU cpu;
static MMU mmu;
static Cartridge cart;
static Timer timer;
static OAM oam;
static Bus bus;
static uint8_t rom_data[0x8000];

static void setup(void) {
    memset(&cpu, 0, sizeof(cpu));
    memset(&mmu, 0, sizeof(mmu));
    memset(&cart, 0, sizeof(cart));
    memset(&timer, 0, sizeof(timer));
    memset(rom_data, 0, sizeof(rom_data));
    mmu_init(&mmu);
    cpu_init(&cpu);
    timer_init(&mmu, &timer);
    cart.rom = rom_data;
    cart.rom_size = sizeof(rom_data);
    cart.rom_bank = 1;
    cart.boot_rom_enabled = false;
    bus.mmu   = &mmu;
    bus.cart  = &cart;
    bus.timer = &timer;
    bus.oam   = &oam;
}

/* Place opcodes at a given PC and set PC there */
static void place_code(uint16_t addr, const uint8_t *code, int len) {
    if (addr < 0x8000) {
        memcpy(&rom_data[addr], code, len);
    } else if (addr >= 0xC000 && addr < 0xE000) {
        memcpy(&mmu.wram[addr - 0xC000], code, len);
    } else if (addr >= 0xFF80 && addr < 0xFFFF) {
        memcpy(&mmu.hram[addr - 0xFF80], code, len);
    }
}

/* ========== NOP ========== */
static void test_nop(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x00}; /* NOP */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(4, cycles, "NOP cycles");
    ASSERT_EQ(0x0101, cpu.pc, "NOP advances PC");
}

/* ========== LD reg, imm8 ========== */
static void test_ld_b_imm8(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x06, 0x42}; /* LD B, 0x42 */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(8, cycles, "LD B,d8 cycles");
    ASSERT_EQ(0x42, cpu.b, "LD B,d8 value");
}

static void test_ld_c_imm8(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x0E, 0x55}; /* LD C, 0x55 */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x55, cpu.c, "LD C,d8 value");
}

static void test_ld_a_imm8(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x3E, 0xAB}; /* LD A, 0xAB */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xAB, cpu.a, "LD A,d8 value");
}

/* ========== LD reg16, imm16 ========== */
static void test_ld_bc_imm16(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x01, 0x34, 0x12}; /* LD BC, 0x1234 */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(12, cycles, "LD BC,d16 cycles");
    ASSERT_EQ(0x12, cpu.b, "LD BC,d16 B");
    ASSERT_EQ(0x34, cpu.c, "LD BC,d16 C");
}

static void test_ld_de_imm16(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x11, 0xCD, 0xAB}; /* LD DE, 0xABCD */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xAB, cpu.d, "LD DE,d16 D");
    ASSERT_EQ(0xCD, cpu.e, "LD DE,d16 E");
}

static void test_ld_hl_imm16(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x21, 0x00, 0xC0}; /* LD HL, 0xC000 */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xC0, cpu.h, "LD HL,d16 H");
    ASSERT_EQ(0x00, cpu.l, "LD HL,d16 L");
}

static void test_ld_sp_imm16(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x31, 0xFE, 0xFF}; /* LD SP, 0xFFFE */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xFFFE, cpu.sp, "LD SP,d16 value");
}

/* ========== INC / DEC 8-bit ========== */
static void test_inc_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x0F;
    uint8_t code[] = {0x04}; /* INC B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x10, cpu.b, "INC B value");
    ASSERT_TRUE(cpu.f & FLAG_H, "INC B half-carry set");
    ASSERT_FALSE(cpu.f & FLAG_Z, "INC B zero clear");
    ASSERT_FALSE(cpu.f & FLAG_N, "INC B subtract clear");
}

static void test_inc_b_zero(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0xFF;
    uint8_t code[] = {0x04}; /* INC B wraps to 0 */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.b, "INC B wraps to 0");
    ASSERT_TRUE(cpu.f & FLAG_Z, "INC B zero flag");
    ASSERT_TRUE(cpu.f & FLAG_H, "INC B half-carry on wrap");
}

static void test_dec_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x10;
    uint8_t code[] = {0x05}; /* DEC B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0F, cpu.b, "DEC B value");
    ASSERT_TRUE(cpu.f & FLAG_H, "DEC B half-carry set");
    ASSERT_TRUE(cpu.f & FLAG_N, "DEC B subtract set");
}

static void test_dec_b_zero(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x01;
    uint8_t code[] = {0x05}; /* DEC B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.b, "DEC B to zero");
    ASSERT_TRUE(cpu.f & FLAG_Z, "DEC B zero flag");
}

/* ========== INC / DEC 16-bit ========== */
static void test_inc_bc(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x00; cpu.c = 0xFF;
    uint8_t code[] = {0x03}; /* INC BC */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(8, cycles, "INC BC cycles");
    ASSERT_EQ(0x01, cpu.b, "INC BC high byte");
    ASSERT_EQ(0x00, cpu.c, "INC BC low byte");
}

static void test_dec_bc(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x01; cpu.c = 0x00;
    uint8_t code[] = {0x0B}; /* DEC BC */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.b, "DEC BC high byte");
    ASSERT_EQ(0xFF, cpu.c, "DEC BC low byte");
}

/* ========== ADD A, reg ========== */
static void test_add_a_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x3A;
    cpu.b = 0xC6;
    uint8_t code[] = {0x80}; /* ADD A, B */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(4, cycles, "ADD A,B cycles");
    ASSERT_EQ(0x00, cpu.a, "ADD A,B result (0x3A+0xC6=0x00)");
    ASSERT_TRUE(cpu.f & FLAG_Z, "ADD A,B zero flag");
    ASSERT_TRUE(cpu.f & FLAG_H, "ADD A,B half-carry");
    ASSERT_TRUE(cpu.f & FLAG_C, "ADD A,B carry");
}

static void test_add_a_imm8(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x10;
    uint8_t code[] = {0xC6, 0x20}; /* ADD A, 0x20 */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x30, cpu.a, "ADD A,d8 result");
    ASSERT_FALSE(cpu.f & FLAG_Z, "ADD A,d8 not zero");
    ASSERT_FALSE(cpu.f & FLAG_C, "ADD A,d8 no carry");
}

/* ========== ADC A, reg ========== */
static void test_adc_a_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0xE1;
    cpu.b = 0x0F;
    cpu.f = FLAG_C; /* carry set */
    uint8_t code[] = {0x88}; /* ADC A, B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xF1, cpu.a, "ADC A,B result (0xE1+0x0F+1)");
    ASSERT_TRUE(cpu.f & FLAG_H, "ADC A,B half-carry");
    ASSERT_FALSE(cpu.f & FLAG_C, "ADC A,B no carry");
}

/* ========== SUB A, reg ========== */
static void test_sub_a_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x3E;
    cpu.b = 0x3E;
    uint8_t code[] = {0x90}; /* SUB B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.a, "SUB B result");
    ASSERT_TRUE(cpu.f & FLAG_Z, "SUB B zero");
    ASSERT_TRUE(cpu.f & FLAG_N, "SUB B subtract");
    ASSERT_FALSE(cpu.f & FLAG_H, "SUB B no half-carry");
    ASSERT_FALSE(cpu.f & FLAG_C, "SUB B no carry");
}

static void test_sub_a_b_borrow(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x10;
    cpu.b = 0x20;
    uint8_t code[] = {0x90}; /* SUB B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xF0, cpu.a, "SUB B borrow result");
    ASSERT_TRUE(cpu.f & FLAG_C, "SUB B carry (borrow)");
}

/* ========== SBC A, reg ========== */
static void test_sbc_a_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x3B;
    cpu.b = 0x2A;
    cpu.f = FLAG_C;
    uint8_t code[] = {0x98}; /* SBC A, B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x10, cpu.a, "SBC A,B result (0x3B-0x2A-1)");
    ASSERT_FALSE(cpu.f & FLAG_Z, "SBC A,B not zero");
    ASSERT_TRUE(cpu.f & FLAG_N, "SBC A,B subtract");
}

/* ========== AND / OR / XOR / CP ========== */
static void test_and_a_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x5A;
    cpu.b = 0x3F;
    uint8_t code[] = {0xA0}; /* AND B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x1A, cpu.a, "AND B result");
    ASSERT_TRUE(cpu.f & FLAG_H, "AND sets H");
    ASSERT_FALSE(cpu.f & FLAG_N, "AND clears N");
    ASSERT_FALSE(cpu.f & FLAG_C, "AND clears C");
}

static void test_or_a_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x5A;
    cpu.b = 0x03;
    uint8_t code[] = {0xB0}; /* OR B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x5B, cpu.a, "OR B result");
    ASSERT_FALSE(cpu.f & FLAG_H, "OR clears H");
}

static void test_xor_a_a(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0xFF;
    uint8_t code[] = {0xAF}; /* XOR A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.a, "XOR A result");
    ASSERT_TRUE(cpu.f & FLAG_Z, "XOR A zero flag");
}

static void test_cp_a_b(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x3C;
    cpu.b = 0x2F;
    uint8_t code[] = {0xB8}; /* CP B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x3C, cpu.a, "CP does not modify A");
    ASSERT_TRUE(cpu.f & FLAG_H, "CP half-carry");
    ASSERT_TRUE(cpu.f & FLAG_N, "CP subtract flag");
    ASSERT_FALSE(cpu.f & FLAG_Z, "CP not zero");
}

static void test_cp_equal(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x42;
    cpu.b = 0x42;
    uint8_t code[] = {0xB8}; /* CP B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_TRUE(cpu.f & FLAG_Z, "CP equal sets zero");
}

/* ========== Rotate/Shift ========== */
static void test_rlca(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x85; /* 10000101 */
    uint8_t code[] = {0x07}; /* RLCA */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0B, cpu.a, "RLCA result"); /* 00001011 */
    ASSERT_TRUE(cpu.f & FLAG_C, "RLCA carry from bit 7");
}

static void test_rrca(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x3B; /* 00111011 */
    uint8_t code[] = {0x0F}; /* RRCA */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x9D, cpu.a, "RRCA result"); /* 10011101 */
    ASSERT_TRUE(cpu.f & FLAG_C, "RRCA carry from bit 0");
}

static void test_rla(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x95; /* 10010101 */
    cpu.f = FLAG_C;
    uint8_t code[] = {0x17}; /* RLA */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x2B, cpu.a, "RLA result"); /* 00101011 */
    ASSERT_TRUE(cpu.f & FLAG_C, "RLA carry from bit 7");
}

static void test_rra(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x81; /* 10000001 */
    cpu.f = 0;
    uint8_t code[] = {0x1F}; /* RRA */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x40, cpu.a, "RRA result"); /* 01000000 */
    ASSERT_TRUE(cpu.f & FLAG_C, "RRA carry from bit 0");
}

/* ========== LD (HL), A and LD A, (HL) ========== */
static void test_ld_hl_a(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x77;
    cpu.h = 0xC0; cpu.l = 0x00; /* HL = 0xC000 (WRAM) */
    uint8_t code[] = {0x77}; /* LD (HL), A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x77, mmu.wram[0], "LD (HL),A written to WRAM");
}

static void test_ld_a_hl(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.h = 0xC0; cpu.l = 0x05;
    mmu.wram[5] = 0xBE;
    uint8_t code[] = {0x7E}; /* LD A, (HL) */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xBE, cpu.a, "LD A,(HL) read from WRAM");
}

/* ========== LDI / LDD ========== */
static void test_ldi_hl_a(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x56;
    cpu.h = 0xC0; cpu.l = 0x00;
    uint8_t code[] = {0x22}; /* LD (HL+), A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x56, mmu.wram[0], "LDI (HL+),A value");
    ASSERT_EQ(0xC0, cpu.h, "LDI HL incremented H");
    ASSERT_EQ(0x01, cpu.l, "LDI HL incremented L");
}

static void test_ldd_hl_a(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x78;
    cpu.h = 0xC0; cpu.l = 0x01;
    uint8_t code[] = {0x32}; /* LD (HL-), A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x78, mmu.wram[1], "LDD (HL-),A value");
    ASSERT_EQ(0xC0, cpu.h, "LDD HL decremented H");
    ASSERT_EQ(0x00, cpu.l, "LDD HL decremented L");
}

/* ========== LDH (a8), A and LDH A, (a8) ========== */
static void test_ldh_a8_a(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x99;
    uint8_t code[] = {0xE0, 0x80}; /* LDH (0xFF80), A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x99, mmu.hram[0], "LDH (a8),A writes to HRAM");
}

static void test_ldh_a_a8(void) {
    setup();
    cpu.pc = 0x0100;
    mmu.hram[0] = 0x44;
    uint8_t code[] = {0xF0, 0x80}; /* LDH A, (0xFF80) */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x44, cpu.a, "LDH A,(a8) reads from HRAM");
}

/* ========== JR (relative jump) ========== */
static void test_jr_unconditional(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0x18, 0x05}; /* JR +5 */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0107, cpu.pc, "JR +5 target (0x0102+5)");
    ASSERT_EQ(12, cycles, "JR cycles");
}

static void test_jr_nz_taken(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.f = 0; /* Z not set */
    uint8_t code[] = {0x20, 0x03}; /* JR NZ, +3 */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0105, cpu.pc, "JR NZ taken");
    ASSERT_EQ(12, cycles, "JR NZ taken cycles");
}

static void test_jr_nz_not_taken(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.f = FLAG_Z;
    uint8_t code[] = {0x20, 0x03}; /* JR NZ, +3 */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0102, cpu.pc, "JR NZ not taken");
    ASSERT_EQ(8, cycles, "JR NZ not taken cycles");
}

static void test_jr_backward(void) {
    setup();
    cpu.pc = 0x0110;
    uint8_t code[] = {0x18, 0xFE}; /* JR -2 (infinite loop) */
    place_code(0x0110, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0110, cpu.pc, "JR -2 loops to self");
}

/* ========== JP (absolute jump) ========== */
static void test_jp_imm16(void) {
    setup();
    cpu.pc = 0x0100;
    uint8_t code[] = {0xC3, 0x50, 0x02}; /* JP 0x0250 */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0250, cpu.pc, "JP a16 target");
    ASSERT_EQ(16, cycles, "JP cycles");
}

/* ========== CALL / RET ========== */
static void test_call_ret(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.sp = 0xFFFE;
    /* CALL 0x0200 */
    uint8_t code[] = {0xCD, 0x00, 0x02};
    place_code(0x0100, code, sizeof(code));
    /* RET at 0x0200 */
    uint8_t ret_code[] = {0xC9};
    place_code(0x0200, ret_code, sizeof(ret_code));

    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(24, cycles, "CALL cycles");
    ASSERT_EQ(0x0200, cpu.pc, "CALL target");
    ASSERT_EQ(0xFFFC, cpu.sp, "CALL pushes return addr");

    cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(16, cycles, "RET cycles");
    ASSERT_EQ(0x0103, cpu.pc, "RET returns correctly");
    ASSERT_EQ(0xFFFE, cpu.sp, "RET restores SP");
}

/* ========== PUSH / POP ========== */
static void test_push_pop_bc(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.sp = 0xFFFE;
    cpu.b = 0x12; cpu.c = 0x34;
    uint8_t code[] = {0xC5, 0xD1}; /* PUSH BC, POP DE */
    place_code(0x0100, code, sizeof(code));

    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xFFFC, cpu.sp, "PUSH BC decrements SP");

    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x12, cpu.d, "POP DE gets B in D");
    ASSERT_EQ(0x34, cpu.e, "POP DE gets C in E");
    ASSERT_EQ(0xFFFE, cpu.sp, "POP DE restores SP");
}

static void test_push_pop_af(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.sp = 0xFFFE;
    cpu.a = 0xAB;
    cpu.f = 0xB0; /* Z, H, C flags */
    uint8_t code[] = {0xF5, 0xF1}; /* PUSH AF, POP AF */
    place_code(0x0100, code, sizeof(code));

    cpu_step(&cpu, &bus);
    cpu.a = 0x00; cpu.f = 0x00; /* clear */
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xAB, cpu.a, "POP AF restores A");
    ASSERT_EQ(0xB0, cpu.f, "POP AF restores F (lower 4 bits cleared)");
}

/* ========== ADD HL, rr ========== */
static void test_add_hl_bc(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.h = 0x8A; cpu.l = 0x23;
    cpu.b = 0x06; cpu.c = 0x05;
    uint8_t code[] = {0x09}; /* ADD HL, BC */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    uint16_t hl = ((uint16_t)cpu.h << 8) | cpu.l;
    ASSERT_EQ(0x9028, hl, "ADD HL,BC result");
    ASSERT_TRUE(cpu.f & FLAG_H, "ADD HL,BC half-carry");
    ASSERT_FALSE(cpu.f & FLAG_C, "ADD HL,BC no carry");
}

/* ========== DAA ========== */
static void test_daa(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x45;
    cpu.b = 0x38;
    /* ADD A, B then DAA */
    uint8_t code[] = {0x80, 0x27}; /* ADD A,B; DAA */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus); /* ADD: 0x45 + 0x38 = 0x7D */
    cpu_step(&cpu, &bus); /* DAA: BCD adjust -> 0x83 */
    ASSERT_EQ(0x83, cpu.a, "DAA after ADD (45+38=83 BCD)");
}

/* ========== CCF / SCF / CPL ========== */
static void test_scf(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.f = 0;
    uint8_t code[] = {0x37}; /* SCF */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_TRUE(cpu.f & FLAG_C, "SCF sets carry");
    ASSERT_FALSE(cpu.f & FLAG_N, "SCF clears N");
    ASSERT_FALSE(cpu.f & FLAG_H, "SCF clears H");
}

static void test_ccf(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.f = FLAG_C;
    uint8_t code[] = {0x3F}; /* CCF */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_FALSE(cpu.f & FLAG_C, "CCF complements carry");
}

static void test_cpl(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x35;
    uint8_t code[] = {0x2F}; /* CPL */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xCA, cpu.a, "CPL complements A");
    ASSERT_TRUE(cpu.f & FLAG_N, "CPL sets N");
    ASSERT_TRUE(cpu.f & FLAG_H, "CPL sets H");
}

/* ========== HALT ========== */
static void test_halt(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.ime = true;
    uint8_t code[] = {0x76}; /* HALT */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_TRUE(cpu.halted, "HALT sets halted");

    /* While halted, cpu_step returns 4 cycles */
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(4, cycles, "Halted returns 4 cycles");
    ASSERT_TRUE(cpu.halted, "Still halted without interrupt");
}

/* ========== EI / DI ========== */
static void test_ei_di(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.ime = false;
    /* EI, NOP (IME enabled after NOP), DI */
    uint8_t code[] = {0xFB, 0x00, 0xF3};
    place_code(0x0100, code, sizeof(code));

    cpu_step(&cpu, &bus); /* EI: sets ime_delay */
    ASSERT_FALSE(cpu.ime, "EI does not immediately enable IME");
    ASSERT_TRUE(cpu.ime_delay, "EI sets ime_delay");

    cpu_step(&cpu, &bus); /* NOP: ime_delay -> ime */
    ASSERT_TRUE(cpu.ime, "IME enabled after instruction following EI");

    cpu_step(&cpu, &bus); /* DI */
    ASSERT_FALSE(cpu.ime, "DI disables IME");
}

/* ========== Interrupts ========== */
static void test_vblank_interrupt(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.sp = 0xFFFE;
    cpu.ime = true;
    mmu.io[0x7F] = 0x01; /* IE: V-Blank enabled */
    mmu.io[0x0F] = 0x01; /* IF: V-Blank pending */
    uint8_t code[] = {0x00}; /* NOP (won't execute, interrupt first) */
    place_code(0x0100, code, sizeof(code));
    /* Place RETI at interrupt vector */
    uint8_t reti[] = {0xD9};
    place_code(0x0040, reti, sizeof(reti));

    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(20, cycles, "Interrupt dispatch cycles");
    ASSERT_EQ(0x0040, cpu.pc, "V-Blank vector");
    ASSERT_FALSE(cpu.ime, "IME disabled during interrupt");
    ASSERT_EQ(0x00, mmu.io[0x0F] & 0x01, "IF cleared for V-Blank");
}

static void test_timer_interrupt(void) {
    setup();
    cpu.pc = 0x0200;
    cpu.sp = 0xFFFE;
    cpu.ime = true;
    mmu.io[0x7F] = 0x04; /* IE: Timer enabled */
    mmu.io[0x0F] = 0x04; /* IF: Timer pending */
    uint8_t code[] = {0x00};
    place_code(0x0200, code, sizeof(code));

    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0050, cpu.pc, "Timer interrupt vector");
}

static void test_halt_wake_on_interrupt(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.sp = 0xFFFE;
    cpu.halted = true;
    cpu.ime = true;
    /* No interrupt yet */
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_TRUE(cpu.halted, "Still halted");

    /* Trigger V-Blank */
    mmu.io[0x7F] = 0x01;
    mmu.io[0x0F] = 0x01;
    cycles = cpu_step(&cpu, &bus);
    ASSERT_FALSE(cpu.halted, "Woken from halt");
}

/* ========== CB prefix: BIT, SET, RES, SWAP, SLA, SRA, SRL ========== */
static void test_cb_bit(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x80; /* bit 7 set */
    uint8_t code[] = {0xCB, 0x78}; /* BIT 7, B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_FALSE(cpu.f & FLAG_Z, "BIT 7,B not zero (bit set)");
    ASSERT_TRUE(cpu.f & FLAG_H, "BIT sets H");
    ASSERT_FALSE(cpu.f & FLAG_N, "BIT clears N");
}

static void test_cb_bit_zero(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x00;
    uint8_t code[] = {0xCB, 0x40}; /* BIT 0, B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_TRUE(cpu.f & FLAG_Z, "BIT 0,B zero (bit clear)");
}

static void test_cb_set(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x00;
    uint8_t code[] = {0xCB, 0xC0}; /* SET 0, B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x01, cpu.b, "SET 0,B");
}

static void test_cb_res(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0xFF;
    uint8_t code[] = {0xCB, 0x80}; /* RES 0, B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xFE, cpu.b, "RES 0,B");
}

static void test_cb_swap(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0xF1;
    uint8_t code[] = {0xCB, 0x37}; /* SWAP A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x1F, cpu.a, "SWAP A nibbles");
    ASSERT_FALSE(cpu.f & FLAG_Z, "SWAP A not zero");
}

static void test_cb_swap_zero(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x00;
    uint8_t code[] = {0xCB, 0x37}; /* SWAP A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.a, "SWAP 0 = 0");
    ASSERT_TRUE(cpu.f & FLAG_Z, "SWAP 0 sets Z");
}

static void test_cb_sla(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x80;
    uint8_t code[] = {0xCB, 0x20}; /* SLA B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.b, "SLA B result");
    ASSERT_TRUE(cpu.f & FLAG_Z, "SLA B zero");
    ASSERT_TRUE(cpu.f & FLAG_C, "SLA B carry from bit 7");
}

static void test_cb_sra(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x8A; /* 10001010 */
    uint8_t code[] = {0xCB, 0x28}; /* SRA B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xC5, cpu.b, "SRA B preserves sign bit"); /* 11000101 */
    ASSERT_FALSE(cpu.f & FLAG_C, "SRA B no carry");
}

static void test_cb_srl(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x01;
    uint8_t code[] = {0xCB, 0x3F}; /* SRL A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.a, "SRL A result");
    ASSERT_TRUE(cpu.f & FLAG_Z, "SRL A zero");
    ASSERT_TRUE(cpu.f & FLAG_C, "SRL A carry from bit 0");
}

static void test_cb_rl(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x80;
    cpu.f = 0;
    uint8_t code[] = {0xCB, 0x10}; /* RL B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.b, "RL B result");
    ASSERT_TRUE(cpu.f & FLAG_C, "RL B carry");
    ASSERT_TRUE(cpu.f & FLAG_Z, "RL B zero");
}

static void test_cb_rr(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0x01;
    cpu.f = 0;
    uint8_t code[] = {0xCB, 0x18}; /* RR B */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0x00, cpu.b, "RR B result");
    ASSERT_TRUE(cpu.f & FLAG_C, "RR B carry");
    ASSERT_TRUE(cpu.f & FLAG_Z, "RR B zero");
}

/* ========== RST ========== */
static void test_rst_38(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.sp = 0xFFFE;
    uint8_t code[] = {0xFF}; /* RST 0x38 */
    place_code(0x0100, code, sizeof(code));
    int cycles = cpu_step(&cpu, &bus);
    ASSERT_EQ(0x0038, cpu.pc, "RST 0x38 target");
    ASSERT_EQ(16, cycles, "RST cycles");
}

/* ========== LD (a16), SP ========== */
static void test_ld_a16_sp(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.sp = 0xFFF8;
    uint8_t code[] = {0x08, 0x00, 0xC0}; /* LD (0xC000), SP */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &bus);
    ASSERT_EQ(0xF8, mmu.wram[0], "LD (a16),SP low byte");
    ASSERT_EQ(0xFF, mmu.wram[1], "LD (a16),SP high byte");
}

/* ========== Small program: loop and count ========== */
static void test_small_program(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.b = 0xFF; /* avoid early break from cpu_init B=0 */
    /* Program: LD A, 0; LD B, 5; loop: INC A; DEC B; JR NZ, loop; HALT */
    uint8_t code[] = {
        0x3E, 0x00,       /* LD A, 0 */
        0x06, 0x05,       /* LD B, 5 */
        0x3C,             /* INC A */
        0x05,             /* DEC B */
        0x20, 0xFC,       /* JR NZ, -4 (back to INC A) */
        0x76,             /* HALT */
    };
    place_code(0x0100, code, sizeof(code));

    /* Execute until HALT */
    for (int i = 0; i < 100; i++) {
        cpu_step(&cpu, &bus);
        if (cpu.halted) break;
    }
    ASSERT_EQ(5, cpu.a, "Loop counted to 5");
    ASSERT_EQ(0, cpu.b, "B decremented to 0");
}

void run_cpu_tests(void) {
    TEST_SUITE("CPU Tests");

    RUN_TEST(test_nop);
    RUN_TEST(test_ld_b_imm8);
    RUN_TEST(test_ld_c_imm8);
    RUN_TEST(test_ld_a_imm8);
    RUN_TEST(test_ld_bc_imm16);
    RUN_TEST(test_ld_de_imm16);
    RUN_TEST(test_ld_hl_imm16);
    RUN_TEST(test_ld_sp_imm16);
    RUN_TEST(test_inc_b);
    RUN_TEST(test_inc_b_zero);
    RUN_TEST(test_dec_b);
    RUN_TEST(test_dec_b_zero);
    RUN_TEST(test_inc_bc);
    RUN_TEST(test_dec_bc);
    RUN_TEST(test_add_a_b);
    RUN_TEST(test_add_a_imm8);
    RUN_TEST(test_adc_a_b);
    RUN_TEST(test_sub_a_b);
    RUN_TEST(test_sub_a_b_borrow);
    RUN_TEST(test_sbc_a_b);
    RUN_TEST(test_and_a_b);
    RUN_TEST(test_or_a_b);
    RUN_TEST(test_xor_a_a);
    RUN_TEST(test_cp_a_b);
    RUN_TEST(test_cp_equal);
    RUN_TEST(test_rlca);
    RUN_TEST(test_rrca);
    RUN_TEST(test_rla);
    RUN_TEST(test_rra);
    RUN_TEST(test_ld_hl_a);
    RUN_TEST(test_ld_a_hl);
    RUN_TEST(test_ldi_hl_a);
    RUN_TEST(test_ldd_hl_a);
    RUN_TEST(test_ldh_a8_a);
    RUN_TEST(test_ldh_a_a8);
    RUN_TEST(test_jr_unconditional);
    RUN_TEST(test_jr_nz_taken);
    RUN_TEST(test_jr_nz_not_taken);
    RUN_TEST(test_jr_backward);
    RUN_TEST(test_jp_imm16);
    RUN_TEST(test_call_ret);
    RUN_TEST(test_push_pop_bc);
    RUN_TEST(test_push_pop_af);
    RUN_TEST(test_add_hl_bc);
    RUN_TEST(test_daa);
    RUN_TEST(test_scf);
    RUN_TEST(test_ccf);
    RUN_TEST(test_cpl);
    RUN_TEST(test_halt);
    RUN_TEST(test_ei_di);
    RUN_TEST(test_vblank_interrupt);
    RUN_TEST(test_timer_interrupt);
    RUN_TEST(test_halt_wake_on_interrupt);
    RUN_TEST(test_cb_bit);
    RUN_TEST(test_cb_bit_zero);
    RUN_TEST(test_cb_set);
    RUN_TEST(test_cb_res);
    RUN_TEST(test_cb_swap);
    RUN_TEST(test_cb_swap_zero);
    RUN_TEST(test_cb_sla);
    RUN_TEST(test_cb_sra);
    RUN_TEST(test_cb_srl);
    RUN_TEST(test_cb_rl);
    RUN_TEST(test_cb_rr);
    RUN_TEST(test_rst_38);
    RUN_TEST(test_ld_a16_sp);
    RUN_TEST(test_small_program);
}
