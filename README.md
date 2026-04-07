# Game Boy Emulator

A Game Boy emulator written in C that runs natively via SDL2 or in the browser via WebAssembly.

## Features

- Sharp LR35902 (Z80-like) CPU emulation
- PPU with background, window, and sprite rendering
- APU with 4 audio channels
- MBC1/3/5 cartridge support
- Cycle-accurate timing
- Native desktop app via SDL2 (macOS/Linux/Windows)
- Browser version via WebAssembly

## Prerequisites

- CMake 3.16+
- **Native build**: SDL2 (`brew install sdl2` on macOS)
- **Web build**: [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)

## Quick Start

### Native (SDL2)

```bash
# 1. Install SDL2
brew install sdl2   # macOS
# or: sudo apt install libsdl2-dev  # Linux

# 2. Build
./scripts/build-sdl.sh

# 3. Run with a ROM
./build-sdl/gameboy_sdl roms/game.gb
```

### Browser (WebAssembly)

```bash
# 1. Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
cd ..

# 2. Setup and build
./scripts/setup.sh
./scripts/build.sh

# 3. Serve
./scripts/serve.sh
```

Then open `http://localhost:8080` in your browser.

## Project Structure

```
├── AGENTS.md           # Development guidelines for AI agents
├── CMakeLists.txt      # CMake build configuration
├── README.md           # This file
├── TESTING.md          # Testing guide
├── roms/               # Place ROM files here (not committed)
├── scripts/            # Build and utility scripts
│   ├── setup.sh        # Configure Emscripten build
│   ├── build.sh        # Compile to WASM
│   ├── build-sdl.sh    # Compile native SDL version
│   ├── serve.sh        # Local development server
│   └── test.sh         # Run unit tests
├── tests/              # Unit and integration tests
│   ├── test_framework.h   # Test assertion macros
│   ├── test_main.c        # Test runner entry point
│   ├── test_cpu.c         # CPU instruction tests
│   ├── test_mmu.c         # Memory mapping tests
│   ├── test_ppu.c         # Graphics rendering tests
│   ├── test_cartridge.c   # Cartridge and timer tests
│   └── test_integration.c # Full emulator tests
├── src/
│   ├── core/           # Emulation core (pure C)
│   │   ├── cpu.c           # CPU instruction set
│   │   ├── ppu.c           # Pixel Processing Unit
│   │   ├── apu.c           # Audio Processing Unit
│   │   ├── mmu.c           # Memory Management Unit
│   │   ├── cartridge.c     # ROM loading + MBC
│   │   ├── timer.c         # TIMA/TMA/TAC timers
│   │   └── gameboy.c       # Main emulation loop
│   └── platform/       # Platform-specific code
│       ├── platform.h      # Platform interface header
│       ├── sdl.c           # SDL2 native implementation
│       └── wasm.c          # Emscripten bindings
├── web/                # Frontend (browser only)
│   ├── index.html      # Main HTML page
│   ├── app.js          # JavaScript glue code
│   └── style.css       # Styles
└── dist/               # Build output (WASM)
    ├── gameboy.js      # JavaScript glue
    └── gameboy.wasm    # WebAssembly binary
```

## Architecture

### Native (SDL2)

```
┌─────────────────────────────────────────────┐
│                  SDL2                       │
│  ┌─────────┐  ┌──────────┐  ┌────────────┐ │
│  │  Input  │  │ Renderer │  │   Audio    │ │
│  │(Keyboard)│ (Texture)  │  │  (Audio)   │ │
│  └────┬────┘  └────┬─────┘  └─────┬──────┘ │
│       │            │              │         │
│  ┌────┴────────────┴──────────────┴──────┐ │
│  │            sdl.c (platform)           │ │
│  └──────────────────┬────────────────────┘ │
└─────────────────────┼──────────────────────┘
                      │
┌─────────────────────┼──────────────────────┐
│     C Core          │                      │
│  ┌──────────────────┴────────────────────┐ │
│  │           Emulation Core              │ │
│  │  ┌──────┐ ┌─────┐ ┌─────┐ ┌───────┐  │ │
│  │  │ CPU  │ │ PPU │ │ APU │ │  MMU  │  │ │
│  │  └──────┘ └─────┘ └─────┘ └───────┘  │ │
│  └───────────────────────────────────────┘ │
└────────────────────────────────────────────┘
```

### Browser (WebAssembly)

```
┌─────────────────────────────────────────────┐
│              Browser (JS/HTML)              │
│  ┌─────────┐  ┌──────────┐  ┌────────────┐ │
│  │  Input  │  │ Renderer │  │   Audio    │ │
│  │(Keyboard)│ (Canvas)   │  │(Web Audio) │ │
│  └────┬────┘  └────┬─────┘  └─────┬──────┘ │
│       │            │              │         │
│  ┌────┴────────────┴──────────────┴──────┐ │
│  │         JavaScript Glue Code          │ │
│  └──────────────────┬────────────────────┘ │
└─────────────────────┼──────────────────────┘
                      │ Emscripten API
┌─────────────────────┼──────────────────────┐
│     C Core          │                      │
│  ┌──────────────────┴────────────────────┐ │
│  │           Emulation Core              │ │
│  │  ┌──────┐ ┌─────┐ ┌─────┐ ┌───────┐  │ │
│  │  │ CPU  │ │ PPU │ │ APU │ │  MMU  │  │ │
│  │  └──────┘ └─────┘ └─────┘ └───────┘  │ │
│  └───────────────────────────────────────┘ │
└────────────────────────────────────────────┘
```

## Development

### Build Commands

```bash
# Native SDL build
./scripts/build-sdl.sh
./scripts/build-sdl.sh --debug

# WebAssembly build
./scripts/setup.sh
./scripts/build.sh
./scripts/build.sh --debug

# Start local server (web version)
./scripts/serve.sh
```

### Native Controls

| Key | Game Boy Button |
|-----|-----------------|
| Z | B |
| X | A |
| Enter | Start |
| Backspace | Select |
| Arrow Keys | D-Pad |

### Testing

```bash
# Run all unit and integration tests
./scripts/test.sh
```

The test suite covers CPU instructions, memory mapping, PPU rendering, cartridge bank switching, and full emulator integration. See [TESTING.md](TESTING.md) for details on the test framework, writing new tests, and test ROM validation.

### Key Constants

| Constant | Value | Description |
|----------|-------|-------------|
| SCREEN_W | 160 | Game Boy screen width |
| SCREEN_H | 144 | Game Boy screen height |
| CPU_FREQ | 4194304 Hz | ~4.19 MHz |
| FPS | ~59.7 | Frames per second |
| FRAME_CYCLES | 70224 | CPU cycles per frame |

## Resources

- [Pan Docs](https://gbdev.io/pandocs/) - Comprehensive Game Boy documentation
- [Game Boy CPU Manual](http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf)
- [Blargg's Test ROMs](https://github.com/retrio/gb-test-roms)
- [mooneye-gb](https://github.com/Gekkio/mooneye-gb) - Test ROM suite

## License

MIT
