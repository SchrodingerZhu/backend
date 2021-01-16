//
// Created by schrodinger on 1/15/21.
//

#include <vcfg/virtual_mips.h>
#include <cstring>
#include <gcolor/graph.h>

using namespace vmips;

static inline void color_to_reg(char * buf, size_t color) {
    if (color < 10) {
        sprintf(buf, "t%zu", color);
    } else {
        sprintf(buf, "s%zu", color - 10);
    }
}

std::atomic_size_t VirtReg::GLOBAL{0};

VirtReg::VirtReg() : allocated(false), spilled(false) {
}

std::shared_ptr<VirtReg> VirtReg::create() {
    auto reg = std::make_shared<VirtReg>();
    reg->id.number = GLOBAL.fetch_add(1);
    return reg;
}

std::shared_ptr<VirtReg> VirtReg::create_constant(const char *name) {
    auto reg = std::make_shared<VirtReg>();
    reg->allocated = true;
    std::strcpy(reg->id.name, name);
    return reg;
}




void Instruction::collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &) const {

}

const char *Instruction::name() const {
    return nullptr;
}

std::shared_ptr<VirtReg> Instruction::def() const {
    return nullptr;
}

bool Instruction::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return false;
}

void Instruction::replace(const std::shared_ptr<VirtReg>& reg, const std::shared_ptr<VirtReg>& target) {
}


void Ternary::collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!lhs->allocated) set.insert(lhs);
    if (!op0->allocated) set.insert(op0);
    if (!op1->allocated) set.insert(op1);
}

Ternary::Ternary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1)
        : lhs(std::move(lhs)), op0(std::move(op0)), op1(std::move(op1)) {

}

std::shared_ptr<VirtReg> Ternary::def() const {
    return lhs;
}

bool Ternary::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return lhs == reg || op0 == reg || op1 == reg;
}

void Ternary::replace(const std::shared_ptr<VirtReg>& reg, const std::shared_ptr<VirtReg>& target) {
    if (lhs == reg) lhs = target;
    if (op0 == reg) op0 = target;
    if (op1 == reg) op1 = target;
}

void Ternary::output(std::ostream & out) const {
    out << name() << " " << *lhs << ", " << *op0 << ", " << *op1;
}

void CFGNode::dfs_collect(std::unordered_set<std::shared_ptr<VirtReg>> &regs) {
    if (visited) return;
    visited = true;
    for (auto &i : instructions) {
        i->collect_register(regs);
    }
    for (auto &i : out_edges) {
        i->dfs_collect(regs);
    }
    visited = false;
}

void CFGNode::setup_living(const std::unordered_set<std::shared_ptr<VirtReg>> &reg) {
    if (visited) return;
    visited = true;
    for (auto &i : out_edges) {
        i->setup_living(reg);
    }
    for (auto &i : reg) {
        for (auto &j: out_edges) {
            if (j->lives.count(i)) {
                lives[i] = instructions.size();
            }
        }
    }
    for (auto &i : reg) {
        for (size_t j = 0; j < instructions.size(); ++j) {
            if (instructions[j]->used_register(i)) {
                lives[i] = lives.count(i) == 0 ? j : std::max(lives[i], j);
            }
        }
    }
    visited = false;
}

void CFGNode::generate_web(std::unordered_set<std::shared_ptr<VirtReg>> &liveness) {
    if (visited) return;
    visited = true;
    std::unordered_map<std::shared_ptr<VirtReg>, size_t> birth; // marks define in this scope

    // find new birth
    for (size_t i = 0; i < instructions.size(); ++i) {
        auto def = instructions[i]->def();
        if (def) {
            liveness.insert(def);
            birth[def] = i;
        }
    }

    // find neighbour
    for (auto &i : liveness) {
        for (auto &j : liveness) {
            if (i->id.number == j->id.number) continue;
            else {
                auto interleaved = lives.count(i) && birth.count(j) && lives[i] < birth[j];
                interleaved = interleaved || (lives.count(j) && birth.count(i) && lives[j] < birth[i]);
                if (!interleaved) {
                    i->neighbors.insert(j);
                }
            }
        }
    }

    // kill death
    for (auto &i : lives) {
        if (i.second < instructions.size()) {
            liveness.erase(i.first);
        }
    }

    // handle child
    for (auto &i : out_edges) {
        i->generate_web(liveness);
    }

    // if no child, one more detection is needed
    if (out_edges.empty()) {
        for (auto &i : liveness) {
            for (auto &j : liveness) {
                if (i->id.number == j->id.number) continue;
                else {
                    i->neighbors.insert(j);
                }
            }
        }
    }

    // recover liveness
    for (auto &i : lives) {
        if (i.second < instructions.size()) {
            liveness.insert(i.first);
        }
    }

    for (auto &j : birth) {
        liveness.erase(j.first);
    }

    visited = false;
}

void CFGNode::spill(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &sp, ssize_t stack_location) {
    if (visited) return;
    visited = true;
    std::vector<std::shared_ptr<Instruction>> new_instr;
    std::shared_ptr<VirtReg> last = nullptr; // consecutive
    for(auto &i : instructions) {
        if (i->used_register(reg)) {
            auto tmp = last? last: VirtReg::create();
            if (last) new_instr.pop_back(); // extra save
            tmp->spilled = true;
            auto load = Memory::create<lw>(tmp, sp, stack_location);
            auto save = Memory::create<sw>(tmp, sp, stack_location);
            if(i->def() != reg && !last) new_instr.push_back(load);
            new_instr.push_back(i);
            new_instr.push_back(save); // TODO: discard on sw on returning
            i->replace(reg, tmp);
            last = tmp;
        } else {
            last = nullptr;
            new_instr.push_back(i);
        }
    }
    instructions = new_instr;
    for (auto & i : out_edges) {
        i->spill(reg, sp, stack_location);
    }
    visited = false;
}

void CFGNode::dfs_reset() {
    if (visited) return;
    visited = true;
    lives.clear();
    for (auto & i : out_edges) {
        i->dfs_reset();
    }
    visited = false;
}
#include <iostream>
void CFGNode::color(const std::shared_ptr<VirtReg>& sp, ssize_t& current_stack_shift) {
    auto success = false;
    do {
        std::unordered_set<std::shared_ptr<VirtReg>> regs;
        dfs_collect(regs);
        setup_living(regs);
        std::unordered_set<std::shared_ptr<VirtReg>> liveness;
        generate_web(liveness);
        std::vector<std::shared_ptr<VirtReg>> vec;
        for (auto i : regs) {
            vec.push_back(i);
        }
        std::sort(vec.begin(), vec.end(),
                  [](const std::shared_ptr<VirtReg>& a, const std::shared_ptr<VirtReg>& b){ return a->id.number < b->id.number; });
        std::unordered_map<size_t, size_t> idx_map;
        for (auto i = 0; i < vec.size(); ++i) {
            idx_map[vec[i]->id.number] = i;
        }
        std::vector<std::pair<size_t, size_t>> edges;
        for (auto &i : vec) {
            for (auto &j : i->neighbors) {
                if (j->id.number < i->id.number) continue;
                else {
                    edges.emplace_back(idx_map[i->id.number], idx_map[j->id.number]);
                }
            }
        }
        auto g = Graph(edges, vec.size());
        auto colors = g.color(18);
        std::shared_ptr<VirtReg> failure = nullptr;
        if (colors.first.empty()) {
            for (auto & i: vec) {
                i->neighbors.clear();
            }
            dfs_reset();
            for (auto &i : colors.second) {
                if(!vec[i]->spilled) {
                    failure = vec[i];
                    break;
                }
            }
            spill(failure, sp, current_stack_shift);
            std::cout << "spilling: " << failure->id.number << std::endl;
            output(std::cout);
            current_stack_shift -= 4;
        } else {
            success = true;
            for (auto i = 0; i < vec.size(); ++i) {
                vec[i]->allocated = true;
                vec[i]->id.number = 0;
                color_to_reg(vec[i]->id.name, colors.first[i]);
            }
        }
    } while (!success);
}

Memory::Memory(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> addr, ssize_t offset)
: target(std::move(target)), addr(std::move(addr)), offset(offset) {

}

void Memory::collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!target->allocated) set.insert(target);
    if (!addr->allocated) set.insert(addr);
}

std::shared_ptr<VirtReg> Memory::def() const {
    if (name()[0] == 'l' || name()[1] == 'l') {
        return target;
    }
    return nullptr;
}

bool Memory::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return target == reg || addr == reg;
}

void Memory::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (this->target == reg) {this->target = target; }
    if (this->addr == reg) {this->addr = target; }
}

void Memory::output(std::ostream & out) const {
    out << name() << " " << *target << ", " << offset << "(" << *addr << ")";
}

