// -----------------------------------------------------------------------------
// src/core/Interpolator.h
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Interpolación de la curva a tabla de búsqueda (LUT) de 128 valores.
// -----------------------------------------------------------------------------

#pragma once

#include "Curve.h"
#include <cstdint>

class Interpolator {
public:
    static void generate(const Curve& curve, uint8_t lut[128]);
};
