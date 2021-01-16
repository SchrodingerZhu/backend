//
// Created by schrodinger on 1/15/21.
//
#include <vcfg/virtual_mips.h>
#include <gcolor/graph.h>
#include <iostream>
using namespace vmips;
int main() {
    auto zero = VirtReg::create_constant("zero");
    auto sp = VirtReg::create_constant("sp");
    auto r0 = VirtReg::create();
    auto r1 = VirtReg::create();
    auto r2 = VirtReg::create();
    auto r3 = VirtReg::create();
    auto r4 = VirtReg::create();
    auto r5 = VirtReg::create();

    auto a1 = Ternary::create<add>(r0, zero, zero);
    auto a2 = Ternary::create<add>(r1, r0, r0);
    auto a3 = Ternary::create<add>(r2, r0, r0);
    auto a4 = Ternary::create<add>(r3, r1, r2);
    auto a5 = Ternary::create<add>(r4, r0, r3);
    auto a6 = Ternary::create<add>(r5, r2, r1);
    auto node = CFGNode();

    node.instructions.push_back(a1);
    node.instructions.push_back(a2);
    node.instructions.push_back(a3);
    node.instructions.push_back(a4);
    node.instructions.push_back(a5);
    node.instructions.push_back(a6);
    ssize_t shift = 0;
    node.output(std::cout);
    node.label = "colored";
    node.color(sp, shift);
    node.output(std::cout);
    return 0;
}