#include "../core/gameboy.h"

int main(void) {
    GameBoy gb;
    gb_init();
    gb_run_frame();
    return 0;
}
