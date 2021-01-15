//
// Created by schrodinger on 1/15/21.
//

#ifndef GRAPH_COLORING_HEAP_H
#define GRAPH_COLORING_HEAP_H

#include <vector>
#include <algorithm>
#include <functional>

class DecHeap {
    std::vector<std::pair<size_t, size_t>> heap {}; // (degree, index)
    std::vector<size_t> idx_map {}; // index -> heap position

    void trickle_down(size_t idx);

    void bubble_up(size_t idx);

    void swap(size_t a, size_t b);

public:
    explicit DecHeap(const std::vector<size_t>& heap);

    void decrease(size_t node, size_t delta);

    std::pair<size_t, size_t> pop();

    bool empty() const;
};


#endif //GRAPH_COLORING_HEAP_H
