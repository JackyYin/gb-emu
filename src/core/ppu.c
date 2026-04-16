#include "gameboy.h"
#include <stdlib.h>

// https://gbdev.io/pandocs/Tile_Data.html
#define TILE_SHIFT          (3) // 1 << 3 = 8 pixels
#define TILE_SIZE_SHIFT     (4) // 1 << 4 = 16 bytes
#define TILES_PER_ROW_SHIFT (5) // 256 pixels / 8 pixels = 32

// https://gbdev.io/pandocs/Scrolling.html?highlight=WX#ff4aff4b--wy-wx-window-y-position-x-position-plus-7
#define WX_TO_ACTUAL_X(x) \
    ((uint8_t)(wx) - 7)

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

static uint8_t get_tile_pixel(uint8_t lsb_byte, uint8_t msb_byte, int x) {
    uint8_t bit1 = (lsb_byte >> (7 - x)) & 1;
    uint8_t bit2 = (msb_byte >> (7 - x)) & 1;
    return (bit1 | (bit2 << 1));
}

static uint8_t get_color_idx_by_x_y(MMU *mmu, uint16_t tile_data_addr, uint8_t x, uint8_t y) {
    uint8_t line_in_tile = y & ((1 << TILE_SHIFT)-1);
    uint8_t byte1 = read_vram(mmu, tile_data_addr + line_in_tile * 2);
    uint8_t byte2 = read_vram(mmu, tile_data_addr + line_in_tile * 2 + 1);
    return get_tile_pixel(byte1, byte2,  x & ((1 << TILE_SHIFT) -1));
}

static uint16_t get_tile_data_addr_by_idx(bool use_unsigned, uint8_t tile_idx) {
    uint16_t tile_data_addr;
    if (use_unsigned) {
        tile_data_addr = 0x8000 + (tile_idx << TILE_SIZE_SHIFT);
    } else {
        tile_data_addr = 0x9000 + ((int8_t)tile_idx << TILE_SIZE_SHIFT);
    }
    return tile_data_addr;
}

static uint8_t get_tile_idx_by_x_y(MMU *mmu, uint16_t tile_map_addr, uint8_t x, uint8_t y) {
    uint8_t tile_x = x >> TILE_SHIFT;
    uint8_t tile_y = y >> TILE_SHIFT;
    uint16_t tile_addr = tile_map_addr + (tile_y << TILES_PER_ROW_SHIFT) + tile_x;
    return read_vram(mmu, tile_addr);
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

    uint8_t bg_priority[SCREEN_W];

    for (int x = 0; x < SCREEN_W; x++) {
        uint8_t color_idx;

        if (!bg_enabled) {
            color_idx = 0;
        }
        else if (window_enabled && line >= wy && x >= WX_TO_ACTUAL_X(wx)) {
            uint8_t win_x = x - WX_TO_ACTUAL_X(wx);
            uint8_t win_y = line - wy;
            uint8_t tile_idx = get_tile_idx_by_x_y(mmu, win_tile_map, win_x, win_y);
            uint16_t tile_data_addr = get_tile_data_addr_by_idx(use_unsigned, tile_idx);
            color_idx = get_color_idx_by_x_y(mmu, tile_data_addr, win_x, win_y);
        } else {
            uint8_t wrapped_x = (uint8_t)(x + scx);
            uint8_t wrapped_y = (uint8_t)(line + scy);
            uint8_t tile_idx = get_tile_idx_by_x_y(mmu, bg_tile_map, wrapped_x, wrapped_y);
            uint16_t tile_data_addr = get_tile_data_addr_by_idx(use_unsigned, tile_idx);
            color_idx = get_color_idx_by_x_y(mmu, tile_data_addr, wrapped_x, wrapped_y);
        }

        bg_priority[x] = color_idx;

        uint8_t pal_idx = (bgp >> (color_idx * 2)) & 0x03;
        mmu->framebuffer[line * SCREEN_W + x] = gb_colors[pal_idx];
    }

    if (!obj_enabled) return;

    int sprite_w = 8;
    int sprite_h = use_8x16 ? 16 : 8;

    Sprite sprites[MAX_SPRITES_PER_LINE];
    int sprite_count = 0;

    for (int i = 0; i < OAM_COUNT && sprite_count < MAX_SPRITES_PER_LINE; i++) {
        uint16_t oam_addr = 0xFE00 + i * 4;
        uint8_t sprite_y = read_oam(mmu, oam_addr);
        uint8_t sprite_x = read_oam(mmu, oam_addr + 1);

        if (sprite_x == 0 || sprite_x >= 168) continue;

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
            int actual_y = (int)OAM_SPRITE_Y_TO_ACTUAL_Y(sprites[s].y);
            int actual_x = (int)OAM_SPRITE_X_TO_ACTUAL_X(sprites[s].x);

            if (x < actual_x || x >= actual_x + sprite_w) continue;

            uint8_t tile_num = sprites[s].tile;
            uint8_t flags = sprites[s].flags;
            bool x_flip = flags & 0x20;
            bool y_flip = flags & 0x40;
            bool priority = flags & 0x80; // 0 = No, 1 = BG and Window color indices 1–3 are drawn over this OBJ
            uint8_t pal = (flags & 0x10) ? obp1 : obp0;

            if (priority && bg_priority[x] != 0) continue;

            if (use_8x16) {
                tile_num &= 0xFE;
            }

            uint8_t pixel_x = x - actual_x;
            uint8_t pixel_y = line - actual_y;

            if (x_flip) pixel_x = 7 - pixel_x;
            if (y_flip) pixel_y = sprite_h - 1 - pixel_y;

            uint16_t tile_data_addr = 0x8000 + (tile_num << TILE_SIZE_SHIFT);
            uint8_t byte1 = read_vram(mmu, tile_data_addr + pixel_y * 2);
            uint8_t byte2 = read_vram(mmu, tile_data_addr + pixel_y * 2 + 1);
            uint8_t color_idx = get_tile_pixel(byte1, byte2, pixel_x);

            if (color_idx == 0) continue;

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
