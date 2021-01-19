//
// Created by schrodinger on 1/15/21.
//
#include <vcfg/virtual_mips.h>
#include <gcolor/graph.h>
#include <iostream>
using namespace vmips;

void test_br() {
    auto f  = std::make_shared<Function>("test", 2);
    f->entry();
    auto zero = get_special(SpecialReg::zero);;
    auto r0 = f->append<addi>(zero, 1);
    auto r1 = f->append<addi>(zero, 1);
    auto br1 = f->branch<beq>(r0, r1);

    //branch 1
    auto r2 = f->append<addi>(zero, 1);
    auto br2 = f->branch<beq>(zero, zero);

    //branch 11
    f->append<addi>(zero, 1);

    //branch 12
    f->switch_to(br2.second);
    f->append<addi>(zero, 1);

    auto c = f->join(br2.first, br2.second);

    //branch 2
    f->switch_to(br1.second);
    auto r3 = f->append<addi>(zero, 2);
    auto r4 = f->append<add>(r3, r3);
    auto mm = f->append<add>(r0, r0);

    f->join(c, br1.second);
    f->add_phi(r2, r4);
    f->append<add>(r4, r4);
    f->append<addi>(r4, 4);
    f->append<add>(r4, r0);
    auto val = f->append<add>(r1, r0);
    auto ret = f->call(f, val);
    f->append<add>(ret, ret);
    f->output(std::cout);
    std::cout << std::endl;
    f->color();
    std::cout << std::endl;
    f->output(std::cout);
    f->scan_overlap();
    std::cout << std::endl;
    f->output(std::cout);
    f->handle_alloca();
    std::cout << std::endl;
    f->output(std::cout);

}

void test_fibonacci() {
    auto f  = std::make_shared<Function>("fibonacci", 1);
    auto zero = get_special(SpecialReg::zero);
    f->entry();
    auto one = f->append<addi>(zero, 1);
    auto br = f->branch<ble>(get_special(SpecialReg::a0), one);

    // arg > 1
    auto m = f->append<addi>(get_special(SpecialReg::a0), -1);
    auto n = f->append<addi>(get_special(SpecialReg::a0), -2);
    auto res0 = f->call(f, m);
    auto res1 = f->call(f, n);
    auto sum = f->append<add>(res0, res1);
    f->assign_special(SpecialReg::v0, sum);
    f->add_ret();

    // arg <= 1
    f->switch_to(br.second);
    f->assign_special(SpecialReg::v0, 1);

    f->output(std::cout);

    f->color();
    std::cout << std::endl;
    f->output(std::cout);

    f->scan_overlap();
    std::cout << std::endl;
    f->output(std::cout);

    f->handle_alloca();
    std::cout << std::endl;
    f->output(std::cout);

}

void test_prefix_sum() {
    auto f  = std::make_shared<Function>("sum", 1);
    auto zero = get_special(SpecialReg::zero);
    f->entry();
    auto acc = f->append<li>(0);
    auto current = f->append<move>(get_special(SpecialReg::a0));
    auto body = f->new_section();
    auto after = f->new_section_branch<beqz>(current);

    // loop body
    f->switch_to(body);
    auto added = f->append<add>(acc, current);
    auto updated = f->append<addi>(current, -1);
    f->add_phi(acc, added);
    f->add_phi(updated, current);
    f->branch_exsiting<j>(body);

    // loop tail
    f->switch_to(after);
    f->assign_special(SpecialReg::v0, acc);
    f->output(std::cout);
    std::cout << std::endl;
    f->color();
    f->scan_overlap();
    f->handle_alloca();
    f->output(std::cout);
}
int main() {
    //test_fibonacci();
    test_prefix_sum();
}