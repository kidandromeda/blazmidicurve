// -----------------------------------------------------------------------------
// src/core/Curve.cpp
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Implementación del modelo de datos de la curva de velocidad.
// -----------------------------------------------------------------------------

#include "Curve.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

void Curve::reset() {
    count_ = 2;
    nodes_[0] = {0, 0};
    nodes_[1] = {127, 127};
}

void Curve::addNode(int x, int y) {
    if (count_ >= 128) return;
    x = std::clamp(x, 0, 127);
    y = std::clamp(y, 0, 127);
    
    int insert_pos = count_;
    for (int i = 0; i < count_; ++i) {
        if (nodes_[i].x > x) {
            insert_pos = i;
            break;
        }
    }
    
    for (int i = count_; i > insert_pos; --i) {
        nodes_[i] = nodes_[i - 1];
    }
    
    nodes_[insert_pos] = {x, y};
    ++count_;
}

void Curve::removeNode(int index) {
    if (index < 0 || index >= count_ || count_ <= 2) return;
    
    for (int i = index; i < count_ - 1; ++i) {
        nodes_[i] = nodes_[i + 1];
    }
    --count_;
}

void Curve::moveNode(int index, int x, int y) {
    if (index < 0 || index >= count_) return;
    
    x = std::clamp(x, 0, 127);
    y = std::clamp(y, 0, 127);
    
    int min_x = (index > 0) ? nodes_[index - 1].x + 1 : 0;
    int max_x = (index < count_ - 1) ? nodes_[index + 1].x - 1 : 127;
    
    if (min_x > max_x) {
        return;
    }
    
    x = std::clamp(x, min_x, max_x);
    nodes_[index] = {x, y};
}

void Curve::invert() {
    for (int i = 0; i < count_; ++i) {
        nodes_[i].y = 127 - nodes_[i].y;
    }
}

void Curve::smooth() {
    if (count_ < 3) return;
    
    Node temp[128];
    temp[0] = nodes_[0];
    temp[count_ - 1] = nodes_[count_ - 1];
    
    for (int i = 1; i < count_ - 1; ++i) {
        temp[i].x = nodes_[i].x;
        temp[i].y = (nodes_[i - 1].y + nodes_[i].y + nodes_[i + 1].y) / 3;
    }
    
    for (int i = 0; i < count_; ++i) {
        nodes_[i] = temp[i];
    }
}

void Curve::linear() {
    reset();
}

void Curve::expCurve() {
    reset();
    count_ = 5;
    nodes_[0] = {0, 0};
    nodes_[1] = {32, 8};
    nodes_[2] = {64, 32};
    nodes_[3] = {96, 80};
    nodes_[4] = {127, 127};
}

void Curve::logCurve() {
    reset();
    count_ = 5;
    nodes_[0] = {0, 0};
    nodes_[1] = {32, 48};
    nodes_[2] = {64, 96};
    nodes_[3] = {96, 120};
    nodes_[4] = {127, 127};
}

void Curve::randomize() {
    reset();
    count_ = 8;
    nodes_[0] = {0, std::rand() % 128};
    for (int i = 1; i < 7; ++i) {
        nodes_[i] = {(i * 127) / 7, std::rand() % 128};
    }
    nodes_[7] = {127, std::rand() % 128};
}

void Curve::setNodes(const Node* nodes, int count) {
    if (!nodes || count < 2 || count > 128) {
        fprintf(stderr, "[MidiCurve] setNodes: invalid input (nodes=%p, count=%d)\n", 
                (const void*)nodes, count);
        reset();
        return;
    }
    
    count_ = 0;
    for (int i = 0; i < count; ++i) {
        int x = std::clamp(nodes[i].x, 0, 127);
        int y = std::clamp(nodes[i].y, 0, 127);
        
        if (count_ > 0 && x <= nodes_[count_ - 1].x) {
            if (x == nodes_[count_ - 1].x) {
                nodes_[count_ - 1].y = y;
                continue;
            }
            continue;
        }
        
        nodes_[count_++] = {x, y};
    }
    
    if (count_ < 2) {
        fprintf(stderr, "[MidiCurve] setNodes: not enough valid nodes after filtering (%d)\n", count_);
        reset();
    }
}
