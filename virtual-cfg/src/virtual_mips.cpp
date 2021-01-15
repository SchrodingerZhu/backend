//
// Created by schrodinger on 1/15/21.
//

#include <vcfg/virtual_mips.h>
#include <cstring>
using namespace vmips;


std::atomic_size_t VirtReg::GLOBAL {0 };

VirtReg::VirtReg() : allocated(false) {
}

std::shared_ptr<VirtReg> VirtReg::create() {
    auto reg = std::make_shared<VirtReg>();
    reg->id.number = GLOBAL.fetch_add(1);
    return reg;
}

std::shared_ptr<VirtReg> VirtReg::create_constant(const char * name) {
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

void Ternary::collect_register(std::unordered_set<std::shared_ptr<VirtReg>>& set) const {
    if(!lhs->allocated) set.insert(lhs);
    if(!op0->allocated) set.insert(op0);
    if(!op1->allocated) set.insert(op1);
}

Ternary::Ternary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1)
    : lhs(std::move(lhs)), op0(std::move(op0)), op1(std::move(op1))
{

}

std::shared_ptr<VirtReg> Ternary::def() const {
    return lhs;
}

bool Ternary::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return lhs == reg || op0 == reg || op1 == reg;
}

void CFGNode::dfs_collect(std::unordered_set<std::shared_ptr<VirtReg>> &regs) {
    for (auto & i : instructions) {
        i->collect_register(regs);
    }
    for (auto &i : out_edges) {
        i->dfs_collect(regs);
    }
}

void CFGNode::setup_living(const std::unordered_set<std::shared_ptr<VirtReg>> &reg) {
    for (auto & i : out_edges) {
        i->setup_living(reg);
    }
    for (auto & i : reg) {
        for (auto & j: out_edges) {
            if (j->lives.count(i)) {
                lives[i] = instructions.size();
            }
        }
    }
    for (auto & i : reg) {
        for (size_t j = 0; j < instructions.size(); ++j) {
            if (instructions[j]->used_register(i)) {
                lives[i] = lives.count(i) == 0 ? j : std::max(lives[i], j);
            }
        }
    }
}

void CFGNode::generate_web(std::unordered_set<std::shared_ptr<VirtReg>> &liveness) {
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
    for (auto& i : liveness) {
        for (auto& j : liveness) {
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
    for (auto & i : lives) {
        if (i.second < instructions.size()) {
            liveness.erase(i.first);
        }
    }

    // handle child
    for (auto & i : out_edges) {
        i->generate_web(liveness);
    }

    // if no child, one more detection is needed
    if (out_edges.empty()) {
        for (auto& i : liveness) {
            for (auto& j : liveness) {
                if (i->id.number == j->id.number) continue;
                else {
                    i->neighbors.insert(j);
                }
            }
        }
    }

    // recover liveness
    for (auto & i : lives) {
        if (i.second < instructions.size()) {
            liveness.insert(i.first);
        }
    }

    for (auto & j : birth) {
        liveness.erase(j.first);
    }
}
