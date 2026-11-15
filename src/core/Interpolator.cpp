// -----------------------------------------------------------------------------
// src/core/Interpolator.cpp
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Generación de la LUT interpolando los nodos de la curva.
// -----------------------------------------------------------------------------

#include "Interpolator.h"
#include <cstdio>

void Interpolator::generate(const Curve& curve, uint8_t lut[128]) {
    if (curve.size() < 2) {
        for (int i = 0; i < 128; ++i) lut[i] = i;
        return;
    }
    
    int node_idx = 0;
    for (int x = 0; x < 128; ++x) {
        while (node_idx < curve.size() - 2 && curve.nodeAt(node_idx + 1).x <= x) {
            ++node_idx;
        }
        
        if (node_idx >= curve.size() - 1) {
            lut[x] = curve.nodeAt(curve.size() - 1).y;
            continue;
        }
        
        const Node& n0 = curve.nodeAt(node_idx);
        const Node& n1 = curve.nodeAt(node_idx + 1);
        
        if (n1.x == n0.x) {
            lut[x] = n1.y;
            continue;
        }
        
        if (x <= n0.x) {
            lut[x] = n0.y;
        } else if (x >= n1.x) {
            lut[x] = n1.y;
        } else {
            float t = float(x - n0.x) / float(n1.x - n0.x);
            int val = n0.y + t * (n1.y - n0.y);
            lut[x] = static_cast<uint8_t>(val < 0 ? 0 : (val > 127 ? 127 : val));
        }
    }
}
