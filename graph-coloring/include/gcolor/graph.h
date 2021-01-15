//
// Created by schrodinger on 1/15/21.
//

#ifndef GRAPH_COLORING_GRAPH_H
#define GRAPH_COLORING_GRAPH_H

#include "heap.h"
#include <vector>
#include <cstdint>
#include <stack>

using bitmask_t = uint64_t;

void mark(bitmask_t& mask, size_t i);

size_t get(bitmask_t& mask);

class Graph {
    std::vector<std::pair<size_t, size_t>> graph;
    size_t N;
public:
    explicit Graph(std::vector<std::pair<size_t, size_t>> g, size_t N);

    std::vector<size_t> color(size_t colors, size_t& failure);
};

#endif //GRAPH_COLORING_GRAPH_H
