//
// Created by schrodinger on 1/15/21.
//

#include <gcolor/graph.h>
#include <strings.h>
void mark(bitmask_t &mask, size_t i) {
    mask |= (1 << i);
}

size_t get(bitmask_t &mask) {
    return ffsll(~mask) - 1;
}

std::pair<std::vector<size_t>, std::vector<size_t>>  Graph::color(size_t colors) {
    std::vector<size_t> data;
    std::vector<size_t> result;
    std::vector<std::vector<size_t>> connections;

    result.resize(N, -1);
    data.resize(N, 0);
    connections.resize(N);

    bool failed = false;
    // degrees
    for (auto& i : graph) {
        data[i.second] += 1;
        data[i.first] += 1;
        connections[i.first].push_back(i.second);
        connections[i.second].push_back(i.first);
    }

    auto heap = DecHeap(data);
    std::stack<size_t> order;
    while (!heap.empty()) {
        auto node = heap.pop();
        if (node.first >= colors) {
            failed = true;
            goto ending;
        } else {
            for (auto & i : connections[node.second]) {
                heap.decrease(i, 1);
            }
            order.push(node.second);
        }
    }
    while (!order.empty()) {
        size_t t = order.top();
        order.pop();
        bitmask_t mask = 0;
        for (auto & i : connections[t]) {
            mark(mask, result[i]);
        }
        result[t] = get(mask);
    }
ending:
    std::vector<size_t> info;
    if (failed) {
        result.clear();
        for(size_t i = 0; i < data.size(); ++i) {
            info.push_back(i);
        }
        std::sort(info.begin(), info.end(), [&](size_t a, size_t b) {return data[a] > data[b]; });
    }
    return {result, info};
}

Graph::Graph(std::vector<std::pair<size_t, size_t>> g, size_t N) : graph(std::move(g)), N(N) {

}
