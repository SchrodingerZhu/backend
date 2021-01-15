//
// Created by schrodinger on 1/15/21.
//
#include <vcfg/virtual_mips.h>
#include <gcolor/graph.h>
#include <iostream>
using namespace vmips;
int main() {
    auto zero = VirtReg::create_constant("zero");
    auto r0 = VirtReg::create();
    auto r1 = VirtReg::create();
    auto r2 = VirtReg::create();
    auto r3 = VirtReg::create();

    auto a1 = Ternary::create<add>(r0, zero, zero);
    auto a2 = Ternary::create<add>(r1, r0, r0);
    auto a3 = Ternary::create<add>(r2, r0, r0);
    auto a4 = Ternary::create<add>(r3, r1, r2);
    auto node = CFGNode();

    node.instructions.push_back(a1);
    node.instructions.push_back(a2);
    node.instructions.push_back(a3);
    node.instructions.push_back(a4);

    std::unordered_set<std::shared_ptr<VirtReg>> regs;

    node.dfs_collect(regs);

    for (auto i : regs) {
        std::cout << i->id.number << std::endl;
    }

    node.setup_living(regs);
    for (auto i : node.lives) {
        std::cout << i.first->id.number << ":" << i.second;
        std::cout << std::endl;
    }
    std::unordered_set<std::shared_ptr<VirtReg>> liveness;
    node.generate_web(liveness);
    std::cout << std::endl;
    for (auto i : regs) {
        std::cout << i->id.number << ":";
        for (auto j : i->neighbors) {
            std::cout << " " << j->id.number;
        }
        std::cout << std::endl;
    }
    std::vector<std::shared_ptr<VirtReg>> vec;
    for (auto i : regs) {
        vec.push_back(i);
    }
    std::sort(vec.begin(), vec.end(),
              [](const std::shared_ptr<VirtReg>& a, const std::shared_ptr<VirtReg>& b){ return a->id.number < b->id.number; });
    std::vector<std::pair<size_t, size_t>> edges;
    for (auto &i : vec) {
        for (auto &j : i->neighbors) {
            if (j->id.number < i->id.number) continue;
            else {
                edges.emplace_back(i->id.number, j->id.number);
            }
        }
    }
    std::cout << std::endl;
    auto g = Graph(edges, regs.size());
    size_t failure;
    for (auto & i : edges) {
        std::cout << i.first << " " << i.second << std::endl;
    }
    std::cout << std::endl;
    for (auto & i : g.color(8, failure)) {
        std::cout << i << std::endl;
    }
    return 0;
}