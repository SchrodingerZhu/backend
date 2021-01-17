//
// Created by schrodinger on 1/15/21.
//
#include <vcfg/virtual_mips.h>
#include <gcolor/graph.h>
#include <iostream>
using namespace vmips;

void test_br() {
    Function f {"test"};
    f.entry();
    auto zero = get_special(SpecialReg::zero);;
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