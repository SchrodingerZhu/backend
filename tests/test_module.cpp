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
    auto write = module.create_extern("write", 3);
    auto malloc = module.create_extern("malloc", 1);
    auto memcpy = module.create_extern("memcpy", 3);
    auto free = module.create_extern("free", 1);
    auto f = module.create_function("main", 3);
    auto greetings = f->create_data<asciiz>(true, data );
    auto size = f->append<li>(sizeof(data));
    auto fd = f->append<li>(0);
    auto waddr = f->append<la>(greetings);
    f->call_void(write, fd, waddr, size);
    auto addr = f->call(malloc, size);
    f->call_void(memcpy, addr, waddr, size);
    f->call_void(write, fd, waddr, size);
    f->call_void(free, addr);
    f->assign_special(SpecialReg::v0, 0);
    module.finalize();
    module.output(std::cout);
}