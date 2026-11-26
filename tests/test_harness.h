#ifndef KOCHEN_TEST_HARNESS_H
#define KOCHEN_TEST_HARNESS_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;

#define ASSERT_TRUE(cond) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

#define ASSERT_EQ_INT(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    g_tests_run++; \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, #a, _a, _b); \
        exit(1); \
    } \
} while (0)

#define ASSERT_NEAR(a, b, eps) do { \
    double _a = (double)(a), _b = (double)(b), _e = (double)(eps); \
    g_tests_run++; \
    if (fabs(_a - _b) > _e) { \
        fprintf(stderr, "FAIL %s:%d: %s == %.4f, expected %.4f +/- %.4f\n", __FILE__, __LINE__, #a, _a, _b, _e); \
        exit(1); \
    } \
} while (0)

#endif /* KOCHEN_TEST_HARNESS_H */
