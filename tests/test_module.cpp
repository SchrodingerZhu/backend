//
// Created by schrodinger on 1/19/21.
//

#include <vcfg/virtual_mips.h>
#include <gcolor/graph.h>
#include <iostream>
using namespace vmips;

int main() {
    auto module = Module("test");
    auto printf = module.create_extern("printf", 2);
    auto regs = module.create_extern("registers_13", 1);
    auto f = module.create_function("main", 3);
    auto greatings = f->create_data<asciiz>(true, std::vector<std::string>{ "hello, world!\n" });
    auto format = f->create_data<asciiz>(true, std::vector<std::string>{ "%d\n" });
    auto waddr = f->append<la>(greatings);
    auto faddr = f->append<la>(format);
    f->call(printf, waddr);
    auto retval = f->call(regs, get_special(SpecialReg::zero));
    f->call(printf, faddr, retval);
    f->assign_special(SpecialReg::v0, 0);
    module.finalize();
    module.output(std::cout);
}