#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "src/core/Curve.h"
#include "src/core/Interpolator.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s\n  (%s)\n", name, #expr); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

void test_identity_lut() {
    Curve c;
    uint8_t lut[128];
    Interpolator::generate(c, lut);

    TEST("lut[0] == 0", lut[0] == 0);
    TEST("lut[64] == 64", lut[64] == 64);
    TEST("lut[127] == 127", lut[127] == 127);
    for (int i = 0; i < 128; ++i)
        TEST("identity lut[i] == i", lut[i] == i);
}

void test_linear_interpolation() {
    Curve c;
    Node nodes[] = {{0, 0}, {64, 0}, {127, 127}};
    c.setNodes(nodes, 3);
    uint8_t lut[128];
    Interpolator::generate(c, lut);

    TEST("lut[0] == 0", lut[0] == 0);
    TEST("lut[32] == 0", lut[32] == 0); // still in first flat segment
    TEST("lut[64] == 0", lut[64] == 0);
    // After x=64, interpolates from 0 to 127
    TEST("lut[96] == 64", lut[96] == static_cast<uint8_t>(0 + (96-64) * 127 / (127-64)));
    TEST("lut[127] == 127", lut[127] == 127);
}

void test_midpoint_interpolation() {
    Curve c;
    Node nodes[] = {{0, 0}, {20, 30}, {40, 100}, {127, 127}};
    c.setNodes(nodes, 4);
    uint8_t lut[128];
    Interpolator::generate(c, lut);

    // Between (20,30) and (40,100): slope = (100-30)/(40-20) = 3.5
    TEST("lut[20] == 30", lut[20] == 30);
    TEST("lut[30] == 65", lut[30] == static_cast<uint8_t>(30 + (30-20) * 70 / 20));
    TEST("lut[40] == 100", lut[40] == 100);
}

void test_range_bounds() {
    Curve c;
    uint8_t lut[128];
    Interpolator::generate(c, lut);

    for (int i = 0; i < 128; ++i) {
        TEST("lut in range", lut[i] >= 0 && lut[i] <= 127);
    }
}

void test_custom_curve() {
    Curve c;
    c.addNode(32, 18);
    c.addNode(60, 90);
    c.addNode(100, 100);

    uint8_t lut[128];
    Interpolator::generate(c, lut);

    TEST("lut validates: 0-based", lut[0] == 0);
    TEST("lut[32] == 18", lut[32] == 18);
    TEST("lut[60] == 90", lut[60] == 90);
    TEST("lut[100] == 100", lut[100] == 100);
    TEST("lut[127] == 127", lut[127] == 127);

    // All values in range
    for (int i = 0; i < 128; ++i)
        TEST("all in 0..127", lut[i] >= 0 && lut[i] <= 127);
}

int main() {
    fprintf(stderr, "=== Interpolator Tests ===\n");

    test_identity_lut();
    test_linear_interpolation();
    test_midpoint_interpolation();
    test_range_bounds();
    test_custom_curve();

    fprintf(stderr, "\nResults: %d passed, %d failed\n",
            tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
