#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "src/core/Curve.h"

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

void test_default_curve() {
    Curve c;
    TEST("default has 2 nodes", c.size() == 2);
    TEST("first node x=0", c.nodeAt(0).x == 0);
    TEST("first node y=0", c.nodeAt(0).y == 0);
    TEST("last node x=127", c.nodeAt(1).x == 127);
    TEST("last node y=127", c.nodeAt(1).y == 127);
    TEST("curve validates", c.validate());
}

void test_add_node() {
    Curve c;
    c.addNode(64, 50);
    TEST("added node -> 3 nodes", c.size() == 3);
    bool found = false;
    for (int i = 0; i < c.size(); ++i) {
        if (c.nodeAt(i).x == 64 && c.nodeAt(i).y == 50)
            found = true;
    }
    TEST("node (64,50) exists", found);
    TEST("curve still validates", c.validate());
}

void test_remove_node() {
    Curve c;
    c.addNode(64, 50);
    c.addNode(100, 30);
    TEST("4 nodes after adding two", c.size() == 4);
    c.removeNode(1); // Remove first added node
    TEST("3 nodes after removal", c.size() == 3);
    TEST("curve validates after removal", c.validate());

    // Try to remove endpoints (should fail)
    c.removeNode(0);
    TEST("cannot remove first node", c.size() == 3);
    c.removeNode(c.size() - 1);
    TEST("cannot remove last node", c.size() == 3);
}

void test_move_node() {
    Curve c;
    c.addNode(64, 64);
    c.moveNode(1, 32, 20); // Move the middle node
    for (int i = 0; i < c.size(); ++i) {
        if (c.nodeAt(i).x == 32) {
            TEST("moved node y=20", c.nodeAt(i).y == 20);
            break;
        }
    }
    TEST("curve validates after move", c.validate());
}

void test_presets() {
    Curve c;
    c.reset();
    TEST("reset: (0,0)", c.nodeAt(0).x == 0 && c.nodeAt(0).y == 0);
    TEST("reset: (127,127)", c.nodeAt(1).x == 127 && c.nodeAt(1).y == 127);

    c.invert();
    TEST("invert: (0,127)", c.nodeAt(0).x == 0 && c.nodeAt(0).y == 127);
    TEST("invert: (127,0)", c.nodeAt(1).x == 127 && c.nodeAt(1).y == 0);

    c.linear();
    TEST("linear: identity", c.nodeAt(0).y == 0 && c.nodeAt(1).y == 127);

    c.expCurve();
    TEST("exp curve validates", c.validate());
    TEST("exp: (0,0)", c.nodeAt(0).y == 0);
    TEST("exp: (127,127)", c.nodeAt(c.size()-1).y == 127);

    c.logCurve();
    TEST("log curve validates", c.validate());
    TEST("log: (0,0)", c.nodeAt(0).y == 0);
    TEST("log: (127,127)", c.nodeAt(c.size()-1).y == 127);

    c.randomize();
    TEST("random curve validates", c.validate());
}

void test_curve_constraints() {
    Curve c;
    c.addNode(50, 50);
    c.addNode(60, 60);

    // Try to move node past neighbour
    c.moveNode(1, 80, 80); // Move first inner node to 80
    c.moveNode(2, 40, 70); // Move second inner node to 40 — should be clamped
    TEST("nodes still sorted", c.validate());
    // Verify order
    for (int i = 1; i < c.size(); ++i)
        TEST("node ordering", c.nodeAt(i).x > c.nodeAt(i-1).x);
}

void test_json_serialization() {
    Curve c;
    c.addNode(32, 50);
    c.addNode(96, 100);

    char buf[1024];
    serializeCurveStateJSON(c.nodes(), c.size(), buf, sizeof(buf));

    Node parsed[128];
    int count = parseCurveStateJSON(buf, parsed, 128);
    TEST("parsed same number of nodes", count == c.size());
    for (int i = 0; i < count && i < c.size(); ++i) {
        TEST("parsed node x matches", parsed[i].x == c.nodeAt(i).x);
        TEST("parsed node y matches", parsed[i].y == c.nodeAt(i).y);
    }

    // Test round-trip: parse then re-serialize
    char buf2[1024];
    serializeCurveStateJSON(parsed, count, buf2, sizeof(buf2));
    TEST("re-serialized matches", strcmp(buf, buf2) == 0);
}

int main() {
    fprintf(stderr, "=== Curve Tests ===\n");

    test_default_curve();
    test_add_node();
    test_remove_node();
    test_move_node();
    test_presets();
    test_curve_constraints();
    test_json_serialization();

    fprintf(stderr, "\nResults: %d passed, %d failed\n",
            tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
