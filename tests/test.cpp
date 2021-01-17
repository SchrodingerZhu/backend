//
// Created by schrodinger on 1/15/21.
//
#include <vcfg/virtual_mips.h>
#include <gcolor/graph.h>
#include <iostream>
using namespace vmips;

void test_simple() {
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
    auto node = CFGNode("uncolored");

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
}

void test_br() {
    Function f {"test"};
    f.entry();
    auto zero = VirtReg::create_constant("zero");
    auto r0 = f.append<addi>(zero, 1);
    auto r1 = f.append<addi>(zero, 1);
    auto br1 = f.branch<beq>(r0, r1);

    //branch 1
    auto r2 = f.append<addi>(zero, 1);
    auto br2 = f.branch<beq>(zero, zero);

    //branch 11
    f.append<addi>(zero, 1);

    //branch 12
    f.switch_to(br2.second);
    f.append<addi>(zero, 1);

    auto c = f.join(br2.first, br2.second);

    //branch 2
    f.switch_to(br1.second);
    auto r3 = f.append<addi>(zero, 2);
    auto r4 = f.append<add>(r3, r3);
    auto mm = f.append<add>(r0, r0);

    f.join(c, br1.second);
    f.add_phi(r2, r4);
    f.append<add>(r4, r4);
    f.append<addi>(r4, 4);
    f.append<add>(r4, r0);
    f.append<add>(r1, r0);
    f.output(std::cout);
    std::cout << std::endl;
    f.color();
    std::cout << std::endl;
    f.output(std::cout);
}
int main() {
    test_br();
}