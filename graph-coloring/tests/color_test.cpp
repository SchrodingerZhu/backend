//
// Created by schrodinger on 1/15/21.
//
#include <gcolor/graph.h>
#include <iostream>
int main() {
    std::vector<std::pair<size_t, size_t>> data = {
            {0, 1},
            {0, 2},
            {0, 3},
            {1, 2},
            {1, 3},
            {2, 4},
            {3, 4},
    };
    auto g = Graph(data, 5);
    size_t failure;
    auto res = g.color(3, failure);
    for (size_t i = 0; i < res.size(); ++i) {
        std::cout << i << " : " << res[i] << std::endl;
    }
    if (res.empty()) abort();
}