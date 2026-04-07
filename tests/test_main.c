#include "test_framework.h"

int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

extern void run_cpu_tests(void);
extern void run_mmu_tests(void);
extern void run_ppu_tests(void);
extern void run_cartridge_tests(void);
extern void run_integration_tests(void);

int main(void) {
    printf("Game Boy Emulator Unit Tests\n");
    printf("============================\n");

    run_cpu_tests();
    run_mmu_tests();
    run_ppu_tests();
    run_cartridge_tests();
    run_integration_tests();

    TEST_REPORT();

    return tests_failed > 0 ? 1 : 0;
}
