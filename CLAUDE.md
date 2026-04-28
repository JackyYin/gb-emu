# AGENTS.md - Game Boy Emulator Development Guidelines

## Project Overview

Game Boy emulator written in C/C++ compiled to WebAssembly via Emscripten, running in the browser.

## Architecture

```
src/core/     → Emulation core (CPU, PPU, APU, MMU, Cartridge, Timer, Serial, Interrupts)
src/platform/ → Platform-specific code (WASM/Emscripten bindings)
web/          → JavaScript glue, HTML, CSS
dist/         → Build output (JS/WASM files)
roms/         → Test ROMs (not committed)
scripts/      → Build and utility scripts
```

## Code Conventions

### C/C++ Core

- **Naming**: `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for macros/constants, `gb_` or `ppu_` prefix for module functions
- **Types**: Use fixed-width types from `<stdint.h>` (`uint8_t`, `uint16_t`, `uint32_t`)
- **Memory**: No dynamic allocation in hot paths. Use static buffers or stack allocation.
- **Headers**: Each `.c` file has a matching `.h`. Keep headers self-contained with include guards.
- **No C++ features in core**: Keep `src/core/` as pure C for portability. C++ allowed in `src/platform/`.
- **Comments**: Minimal inline comments. Document public API functions only.

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
# Setup build
./scripts/setup.sh

# Build
./scripts/build.sh

# Build with debug symbols
./scripts/build.sh --debug

# Serve locally
./scripts/serve.sh
```


## Testing Strategy

- **Unit tests**: C unit tests for CPU instructions, MMU mapping, etc.
- **Mooneye test suite**: Run Mooneye test suite
- **Integration**: Load commercial ROMs and verify gameplay

### Test Commands

```
# Run unit tests
./scripts/test.sh

# Run mooneye test suite
./scripts/run-mooneye.sh ./mooneye-test-suite
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
