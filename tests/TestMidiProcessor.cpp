#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "src/core/MidiProcessor.h"

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

// Identity velocity map
static const uint8_t kIdentity[128] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
    48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127
};

// Custom velocity map: halve all values
static uint8_t kHalved[128];
static bool kHalvedInitialized = false;
void initHalved() {
    if (!kHalvedInitialized) {
        for (int i = 0; i < 128; ++i)
            kHalved[i] = i / 2;
        kHalvedInitialized = true;
    }
}

void test_note_on_passthrough() {
    uint8_t new_vel = 0;
    // NOTE ON, channel 0, velocity 100
    uint8_t event[] = {0x90, 60, 100};
    bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
    TEST("NOTE ON returns true", result == true);
    TEST("velocity maps 100->100", new_vel == 100);
}

void test_note_on_mapped() {
    initHalved();
    uint8_t new_vel = 0;
    uint8_t event[] = {0x90, 60, 100};
    bool result = MidiProcessor::processNoteOn(kHalved, event, new_vel);
    TEST("NOTE ON mapped", result == true);
    TEST("velocity maps 100->50", new_vel == 50);
}

void test_note_off_passthrough() {
    uint8_t new_vel = 0;
    // NOTE OFF, channel 0
    uint8_t event[] = {0x80, 60, 0};
    bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
    TEST("NOTE OFF returns false", result == false);
}

void test_cc_passthrough() {
    uint8_t new_vel = 0;
    uint8_t event[] = {0xB0, 7, 100}; // CC 7 (volume)
    bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
    TEST("CC returns false", result == false);
}

void test_pitch_bend_passthrough() {
    uint8_t new_vel = 0;
    uint8_t event[] = {0xE0, 0, 64}; // Pitch bend
    bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
    TEST("Pitch Bend returns false", result == false);
}

void test_aftertouch_passthrough() {
    uint8_t new_vel = 0;
    uint8_t event[] = {0xD0, 64, 0}; // Channel aftertouch (2 bytes actually, but we test with 3)
    // Aftertouch status is 0xD0, which doesn't match 0x90 mask
    bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
    TEST("Aftertouch returns false", result == false);
}

void test_poly_aftertouch_passthrough() {
    uint8_t new_vel = 0;
    uint8_t event[] = {0xA0, 60, 64}; // Poly aftertouch
    bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
    TEST("Poly Aftertouch returns false", result == false);
}

void test_program_change_passthrough() {
    uint8_t new_vel = 0;
    uint8_t event[] = {0xC0, 5, 0}; // Program change
    bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
    TEST("Program Change returns false", result == false);
}

void test_all_channels() {
    for (int ch = 0; ch < 16; ++ch) {
        uint8_t event[] = {static_cast<uint8_t>(0x90 | ch), 60, 100};
        uint8_t new_vel = 0;
        bool result = MidiProcessor::processNoteOn(kIdentity, event, new_vel);
        TEST("NOTE ON all channels", result == true);
        TEST("velocity mapped correctly for all channels", new_vel == 100);
    }
}

void test_velocity_zero() {
    initHalved();
    uint8_t new_vel = 0;
    uint8_t event[] = {0x90, 60, 0};
    bool result = MidiProcessor::processNoteOn(kHalved, event, new_vel);
    TEST("NOTE ON vel=0 returns true", result == true);
    TEST("velocity 0 maps to 0", new_vel == 0);

    uint8_t event2[] = {0x90, 60, 1};
    result = MidiProcessor::processNoteOn(kHalved, event2, new_vel);
    TEST("velocity 1 maps to 0", new_vel == 0);
}

int main() {
    fprintf(stderr, "=== MidiProcessor Tests ===\n");

    test_note_on_passthrough();
    test_note_on_mapped();
    test_note_off_passthrough();
    test_cc_passthrough();
    test_pitch_bend_passthrough();
    test_aftertouch_passthrough();
    test_poly_aftertouch_passthrough();
    test_program_change_passthrough();
    test_all_channels();
    test_velocity_zero();

    fprintf(stderr, "\nResults: %d passed, %d failed\n",
            tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
