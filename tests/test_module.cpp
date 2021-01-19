//
// Created by schrodinger on 1/19/21.
//

#include <vcfg/virtual_mips.h>
#include <gcolor/graph.h>
#include <iostream>
using namespace vmips;

int main() {
    char data[] = "hello, world!\n";
    auto module = Module("test");
    auto printf = module.create_extern("printf", 2);
    auto malloc = module.create_extern("malloc", 1);
    auto memcpy = module.create_extern("memcpy", 3);
    auto free = module.create_extern("free", 1);
    auto regs = module.create_extern("registers_13", 1);
    auto f = module.create_function("main", 3);
    auto greetings = f->create_data<asciiz>(true, data );
    auto format = f->create_data<asciiz>(true, "%d\n" );
    auto waddr = f->append<la>(greetings);
    auto faddr = f->append<la>(format);

    f->call_void(printf, waddr);
    auto retval = f->call(regs, get_special(SpecialReg::zero));
    f->call_void(printf, faddr, retval);

    auto size = f->append<li>(sizeof(data));
    auto addr = f->call(malloc, size);
    f->call_void(memcpy, addr, waddr, size);
    f->call_void(printf, addr);
    f->call_void(free, addr);
    f->assign_special(SpecialReg::v0, 0);
    module.finalize();
    module.output(std::cout);
}