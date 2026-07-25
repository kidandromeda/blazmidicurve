#pragma once

#include <cstdint>

struct Node {
    int x = 0;
    int y = 0;
};

class Curve {
public:
    void reset();
    void addNode(int x, int y);
    void removeNode(int index);
    void moveNode(int index, int x, int y);
    void invert();
    void smooth();
    void linear();
    void expCurve();
    void logCurve();
    void randomize();
    void setNodes(const Node* nodes, int count);
    
    int size() const { return count_; }
    const Node& nodeAt(int i) const { return nodes_[i]; }
    const Node* nodes() const { return nodes_; }

private:
    Node nodes_[128];
    int count_ = 0;
};
