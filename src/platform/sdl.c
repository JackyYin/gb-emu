#include "platform.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#define SCALE 3
#define WIN_W (SCREEN_W * SCALE)
#define WIN_H (SCREEN_H * SCALE)
#define AUDIO_FREQ 44100
#define AUDIO_SAMPLES 1024

static Platform *g_platform = NULL;

static uint32_t palette[4] = {
    0xFFFFFFFF,
    0xAAAAAAFF,
    0x555555FF,
    0x000000FF
};

static void audio_callback(void *userdata, uint8_t *stream, int len) {
    (void)userdata;
    float *samples = gb_get_audio_samples();
    uint32_t count = gb_get_audio_sample_count();
    int16_t *out = (int16_t *)stream;
    int frames = len / sizeof(int16_t);

    for (int i = 0; i < frames; i++) {
        if (i < (int)count && count > 0) {
            out[i] = (int16_t)(samples[i] * 32767.0f);
        } else {
            out[i] = 0;
        }
    }

    if (count > 0) {
        gb_get_audio_sample_count();
    }
}

uint32_t convert_pixel(uint32_t color) {
    uint8_t idx = color & 0x03;
    return palette[idx];
}

static void handle_events(Platform *platform) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            platform->running = false;
        } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool pressed = (event.type == SDL_KEYDOWN);
            uint8_t bit = 0xFF;

            switch (event.key.keysym.sym) {
                case SDLK_RIGHT: bit = 0; break;   /* Right */
                case SDLK_LEFT: bit = 1; break;   /* Left */
                case SDLK_UP: bit = 2; break;      /* Up */
                case SDLK_DOWN: bit = 3; break;   /* Down */
                case SDLK_z: bit = 4; break;       /* A */
                case SDLK_x: bit = 5; break;       /* B */
                case SDLK_BACKSPACE: bit = 6; break; /* Select */
                case SDLK_RETURN: bit = 7; break;  /* Start */
                default: continue;
            }

            if (pressed) {
                platform->joypad_state &= ~(1 << bit);
            } else {
                platform->joypad_state |= (1 << bit);
            }
        }
    }
    gb_set_joypad(platform->joypad_state);
}

int platform_init(Platform *platform) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    platform->window = SDL_CreateWindow(
        "Game Boy Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!platform->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    platform->renderer = SDL_CreateRenderer(
        platform->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!platform->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(platform->window);
        SDL_Quit();
        return -1;
    }

    platform->texture = SDL_CreateTexture(
        platform->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H
    );
    if (!platform->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(platform->renderer);
        SDL_DestroyWindow(platform->window);
        SDL_Quit();
        return -1;
    }

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq = AUDIO_FREQ;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = AUDIO_SAMPLES;
    want.callback = audio_callback;

    platform->audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!platform->audio_device) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(platform->audio_device, 0);
    }

    memset(platform->pixels, 0, sizeof(platform->pixels));
    platform->joypad_state = 0xFF;
    platform->running = true;
    g_platform = platform;

    gb_init();

    return 0;
}

void platform_run(Platform *platform) {
    uint32_t *fb = NULL;
    void *pixels_ptr = NULL;
    int pitch = 0;

    while (platform->running) {
        handle_events(platform);

        gb_run_frame();

        fb = gb_get_framebuffer();
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
            uint32_t c = fb[i];
            platform->pixels[i] = (0xFF << 24) | ((c & 0xFF) << 16) | ((c & 0xFF00)) | ((c & 0xFF0000) >> 16);
        }

        SDL_UpdateTexture(platform->texture, NULL, platform->pixels, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(platform->renderer);
        SDL_RenderCopy(platform->renderer, platform->texture, NULL, NULL);
        SDL_RenderPresent(platform->renderer);
    }
}

void platform_cleanup(Platform *platform) {
    if (platform->audio_device) {
        SDL_CloseAudioDevice(platform->audio_device);
    }
    if (platform->texture) {
        SDL_DestroyTexture(platform->texture);
    }
    if (platform->renderer) {
        SDL_DestroyRenderer(platform->renderer);
    }
    if (platform->window) {
        SDL_DestroyWindow(platform->window);
    }
    SDL_Quit();
}

int main(int argc, char *argv[]) {
    Platform platform;
    memset(&platform, 0, sizeof(platform));

    if (platform_init(&platform) < 0) {
        return 1;
    }

    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);

            uint8_t *rom = (uint8_t *)malloc(size);
            if (rom) {
                fread(rom, 1, size, f);
                gb_load_rom(rom, (uint32_t)size);
                printf("Loaded ROM: %s (%ld bytes)\n", argv[1], size);
            }
            fclose(f);
        } else {
            fprintf(stderr, "Failed to open ROM: %s\n", argv[1]);
            platform_cleanup(&platform);
            return 1;
        }
    } else {
        printf("Usage: %s <rom.gb>\n", argv[0]);
        platform_cleanup(&platform);
        return 0;
    }

    platform_run(&platform);
    platform_cleanup(&platform);

    return 0;
}
