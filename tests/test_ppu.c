#include "test_framework.h"
#include "gameboy.h"
#include <string.h>

static MMU mmu;

static void setup(void) {
    memset(&mmu, 0, sizeof(mmu));
    mmu_init(&mmu);
    ppu_init(&mmu);
}

/* ========== PPU init ========== */
static void test_ppu_init_registers(void) {
    setup();
    ASSERT_EQ(0x91, mmu.io[0x40], "LCDC init 0x91");
    ASSERT_EQ(0x85, mmu.io[0x41], "STAT init 0x85");
    ASSERT_EQ(0xFC, mmu.io[0x47], "BGP init 0xFC");
    ASSERT_EQ(0xFF, mmu.io[0x48], "OBP0 init 0xFF");
    ASSERT_EQ(0xFF, mmu.io[0x49], "OBP1 init 0xFF");
}

/* ========== LCD off: no rendering ========== */
static void test_lcd_off_no_render(void) {
    setup();
    mmu.io[0x40] = 0x00; /* LCDC bit 7 off */
    /* Set a known framebuffer value */
    mmu.framebuffer[0] = 0xDEADBEEF;
    ppu_render_scanline(&mmu, 0);
    ASSERT_EQ(0xDEADBEEF, mmu.framebuffer[0], "LCD off: framebuffer unchanged");
}

/* ========== Background tile rendering ========== */
static void test_bg_solid_tile(void) {
    setup();
    /* LCDC: LCD on (0x80), BG enabled (0x01), tile data at 0x8000 (0x10) */
    mmu.io[0x40] = 0x91;
    mmu.io[0x47] = 0xE4; /* BGP: 11 10 01 00 (3,2,1,0) identity palette */
    mmu.io[0x42] = 0;    /* SCY = 0 */
    mmu.io[0x43] = 0;    /* SCX = 0 */

    /* Tile 0 at 0x8000: all pixels color 3 (both planes all 1s) */
    for (int i = 0; i < 16; i++) {
        mmu.vram[i] = 0xFF; /* Both byte1 and byte2 = 0xFF -> color 3 */
    }

    /* Tile map at 0x9800: tile 0 everywhere */
    /* (already zeroed from setup) */

    ppu_render_scanline(&mmu, 0);
    /* Color 3 with identity palette = palette index 3 = 0xFF000000 (black) */
    ASSERT_EQ(0xFF000000, mmu.framebuffer[0], "BG solid tile pixel 0 is black");
    ASSERT_EQ(0xFF000000, mmu.framebuffer[7], "BG solid tile pixel 7 is black");
}

static void test_bg_empty_tile(void) {
    setup();
    mmu.io[0x40] = 0x91;
    mmu.io[0x47] = 0xE4;
    mmu.io[0x42] = 0;
    mmu.io[0x43] = 0;

    /* Tile 0 at 0x8000: all zeros = color 0 */
    /* (already zeroed) */

    ppu_render_scanline(&mmu, 0);
    /* Color 0 with identity palette = palette index 0 = 0xFFFFFFFF (white) */
    ASSERT_EQ(0xFFFFFFFF, mmu.framebuffer[0], "BG empty tile pixel is white");
}

/* ========== Scroll test ========== */
static void test_bg_scroll(void) {
    setup();
    mmu.io[0x40] = 0x91;
    mmu.io[0x47] = 0xE4;
    mmu.io[0x42] = 0; /* SCY = 0 */
    mmu.io[0x43] = 8; /* SCX = 8 (skip first tile) */

    /* Tile 0: all white (0x00) */
    /* Tile 1: all black (0xFF) at 0x8010 */
    for (int i = 0; i < 16; i++) {
        mmu.vram[0x10 + i] = 0xFF;
    }

    /* Tile map: tile 0 at pos 0, tile 1 at pos 1 */
    mmu.vram[0x1800] = 0;
    mmu.vram[0x1801] = 1;

    ppu_render_scanline(&mmu, 0);
    /* With SCX=8, pixel 0 reads from tile at x_tile=1 -> tile 1 -> black */
    ASSERT_EQ(0xFF000000, mmu.framebuffer[0], "Scrolled BG reads tile 1");
}

/* ========== Palette mapping ========== */
static void test_palette_mapping(void) {
    setup();
    mmu.io[0x40] = 0x91;
    /* Reverse palette: color 0->3, 1->2, 2->1, 3->0 */
    mmu.io[0x47] = 0x1B; /* 00 01 10 11 */
    mmu.io[0x42] = 0;
    mmu.io[0x43] = 0;

    /* Tile 0 all zeros = color 0, but palette maps 0 -> 3 (black) */
    ppu_render_scanline(&mmu, 0);
    ASSERT_EQ(0xFF000000, mmu.framebuffer[0], "Reversed palette: color 0 -> black");
}

/* ========== Sprite rendering ========== */
static void test_sprite_basic(void) {
    setup();
    mmu.io[0x40] = 0x93; /* LCD on, sprites on, BG on, tile data 0x8000 */
    mmu.io[0x47] = 0xE4; /* BGP identity */
    mmu.io[0x48] = 0xE4; /* OBP0 identity */

    /* BG tile 0: all white (zeros) - already zeroed */

    /* Sprite tile 1 at 0x8010: solid color 1 on first line */
    mmu.vram[0x10] = 0xFF; /* byte1 = 0xFF */
    mmu.vram[0x11] = 0x00; /* byte2 = 0x00 -> color 1 */

    /* OAM entry 0: Y=16 (screen Y=0), X=8 (screen X=0), tile=1, flags=0 */
    mmu.oam[0] = 16;  /* Y position (sprite_y - 16 + 8 = screen line) */
    mmu.oam[1] = 8;   /* X position */
    mmu.oam[2] = 1;   /* Tile number */
    mmu.oam[3] = 0;   /* Flags */

    /* Render line 8 (where sprite Y=16 means screen Y=8) */
    ppu_render_scanline(&mmu, 0);
    /* Sprite color 1 with OBP0 identity palette -> palette index 1 = light gray */
    ASSERT_EQ(0xFFAAAAAA, mmu.framebuffer[0 * 160 + 0], "Sprite pixel 0 is light gray");
}

static void test_sprite_transparency(void) {
    setup();
    mmu.io[0x40] = 0x93;
    mmu.io[0x47] = 0xE4;
    mmu.io[0x48] = 0xE4;

    /* Sprite tile 1: color 0 (transparent) on first line */
    mmu.vram[0x10] = 0x00;
    mmu.vram[0x11] = 0x00;

    mmu.oam[0] = 16;
    mmu.oam[1] = 8;
    mmu.oam[2] = 1;
    mmu.oam[3] = 0;

    /* BG: all white */
    ppu_render_scanline(&mmu, 8);
    /* Color 0 sprite is transparent, so BG shows through (white) */
    ASSERT_EQ(0xFFFFFFFF, mmu.framebuffer[8 * 160 + 0], "Transparent sprite shows BG");
}

static void test_sprite_x_flip(void) {
    setup();
    mmu.io[0x40] = 0x93;
    mmu.io[0x47] = 0xE4;
    mmu.io[0x48] = 0xE4;

    /* Sprite tile 1 first line: only leftmost pixel is color 1 */
    mmu.vram[0x10] = 0x80; /* byte1: bit 7 set */
    mmu.vram[0x11] = 0x00; /* byte2: 0 -> color 1 at pixel 0 only */

    /* Sprite at Y=16, X=8, tile=1, X-flip (bit 5) */
    mmu.oam[0] = 16;
    mmu.oam[1] = 8;
    mmu.oam[2] = 1;
    mmu.oam[3] = 0x20; /* X-flip */

    ppu_render_scanline(&mmu, 0);
    /* Without flip: pixel 0 has color. With X-flip: pixel 7 has color */
    ASSERT_EQ(0xFFFFFFFF, mmu.framebuffer[0 * 160 + 0], "X-flipped: pixel 0 transparent");
    ASSERT_EQ(0xFFAAAAAA, mmu.framebuffer[0 * 160 + 7], "X-flipped: pixel 7 has color");
}

/* ========== Window rendering ========== */
static void test_window_rendering(void) {
    setup();
    mmu.io[0x40] = 0xB1; /* LCD on, window on (bit 5), BG on, tile data 0x8000 */
    mmu.io[0x47] = 0xE4;
    mmu.io[0x4A] = 0;    /* WY = 0 */
    mmu.io[0x4B] = 7;    /* WX = 7 (window starts at screen X=0) */
    mmu.io[0x42] = 0;
    mmu.io[0x43] = 0;

    /* Window tile map at 0x9800 (LCDC bit 6 = 0) */
    /* Set tile 1 in window tile map */
    mmu.vram[0x1800] = 1;

    /* Tile 1: solid color 2 */
    mmu.vram[0x10] = 0x00; /* byte1 */
    mmu.vram[0x11] = 0xFF; /* byte2 -> color 2 */

    ppu_render_scanline(&mmu, 0);
    /* Color 2 with identity palette -> palette index 2 -> dark gray (0xFF555555) */
    ASSERT_EQ(0xFF555555, mmu.framebuffer[0], "Window tile rendered");
}

/* ========== 10 sprite per line limit ========== */
static void test_sprite_limit(void) {
    setup();
    mmu.io[0x40] = 0x93;
    mmu.io[0x47] = 0xE4;
    mmu.io[0x48] = 0xE4;

    /* Sprite tile 1: color 1 */
    mmu.vram[0x10] = 0xFF;
    mmu.vram[0x11] = 0x00;

    /* Place 11 sprites all on line 8 */
    for (int i = 0; i < 11; i++) {
        mmu.oam[i * 4 + 0] = 16;          /* Y = 16 -> line 8 */
        mmu.oam[i * 4 + 1] = 8 + i * 8;   /* Spread out X */
        mmu.oam[i * 4 + 2] = 1;
        mmu.oam[i * 4 + 3] = 0;
    }

    ppu_render_scanline(&mmu, 0);
    /* First 10 sprites should render, 11th (at X=88+8=96, screen X=88) should not */
    ASSERT_EQ(0xFFAAAAAA, mmu.framebuffer[0 * 160 + 0], "Sprite 0 rendered");
    /* The 11th sprite at screen X=80 should not render (BG white) */
    ASSERT_EQ(0xFFFFFFFF, mmu.framebuffer[0 * 160 + 80], "11th sprite not rendered (limit)");
}

void run_ppu_tests(void) {
    TEST_SUITE("PPU Tests");

    RUN_TEST(test_ppu_init_registers);
    RUN_TEST(test_lcd_off_no_render);
    RUN_TEST(test_bg_solid_tile);
    RUN_TEST(test_bg_empty_tile);
    RUN_TEST(test_bg_scroll);
    RUN_TEST(test_palette_mapping);
    RUN_TEST(test_sprite_basic);
    RUN_TEST(test_sprite_transparency);
    RUN_TEST(test_sprite_x_flip);
    RUN_TEST(test_window_rendering);
    RUN_TEST(test_sprite_limit);
}
