#include "gameboy.h"
#include <stdlib.h>

// https://gbdev.io/pandocs/OAM.html
#define OAM_COUNT 40
#define MAX_SPRITES_PER_LINE 10

#define OAM_SPRITE_X_TO_ACTUAL_X(x) \
    ((int)(x) - 8)

#define OAM_SPRITE_Y_TO_ACTUAL_Y(y) \
    ((int)(y) - 16)

typedef struct {
    uint8_t y;
    uint8_t x;
    uint8_t tile;
    uint8_t flags;
    int index;
} Sprite;

static uint32_t gb_colors[4] = {
    0xFFFFFFFF,
    0xFFAAAAAA,
    0xFF555555,
    0xFF000000
};

static uint8_t read_vram(MMU *mmu, uint16_t addr) {
    return mmu->vram[addr - 0x8000];
}

static uint8_t read_oam(MMU *mmu, uint16_t addr) {
    return mmu->oam[addr - 0xFE00];
}

static int compare_sprites(const void *a, const void *b) {
    const Sprite *sa = (const Sprite *)a;
    const Sprite *sb = (const Sprite *)b;
    if (sa->x != sb->x) return sa->x - sb->x;
    return sa->index - sb->index;
}

void ppu_render_scanline(MMU *mmu, uint8_t line) {
    uint8_t lcdc = mmu->io[0x40];
    uint8_t bgp = mmu->io[0x47];
    uint8_t obp0 = mmu->io[0x48];
    uint8_t obp1 = mmu->io[0x49];
    uint8_t scx = mmu->io[0x43];
    uint8_t scy = mmu->io[0x42];
    uint8_t wy = mmu->io[0x4A];
    uint8_t wx = mmu->io[0x4B];

    bool lcd_enabled = lcdc & 0x80;                          // bit 7 = LCD & PPU Enable
    uint16_t win_tile_map = (lcdc & 0x40) ? 0x9C00 : 0x9800; // bit 6 = Window tile map  (0=0x9800, 1=0x9C00)
    bool window_enabled = lcdc & 0x20;                       // bit 5 = Window Enable
    bool use_unsigned = lcdc & 0x10;                         // bit 4 = BG & Window tile data (0=0x9000 signed, 1=0x8000 unsigned)
    uint16_t bg_tile_map = (lcdc & 0x08) ? 0x9C00 : 0x9800;  // bit 3 = BG tile map (0=0x9800, 1=0x9C00)
    bool use_8x16    = lcdc & 0x04;                          // bit 2 = OBJ size (0=8x8, 1=8x16)
    bool obj_enabled = lcdc & 0x02;                          // bit 1 = OBJ Enable
    bool bg_enabled = lcdc & 0x01;                           // bit 0 = BG and Window Enable


    if (!lcd_enabled) return;

    uint8_t bg_colors[SCREEN_W];
    uint8_t bg_priority[SCREEN_W];

    for (int x = 0; x < SCREEN_W; x++) {
        uint8_t color_idx;

        if (!bg_enabled) {
            color_idx = 0;
        }
        else if (window_enabled && line >= wy && x >= wx - 1) {
            int win_x = x - (wx - 1);
            int win_y = line - wy;
            uint8_t tile_x = win_x / 8;
            uint8_t tile_y = win_y / 8;
            uint16_t tile_addr = win_tile_map + tile_y * 32 + tile_x;
            uint8_t tile_num = read_vram(mmu, tile_addr);
            uint16_t tile_data_addr;
            if (use_unsigned) {
                tile_data_addr = 0x8000 + tile_num * 16;
            } else {
                tile_data_addr = 0x9000 + (int8_t)tile_num * 16;
            }
            uint8_t line_in_tile = win_y % 8;
            uint8_t bit_x = 7 - (win_x % 8);
            uint8_t byte1 = read_vram(mmu, tile_data_addr + line_in_tile * 2);
            uint8_t byte2 = read_vram(mmu, tile_data_addr + line_in_tile * 2 + 1);
            uint8_t bit1 = (byte1 >> bit_x) & 1;
            uint8_t bit2 = (byte2 >> bit_x) & 1;
            color_idx = (bit2 << 1) | bit1;
        } else {
            uint8_t wrapped_x = (uint8_t)(x + scx);
            uint8_t wrapped_y = (uint8_t)(line + scy);
            uint8_t tile_x = wrapped_x / 8;
            uint8_t tile_y = wrapped_y / 8;
            uint16_t tile_addr = bg_tile_map + tile_y * 32 + tile_x;
            uint8_t tile_num = read_vram(mmu, tile_addr);
            uint16_t tile_data_addr;
            if (use_unsigned) {
                tile_data_addr = 0x8000 + tile_num * 16;
            } else {
                tile_data_addr = 0x9000 + (int8_t)tile_num * 16;
            }
            uint8_t line_in_tile = wrapped_y % 8;
            uint8_t bit_x = 7 - (wrapped_x % 8);
            uint8_t byte1 = read_vram(mmu, tile_data_addr + line_in_tile * 2);
            uint8_t byte2 = read_vram(mmu, tile_data_addr + line_in_tile * 2 + 1);
            uint8_t bit1 = (byte1 >> bit_x) & 1;
            uint8_t bit2 = (byte2 >> bit_x) & 1;
            color_idx = (bit2 << 1) | bit1;
        }

        bg_colors[x] = color_idx;
        bg_priority[x] = color_idx;

        uint8_t pal_idx = (bgp >> (color_idx * 2)) & 0x03;
        mmu->framebuffer[line * SCREEN_W + x] = gb_colors[pal_idx];
    }

    if (!obj_enabled) return;

    Sprite sprites[MAX_SPRITES_PER_LINE];
    int sprite_count = 0;

    for (int i = 0; i < OAM_COUNT && sprite_count < MAX_SPRITES_PER_LINE; i++) {
        uint16_t oam_addr = 0xFE00 + i * 4;
        uint8_t sprite_y = read_oam(mmu, oam_addr);
        uint8_t sprite_x = read_oam(mmu, oam_addr + 1);

        if (sprite_x == 0 || sprite_x >= 168) continue;

        int sprite_h = use_8x16 ? 16 : 8;
        int actual_y = (int)OAM_SPRITE_Y_TO_ACTUAL_Y(sprite_y);
        int actual_x = (int)OAM_SPRITE_X_TO_ACTUAL_X(sprite_x);

        if (line >= actual_y && line < actual_y + sprite_h) {
            sprites[sprite_count].y = sprite_y;
            sprites[sprite_count].x = sprite_x;
            sprites[sprite_count].tile = read_oam(mmu, oam_addr + 2);
            sprites[sprite_count].flags = read_oam(mmu, oam_addr + 3);
            sprites[sprite_count].index = i;
            sprite_count++;
        }
    }

    if (sprite_count == 0) return;

    qsort(sprites, sprite_count, sizeof(Sprite), compare_sprites);

    for (int x = SCREEN_W - 1; x >= 0; x--) {
        for (int s = 0; s < sprite_count; s++) {
            int sprite_h = use_8x16 ? 16 : 8;
            int actual_y = (int)OAM_SPRITE_Y_TO_ACTUAL_Y(sprites[s].y);
            int actual_x = (int)OAM_SPRITE_X_TO_ACTUAL_X(sprites[s].x);

            if (x < actual_x || x >= actual_x + 8) continue;

            uint8_t tile_num = sprites[s].tile;
            uint8_t flags = sprites[s].flags;
            bool x_flip = flags & 0x20;
            bool y_flip = flags & 0x40;
            bool priority = flags & 0x80;
            uint8_t pal = (flags & 0x10) ? obp1 : obp0;

            if (use_8x16) {
                tile_num &= 0xFE;
            }

            uint16_t tile_data_addr = 0x8000 + tile_num * 16;

            int pixel_x = x - actual_x;
            int pixel_y = line - actual_y;

            if (x_flip) pixel_x = 7 - pixel_x;
            if (y_flip) pixel_y = sprite_h - 1 - pixel_y;

            uint8_t byte1 = read_vram(mmu, tile_data_addr + pixel_y * 2);
            uint8_t byte2 = read_vram(mmu, tile_data_addr + pixel_y * 2 + 1);

            uint8_t bit1 = (byte1 >> (7 - pixel_x)) & 1;
            uint8_t bit2 = (byte2 >> (7 - pixel_x)) & 1;
            uint8_t color_idx = (bit2 << 1) | bit1;

            if (color_idx == 0) continue;

            if (priority && bg_priority[x] != 0) continue;

            uint8_t pal_idx = (pal >> (color_idx * 2)) & 0x03;
            mmu->framebuffer[line * SCREEN_W + x] = gb_colors[pal_idx];
            break;
        }
    }
}

void ppu_init(MMU *mmu) {
    mmu->io[0x40] = 0x91;
    mmu->io[0x41] = 0x85;
    mmu->io[0x47] = 0xFC;
    mmu->io[0x48] = 0xFF;
    mmu->io[0x49] = 0xFF;
}
