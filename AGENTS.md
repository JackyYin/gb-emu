# AGENTS.md - Game Boy Emulator Development Guidelines

## Project Overview

Game Boy emulator written in C. Runs natively via SDL2 on desktop (macOS/Linux/Windows) or in the browser via WebAssembly/Emscripten.

## Architecture

```
src/core/         → Emulation core (CPU, PPU, APU, MMU, Cartridge, Timer, Serial, Interrupts)
src/platform/
  ├── platform.h  → Platform interface header
  ├── sdl.c       → SDL2 native implementation (macOS/Linux/Windows)
  └── wasm.c      → Emscripten bindings (browser)
web/              → JavaScript glue, HTML, CSS (browser only)
dist/             → WASM build output
build-sdl/        → Native SDL build output
build/            → Emscripten build output
roms/             → Test ROMs (not committed)
scripts/          → Build and utility scripts
```

## Code Conventions

### C/C++ Core

- **Naming**: `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for macros/constants, `gb_` or `ppu_` prefix for module functions
- **Types**: Use fixed-width types from `<stdint.h>` (`uint8_t`, `uint16_t`, `uint32_t`)
- **Memory**: No dynamic allocation in hot paths. Use static buffers or stack allocation.
- **Headers**: Each `.c` file has a matching `.h`. Keep headers self-contained with include guards.
- **No C++ features in core**: Keep `src/core/` as pure C for portability. C++ allowed in `src/platform/`.
- **Comments**: Minimal inline comments. Document public API functions only.

### Platform Code

- **Separation**: `src/platform/sdl.c` for native, `src/platform/wasm.c` for browser
- **Interface**: Both implement the same abstract API defined in `src/platform/platform.h`
- **SDL**: Only `src/platform/sdl.c` includes `<SDL2/SDL.h>`
- **WASM**: Only `src/platform/wasm.c` includes `<emscripten.h>`

### JavaScript/Web

- **Style**: Modern ES6+, no frameworks needed
- **Modules**: ES modules with `import`/`export`
- **No global state**: Encapsulate in classes or modules

## Build System

- **Primary**: CMake with Emscripten toolchain
- **Command**: `./scripts/build.sh` or `cmake --build build`
- **Output**: `dist/gameboy.js` + `dist/gameboy.wasm`

### Build Commands

```bash
# Native SDL build
./scripts/build-sdl.sh
./scripts/build-sdl.sh --debug

# WASM build
./scripts/setup.sh
./scripts/build.sh
./scripts/build.sh --debug

# Serve locally
./scripts/serve.sh
```

## Testing Strategy

- **Unit tests**: C unit tests in `tests/` covering CPU instructions, MMU mapping, PPU rendering, cartridge bank switching, and timer initialization. Run with `./scripts/test.sh`.
- **Integration tests**: Full emulator lifecycle tests (init, ROM load, frame execution, interrupts) in `tests/test_integration.c`.
- **Blargg ROMs**: Run official Game Boy test ROMs from https://github.com/retrio/gb-test-roms for hardware accuracy validation.
- **mooneye-gb**: Edge-case hardware tests from https://github.com/Gekkio/mooneye-gb.
- **GBDK-2020**: Build custom test ROMs using https://github.com/gbdk-2020/gbdk-2020.

See [TESTING.md](TESTING.md) for full details on the test framework, coverage, and writing new tests.

### Test Commands

```bash
# Run all tests
./scripts/test.sh

# Or manually
mkdir -p build-test && cd build-test
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target gameboy_unit_tests
./gameboy_unit_tests
```

## Git Conventions

- **Branches**: `main`, `feature/<name>`, `fix/<name>`
- **Commits**: Conventional commits (`feat:`, `fix:`, `refactor:`, `docs:`)
- **NEVER commit**: ROM files, build artifacts, `.wasm` files

## Key Constants

| Constant | Value | Description |
|----------|-------|-------------|
| SCREEN_W | 160 | Game Boy screen width |
| SCREEN_H | 144 | Game Boy screen height |
| CPU_FREQ | 4194304 Hz | ~4.19 MHz |
| FPS | ~59.7 | Frames per second |
| FRAME_CYCLES | 70224 | CPU cycles per frame |

## Memory Map Reference

| Range | Size | Description |
|-------|------|-------------|
| 0x0000-0x3FFF | 16KB | ROM Bank 0 (fixed) |
| 0x4000-0x7FFF | 16KB | ROM Bank N (switchable) |
| 0x8000-0x9FFF | 8KB | VRAM |
| 0xA000-0xBFFF | 8KB | Cartridge RAM |
| 0xC000-0xCFFF | 4KB | WRAM Bank 0 |
| 0xD000-0xDFFF | 4KB | WRAM Bank N (switchable, CGB) |
| 0xE000-0xFDFF | 4KB | Echo RAM |
| 0xFE00-0xFE9F | 160B | OAM (Sprite attributes) |
| 0xFEA0-0xFEFF | - | Unusable |
| 0xFF00-0xFF7F | 128B | I/O Registers |
| 0xFF80-0xFFFE | 127B | HRAM |
| 0xFFFF | 1B | Interrupt Enable |

## Implementation Order

1. CPU core (common instructions first)
2. MMU + basic memory map
3. Cartridge ROM loading + MBC1
4. PPU (background rendering → window → sprites)
5. Timer + VBLANK interrupt
6. JS rendering pipeline + game loop
7. APU (square → wave → noise)
8. Joypad input
9. MBC3/MBC5 support
10. Save states

## Resources

- Pan Docs: https://gbdev.io/pandocs/
- Game Boy CPU Manual: http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf
- Blargg's Test ROMs: https://github.com/retrio/gb-test-roms
- mooneye-gb: https://github.com/Gekkio/mooneye-gb
