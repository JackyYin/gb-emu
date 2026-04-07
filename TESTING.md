# Testing Guide

## Overview

The emulator has a multi-layered testing strategy:

1. **Unit tests** - C tests for individual modules (CPU, MMU, PPU, cartridge, timer)
2. **Integration tests** - End-to-end tests that initialize the full emulator and run small programs
3. **Test ROMs** - External ROM-based tests (Blargg, mooneye-gb) for hardware accuracy validation

## Running Tests

```bash
# Run all unit and integration tests
./scripts/test.sh

# Or manually:
mkdir -p build-test && cd build-test
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target gameboy_unit_tests
./gameboy_unit_tests
```

The test binary returns exit code 0 on success, 1 on failure.

## Test Structure

```
tests/
├── test_framework.h      # Minimal assertion macros and test runner
├── test_main.c           # Entry point, runs all test suites
├── test_cpu.c            # CPU instruction tests
├── test_mmu.c            # Memory management tests
├── test_ppu.c            # Graphics rendering tests
├── test_cartridge.c      # Cartridge and timer tests
└── test_integration.c    # Full emulator integration tests
```

## Test Framework

The project uses a lightweight custom test framework defined in `tests/test_framework.h`. No external dependencies are required.

### Assertion Macros

| Macro | Description |
|-------|-------------|
| `ASSERT_EQ(expected, actual, msg)` | Assert integer/pointer equality |
| `ASSERT_TRUE(cond, msg)` | Assert condition is true |
| `ASSERT_FALSE(cond, msg)` | Assert condition is false |
| `ASSERT_MEM_EQ(expected, actual, len, msg)` | Assert memory regions are equal |

### Runner Macros

| Macro | Description |
|-------|-------------|
| `TEST_SUITE(name)` | Print suite header |
| `RUN_TEST(func)` | Run a test function and print its name |
| `TEST_REPORT()` | Print pass/fail summary |

## Test Suites

### CPU Tests (`test_cpu.c`)

Tests the LR35902 CPU instruction set against expected behavior from the [Pan Docs](https://gbdev.io/pandocs/) and [Game Boy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf).

Each test sets up isolated CPU/MMU/Cartridge state, places machine code in ROM, and executes one or more `cpu_step()` calls.

**Coverage:**

- **Load instructions**: LD r,imm8 / LD rr,imm16 / LD (HL),A / LD A,(HL) / LDI / LDD / LDH / LD (a16),SP
- **8-bit arithmetic**: ADD, ADC, SUB, SBC, INC, DEC (with flag verification for Z, N, H, C)
- **16-bit arithmetic**: INC rr, DEC rr, ADD HL,rr
- **Logic**: AND, OR, XOR, CP
- **Rotates**: RLCA, RRCA, RLA, RRA
- **Jumps**: JR (unconditional, NZ, Z, forward, backward), JP a16
- **Calls/Returns**: CALL, RET, RST
- **Stack**: PUSH, POP (including AF with lower nibble masking)
- **Misc**: DAA, SCF, CCF, CPL, HALT, EI/DI (with delayed IME enable)
- **Interrupts**: V-Blank dispatch, Timer dispatch, halt wakeup on interrupt
- **CB prefix**: BIT, SET, RES, SWAP, SLA, SRA, SRL, RL, RR
- **Program test**: Small loop program verifying multi-instruction interaction

### MMU Tests (`test_mmu.c`)

Tests the memory map, ensuring reads and writes route to the correct backing store for every address region.

**Coverage:**

- ROM bank 0 and switchable bank reads
- Boot ROM overlay and disable (write to 0xFF50)
- VRAM (0x8000-0x9FFF)
- WRAM (0xC000-0xDFFF) and Echo RAM mirroring (0xE000-0xFDFF)
- OAM (0xFE00-0xFE9F)
- Unusable region (0xFEA0-0xFEFF) returns 0xFF
- HRAM (0xFF80-0xFFFE)
- I/O registers (0xFF00-0xFF7F)
- IE register (0xFFFF)
- LY reset on write to 0xFF44
- Cartridge RAM enable/disable and read/write
- MBC1 ROM bank switching (5-bit, upper 2-bit)
- MBC3 ROM bank switching (7-bit), RAM bank, RTC register select
- Joypad register (P1/0xFF00): d-pad selection, button selection, no selection

### PPU Tests (`test_ppu.c`)

Tests scanline rendering against the Game Boy's tile-based graphics pipeline.

**Coverage:**

- PPU register initialization (LCDC, STAT, BGP, OBP0, OBP1)
- LCD off prevents rendering
- Background tile rendering (solid color, empty/white)
- Background scrolling via SCX/SCY
- Palette mapping (identity and reversed)
- Sprite rendering: basic display, color 0 transparency, X-flip
- Window layer rendering (WX, WY)
- 10 sprites per scanline hardware limit

### Cartridge & Timer Tests (`test_cartridge.c`)

Tests the cartridge module's standalone functions and timer initialization.

**Coverage:**

- `cart_init()` default state
- `cart_read_rom()` bank 0 and switchable bank reads
- ROM out-of-bounds returns 0xFF
- `cart_write_ram()` / `cart_read_ram()` round-trip
- RAM out-of-range and NULL RAM handling
- `timer_init()` zeroes all fields
- `timer_tick()` stub runs without crash

### Integration Tests (`test_integration.c`)

Tests the full emulator lifecycle through the public API (`gb_init`, `gb_load_rom`, `gb_run_frame`, etc.).

**Coverage:**

- `gb_init()` returns valid framebuffer and audio pointers
- ROM loading and single frame execution
- Boot ROM loading
- Joypad state setting
- Infinite loop program runs a frame without hang
- Multiple consecutive frames (10 frames)
- Program that writes to VRAM
- Program with CALL/RET subroutine
- Program with interrupt handling (EI + V-Blank)

## Writing New Tests

1. Add test functions to the appropriate `test_*.c` file (or create a new one).
2. Register each test with `RUN_TEST(test_func)` in the suite's `run_*_tests()` function.
3. If adding a new file, add it to `TEST_SOURCES` in `CMakeLists.txt` and declare `run_*_tests()` as extern in `test_main.c`.

### Example

```c
static void test_example(void) {
    setup();
    cpu.pc = 0x0100;
    cpu.a = 0x10;
    uint8_t code[] = {0x3C}; /* INC A */
    place_code(0x0100, code, sizeof(code));
    cpu_step(&cpu, &mmu, &cart);
    ASSERT_EQ(0x11, cpu.a, "INC A from 0x10");
}
```

## Test ROM Validation

For hardware accuracy beyond unit tests, use external test ROM suites:

- [Blargg's test ROMs](https://github.com/retrio/gb-test-roms) - CPU instructions, timing, memory, audio
- [mooneye-gb](https://github.com/Gekkio/mooneye-gb) - Edge-case hardware behavior
- [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) - Game Boy Development Kit for building custom test ROMs

To run a test ROM, load it through the emulator's web interface or native test binary and check the on-screen output or serial port (`0xFF01`) for pass/fail status. Blargg ROMs typically print results to the serial port and display them on screen.
