#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

extern int tests_run;
extern int tests_passed;
extern int tests_failed;

#define ASSERT_EQ(expected, actual, msg) do { \
    tests_run++; \
    if ((expected) == (actual)) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (expected 0x%X, got 0x%X) at %s:%d\n", \
            msg, (unsigned int)(expected), (unsigned int)(actual), __FILE__, __LINE__); \
    } \
} while (0)

#define ASSERT_TRUE(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); \
    } \
} while (0)

#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)

#define ASSERT_MEM_EQ(expected, actual, len, msg) do { \
    tests_run++; \
    if (memcmp((expected), (actual), (len)) == 0) { \
        tests_passed++; \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s (memory mismatch, %d bytes) at %s:%d\n", \
            msg, (int)(len), __FILE__, __LINE__); \
    } \
} while (0)

#define RUN_TEST(test_func) do { \
    printf("  %s...\n", #test_func); \
    test_func(); \
} while (0)

#define TEST_SUITE(name) \
    printf("\n=== %s ===\n", name)

#define TEST_REPORT() do { \
    printf("\n--- Results: %d/%d passed", tests_passed, tests_run); \
    if (tests_failed > 0) printf(", %d FAILED", tests_failed); \
    printf(" ---\n"); \
} while (0)

#endif
