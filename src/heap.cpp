//
// Created by schrodinger on 1/15/21.
//

#include <gcolor/heap.h>

DecHeap::DecHeap(const std::vector<size_t> &heap) {
    for (size_t i = 0; i < heap.size(); ++i) {
        this->heap.emplace_back(heap[i], i);
    }
    std::make_heap(this->heap.begin(), this->heap.end(), std::greater<std::pair<size_t, size_t>>());
    this->idx_map.resize(heap.size());
    for (size_t i = 0; i < this->heap.size(); ++i) {
        this->idx_map[this->heap[i].second] = i;
    }
}

void DecHeap::swap(size_t a, size_t b) {
    std::swap(heap[a],heap[b]);
    idx_map[heap[a].second] = a;
    idx_map[heap[b].second] = b;
}

void DecHeap::bubble_up(size_t idx) {
    while (idx && heap[(idx - 1)/2].first > heap[idx].first) {
        this->swap((idx - 1)/2, idx);
        idx = (idx - 1) / 2;
    }
}

void DecHeap::decrease(size_t node, size_t delta) {
    auto idx = idx_map[node];
    if (idx == -1) return;
    heap[idx].first -= delta;
    bubble_up(idx);
}

void DecHeap::trickle_down(size_t idx) {
    while ((idx * 2 + 1) < heap.size()) {
        auto min_idx = idx * 2 + 1;
        if ((idx * 2 + 2) < heap.size() && heap[idx * 2 + 2].first < heap[idx * 2 + 1].first) {
            min_idx += 1;
        }
        if (heap[min_idx].first < heap[idx].first) {
            this->swap(idx, min_idx);
            idx = min_idx;
        } else {
            break;
        }
    }
}

std::pair<size_t, size_t> DecHeap::pop() {
    this->swap(0, heap.size() - 1);
    auto value = heap.back();
    idx_map[heap.back().second] = (size_t)-1;
    heap.pop_back();
    trickle_down(0);
    return value;
}

bool DecHeap::empty() const {
    return heap.empty();
}
