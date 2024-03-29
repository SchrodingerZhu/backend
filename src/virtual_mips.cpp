//
// Created by schrodinger on 1/15/21.
//

#include <vcfg/virtual_mips.h>
#include <cstring>
#include <gcolor/graph.h>

using namespace vmips;

char vmips::special_names[(size_t) SpecialReg::s8 + 1][8] = {
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
        "ra",
        "s8"
};

std::shared_ptr<VirtReg> vmips::get_special(SpecialReg reg) {
    static std::shared_ptr<VirtReg> specials[(size_t) SpecialReg::s8 + 1] = {nullptr};
    if (!specials[(size_t) reg]) {
        specials[(size_t) reg] = VirtReg::create_constant(special_names[(size_t) reg]);
    }
    return specials[(size_t) reg];
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

std::shared_ptr <VirtReg> vmips::div::def() const {
    return nullptr;
}

std::ostream &vmips::operator<<(std::ostream &out, const VirtReg &reg) {
    auto root = find_root(reg.parent.lock());
    if (root->allocated) {
        out << "$" << root->id.name;
    } else {
        out << "$undef<" << root->id.number << ">";
    }
    return out;
}

std::ostream &vmips::operator<<(std::ostream &out, const MemoryLocation &location) {
    if (location.status == MemoryLocation::Argument) {
        out << location.offset * 4 + location.function->stack_size << "(" << *location.base << ")";
    } else if (location.status == MemoryLocation::Assigned || location.status == MemoryLocation::Static) {
        out << location.offset << "(" << *location.base << ")";
    } else {
        out << "unallocated<" << location.identifier << ">";
    }
    return out;
}

static inline void color_to_reg(char *buf, size_t color) {
    if (color < SAVE_START) {
        sprintf(buf, "t%zu", color);
    } else {
        sprintf(buf, "s%zu", color - SAVE_START);
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


void Instruction::collect_register(unordered_set<std::shared_ptr<VirtReg>> &) const {

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


void Ternary::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
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
    return *lhs == *reg || *op0 == *reg || *op1 == *reg;
}

void Ternary::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*lhs == *reg) lhs = target;
    if (*op0 == *reg) op0 = target;
    if (*op1 == *reg) op1 = target;
}

void Ternary::output(std::ostream &out) const {
    out << name() << " " << *lhs << ", " << *op0 << ", " << *op1;
}

void CFGNode::dfs_collect(unordered_set<std::shared_ptr<VirtReg>> &regs) {
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

void CFGNode::setup_living(const unordered_set<std::shared_ptr<VirtReg>> &reg) {
    if (visited) return;
    visited = true;
    for (auto &i : reg) {
        for (size_t j = 0; j < instructions.size(); ++j) {
            if (instructions[j]->used_register(i)) {
                lives[i] = lives.count(i) == 0 ? j : std::max(lives[i], j);
            }
        }
    }
    for (auto &i : out_edges) {
        std::shared_ptr<CFGNode> n{i};
        n->setup_living(reg);
    }
    for (auto &i : reg) {
        if (i->spilled) continue;
        for (auto &j: out_edges) {
            std::shared_ptr<CFGNode> n{j};
            if (n->lives.count(i)) {
                lives[i] = instructions.size();
            }
        }
    }
    visited = false;
}

void CFGNode::generate_web(unordered_set<std::shared_ptr<VirtReg>> &liveness) {

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
    unordered_map<std::shared_ptr<VirtReg>, size_t> birth; // marks define in this scope

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

void CFGNode::spill(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<MemoryLocation> &location) {
    if (visited) return;
    visited = true;
    std::vector<std::shared_ptr<Instruction>> new_instr;
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i]->used_register(reg)) {
            auto tmp = VirtReg::create();
            tmp->spilled = true;
            auto load = Memory::create<lw>(tmp, location);
            auto save = Memory::create<sw>(tmp, location);
            new_instr.push_back(load);
            new_instr.push_back(instructions[i]);
            if (instructions[i]->def() && *instructions[i]->def() == *reg)
                new_instr.push_back(save);
            instructions[i]->replace(reg, tmp);
        } else {
            if (dynamic_cast<phi *>(instructions[i].get())) {
                continue;
            }
            new_instr.push_back(instructions[i]);
        }
    }
    instructions = new_instr;
    for (auto &i : out_edges) {
        std::shared_ptr<CFGNode> n{i};
        n->spill(reg, location);
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

size_t CFGNode::color(const std::shared_ptr<VirtReg> &sp) {
    auto success = false;
    unordered_set<size_t> res;
    do {
        unordered_set<std::shared_ptr<VirtReg>> regs;
        dfs_collect(regs);
        if (regs.empty()) {
            return 0;
        }
        setup_living(regs);
        unordered_set<std::shared_ptr<VirtReg>> liveness;
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
        unordered_map<size_t, size_t> idx_map;
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
            auto location = function->new_memory(4);
            spill(failure, location);
        } else {
            success = true;
            for (auto i = 0; i < vec.size(); ++i) {
                vec[i]->allocated = true;
                vec[i]->id.number = 0;
                color_to_reg(vec[i]->id.name, colors.first[i]);
            }
            for (auto i : colors.first) {
                if (i >= SAVE_START) res.insert(i);
            }
        }
    } while (!success);
    return res.size();
}

Memory::Memory(std::shared_ptr<VirtReg> target, std::shared_ptr<MemoryLocation> location)
        : target(std::move(target)), location(std::move(location)) {

}

void Memory::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!target->allocated) set.insert(target);
    if (location->status == MemoryLocation::Static && !location->base->allocated) set.insert(location->base);
}

std::shared_ptr<VirtReg> Memory::def() const {
    if (name()[0] == 'l' || name()[1] == 'l') {
        return target;
    }
    return nullptr;
}

bool Memory::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return *target == *reg || *location->base == *reg;
}

void Memory::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*this->target == *reg) { this->target = target; }
    if (*this->location->base == *reg) { location->base = target; }
}

void Memory::output(std::ostream &out) const {
    out << name() << " " << *target << ", " << *location;
}

BinaryImm::BinaryImm(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs, ssize_t imm)
        : lhs(std::move(lhs)), rhs(std::move(rhs)), imm(imm) {
}

void BinaryImm::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!lhs->allocated) set.insert(lhs);
    if (!rhs->allocated) set.insert(rhs);
}

std::shared_ptr<VirtReg> BinaryImm::def() const {
    return lhs;
}

bool BinaryImm::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return *lhs == *reg || *rhs == *reg;
}

void BinaryImm::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*lhs == *reg) lhs = target;
    if (*rhs == *reg) rhs = target;
}

void BinaryImm::output(std::ostream &out) const {
    out << name() << " " << *lhs << ", " << *rhs << ", " << imm;
}

Binary::Binary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs)
        : lhs(std::move(lhs)), rhs(std::move(rhs)) {

}

Unary::Unary(std::shared_ptr<VirtReg> t)
        : target(std::move(t)) {

}

void Unary::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!target->allocated) set.insert(target);
}

std::shared_ptr<VirtReg> Unary::def() const {
    return target;
}

bool Unary::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return *target == *reg;
}

void Unary::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*this->target == *reg) this->target = target;
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
    return *lhs == *reg || *rhs == *reg;
}

void Binary::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*lhs == *reg) lhs = target;
    if (*rhs == *reg) rhs = target;
}

void Binary::output(std::ostream &out) const {
    out << name() << " " << *lhs << ", " << *rhs;
}

void Binary::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!lhs->allocated) set.insert(lhs);
    if (!rhs->allocated) set.insert(rhs);
}

void CFGNode::output(std::ostream &out) {
    if (visited) return;
    visited = true;
    out << label << ":" << std::endl;
    for (auto &i : instructions) {
        if (!dynamic_cast<callfunc *>(i.get())) out << "\t";
        i->output(out);
        if (!dynamic_cast<callfunc *>(i.get())) out << "\n";
    }
    visited = false;
}

CFGNode::CFGNode(Function *function, std::string name) : label(std::move(name)), function(function) {

}

void CFGNode::scan_overlap(unordered_set<std::shared_ptr<VirtReg>> &liveness) {
    if (visited) return;

    visited = true;
    unordered_map<std::shared_ptr<VirtReg>, size_t> birth; // marks define in this scope

    // find new birth
    for (size_t i = 0; i < instructions.size(); ++i) {
        auto def = instructions[i]->def();
        if (def) {
            liveness.insert(def);
            birth[def] = i;
        }
    }


    for (size_t j = 0; j < instructions.size(); ++j) {
        auto call = dynamic_cast<callfunc *>(instructions[j].get());
        if (!call) continue;
        call->scanned = true;
        for (auto &i : liveness) {
            auto k = find_root(i);
            if (k->id.name[0] != 't') continue;
            auto interleaved = (lives.count(i) && lives[i] < j) || (birth.count(i) && birth[i] >= j);
            if (!interleaved) {
                call->overlap_temp.insert(k);
                if (!k->overlap_location) {
                    k->overlap_location = function->new_memory(4);
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
        n->scan_overlap(liveness);
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

void UnaryImm::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (!target->allocated) set.insert(target);
}

std::shared_ptr<VirtReg> UnaryImm::def() const {
    return target;
}

bool UnaryImm::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return *target == *reg;
}

void UnaryImm::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*this->target == *reg) this->target = target;
}

void UnaryImm::output(std::ostream &out) const {
    out << name() << " " << *target << ", " << imm;
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
    ss << ".L"<< name << "_" << count++;
    return ss.str();
}

std::shared_ptr<CFGNode> Function::entry() {
    auto ret = std::make_shared<CFGNode>(this, next_name());
    blocks.push_back(ret);
    switch_to(ret);
    return ret;
}

void Function::output(std::ostream &out) const {
    out << "# data sections of function " << name << std::endl;
    for (auto &i : data_blocks) {
        i->output(out);
    }
    out << "# gcc headers for " << name << std::endl;
    out << "\t.text" << std::endl;
    out << "\t.globl " << name << std::endl;
    out << "\t.ent " << name << std::endl;
    out << name << ":" << std::endl;
    out << "\t# prologue area" << std::endl;
    if (allocated) {
        out << "\t.set noreorder" << std::endl;
        out << "\t.frame $s8, " << stack_size << ", $ra" << std::endl;
        out << "\t.cpload $t9" << std::endl;
        out << "\t.set reorder " << std::endl;
        out << "\taddi $sp, $sp, -" << stack_size << std::endl;
        out << "\t.cprestore " << pic_location.offset << std::endl;
        if (has_sub) {
            out << "\tsw $ra, " << ra_location << std::endl;
        }
        if (save_regs > 0) {
            auto base = sub_argc * 4 + EXTRA_STACK;
            for (size_t i = 0; i < save_regs; ++i) {
                out << "\tsw " << "$s" << i << ", "
                    << base + i * 4 << "($sp)" << std::endl;
            }
        }
        out << "\tsw $s8, " << s8_location << std::endl;
        out << "\tmove $s8, $sp" << std::endl;
    }
    for (auto &i : blocks) {
        i->output(out);
    }
    out << ".L" << name << "_epilogue:" << std::endl;
    out << "\t# epilogue area" << std::endl;
    if (allocated) {
        out << "\tmove $sp, $s8" << std::endl;
        out << "\tlw $s8, " << s8_location << std::endl;
        if (save_regs > 0) {
            auto base = sub_argc * 4 + EXTRA_STACK;
            for (size_t i = 0; i < save_regs; ++i) {
                out << "\tlw " << "$s" << i << ", "
                    << base + i * 4 << "($sp)" << std::endl;
            }
        }
        if (has_sub) {
            out << "\tlw $ra, " << ra_location << std::endl;
        }
        out << "\taddi $sp, $sp, " << stack_size << std::endl;
    }
    out << "\tjr $ra" << std::endl;
    out << "\t.end " << name << std::endl;
}

size_t Function::color() {
    auto s8 = get_special(SpecialReg::s8);
    return save_regs = blocks[0]->color(s8);
}

Function::Function(std::string name, size_t argc) : name(std::move(name)), argc(argc) {
    ra_location.status = MemoryLocation::Undetermined;
    ra_location.identifier = memory_count++;
    ra_location.size = 4;
    ra_location.base = get_special(SpecialReg::sp);

    pic_location.status = MemoryLocation::Undetermined;
    pic_location.identifier = memory_count++;
    pic_location.size = 4;
    pic_location.base = get_special(SpecialReg::sp);

    s8_location.status = MemoryLocation::Undetermined;
    s8_location.identifier = memory_count++;
    s8_location.size = 4;
    s8_location.base = get_special(SpecialReg::sp);
}

std::shared_ptr<MemoryLocation> Function::new_static_mem(size_t size, std::shared_ptr<VirtReg> reg, size_t) {
    auto res = std::make_shared<MemoryLocation>();
    res->size = size;
    res->identifier = memory_count++;
    res->status = MemoryLocation::Static;
    res->offset = -1;
    res->base = std::move(reg);
    return res;
}

std::shared_ptr<MemoryLocation> Function::new_memory(size_t size) {
    auto res = std::make_shared<MemoryLocation>();
    res->size = size;
    res->identifier = memory_count++;
    res->status = MemoryLocation::Undetermined;
    res->offset = -1;
    res->base = get_special(SpecialReg::s8);
    mem_blocks.push_back(res);
    return res;
}

std::shared_ptr<CFGNode> Function::join(const std::shared_ptr<CFGNode> &x, const std::shared_ptr<CFGNode> &y) {
    auto node = std::make_shared<CFGNode>(this, next_name());
    if (blocks.back() != x) x->branch_existing<j>(node);
    if (blocks.back() != y) y->branch_existing<j>(node);
    blocks.push_back(node);
    switch_to(node);
    return node;
}

void Function::add_phi(const std::shared_ptr<VirtReg> &x, const std::shared_ptr<VirtReg> &y) {
    cursor->add_phi(x, y);
}

void Function::switch_to(const std::shared_ptr<CFGNode> &target) {
    cursor = target;
}

void Function::scan_overlap() {
    unordered_set<std::shared_ptr<VirtReg>> liveness;
    blocks[0]->scan_overlap(liveness);
}

void Function::handle_alloca() {
    stack_size = 4 * sub_argc + EXTRA_STACK + 4 * save_regs; // sub args | PIC section | saved registers

    // ra and pic
    if (has_sub) {
        ra_location.status = MemoryLocation::Assigned;
        ra_location.offset = stack_size;
        stack_size += 4;
    }

    pic_location.status = MemoryLocation::Assigned;
    pic_location.offset = stack_size;
    stack_size += pic_location.size;

    s8_location.status = MemoryLocation::Assigned;
    s8_location.offset = stack_size;
    stack_size += s8_location.size;

    stack_size += (-stack_size & MASK);

    for (auto &i : mem_blocks) {
        if (i->status == MemoryLocation::Undetermined) {
            i->status = MemoryLocation::Assigned;
            i->offset = stack_size;
            stack_size += i->size;
        }
    }
    stack_size += (-stack_size & MASK); // align
    allocated = true;
}

void Function::add_ret() {
    static auto ending = std::make_shared<text>(std::string{"j "} + ".L" + this->name + "_epilogue");
    cursor->instructions.push_back(ending);
}

void Function::assign_special(SpecialReg special, std::shared_ptr<VirtReg> reg) {
    cursor->instructions.push_back(std::make_shared<move>(get_special(special), std::move(reg)));
}

void Function::assign_special(SpecialReg special, ssize_t value) {
    cursor->instructions.push_back(std::make_shared<addi>(get_special(special), get_special(SpecialReg::zero), value));
}

std::shared_ptr<CFGNode> Function::new_section() {
    auto node = std::make_shared<CFGNode>(this, next_name());
    if (cursor != blocks.back()) {
        cursor->branch_existing<j>(node);
    } else {
        cursor->out_edges.push_back(node);
    }
    this->blocks.push_back(node);
    switch_to(node);
    return node;
}

std::shared_ptr<MemoryLocation> Function::argument(size_t offset) {
    auto loc = std::make_shared<MemoryLocation>();
    loc->status = MemoryLocation::Argument;
    loc->offset = offset;
    loc->function = this;
    loc->base = get_special(SpecialReg::s8);
    loc->size = 4;
    return loc;
}


phi::phi(std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1) : op0(std::move(op0)), op1(std::move(op1)) {

}

void phi::output(std::ostream &out) const {
    out << "# phi node";
}

void phi::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*op0 == *reg) op0 = target;
    if (*op1 == *reg) op1 = target;
}

void callfunc::output(std::ostream &out) const {
    auto f = function.lock();
    if (scanned) {

        // save all overlaps
        out << "\t# start calling " << f->name << std::endl;
        for (auto &i : overlap_temp) {
            if (i->overlap_location != nullptr) {
                out << "\tsw " << *i << ", " << *i->overlap_location << std::endl;
            } else {
                out << "\tsw " << *i << ", undef # error: overlap location is not assigned" << std::endl;
            }
        }

        // save all arguments to stack
        for (size_t i = 0; i < call_with.size(); ++i) {
            out << "\tsw " << *call_with[i] << ", " << i * 4 << "($s8)" << std::endl;
        }

        // load first several arguments into register
        for (size_t i = 0; i < std::min(call_with.size(), (size_t) 4); ++i) {
            out << "\tlw $a" << i << ", " << i * 4 << "($s8)" << std::endl;
        }

        // call function
        out << "\tjal " << function.lock()->name << std::endl;

        // recover overlaps
        for (auto &i : overlap_temp) {
            if (i->overlap_location != nullptr) {
                out << "\tlw " << *i << ", " << *i->overlap_location << std::endl;
            } else {
                out << "\tlw " << *i << ", undef # error: overlap location is not assigned" << std::endl;
            }
        }
        // load v0
        if (ret) out << "\tmove " << *ret << ", $v0" << std::endl;
        out << "\t# end calling " << f->name << std::endl;
    } else {
        if (def()) {
            out << "\t" << *def() << " = call " << function.lock()->name << "(";
        } else {
            out << "\tcall " << f->name << "(";
        }
        for (size_t i = 0; i < call_with.size(); ++i) {
            out << *call_with[i];
            if (i != call_with.size() - 1) {
                out << ", ";
            }
        }
        out << ")" << std::endl;
    }

}

void callfunc::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    if (*ret == *reg) ret = target;
    for (auto &i : call_with) {
        if (*i == *reg) {
            i = target;
        }
    }
}

callfunc::callfunc(std::shared_ptr<VirtReg> ret, Function *current, std::weak_ptr<Function> function,
                   std::vector<std::shared_ptr<VirtReg>> call_with)
        : ret(std::move(ret)), function(std::move(function)), call_with(std::move(call_with)), current(current) {}

void callfunc::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
    if (ret && !ret->allocated) set.insert(ret);
    for (auto &i : call_with) {
        if (!i->allocated) {
            set.insert(i);
        }
    }
}

std::shared_ptr<VirtReg> callfunc::def() const {
    return ret;
}

bool callfunc::used_register(const std::shared_ptr<VirtReg> &reg) const {
    if (ret && *ret == *reg) return true;
    return std::any_of(call_with.begin(), call_with.end(),
                       [&](const std::shared_ptr<VirtReg> &t) { return *t == *reg; });
}

std::shared_ptr<VirtReg> jr::def() const {
    return nullptr;
}

jr::jr(std::shared_ptr<VirtReg> reg) : Unary(std::move(reg)) {}

text::text(std::string context) : Instruction(), context(std::move(context)) {}

const char *text::name() const {
    return context.data();
}

void text::output(std::ostream &out) const {
    out << name();
}

Data::Data(std::string name, bool read_only) : name(std::move(name)), read_only(read_only) {

}

IMPLEMENT_DATA(byte, 0, char_wrap);

IMPLEMENT_DATA(ascii, 0, str_wrap);

IMPLEMENT_DATA(asciiz, 0, str_wrap);

IMPLEMENT_DATA(word, 2, normal);

IMPLEMENT_DATA(hword, 1, normal);

IMPLEMENT_DATA(space, 0, normal);

Module::Module(std::string name) : name(std::move(name)) {}

std::shared_ptr<Function> Module::create_function(std::string fname, size_t argc) {
    auto function = std::make_shared<Function>(std::move(fname), argc);
    function->entry();
    functions.push_back(function);
    return function;
}

void Module::output(std::ostream &out) const {
    out << "# Module : " << name << std::endl;
    for (auto &i : externs) {
        out << "\t.extern " << i->name << std::endl;
    }
    for (auto &i : global_data_section) {
        i->output(out);
    }
    for (auto &i : functions) {
        i->output(out);
    }
}

std::shared_ptr<Function> Module::create_extern(std::string fname, size_t argc) {
    externs.push_back(std::make_shared<Function>(std::move(fname), argc));
    return externs.back();
}


la::la(std::shared_ptr<VirtReg> reg, std::shared_ptr<Data> data) : Unary(std::move(reg)), data(std::move(data)) {}

const char *la::name() const {
    return "la";
}

void la::output(std::ostream &out) const {
    out << name() << " " << *this->target << ", " << data->name;
}

address::address(std::shared_ptr<VirtReg> reg, std::shared_ptr<MemoryLocation> data) : Unary(std::move(reg)),
                                                                                       data(std::move(data)) {

}

void address::output(std::ostream &out) const {
    if (data->status != MemoryLocation::Undetermined) {
        out << "li " << *this->target << ", " << data->offset;
    } else {
        out << "li " << *this->target << ", " << "<stack_offset>";
    }
}

ArrayAccess::ArrayAccess(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> offset,
                         std::shared_ptr<MemoryLocation> location) : Memory(std::move(target), std::move(location)),
                                                                     offset(std::move(offset)) {
}

void ArrayAccess::collect_register(unordered_set<std::shared_ptr<VirtReg>> &set) const {
    Memory::collect_register(set);
    if (offset && !offset->allocated) set.insert(offset);
}

bool ArrayAccess::used_register(const std::shared_ptr<VirtReg> &reg) const {
    return Memory::used_register(reg) || *reg == *offset;
}

void ArrayAccess::replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) {
    Memory::replace(reg, target);
    if (*reg == *offset) offset = target;
}

void ArrayAccess::output(std::ostream &out) const {
    if (location->status == MemoryLocation::Undetermined) {
        out << name() << " " << *target << ", " << *location << ", shifted by " << *offset;
    } else {
        out << "# array access: " << name() << std::endl;
        out << "\tsll $at, " << *offset << ", 2"  << std::endl;
        out << "\taddu $at, " << *location->base << ", $at" << std::endl;
        out << "\t" << name() << " " << *target << ", " << location->offset << "($at)";
    }

}

array_load::array_load(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> offset,
                       std::shared_ptr<MemoryLocation> location) : ArrayAccess(std::move(target), std::move(offset),
                                                                               std::move(location)) {

}

const char *array_load::name() const {
    return "lw";
}

array_store::array_store(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> offset,
                         std::shared_ptr<MemoryLocation> location) : ArrayAccess(std::move(target), std::move(offset),
                                                                                 std::move(location)) {

}

const char *array_store::name() const {
    return "sw";
}
