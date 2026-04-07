#include <emscripten.h>
#include <stdint.h>

extern void gb_init(void);
extern void gb_load_boot_rom(const uint8_t *rom, uint32_t size);
extern void gb_load_rom(const uint8_t *rom, uint32_t size);
extern void gb_run_frame(void);
extern uint32_t *gb_get_framebuffer(void);
extern void gb_set_joypad(uint8_t state);
extern float *gb_get_audio_samples(void);
extern uint32_t gb_get_audio_sample_count(void);

EMSCRIPTEN_KEEPALIVE
void gb_init_wasm(void) {
    gb_init();
}

EMSCRIPTEN_KEEPALIVE
void gb_load_boot_rom_wasm(const uint8_t *rom, uint32_t size) {
    gb_load_boot_rom(rom, size);
}

EMSCRIPTEN_KEEPALIVE
void gb_load_rom_wasm(const uint8_t *rom, uint32_t size) {
    gb_load_rom(rom, size);
}

EMSCRIPTEN_KEEPALIVE
void gb_run_frame_wasm(void) {
    gb_run_frame();
}

EMSCRIPTEN_KEEPALIVE
uint32_t *gb_get_framebuffer_wasm(void) {
    return gb_get_framebuffer();
}

EMSCRIPTEN_KEEPALIVE
void gb_set_joypad_wasm(uint8_t state) {
    gb_set_joypad(state);
}

EMSCRIPTEN_KEEPALIVE
float *gb_get_audio_samples_wasm(void) {
    return gb_get_audio_samples();
}

EMSCRIPTEN_KEEPALIVE
uint32_t gb_get_audio_sample_count_wasm(void) {
    return gb_get_audio_sample_count();
}
