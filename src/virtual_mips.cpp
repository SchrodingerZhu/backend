//
// Created by schrodinger on 1/15/21.
//

#include <vcfg/virtual_mips.h>
#include <cstring>
#include <gcolor/graph.h>

using namespace vmips;

char vmips::special_names[(size_t) SpecialReg::ra + 1][8] = {
        "zero",
        "at",
        "v0",
        "v1",
        "a0",
        "a1",
        "a2",
        "a3",
        "k0",
        "k1",
        "gp",
        "sp",
        "fp",
        "ra"
};

std::shared_ptr<VirtReg> vmips::get_special(SpecialReg reg) {
    static std::shared_ptr<VirtReg> specials[(size_t)SpecialReg::ra + 1] = {nullptr};
    if (!specials[(size_t)reg]) {
        specials[(size_t)reg] = VirtReg::create_constant(special_names[(size_t)reg]);
    }
    return specials[(size_t)reg];
}

void vmips::unite(std::shared_ptr<VirtReg> x, std::shared_ptr<VirtReg> y) {
    x = find_root(x);
    y = find_root(y);
    if (x == y) return;
    if (x->union_size < y->union_size) {
        std::swap(x, y);
    }
    y->parent = x;
    x->union_size += y->union_size;
}

std::shared_ptr<VirtReg> vmips::find_root(std::shared_ptr<VirtReg> x) {
    std::shared_ptr<VirtReg> root = x;
    while (root->parent.lock() != root) {
        root = root->parent.lock();
    }
    while (x->parent.lock() != root) {
        std::shared_ptr<VirtReg> parent = x->parent.lock();
        x->parent = root;
        x = parent;
    }
    return root;
}

std::ostream &vmips::operator<<(std::ostream &out, const VirtReg &reg) {
    auto root = find_root(reg.parent.lock());
    if (root->allocated) {
        out << "$" << root->id.name;
    } else {
        out << "$undef(" << root->id.number << ")";
    }
    return out;
}

static inline void color_to_reg(char *buf, size_t color) {
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
    reg->parent = reg;
    return reg;
}

std::shared_ptr<VirtReg> VirtReg::create_constant(const char *name) {
    auto reg = std::make_shared<VirtReg>();
    reg->allocated = true;
    std::strcpy(reg->id.name, name);
    reg->parent = reg;
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

void Instruction::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
}

std::shared_ptr<CFGNode> Instruction::branch() {
    return nullptr;
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

void Ternary::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (lhs == reg) lhs = target;
    if (op0 == reg) op0 = target;
    if (op1 == reg) op1 = target;
}

void Ternary::output(std::ostream &out) const {
    out << name() << " " << *lhs << ", " << *op0 << ", " << *op1;
}

void CFGNode::dfs_collect(std::unordered_set<std::shared_ptr<VirtReg>> &regs) {
    if (visited) return;
    visited = true;
    for (auto &i : instructions) {
        auto trial = dynamic_cast<phi *>(i.get());
        if (trial) {
            unite(trial->op0, trial->op1);
        }
        i->collect_register(regs);
    }
    for (auto &i : out_edges) {
        std::shared_ptr<CFGNode> n{i};
        n->dfs_collect(regs);
    }
    visited = false;
}

void CFGNode::setup_living(const std::unordered_set<std::shared_ptr<VirtReg>> &reg) {
    if (visited) return;
    visited = true;
    for (auto &i : out_edges) {
        std::shared_ptr<CFGNode> n{i};
        n->setup_living(reg);
    }
    for (auto &i : reg) {
        for (auto &j: out_edges) {
            std::shared_ptr<CFGNode> n{j};
            if (n->lives.count(i)) {
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

    if (visited) {
        // detect once more on loop
        if (out_edges.empty()) {
            for (auto &i : liveness) {
                for (auto &j : liveness) {
                    if (i->id.number == j->id.number) continue;
                    else {
                        find_root(i)->neighbors.insert(j);
                    }
                }
            }
        }
        return;
    }
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
                    find_root(i)->neighbors.insert(j);
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
        std::shared_ptr<CFGNode> n{i};
        n->generate_web(liveness);
    }

    // if no child, one more detection is needed
    if (out_edges.empty()) {
        for (auto &i : liveness) {
            for (auto &j : liveness) {
                if (i->id.number == j->id.number) continue;
                else {
                    find_root(i)->neighbors.insert(j);
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
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i]->used_register(reg)) {
            auto tmp = last ? last : VirtReg::create();
            //if (last) new_instr.pop_back();// extra save
            tmp->spilled = true;
            auto load = Memory::create<lw>(tmp, sp, stack_location);
            auto save = Memory::create<sw>(tmp, sp, stack_location);
            if (instructions[i]->def() != reg && !last) new_instr.push_back(load);
            new_instr.push_back(instructions[i]);
            if (instructions[i]->def() == reg)
                new_instr.push_back(save);
            instructions[i]->replace(reg, tmp);
            last = tmp;
        } else {
            last = nullptr;
            new_instr.push_back(instructions[i]);
        }
    }
    instructions = new_instr;
    for (auto &i : out_edges) {
        std::shared_ptr<CFGNode> n{i};
        n->spill(reg, sp, stack_location);
    }
    visited = false;
}

void CFGNode::dfs_reset() {
    if (visited) return;
    visited = true;
    lives.clear();
    for (auto &i : out_edges) {
        std::shared_ptr<CFGNode> n{i};
        n->dfs_reset();
    }
    visited = false;
}

#include <iostream>
#include <utility>

void CFGNode::color(const std::shared_ptr<VirtReg> &sp, ssize_t &current_stack_shift) {
    auto success = false;
    do {
        std::unordered_set<std::shared_ptr<VirtReg>> regs;
        dfs_collect(regs);
        setup_living(regs);
        std::unordered_set<std::shared_ptr<VirtReg>> liveness;
        generate_web(liveness);
        std::vector<std::shared_ptr<VirtReg>> vec;
        for (auto &i : regs) {
            if (find_root(i) == i) {
                vec.push_back(i);
            }
        }
        std::sort(vec.begin(), vec.end(),
                  [](const std::shared_ptr<VirtReg> &a, const std::shared_ptr<VirtReg> &b) {
                      return a->id.number < b->id.number;
                  });
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
        auto colors = g.color(REG_NUM);
        std::shared_ptr<VirtReg> failure = nullptr;
        if (colors.first.empty()) {
            for (auto &i: vec) {
                i->neighbors.clear();
                i->union_size = 1;
                i->parent = i;
            }
            dfs_reset();
            for (auto &i : colors.second) {
                if (!vec[i]->spilled) {
                    failure = vec[i];
                    break;
                }
            }
            spill(failure, sp, current_stack_shift);
            current_stack_shift += 4;
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
    if (this->target == reg) { this->target = target; }
    if (this->addr == reg) { this->addr = target; }
}

void Memory::output(std::ostream &out) const {
    out << name() << " " << *target << ", " << offset << "(" << *addr << ")";
}

BinaryImm::BinaryImm(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs, ssize_t imm)
        : lhs(std::move(lhs)), rhs(std::move(rhs)), imm(imm) {
}

void BinaryImm::collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!lhs->allocated) set.insert(lhs);
    if (!rhs->allocated) set.insert(rhs);
}

std::shared_ptr<VirtReg> BinaryImm::def() const {
    return lhs;
}

bool BinaryImm::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return lhs == reg || rhs == reg;
}

void BinaryImm::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (lhs == reg) lhs = target;
    if (rhs == reg) rhs = target;
}

void BinaryImm::output(std::ostream &out) const {
    out << name() << " " << *lhs << ", " << *rhs << ", " << std::hex << std::showbase << imm << std::noshowbase;
}

Binary::Binary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs)
        : lhs(std::move(lhs)), rhs(std::move(rhs)) {

}

Unary::Unary(std::shared_ptr<VirtReg> t)
        : target(std::move(t)) {

}

void Unary::collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!target->allocated) set.insert(target);
}

std::shared_ptr<VirtReg> Unary::def() const {
    return target;
}

bool Unary::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return target == reg;
}

void Unary::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (this->target == reg) this->target = target;
}

void Unary::output(std::ostream &out) const {
    out << name() << " " << *target;
}

UnaryImm::UnaryImm(std::shared_ptr<VirtReg> t, ssize_t imm)
        : target(std::move(t)), imm(imm) {

}

std::shared_ptr<VirtReg> Binary::def() const {
    return lhs;
}

bool Binary::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return lhs == reg || rhs == reg;
}

void Binary::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (lhs == reg) lhs = target;
    if (rhs == reg) rhs = target;
}

void Binary::output(std::ostream &out) const {
    out << name() << " " << *lhs << ", " << *rhs;
}

void Binary::collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!lhs->allocated) set.insert(lhs);
    if (!rhs->allocated) set.insert(rhs);
}

void CFGNode::output(std::ostream &out) {
    if (visited) return;
    visited = true;
    out << label << ":" << std::endl;
    for (auto &i : instructions) {
        out << "\t";
        i->output(out);
        out << "\n";
    }
    visited = false;
}

CFGNode::CFGNode(std::string name) : label(std::move(name)) {

}

void UnaryImm::collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!target->allocated) set.insert(target);
}

std::shared_ptr<VirtReg> UnaryImm::def() const {
    return target;
}

bool UnaryImm::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return target == reg;
}

void UnaryImm::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (this->target == reg) this->target = target;
}

void UnaryImm::output(std::ostream &out) const {
    out << name() << " " << target << ", " << std::hex << std::showbase << imm << std::noshowbase;
}

jal::jal(std::string name) : function_name(std::move(name)) {

}

const char *jal::name() const {
    return "jal";
}

void jal::output(std::ostream &out) const {
    out << name() << " " << function_name;
}

Unconditional::Unconditional(std::weak_ptr<CFGNode> block) : block(std::move(block)) {}

void Unconditional::output(std::ostream &out) const {
    out << name() << " " << block.lock()->label;
}

std::shared_ptr<CFGNode> Unconditional::branch() {
    return block.lock();
}

std::shared_ptr<VirtReg> Unconditional::def() const {
    return nullptr;
}

ZeroBranch::ZeroBranch(std::weak_ptr<CFGNode> block, std::shared_ptr<VirtReg> check)
        : block(std::move(block)), Unary(std::move(check)) {

}

void ZeroBranch::output(std::ostream &out) const {
    out << name() << " " << *this->target << ", " << block.lock()->label;
}

std::shared_ptr<CFGNode> ZeroBranch::branch() {
    return block.lock();
}

std::shared_ptr<VirtReg> ZeroBranch::def() const {
    return nullptr;
}

CmpBranch::CmpBranch(std::weak_ptr<CFGNode> block,
                     std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1)
        : Binary(std::move(op0), std::move(op1)), block(std::move(block)) {

}

void CmpBranch::output(std::ostream &out) const {
    out << name() << " " << *this->lhs << ", " << *this->rhs << ", " << block.lock()->label;
}

std::shared_ptr<CFGNode> CmpBranch::branch() {
    return block.lock();
}

std::shared_ptr<VirtReg> CmpBranch::def() const {
    return nullptr;
}

std::string Function::next_name() {
    std::stringstream ss;
    ss << name << "_$$branch$$_" << count++;
    return ss.str();
}

std::shared_ptr<CFGNode> Function::entry() {
    auto ret = std::make_shared<CFGNode>(name);
    blocks.push_back(ret);
    switch_to(ret);
    return ret;
}

void Function::output(std::ostream &out) const {
    for (auto &i : blocks) {
        i->output(out);
    }
}

void Function::color() {
    auto sp = get_special(SpecialReg::sp);
    blocks[0]->color(sp, stack_size);
}

Function::Function(std::string name) : name(std::move(name)) {}

phi::phi(std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1) : op0(std::move(op0)), op1(std::move(op1)) {

}

void phi::output(std::ostream &out) const {
    out << "# phi node";
}

void phi::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (op0 == reg) op0 = target;
    if (op1 == reg) op1 = target;
}
