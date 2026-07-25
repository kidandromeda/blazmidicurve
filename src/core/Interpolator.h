#pragma once

#include "Curve.h"
#include <cstdint>

class Interpolator {
public:
    static void generate(const Curve& curve, uint8_t lut[128]);
};
