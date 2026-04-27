cmake_minimum_required(VERSION 3.16)
project(gameboy-emu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Source files
set(CORE_SOURCES
    src/core/cpu.c
    src/core/ppu.c
    src/core/apu.c
    src/core/mmu.c
    src/core/cartridge.c
    src/core/timer.c
    src/core/serial.c
    src/core/gameboy.c
    src/core/oam.c
)

set(PLATFORM_WASM_SOURCES
    src/platform/wasm.c
)

# Include directories
include_directories(src/core src/platform)

# Emscripten-specific settings
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    add_executable(gameboy ${CORE_SOURCES} ${PLATFORM_WASM_SOURCES})

    set_target_properties(gameboy PROPERTIES
        LINK_FLAGS "\
            -s WASM=1 \
            -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8'] \
            -s ALLOW_MEMORY_GROWTH=1 \
            -s MODULARIZE=1 \
            -s EXPORT_ES6=1 \
            -s EXPORT_NAME='GameBoyModule' \
            -s ENVIRONMENT='web' \
            -s EXPORTED_FUNCTIONS=['_gb_init_wasm','_gb_load_rom_wasm','_gb_run_frame_wasm','_gb_get_framebuffer_wasm','_gb_set_joypad_wasm','_gb_get_audio_samples_wasm','_gb_get_audio_sample_count_wasm','_malloc','_free'] \
        "
    )

    set_target_properties(gameboy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist"
    )
else()
    option(BUILD_SDL "Build SDL version" ON)

    if(BUILD_SDL)
        find_package(SDL2 REQUIRED)

        if(SDL2_FOUND)
            add_executable(gameboy_sdl ${CORE_SOURCES} src/platform/sdl.c)
            target_include_directories(gameboy_sdl PRIVATE ${SDL2_INCLUDE_DIRS})
            target_link_libraries(gameboy_sdl ${SDL2_LIBRARIES})
            if(APPLE)
                target_link_libraries(gameboy_sdl "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
            endif()
        else()
            message(WARNING "SDL2 not found, falling back to test build")
            add_executable(gameboy_test ${CORE_SOURCES} src/platform/native.c)
        endif()
    else()
        add_executable(gameboy_test ${CORE_SOURCES} src/platform/native.c)
    endif()

    # Unit tests
    set(TEST_SOURCES
        tests/test_main.c
        tests/test_cpu.c
        tests/test_mmu.c
        tests/test_ppu.c
        tests/test_cartridge.c
        tests/test_integration.c
    )
    add_executable(gameboy_unit_tests ${CORE_SOURCES} ${TEST_SOURCES})
    target_include_directories(gameboy_unit_tests PRIVATE tests)

    # Mooneye test runner
    add_executable(mooneye_runner tests/test_mooneye_runner.c ${CORE_SOURCES})
    target_include_directories(mooneye_runner PRIVATE src/core)

    enable_testing()
    add_test(NAME unit_tests COMMAND gameboy_unit_tests)
endif()
