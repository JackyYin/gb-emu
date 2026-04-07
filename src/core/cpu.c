#include "gameboy.h"
#include <stdio.h>

uint8_t mmu_read_byte(MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr);
void mmu_write_byte(MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr, uint8_t value);

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

static uint8_t read_byte(CPU *cpu, MMU *mmu, Cartridge *cart, Timer *timer) {
    uint8_t val = mmu_read_byte(mmu, cart, timer, cpu->pc);
    cpu->pc++;
    return val;
}

static uint16_t read_word(CPU *cpu, MMU *mmu, Cartridge *cart, Timer *timer) {
    uint16_t lo = read_byte(cpu, mmu, cart, timer);
    uint16_t hi = read_byte(cpu, mmu, cart, timer);
    return (hi << 8) | lo;
}

static uint8_t read_mem(CPU *cpu, MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr) {
    return mmu_read_byte(mmu, cart, timer, addr);
}

static void write_mem(CPU *cpu, MMU *mmu, Cartridge *cart, Timer *timer, uint16_t addr, uint8_t val) {
    mmu_write_byte(mmu, cart, timer, addr, val);
}

static void push(CPU *cpu, MMU *mmu, Cartridge *cart, Timer *timer, uint8_t hi, uint8_t lo) {
    cpu->sp -= 2;
    write_mem(cpu, mmu, cart, timer, cpu->sp, lo);
    write_mem(cpu, mmu, cart, timer, cpu->sp + 1, hi);
}

static void pop(CPU *cpu, MMU *mmu, Cartridge *cart, Timer *timer, uint8_t *hi, uint8_t *lo) {
    *lo = read_mem(cpu, mmu, cart, timer, cpu->sp);
    *hi = read_mem(cpu, mmu, cart, timer, cpu->sp + 1);
    cpu->sp += 2;
}

int cpu_step(CPU *cpu, MMU *mmu, Cartridge *cart, Timer *timer) {
    static int trace_count;
    uint8_t opcode;
    int cycles = 0;

    if (cpu->halted) {
        uint8_t ie = mmu->io[0x7F];
        uint8_t if_ = mmu->io[0x0F];
        if (ie & if_) {
            cpu->halted = false;
        }
        return 4;
    }

    if (cpu->ime_delay) {
        cpu->ime = true;
        cpu->ime_delay = false;
    }

    if (cpu->ime) {
        uint8_t ie = mmu->io[0x7F];
        uint8_t if_ = mmu->io[0x0F];
        uint8_t pending = ie & if_;
        if (pending) {
            cpu->ime = false;
            cpu->ime_delay = false;
            if (pending & 0x01) {
                mmu->io[0x0F] &= ~0x01;
                cpu->sp -= 2;
                write_mem(cpu, mmu, cart, timer, cpu->sp, cpu->pc & 0xFF);
                write_mem(cpu, mmu, cart, timer, cpu->sp + 1, cpu->pc >> 8);
                cpu->pc = 0x0040;
                return 20;
            }
            if (pending & 0x02) {
                mmu->io[0x0F] &= ~0x02;
                cpu->sp -= 2;
                write_mem(cpu, mmu, cart, timer, cpu->sp, cpu->pc & 0xFF);
                write_mem(cpu, mmu, cart, timer, cpu->sp + 1, cpu->pc >> 8);
                cpu->pc = 0x0048;
                return 20;
            }
            if (pending & 0x04) {
                mmu->io[0x0F] &= ~0x04;
                cpu->sp -= 2;
                write_mem(cpu, mmu, cart, timer, cpu->sp, cpu->pc & 0xFF);
                write_mem(cpu, mmu, cart, timer, cpu->sp + 1, cpu->pc >> 8);
                cpu->pc = 0x0050;
                return 20;
            }
            if (pending & 0x08) {
                mmu->io[0x0F] &= ~0x08;
                cpu->sp -= 2;
                write_mem(cpu, mmu, cart, timer, cpu->sp, cpu->pc & 0xFF);
                write_mem(cpu, mmu, cart, timer, cpu->sp + 1, cpu->pc >> 8);
                cpu->pc = 0x0058;
                return 20;
            }
            if (pending & 0x10) {
                mmu->io[0x0F] &= ~0x10;
                cpu->sp -= 2;
                write_mem(cpu, mmu, cart, timer, cpu->sp, cpu->pc & 0xFF);
                write_mem(cpu, mmu, cart, timer, cpu->sp + 1, cpu->pc >> 8);
                cpu->pc = 0x0060;
                return 20;
            }
        }
    }

    opcode = read_byte(cpu, mmu, cart, timer);

    /* if (opcode != 0x18) */
    /*     printf("  instr %d: PC=0x%04X, opcode=0x%02X\n", trace_count, cpu->pc - 1, opcode); */
    /* trace_count++; */

    switch (opcode) {
        case 0x00: /* NOP */
            cycles = 4;
            break;
        case 0x01: /* LD BC, d16 */
            cpu->c = read_byte(cpu, mmu, cart, timer);
            cpu->b = read_byte(cpu, mmu, cart, timer);
            cycles = 12;
            break;
        case 0x02: /* LD (BC), A */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->b << 8 | cpu->c, cpu->a);
            cycles = 8;
            break;
        case 0x03: /* INC BC */
            {
                uint16_t bc = ((uint16_t)cpu->b << 8) | cpu->c;
                bc++;
                cpu->b = bc >> 8;
                cpu->c = bc & 0xFF;
            }
            cycles = 8;
            break;
        case 0x04: /* INC B */
            cpu->f = (cpu->f & FLAG_C) | ((cpu->b & 0x0F) == 0x0F ? FLAG_H : 0);
            cpu->b++;
            cpu->f |= (cpu->b == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x05: /* DEC B */
            cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((cpu->b & 0x0F) == 0x00 ? FLAG_H : 0);
            cpu->b--;
            cpu->f |= (cpu->b == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x06: /* LD B, d8 */
            cpu->b = read_byte(cpu, mmu, cart, timer);
            cycles = 8;
            break;
        case 0x07: /* RLCA */
            cpu->f = (cpu->a & 0x80) ? FLAG_C : 0;
            cpu->a = (cpu->a << 1) | (cpu->a >> 7);
            cycles = 4;
            break;
        case 0x08: /* LD (a16), SP */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                write_mem(cpu, mmu, cart, timer, addr, cpu->sp & 0xFF);
                write_mem(cpu, mmu, cart, timer, addr + 1, cpu->sp >> 8);
            }
            cycles = 20;
            break;
        case 0x09: /* ADD HL, BC */
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                uint16_t bc = (uint16_t)cpu->b << 8 | cpu->c;
                uint32_t res = hl + bc;
                cpu->f = (cpu->f & FLAG_Z) | ((hl & 0x0FFF) + (bc & 0x0FFF) > 0x0FFF ? FLAG_H : 0) | (res > 0xFFFF ? FLAG_C : 0);
                cpu->h = (res >> 8) & 0xFF;
                cpu->l = res & 0xFF;
            }
            cycles = 8;
            break;
        case 0x0A: /* LD A, (BC) */
            cpu->a = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->b << 8 | cpu->c);
            cycles = 8;
            break;
        case 0x0B: /* DEC BC */
            {
                uint16_t bc = ((uint16_t)cpu->b << 8) | cpu->c;
                bc--;
                cpu->b = bc >> 8;
                cpu->c = bc & 0xFF;
            }
            cycles = 8;
            break;
        case 0x0C: /* INC C */
            cpu->f = (cpu->f & FLAG_C) | ((cpu->c & 0x0F) == 0x0F ? FLAG_H : 0);
            cpu->c++;
            cpu->f |= (cpu->c == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x0D: /* DEC C */
            cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((cpu->c & 0x0F) == 0x00 ? FLAG_H : 0);
            cpu->c--;
            cpu->f |= (cpu->c == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x0E: /* LD C, d8 */
            cpu->c = read_byte(cpu, mmu, cart, timer);
            cycles = 8;
            break;
        case 0x0F: /* RRCA */
            cpu->f = (cpu->a & 0x01) ? FLAG_C : 0;
            cpu->a = (cpu->a >> 1) | ((cpu->a & 0x01) << 7);
            cycles = 4;
            break;
        case 0x10: /* STOP */
            read_byte(cpu, mmu, cart, timer);
            cycles = 4;
            break;
        case 0x11: /* LD DE, d16 */
            cpu->e = read_byte(cpu, mmu, cart, timer);
            cpu->d = read_byte(cpu, mmu, cart, timer);
            cycles = 12;
            break;
        case 0x12: /* LD (DE), A */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->d << 8 | cpu->e, cpu->a);
            cycles = 8;
            break;
        case 0x13: /* INC DE */
            {
                uint16_t de = ((uint16_t)cpu->d << 8) | cpu->e;
                de++;
                cpu->d = de >> 8;
                cpu->e = de & 0xFF;
            }
            cycles = 8;
            break;
        case 0x14: /* INC D */
            cpu->f = (cpu->f & FLAG_C) | ((cpu->d & 0x0F) == 0x0F ? FLAG_H : 0);
            cpu->d++;
            cpu->f |= (cpu->d == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x15: /* DEC D */
            cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((cpu->d & 0x0F) == 0x00 ? FLAG_H : 0);
            cpu->d--;
            cpu->f |= (cpu->d == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x16: /* LD D, d8 */
            cpu->d = read_byte(cpu, mmu, cart, timer);
            cycles = 8;
            break;
        case 0x17: /* RLA */
            {
                uint8_t old_c = (cpu->f & FLAG_C) ? 1 : 0;
                cpu->f = (cpu->a & 0x80) ? FLAG_C : 0;
                cpu->a = (cpu->a << 1) | old_c;
            }
            cycles = 4;
            break;
        case 0x18: /* JR r8 */
            {
                int8_t offset = (int8_t)read_byte(cpu, mmu, cart, timer);
                cpu->pc += offset;
                cycles = 12;
            }
            break;
        case 0x19: /* ADD HL, DE */
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                uint16_t de = (uint16_t)cpu->d << 8 | cpu->e;
                uint32_t res = hl + de;
                cpu->f = (cpu->f & FLAG_Z) | ((hl & 0x0FFF) + (de & 0x0FFF) > 0x0FFF ? FLAG_H : 0) | (res > 0xFFFF ? FLAG_C : 0);
                cpu->h = (res >> 8) & 0xFF;
                cpu->l = res & 0xFF;
            }
            cycles = 8;
            break;
        case 0x1A: /* LD A, (DE) */
            cpu->a = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->d << 8 | cpu->e);
            cycles = 8;
            break;
        case 0x1B: /* DEC DE */
            {
                uint16_t de = ((uint16_t)cpu->d << 8) | cpu->e;
                de--;
                cpu->d = de >> 8;
                cpu->e = de & 0xFF;
            }
            cycles = 8;
            break;
        case 0x1C: /* INC E */
            cpu->f = (cpu->f & FLAG_C) | ((cpu->e & 0x0F) == 0x0F ? FLAG_H : 0);
            cpu->e++;
            cpu->f |= (cpu->e == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x1D: /* DEC E */
            cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((cpu->e & 0x0F) == 0x00 ? FLAG_H : 0);
            cpu->e--;
            cpu->f |= (cpu->e == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x1E: /* LD E, d8 */
            cpu->e = read_byte(cpu, mmu, cart, timer);
            cycles = 8;
            break;
        case 0x1F: /* RRA */
            {
                uint8_t old_c = (cpu->f & FLAG_C) ? 0x80 : 0;
                cpu->f = (cpu->a & 0x01) ? FLAG_C : 0;
                cpu->a = (cpu->a >> 1) | old_c;
            }
            cycles = 4;
            break;
        case 0x20: /* JR NZ, r8 */
            {
                int8_t offset = (int8_t)read_byte(cpu, mmu, cart, timer);
                if (!(cpu->f & FLAG_Z)) {
                    cpu->pc += offset;
                    cycles = 12;
                } else {
                    cycles = 8;
                }
            }
            break;
        case 0x21: /* LD HL, d16 */
            cpu->l = read_byte(cpu, mmu, cart, timer);
            cpu->h = read_byte(cpu, mmu, cart, timer);
            cycles = 12;
            break;
        case 0x22: /* LD (HL+), A */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->a);
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                hl++;
                cpu->h = hl >> 8;
                cpu->l = hl & 0xFF;
            }
            cycles = 8;
            break;
        case 0x23: /* INC HL */
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                hl++;
                cpu->h = hl >> 8;
                cpu->l = hl & 0xFF;
            }
            cycles = 8;
            break;
        case 0x24: /* INC H */
            cpu->f = (cpu->f & FLAG_C) | ((cpu->h & 0x0F) == 0x0F ? FLAG_H : 0);
            cpu->h++;
            cpu->f |= (cpu->h == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x25: /* DEC H */
            cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((cpu->h & 0x0F) == 0x00 ? FLAG_H : 0);
            cpu->h--;
            cpu->f |= (cpu->h == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x26: /* LD H, d8 */
            cpu->h = read_byte(cpu, mmu, cart, timer);
            cycles = 8;
            break;
        case 0x27: /* DAA */
            {
                uint16_t a = cpu->a;
                bool subtract = !!(cpu->f & FLAG_N);
                bool half_carry = !!(cpu->f & FLAG_H);
                bool carry = !!(cpu->f & FLAG_C);
                bool should_carry = false;

                uint8_t offset = 0;
                if ((!subtract && ( a & 0x0F) > 9) || half_carry)
                    offset |= 0x06;

                if ((!subtract && ( a & 0xFF) > 0x99) || carry) {
                    offset |= 0x60;
                    should_carry = true;
                }

                if (subtract)
                    a -= offset;
                else
                    a += offset;

                cpu->f &= ~(FLAG_H | FLAG_Z);
                if (should_carry) cpu->f |= FLAG_C;
                cpu->a = a & 0xFF;
                if (cpu->a == 0) cpu->f |= FLAG_Z;
            }
            cycles = 4;
            break;
        case 0x28: /* JR Z, r8 */
            {
                int8_t offset = (int8_t)read_byte(cpu, mmu, cart, timer);
                if (cpu->f & FLAG_Z) {
                    cpu->pc += offset;
                    cycles = 12;
                } else {
                    cycles = 8;
                }
            }
            break;
        case 0x29: /* ADD HL, HL */
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                uint32_t res = hl + hl;
                cpu->f = (cpu->f & FLAG_Z) | ((hl & 0x0FFF) + (hl & 0x0FFF) > 0x0FFF ? FLAG_H : 0) | (res > 0xFFFF ? FLAG_C : 0);
                cpu->h = (res >> 8) & 0xFF;
                cpu->l = res & 0xFF;
            }
            cycles = 8;
            break;
        case 0x2A: /* LD A, (HL+) */
            cpu->a = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                hl++;
                cpu->h = hl >> 8;
                cpu->l = hl & 0xFF;
            }
            cycles = 8;
            break;
        case 0x2B: /* DEC HL */
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                hl--;
                cpu->h = hl >> 8;
                cpu->l = hl & 0xFF;
            }
            cycles = 8;
            break;
        case 0x2C: /* INC L */
            cpu->f = (cpu->f & FLAG_C) | ((cpu->l & 0x0F) == 0x0F ? FLAG_H : 0);
            cpu->l++;
            cpu->f |= (cpu->l == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x2D: /* DEC L */
            cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((cpu->l & 0x0F) == 0x00 ? FLAG_H : 0);
            cpu->l--;
            cpu->f |= (cpu->l == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x2E: /* LD L, d8 */
            cpu->l = read_byte(cpu, mmu, cart, timer);
            cycles = 8;
            break;
        case 0x2F: /* CPL */
            cpu->a = ~cpu->a;
            cpu->f |= FLAG_N | FLAG_H;
            cycles = 4;
            break;
        case 0x30: /* JR NC, r8 */
            {
                int8_t offset = (int8_t)read_byte(cpu, mmu, cart, timer);
                if (!(cpu->f & FLAG_C)) {
                    cpu->pc += offset;
                    cycles = 12;
                } else {
                    cycles = 8;
                }
            }
            break;
        case 0x31: /* LD SP, d16 */
            cpu->sp = read_word(cpu, mmu, cart, timer);
            cycles = 12;
            break;
        case 0x32: /* LD (HL-), A */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->a);
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                hl--;
                cpu->h = hl >> 8;
                cpu->l = hl & 0xFF;
            }
            cycles = 8;
            break;
        case 0x33: /* INC SP */
            cpu->sp++;
            cycles = 8;
            break;
        case 0x34: /* INC (HL) */
            {
                uint16_t addr = (uint16_t)cpu->h << 8 | cpu->l;
                uint8_t val = read_mem(cpu, mmu, cart, timer, addr);
                cpu->f = (cpu->f & FLAG_C) | ((val & 0x0F) == 0x0F ? FLAG_H : 0);
                val++;
                cpu->f |= (val == 0 ? FLAG_Z : 0);
                write_mem(cpu, mmu, cart, timer, addr, val);
            }
            cycles = 12;
            break;
        case 0x35: /* DEC (HL) */
            {
                uint16_t addr = (uint16_t)cpu->h << 8 | cpu->l;
                uint8_t val = read_mem(cpu, mmu, cart, timer, addr);
                cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((val & 0x0F) == 0x00 ? FLAG_H : 0);
                val--;
                cpu->f |= (val == 0 ? FLAG_Z : 0);
                write_mem(cpu, mmu, cart, timer, addr, val);
            }
            cycles = 12;
            break;
        case 0x36: /* LD (HL), d8 */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, read_byte(cpu, mmu, cart, timer));
            cycles = 12;
            break;
        case 0x37: /* SCF */
            cpu->f &= ~(FLAG_N | FLAG_H);
            cpu->f |= FLAG_C;
            cycles = 4;
            break;
        case 0x38: /* JR C, r8 */
            {
                int8_t offset = (int8_t)read_byte(cpu, mmu, cart, timer);
                if (cpu->f & FLAG_C) {
                    cpu->pc += offset;
                    cycles = 12;
                } else {
                    cycles = 8;
                }
            }
            break;
        case 0x39: /* ADD HL, SP */
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                uint32_t res = hl + cpu->sp;
                cpu->f = (cpu->f & FLAG_Z) | ((hl & 0x0FFF) + (cpu->sp & 0x0FFF) > 0x0FFF ? FLAG_H : 0) | (res > 0xFFFF ? FLAG_C : 0);
                cpu->h = (res >> 8) & 0xFF;
                cpu->l = res & 0xFF;
            }
            cycles = 8;
            break;
        case 0x3A: /* LD A, (HL-) */
            cpu->a = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            {
                uint16_t hl = (uint16_t)cpu->h << 8 | cpu->l;
                hl--;
                cpu->h = hl >> 8;
                cpu->l = hl & 0xFF;
            }
            cycles = 8;
            break;
        case 0x3B: /* DEC SP */
            cpu->sp--;
            cycles = 8;
            break;
        case 0x3C: /* INC A */
            cpu->f = (cpu->f & FLAG_C) | ((cpu->a & 0x0F) == 0x0F ? FLAG_H : 0);
            cpu->a++;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x3D: /* DEC A */
            cpu->f = FLAG_N | (cpu->f & FLAG_C) | ((cpu->a & 0x0F) == 0x00 ? FLAG_H : 0);
            cpu->a--;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x3E: /* LD A, d8 */
            cpu->a = read_byte(cpu, mmu, cart, timer);
            cycles = 8;
            break;
        case 0x3F: /* CCF */
            cpu->f &= ~(FLAG_N | FLAG_H);
            cpu->f ^= FLAG_C;
            cycles = 4;
            break;
        case 0x40: /* LD B, B */
            cycles = 4;
            break;
        case 0x41: /* LD B, C */
            cpu->b = cpu->c;
            cycles = 4;
            break;
        case 0x42: /* LD B, D */
            cpu->b = cpu->d;
            cycles = 4;
            break;
        case 0x43: /* LD B, E */
            cpu->b = cpu->e;
            cycles = 4;
            break;
        case 0x44: /* LD B, H */
            cpu->b = cpu->h;
            cycles = 4;
            break;
        case 0x45: /* LD B, L */
            cpu->b = cpu->l;
            cycles = 4;
            break;
        case 0x46: /* LD B, (HL) */
            cpu->b = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cycles = 8;
            break;
        case 0x47: /* LD B, A */
            cpu->b = cpu->a;
            cycles = 4;
            break;
        case 0x48: /* LD C, B */
            cpu->c = cpu->b;
            cycles = 4;
            break;
        case 0x49: /* LD C, C */
            cycles = 4;
            break;
        case 0x4A: /* LD C, D */
            cpu->c = cpu->d;
            cycles = 4;
            break;
        case 0x4B: /* LD C, E */
            cpu->c = cpu->e;
            cycles = 4;
            break;
        case 0x4C: /* LD C, H */
            cpu->c = cpu->h;
            cycles = 4;
            break;
        case 0x4D: /* LD C, L */
            cpu->c = cpu->l;
            cycles = 4;
            break;
        case 0x4E: /* LD C, (HL) */
            cpu->c = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cycles = 8;
            break;
        case 0x4F: /* LD C, A */
            cpu->c = cpu->a;
            cycles = 4;
            break;
        case 0x50: /* LD D, B */
            cpu->d = cpu->b;
            cycles = 4;
            break;
        case 0x51: /* LD D, C */
            cpu->d = cpu->c;
            cycles = 4;
            break;
        case 0x52: /* LD D, D */
            cycles = 4;
            break;
        case 0x53: /* LD D, E */
            cpu->d = cpu->e;
            cycles = 4;
            break;
        case 0x54: /* LD D, H */
            cpu->d = cpu->h;
            cycles = 4;
            break;
        case 0x55: /* LD D, L */
            cpu->d = cpu->l;
            cycles = 4;
            break;
        case 0x56: /* LD D, (HL) */
            cpu->d = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cycles = 8;
            break;
        case 0x57: /* LD D, A */
            cpu->d = cpu->a;
            cycles = 4;
            break;
        case 0x58: /* LD E, B */
            cpu->e = cpu->b;
            cycles = 4;
            break;
        case 0x59: /* LD E, C */
            cpu->e = cpu->c;
            cycles = 4;
            break;
        case 0x5A: /* LD E, D */
            cpu->e = cpu->d;
            cycles = 4;
            break;
        case 0x5B: /* LD E, E */
            cycles = 4;
            break;
        case 0x5C: /* LD E, H */
            cpu->e = cpu->h;
            cycles = 4;
            break;
        case 0x5D: /* LD E, L */
            cpu->e = cpu->l;
            cycles = 4;
            break;
        case 0x5E: /* LD E, (HL) */
            cpu->e = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cycles = 8;
            break;
        case 0x5F: /* LD E, A */
            cpu->e = cpu->a;
            cycles = 4;
            break;
        case 0x60: /* LD H, B */
            cpu->h = cpu->b;
            cycles = 4;
            break;
        case 0x61: /* LD H, C */
            cpu->h = cpu->c;
            cycles = 4;
            break;
        case 0x62: /* LD H, D */
            cpu->h = cpu->d;
            cycles = 4;
            break;
        case 0x63: /* LD H, E */
            cpu->h = cpu->e;
            cycles = 4;
            break;
        case 0x64: /* LD H, H */
            cycles = 4;
            break;
        case 0x65: /* LD H, L */
            cpu->h = cpu->l;
            cycles = 4;
            break;
        case 0x66: /* LD H, (HL) */
            cpu->h = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cycles = 8;
            break;
        case 0x67: /* LD H, A */
            cpu->h = cpu->a;
            cycles = 4;
            break;
        case 0x68: /* LD L, B */
            cpu->l = cpu->b;
            cycles = 4;
            break;
        case 0x69: /* LD L, C */
            cpu->l = cpu->c;
            cycles = 4;
            break;
        case 0x6A: /* LD L, D */
            cpu->l = cpu->d;
            cycles = 4;
            break;
        case 0x6B: /* LD L, E */
            cpu->l = cpu->e;
            cycles = 4;
            break;
        case 0x6C: /* LD L, H */
            cpu->l = cpu->h;
            cycles = 4;
            break;
        case 0x6D: /* LD L, L */
            cycles = 4;
            break;
        case 0x6E: /* LD L, (HL) */
            cpu->l = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cycles = 8;
            break;
        case 0x6F: /* LD L, A */
            cpu->l = cpu->a;
            cycles = 4;
            break;
        case 0x70: /* LD (HL), B */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->b);
            cycles = 8;
            break;
        case 0x71: /* LD (HL), C */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->c);
            cycles = 8;
            break;
        case 0x72: /* LD (HL), D */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->d);
            cycles = 8;
            break;
        case 0x73: /* LD (HL), E */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->e);
            cycles = 8;
            break;
        case 0x74: /* LD (HL), H */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->h);
            cycles = 8;
            break;
        case 0x75: /* LD (HL), L */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->l);
            cycles = 8;
            break;
        case 0x76: /* HALT */
            cpu->halted = true;
            cycles = 4;
            break;
        case 0x77: /* LD (HL), A */
            write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, cpu->a);
            cycles = 8;
            break;
        case 0x78: /* LD A, B */
            cpu->a = cpu->b;
            cycles = 4;
            break;
        case 0x79: /* LD A, C */
            cpu->a = cpu->c;
            cycles = 4;
            break;
        case 0x7A: /* LD A, D */
            cpu->a = cpu->d;
            cycles = 4;
            break;
        case 0x7B: /* LD A, E */
            cpu->a = cpu->e;
            cycles = 4;
            break;
        case 0x7C: /* LD A, H */
            cpu->a = cpu->h;
            cycles = 4;
            break;
        case 0x7D: /* LD A, L */
            cpu->a = cpu->l;
            cycles = 4;
            break;
        case 0x7E: /* LD A, (HL) */
            cpu->a = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cycles = 8;
            break;
        case 0x7F: /* LD A, A */
            cycles = 4;
            break;
        case 0x80: /* ADD A, B */
            cpu->f = (cpu->a + cpu->b > 0xFF ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) + (cpu->b & 0x0F) > 0x0F ? FLAG_H : 0);
            cpu->a += cpu->b;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x81: /* ADD A, C */
            cpu->f = (cpu->a + cpu->c > 0xFF ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) + (cpu->c & 0x0F) > 0x0F ? FLAG_H : 0);
            cpu->a += cpu->c;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x82: /* ADD A, D */
            cpu->f = (cpu->a + cpu->d > 0xFF ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) + (cpu->d & 0x0F) > 0x0F ? FLAG_H : 0);
            cpu->a += cpu->d;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x83: /* ADD A, E */
            cpu->f = (cpu->a + cpu->e > 0xFF ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) + (cpu->e & 0x0F) > 0x0F ? FLAG_H : 0);
            cpu->a += cpu->e;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x84: /* ADD A, H */
            cpu->f = (cpu->a + cpu->h > 0xFF ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) + (cpu->h & 0x0F) > 0x0F ? FLAG_H : 0);
            cpu->a += cpu->h;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x85: /* ADD A, L */
            cpu->f = (cpu->a + cpu->l > 0xFF ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) + (cpu->l & 0x0F) > 0x0F ? FLAG_H : 0);
            cpu->a += cpu->l;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x86: /* ADD A, (HL) */
            {
                uint8_t val = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
                cpu->f = (cpu->a + val > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (val & 0x0F) > 0x0F ? FLAG_H : 0);
                cpu->a += val;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0x87: /* ADD A, A */
            cpu->f = (cpu->a + cpu->a > 0xFF ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) + (cpu->a & 0x0F) > 0x0F ? FLAG_H : 0);
            cpu->a += cpu->a;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x88: /* ADC A, B */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + cpu->b + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (cpu->b & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x89: /* ADC A, C */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + cpu->c + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (cpu->c & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x8A: /* ADC A, D */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + cpu->d + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (cpu->d & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x8B: /* ADC A, E */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + cpu->e + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (cpu->e & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x8C: /* ADC A, H */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + cpu->h + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (cpu->h & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x8D: /* ADC A, L */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + cpu->l + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (cpu->l & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x8E: /* ADC A, (HL) */
            {
                uint8_t val = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + val + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (val & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0x8F: /* ADC A, A */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + cpu->a + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (cpu->a & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x90: /* SUB B */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->b ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->b & 0x0F) ? FLAG_H : 0);
            cpu->a -= cpu->b;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x91: /* SUB C */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->c ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->c & 0x0F) ? FLAG_H : 0);
            cpu->a -= cpu->c;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x92: /* SUB D */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->d ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->d & 0x0F) ? FLAG_H : 0);
            cpu->a -= cpu->d;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x93: /* SUB E */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->e ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->e & 0x0F) ? FLAG_H : 0);
            cpu->a -= cpu->e;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x94: /* SUB H */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->h ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->h & 0x0F) ? FLAG_H : 0);
            cpu->a -= cpu->h;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x95: /* SUB L */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->l ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->l & 0x0F) ? FLAG_H : 0);
            cpu->a -= cpu->l;
            cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0x96: /* SUB (HL) */
            {
                uint8_t val = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
                cpu->f = FLAG_N |
                         (cpu->a < val ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (val & 0x0F) ? FLAG_H : 0);
                cpu->a -= val;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0x97: /* SUB A */
            cpu->a = 0;
            cpu->f = FLAG_Z | FLAG_N;
            cycles = 4;
            break;
        case 0x98: /* SBC A, B */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - cpu->b - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (cpu->b & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x99: /* SBC A, C */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - cpu->c - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (cpu->c & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x9A: /* SBC A, D */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - cpu->d - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (cpu->d & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x9B: /* SBC A, E */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - cpu->e - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (cpu->e & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x9C: /* SBC A, H */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - cpu->h - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (cpu->h & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x9D: /* SBC A, L */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - cpu->l - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (cpu->l & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 4;
            break;
        case 0x9E: /* SBC A, (HL) */
            {
                uint8_t val = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - val - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (val & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0x9F: /* SBC A, A */
            {
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint8_t res = cpu->a - cpu->a - c;
                cpu->f = FLAG_N | (res ? 0 : FLAG_Z) | (c ? FLAG_C : 0) | ((0x0F < c) ? FLAG_H : 0);
                cpu->a = res;
            }
            cycles = 4;
            break;
        case 0xA0: /* AND B */
            cpu->a &= cpu->b;
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA1: /* AND C */
            cpu->a &= cpu->c;
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA2: /* AND D */
            cpu->a &= cpu->d;
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA3: /* AND E */
            cpu->a &= cpu->e;
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA4: /* AND H */
            cpu->a &= cpu->h;
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA5: /* AND L */
            cpu->a &= cpu->l;
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA6: /* AND (HL) */
            cpu->a &= read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 8;
            break;
        case 0xA7: /* AND A */
            cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA8: /* XOR B */
            cpu->a ^= cpu->b;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xA9: /* XOR C */
            cpu->a ^= cpu->c;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xAA: /* XOR D */
            cpu->a ^= cpu->d;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xAB: /* XOR E */
            cpu->a ^= cpu->e;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xAC: /* XOR H */
            cpu->a ^= cpu->h;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xAD: /* XOR L */
            cpu->a ^= cpu->l;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xAE: /* XOR (HL) */
            cpu->a ^= read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 8;
            break;
        case 0xAF: /* XOR A */
            cpu->a = 0;
            cpu->f = FLAG_Z;
            cycles = 4;
            break;
        case 0xB0: /* OR B */
            cpu->a |= cpu->b;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB1: /* OR C */
            cpu->a |= cpu->c;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB2: /* OR D */
            cpu->a |= cpu->d;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB3: /* OR E */
            cpu->a |= cpu->e;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB4: /* OR H */
            cpu->a |= cpu->h;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB5: /* OR L */
            cpu->a |= cpu->l;
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB6: /* OR (HL) */
            cpu->a |= read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 8;
            break;
        case 0xB7: /* OR A */
            cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB8: /* CP B */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->b ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->b & 0x0F) ? FLAG_H : 0) |
                     (cpu->a == cpu->b ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xB9: /* CP C */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->c ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->c & 0x0F) ? FLAG_H : 0) |
                     (cpu->a == cpu->c ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xBA: /* CP D */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->d ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->d & 0x0F) ? FLAG_H : 0) |
                     (cpu->a == cpu->d ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xBB: /* CP E */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->e ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->e & 0x0F) ? FLAG_H : 0) |
                     (cpu->a == cpu->e ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xBC: /* CP H */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->h ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->h & 0x0F) ? FLAG_H : 0) |
                     (cpu->a == cpu->h ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xBD: /* CP L */
            cpu->f = FLAG_N |
                     (cpu->a < cpu->l ? FLAG_C : 0) |
                     ((cpu->a & 0x0F) < (cpu->l & 0x0F) ? FLAG_H : 0) |
                     (cpu->a == cpu->l ? FLAG_Z : 0);
            cycles = 4;
            break;
        case 0xBE: /* CP (HL) */
            {
                uint8_t val = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
                cpu->f = FLAG_N |
                         (cpu->a < val ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (val & 0x0F) ? FLAG_H : 0) |
                         (cpu->a == val ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xBF: /* CP A */
            cpu->f = FLAG_Z | FLAG_N;
            cycles = 4;
            break;
        case 0xC0: /* RET NZ */
            cycles = 8;
            if (!(cpu->f & FLAG_Z)) {
                uint8_t lo = read_mem(cpu, mmu, cart, timer, cpu->sp);
                uint8_t hi = read_mem(cpu, mmu, cart, timer, cpu->sp + 1);
                cpu->sp += 2;
                cpu->pc = (uint16_t)hi << 8 | lo;
                cycles = 20;
            }
            break;
        case 0xC1: /* POP BC */
            pop(cpu, mmu, cart, timer, &cpu->b, &cpu->c);
            cycles = 12;
            break;
        case 0xC2: /* JP NZ, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                if (!(cpu->f & FLAG_Z)) {
                    cpu->pc = addr;
                }
                cycles = 12;
            }
            break;
        case 0xC3: /* JP a16 */
            cpu->pc = read_word(cpu, mmu, cart, timer);
            cycles = 16;
            break;
        case 0xC4: /* CALL NZ, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                cycles = 12;
                if (!(cpu->f & FLAG_Z)) {
                    push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
                    cpu->pc = addr;
                    cycles = 24;
                }
            }
            break;
        case 0xC5: /* PUSH BC */
            push(cpu, mmu, cart, timer, cpu->b, cpu->c);
            cycles = 16;
            break;
        case 0xC6: /* ADD A, d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                cpu->f = (cpu->a + val > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (val & 0x0F) > 0x0F ? FLAG_H : 0);
                cpu->a += val;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xC7: /* RST 00H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0000;
            cycles = 16;
            break;
        case 0xC8: /* RET Z */
            cycles = 8;
            if (cpu->f & FLAG_Z) {
                uint8_t lo = read_mem(cpu, mmu, cart, timer, cpu->sp);
                uint8_t hi = read_mem(cpu, mmu, cart, timer, cpu->sp + 1);
                cpu->sp += 2;
                cpu->pc = (uint16_t)hi << 8 | lo;
                cycles = 20;
            }
            break;
        case 0xC9: /* RET */
            {
                uint8_t lo = read_mem(cpu, mmu, cart, timer, cpu->sp);
                uint8_t hi = read_mem(cpu, mmu, cart, timer, cpu->sp + 1);
                cpu->sp += 2;
                cpu->pc = (uint16_t)hi << 8 | lo;
            }
            cycles = 16;
            break;
        case 0xCA: /* JP Z, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                if (cpu->f & FLAG_Z) {
                    cpu->pc = addr;
                }
                cycles = 12;
            }
            break;
        case 0xCB: /* CB prefix */
            {
                uint8_t cb = read_byte(cpu, mmu, cart, timer);
                uint8_t reg_idx = cb & 0x07;
                uint8_t op = (cb >> 3) & 0x07;
                uint8_t val;

                if (reg_idx == 6) {
                    val = read_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l);
                } else {
                    uint8_t *regs[] = {&cpu->b, &cpu->c, &cpu->d, &cpu->e, &cpu->h, &cpu->l, NULL, &cpu->a};
                    val = *regs[reg_idx];
                }

                if (cb < 0x40) {
                    /* Shift/Rotate operations: 0x00-0x3F */
                    if (op == 0) { /* RLC */
                        uint8_t new_c = (val >> 7) & 1;
                        val = (val << 1) | new_c;
                        cpu->f = (new_c ? FLAG_C : 0) | (val == 0 ? FLAG_Z : 0);
                    } else if (op == 1) { /* RRC */
                        uint8_t new_c = val & 1;
                        val = (val >> 1) | (new_c << 7);
                        cpu->f = (new_c ? FLAG_C : 0) | (val == 0 ? FLAG_Z : 0);
                    } else if (op == 2) { /* RL */
                        uint8_t old_c = (cpu->f & FLAG_C) ? 1 : 0;
                        uint8_t new_c = (val >> 7) & 1;
                        val = (val << 1) | old_c;
                        cpu->f = (new_c ? FLAG_C : 0) | (val == 0 ? FLAG_Z : 0);
                    } else if (op == 3) { /* RR */
                        uint8_t old_c = (cpu->f & FLAG_C) ? 1 : 0;
                        uint8_t new_c = val & 1;
                        val = (val >> 1) | (old_c << 7);
                        cpu->f = (new_c ? FLAG_C : 0) | (val == 0 ? FLAG_Z : 0);
                    } else if (op == 4) { /* SLA */
                        uint8_t new_c = (val >> 7) & 1;
                        val <<= 1;
                        cpu->f = (new_c ? FLAG_C : 0) | (val == 0 ? FLAG_Z : 0);
                    } else if (op == 5) { /* SRA */
                        uint8_t new_c = val & 1;
                        val = (val >> 1) | (val & 0x80);
                        cpu->f = (new_c ? FLAG_C : 0) | (val == 0 ? FLAG_Z : 0);
                    } else if (op == 6) { /* SWAP */
                        val = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
                        cpu->f = val == 0 ? FLAG_Z : 0;
                    } else { /* SRL */
                        uint8_t new_c = val & 1;
                        val >>= 1;
                        cpu->f = (new_c ? FLAG_C : 0) | (val == 0 ? FLAG_Z : 0);
                    }
                    if (reg_idx == 6) {
                        write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, val);
                        cycles = 16;
                    } else {
                        uint8_t *regs[] = {&cpu->b, &cpu->c, &cpu->d, &cpu->e, &cpu->h, &cpu->l, NULL, &cpu->a};
                        *regs[reg_idx] = val;
                        cycles = 8;
                    }
                } else if ((cb & 0xC0) == 0x40) { /* BIT */
                    uint8_t bit = op;
                    cpu->f = (cpu->f & FLAG_C) | FLAG_H | (!(val & (1 << bit)) ? FLAG_Z : 0);
                    cycles = reg_idx == 6 ? 12 : 8;
                } else if ((cb & 0xC0) == 0x80) { /* RES */
                    uint8_t bit = op;
                    val &= ~(1 << bit);
                    if (reg_idx == 6) {
                        write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, val);
                        cycles = 16;
                    } else {
                        uint8_t *regs[] = {&cpu->b, &cpu->c, &cpu->d, &cpu->e, &cpu->h, &cpu->l, NULL, &cpu->a};
                        *regs[reg_idx] = val;
                        cycles = 8;
                    }
                } else { /* SET */
                    uint8_t bit = op;
                    val |= (1 << bit);
                    if (reg_idx == 6) {
                        write_mem(cpu, mmu, cart, timer, (uint16_t)cpu->h << 8 | cpu->l, val);
                        cycles = 16;
                    } else {
                        uint8_t *regs[] = {&cpu->b, &cpu->c, &cpu->d, &cpu->e, &cpu->h, &cpu->l, NULL, &cpu->a};
                        *regs[reg_idx] = val;
                        cycles = 8;
                    }
                }
            }
            break;
        case 0xCC: /* CALL Z, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                cycles = 12;
                if (cpu->f & FLAG_Z) {
                    push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
                    cpu->pc = addr;
                    cycles = 24;
                }
            }
            break;
        case 0xCD: /* CALL a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
                cpu->pc = addr;
            }
            cycles = 24;
            break;
        case 0xCE: /* ADC A, d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t sum = cpu->a + val + c;
                cpu->f = (sum > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) + (val & 0x0F) + c > 0x0F ? FLAG_H : 0);
                cpu->a = sum & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xCF: /* RST 08H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0008;
            cycles = 16;
            break;
        case 0xD0: /* RET NC */
            cycles = 8;
            if (!(cpu->f & FLAG_C)) {
                uint8_t lo = read_mem(cpu, mmu, cart, timer, cpu->sp);
                uint8_t hi = read_mem(cpu, mmu, cart, timer, cpu->sp + 1);
                cpu->sp += 2;
                cpu->pc = (uint16_t)hi << 8 | lo;
                cycles = 20;
            }
            break;
        case 0xD1: /* POP DE */
            pop(cpu, mmu, cart, timer, &cpu->d, &cpu->e);
            cycles = 12;
            break;
        case 0xD2: /* JP NC, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                if (!(cpu->f & FLAG_C)) {
                    cpu->pc = addr;
                }
                cycles = 12;
            }
            break;
        case 0xD3: /* ILLEGAL */
            cycles = 4;
            break;
        case 0xD4: /* CALL NC, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                cycles = 12;
                if (!(cpu->f & FLAG_C)) {
                    push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
                    cpu->pc = addr;
                    cycles = 24;
                }
            }
            break;
        case 0xD5: /* PUSH DE */
            push(cpu, mmu, cart, timer, cpu->d, cpu->e);
            cycles = 16;
            break;
        case 0xD6: /* SUB d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                cpu->f = FLAG_N |
                         (cpu->a < val ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (val & 0x0F) ? FLAG_H : 0);
                cpu->a -= val;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xD7: /* RST 10H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0010;
            cycles = 16;
            break;
        case 0xD8: /* RET C */
            cycles = 8;
            if (cpu->f & FLAG_C) {
                uint8_t lo = read_mem(cpu, mmu, cart, timer, cpu->sp);
                uint8_t hi = read_mem(cpu, mmu, cart, timer, cpu->sp + 1);
                cpu->sp += 2;
                cpu->pc = (uint16_t)hi << 8 | lo;
                cycles = 20;
            }
            break;
        case 0xD9: /* RETI */
            {
                uint8_t lo = read_mem(cpu, mmu, cart, timer, cpu->sp);
                uint8_t hi = read_mem(cpu, mmu, cart, timer, cpu->sp + 1);
                cpu->sp += 2;
                cpu->pc = (uint16_t)hi << 8 | lo;
            }
            cpu->ime = true;
            cycles = 16;
            break;
        case 0xDA: /* JP C, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                if (cpu->f & FLAG_C) {
                    cpu->pc = addr;
                }
                cycles = 12;
            }
            break;
        case 0xDC: /* CALL C, a16 */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                cycles = 12;
                if (cpu->f & FLAG_C) {
                    push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
                    cpu->pc = addr;
                    cycles = 24;
                }
            }
            break;
        case 0xDE: /* SBC A, d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                uint8_t c = (cpu->f & FLAG_C) ? 1 : 0;
                uint16_t res = cpu->a - val - c;
                cpu->f = FLAG_N |
                         (res > 0xFF ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (val & 0x0F) + c ? FLAG_H : 0);
                cpu->a = res & 0xFF;
                cpu->f |= (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xDF: /* RST 18H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0018;
            cycles = 16;
            break;
        case 0xE0: /* LDH (a8), A */
            {
                uint8_t n = read_byte(cpu, mmu, cart, timer);
                write_mem(cpu, mmu, cart, timer, 0xFF00 | n , cpu->a);
            }
            cycles = 12;
            break;
        case 0xE1: /* POP HL */
            pop(cpu, mmu, cart, timer, &cpu->h, &cpu->l);
            cycles = 12;
            break;
        case 0xE2: /* LD (C), A */
            write_mem(cpu, mmu, cart, timer, 0xFF00 | cpu->c, cpu->a);
            cycles = 8;
            break;
        case 0xE3: /* ILLEGAL */
        case 0xE4: /* ILLEGAL */
            { read_byte(cpu, mmu, cart, timer); cycles = 4; }
            break;
        case 0xE5: /* PUSH HL */
            push(cpu, mmu, cart, timer, cpu->h, cpu->l);
            cycles = 16;
            break;
        case 0xE6: /* AND d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                cpu->a &= val;
                cpu->f = FLAG_H | (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xE8: /* ADD SP, r8 */
            {
                int8_t offset = (int8_t)read_byte(cpu, mmu, cart, timer);
                cpu->f = ((cpu->sp & 0xFF) + (offset & 0xFF) > 0xFF ? FLAG_C : 0) |
                         ((cpu->sp & 0x0F) + (offset & 0x0F) > 0x0F ? FLAG_H : 0);
                cpu->sp += offset;
            }
            cycles = 16;
            break;
        case 0xE9: /* JP (HL) */
            cpu->pc = (uint16_t)cpu->h << 8 | cpu->l;
            cycles = 4;
            break;
        case 0xEA: /* LD (a16), A */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                write_mem(cpu, mmu, cart, timer, addr, cpu->a);
            }
            cycles = 16;
            break;
        case 0xEE: /* XOR d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                cpu->a ^= val;
                cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xEF: /* RST 28H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0028;
            cycles = 16;
            break;
        case 0xE7: /* RST 20H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0020;
            cycles = 16;
            break;
        case 0xF0: /* LDH A, (a8) */
            {
                uint8_t n = read_byte(cpu, mmu, cart, timer);
                cpu->a = read_mem(cpu, mmu, cart, timer, 0xFF00 | n);
            }
            cycles = 12;
            break;
        case 0xF1: /* POP AF */
            {
                uint8_t lo, hi;
                pop(cpu, mmu, cart, timer, &hi, &lo);
                cpu->a = hi;
                cpu->f = lo & 0xF0;
            }
            cycles = 12;
            break;
        case 0xF2: /* LD A, (C) */
            cpu->a = read_mem(cpu, mmu, cart, timer, 0xFF00 | cpu->c);
            cycles = 8;
            break;
        case 0xF3: /* DI */
            cpu->ime = false;
            cpu->ime_delay = false;
            cycles = 4;
            break;
        case 0xF5: /* PUSH AF */
            push(cpu, mmu, cart, timer, cpu->a, cpu->f);
            cycles = 16;
            break;
        case 0xF6: /* OR d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                cpu->a |= val;
                cpu->f = (cpu->a == 0 ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xF7: /* RST 30H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0030;
            cycles = 16;
            break;
        case 0xF8: /* LD HL, SP+r8 */
            {
                int8_t offset = (int8_t)read_byte(cpu, mmu, cart, timer);
                cpu->f = ((cpu->sp & 0xFF) + (offset & 0xFF) > 0xFF ? FLAG_C : 0) |
                         ((cpu->sp & 0x0F) + (offset & 0x0F) > 0x0F ? FLAG_H : 0);
                uint16_t result = cpu->sp + offset;
                cpu->h = result >> 8;
                cpu->l = result & 0xFF;
            }
            cycles = 12;
            break;
        case 0xF9: /* LD SP, HL */
            cpu->sp = (uint16_t)cpu->h << 8 | cpu->l;
            cycles = 8;
            break;
        case 0xFA: /* LD A, (a16) */
            {
                uint16_t addr = read_word(cpu, mmu, cart, timer);
                cpu->a = read_mem(cpu, mmu, cart, timer, addr);
            }
            cycles = 16;
            break;
        case 0xFB: /* EI */
            cpu->ime_delay = true;
            cycles = 4;
            break;
        case 0xFE: /* CP d8 */
            {
                uint8_t val = read_byte(cpu, mmu, cart, timer);
                cpu->f = FLAG_N |
                         (cpu->a < val ? FLAG_C : 0) |
                         ((cpu->a & 0x0F) < (val & 0x0F) ? FLAG_H : 0) |
                         (cpu->a == val ? FLAG_Z : 0);
            }
            cycles = 8;
            break;
        case 0xFF: /* RST 38H */
            push(cpu, mmu, cart, timer, cpu->pc >> 8, cpu->pc & 0xFF);
            cpu->pc = 0x0038;
            cycles = 16;
            break;
        default:
            printf("FATAL: unrecognized opcode: 0x%x.....\n", opcode);
            cycles = 4;
            break;
    }
    return cycles;
}

void cpu_init(CPU *cpu) {
    cpu->a = 0x01;
    cpu->f = 0xB0;
    cpu->b = 0x00;
    cpu->c = 0x13;
    cpu->d = 0x00;
    cpu->e = 0xD8;
    cpu->h = 0x01;
    cpu->l = 0x4D;
    cpu->sp = 0xFFFE;
    cpu->pc = 0x0100;
    cpu->ime = false;
    cpu->ime_delay = false;
    cpu->halted = false;
}
