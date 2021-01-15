//
// Created by schrodinger on 1/15/21.
//
#include <gcolor/heap.h>
int main() {
    std::vector<size_t> data;
    for (int i = 0; i < 100000; ++i) {
        data.emplace_back(rand());
    }
    auto heap = DecHeap(data);
    for (int i = 0; i < 100000; ++i) {
        auto n = rand() % data.size();
        if (data[n] >= 1000) {
            data[n] -= 1000;
            heap.decrease(n, 1000);
        }
    }
    std::vector<size_t> res;
    while (!heap.empty()) {
        res.push_back(data[heap.pop().second]);
    }
    std::sort(data.begin(), data.end());
    if (res != data) abort();
}
