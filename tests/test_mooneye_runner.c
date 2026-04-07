#include "../src/core/gameboy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIMEOUT_FRAMES 600  /* ~10 seconds at 59.7 FPS */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom_file> [--verbose]\n", argv[0]);
        return 2;
    }

    bool verbose = false;
    if (argc >= 3 && strcmp(argv[2], "--verbose") == 0) {
        verbose = true;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "FAIL: Cannot open %s\n", argv[1]);
        return 2;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *rom = malloc(size);
    if (!rom) {
        fprintf(stderr, "FAIL: Cannot allocate %ld bytes\n", size);
        fclose(f);
        return 2;
    }
    fread(rom, 1, size, f);
    fclose(f);

    gb_init();
    gb_load_rom(rom, (uint32_t)size);

    int frames = 0;
    while (frames < TIMEOUT_FRAMES) {
        gb_run_frame();
        frames++;
        if (gb_is_test_done()) break;
    }

    bool passed = gb_check_mooneye_pass();
    CPU *cpu = gb_get_cpu();

    /* Extract just the filename for cleaner output */
    const char *name = strrchr(argv[1], '/');
    name = name ? name + 1 : argv[1];

    if (passed) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s", name);
        if (!gb_is_test_done()) {
            printf(" (timeout after %d frames)", frames);
        }
        printf("\n");
    }

    if (verbose || !passed) {
        printf("  Registers: B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X\n",
               cpu->b, cpu->c, cpu->d, cpu->e, cpu->h, cpu->l);
        printf("  Expected:  B=03 C=05 D=08 E=0D H=15 L=22\n");
        int slen = gb_get_serial_output_len();
        if (slen > 0) {
            printf("  Serial: %s\n", gb_get_serial_output());
        }
    }

    free(rom);
    return passed ? 0 : 1;
}
