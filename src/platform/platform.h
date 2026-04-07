#ifndef PLATFORM_H
#define PLATFORM_H

#include "../core/gameboy.h"

typedef struct {
    void *window;
    void *renderer;
    void *texture;
    uint32_t audio_device;
    uint32_t pixels[SCREEN_W * SCREEN_H];
    volatile uint8_t joypad_state;
    volatile bool running;
} Platform;

int platform_init(Platform *platform);
void platform_run(Platform *platform);
void platform_cleanup(Platform *platform);

#endif
