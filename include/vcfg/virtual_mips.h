//
// Created by schrodinger on 1/15/21.
//

#ifndef BACKEND_VIRTUAL_MIPS_H
#define BACKEND_VIRTUAL_MIPS_H

#include <vector>
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <ostream>
#include <algorithm>
#include <sstream>

namespace vmips {
    struct CFGNode;
    struct Function;

    // must be SSA
    class VirtReg {
        static std::atomic_size_t GLOBAL;
    public:
        std::weak_ptr<VirtReg> parent;
        size_t union_size = 1;
        std::unordered_set<std::shared_ptr<VirtReg>> neighbors;
        union {
            size_t number = 0;
            char name[sizeof(size_t)];
        } id;
        bool allocated;
        bool spilled;

        VirtReg();

        static std::shared_ptr<VirtReg> create();

        static std::shared_ptr<VirtReg> create_constant(const char *name);

        friend std::ostream &operator<<(std::ostream &, const VirtReg &);
    };

    static inline std::shared_ptr<VirtReg> find_root(std::shared_ptr<VirtReg> x) {
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

    static inline void unite(std::shared_ptr<VirtReg> x, std::shared_ptr<VirtReg> y) {
        x = find_root(x);
        y = find_root(y);
        if (x == y) return;
        if (x->union_size < y->union_size) {
            std::swap(x, y);
        }
        y->parent = x;
        x->union_size += y->union_size;
    }

    class Instruction {
    public:
        virtual void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &) const;

        virtual const char *name() const;

        virtual std::shared_ptr<VirtReg> def() const;

        virtual bool used_register(const std::shared_ptr<VirtReg> &reg) const;

        virtual void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target);

        virtual void output(std::ostream &) const = 0;

        virtual std::shared_ptr<CFGNode> branch();
    };

    class phi : public Instruction {
    public:
        std::shared_ptr<VirtReg> op0, op1;
        phi(std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);
        virtual void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target);
        void output(std::ostream &out) const override;
    };

    class Ternary : public Instruction {
    public:
        std::shared_ptr<VirtReg> lhs;
        std::shared_ptr<VirtReg> op0, op1;

        Ternary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        template<class T>
        static std::shared_ptr<Instruction>
        create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;
    };

    class BinaryImm : public Instruction {
    public:
        std::shared_ptr<VirtReg> lhs;
        std::shared_ptr<VirtReg> rhs;
        ssize_t imm;

        BinaryImm(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs, ssize_t imm);

        template<class T>
        static std::shared_ptr<Instruction>
        create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs, ssize_t imm);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;
    };

    class Binary : public Instruction {
    public:
        std::shared_ptr<VirtReg> lhs;
        std::shared_ptr<VirtReg> rhs;

        Binary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;
    };

    class Unary : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;

        Unary(std::shared_ptr<VirtReg> t);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> t);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;

    };

    template<class T>
    std::shared_ptr<Instruction> Unary::create(std::shared_ptr<VirtReg> t) {
        return std::make_shared<T>(t);
    }


    class UnaryImm : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;
        ssize_t imm;

        UnaryImm(std::shared_ptr<VirtReg> t, ssize_t imm);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> t, ssize_t imm);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;
    };

    class Unconditional : public Instruction {
    public:
        std::weak_ptr<CFGNode> block;
        explicit Unconditional(std::weak_ptr<CFGNode> block);
        void output(std::ostream &) const override;
        std::shared_ptr<CFGNode> branch() override;
        std::shared_ptr<VirtReg> def() const override;
    };

    class ZeroBranch : public Unary {
    public:
        std::weak_ptr<CFGNode> block;
        ZeroBranch(std::weak_ptr<CFGNode> block, std::shared_ptr<VirtReg> check);
        void output(std::ostream &) const override;
        std::shared_ptr<CFGNode> branch() override;
        std::shared_ptr<VirtReg> def() const override;
    };

    class CmpBranch : public Binary {
    public:
        std::weak_ptr<CFGNode> block;
        CmpBranch(std::weak_ptr<CFGNode> block,
                  std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);
        void output(std::ostream &) const override;
        std::shared_ptr<CFGNode> branch() override;
        std::shared_ptr<VirtReg> def() const override;
    };

    class jal : public Instruction {
    public:
        std::string function_name;
        explicit jal(std::string name);
        const char *name() const override;
        void output(std::ostream &out) const override;
    };

    class Memory : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;
        std::shared_ptr<VirtReg> addr;
        ssize_t offset;

        Memory(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> addr, ssize_t offset);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        template<class T>
        static std::shared_ptr<Instruction>
        create(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> addr, size_t offset);

        void output(std::ostream &) const override;
    };

    //self defined marker
    class Syscall : public Instruction {
    };

#define BASE_INIT(S, B) \
template <typename ...Args> \
explicit S(Args&& ...args) : B(std::forward<Args>(args)...) {}

#define DECLARE(S, B)                           \
class S : public B {                            \
public:                                         \
    BASE_INIT(S, B)                             \
    const char * name() const {                 \
        return #S;                              \
    }                                           \
};


    DECLARE(add, Ternary);

    DECLARE(addiu, BinaryImm);

    DECLARE(addi, BinaryImm);

    DECLARE(addu, Ternary);

    DECLARE(clo, Binary);

    DECLARE(clz, Binary);

    //TODO: fix LA
    class la : public Instruction {
    };

    DECLARE(li, UnaryImm);

    DECLARE(lui, UnaryImm);

    DECLARE(move, Binary);

    DECLARE(negu, Binary);

    DECLARE(seb, Binary);

    DECLARE(seh, Binary);

    DECLARE(sub, Ternary);

    DECLARE(subu, Ternary);

    DECLARE(lw, Memory);

    DECLARE(sw, Memory);


    DECLARE(b, Unconditional);

    DECLARE(beq, CmpBranch);

    // upward links are broken down

    struct CFGNode {
        std::string label;
        bool visited = false;
        std::vector<std::shared_ptr<Instruction>> instructions {};
        std::vector<std::weak_ptr<CFGNode>> out_edges {}; // at most two
        std::unordered_map<std::shared_ptr<VirtReg>, size_t> lives {}; // instructions.size  means live through

        explicit CFGNode(std::string name);
        void dfs_collect(std::unordered_set<std::shared_ptr<VirtReg>> &regs);

        void dfs_reset();

        void setup_living(const std::unordered_set<std::shared_ptr<VirtReg>> &reg);

        void generate_web(std::unordered_set<std::shared_ptr<VirtReg>> &liveness);

        void spill(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &sp, ssize_t stack_location);

        void color(const std::shared_ptr<VirtReg> &sp, ssize_t &current_stack_shift);

        void output(std::ostream &out);

        template<typename Instr, typename ...Args>
        std::shared_ptr<VirtReg> append(Args&& ...args) {
            auto ret = VirtReg::create();
            auto instr = std::make_shared<Instr>(ret, std::forward<Args>(args)...);
            instructions.push_back(instr);
            return ret;
        }

        void add_phi(std::shared_ptr<VirtReg> x, std::shared_ptr<VirtReg> y) {
            instructions.push_back(std::make_shared<phi>(std::move(x), std::move(y)));
        }

        template<typename Instr, typename ...Args>
        void branch_existing(const std::shared_ptr<CFGNode>& node, Args&&... args) {
            out_edges.push_back(node);
            auto instr = std::make_shared<Instr>(node, std::forward<Args>(args)...);
            instructions.push_back(instr);
        }

        template<typename Instr, typename ...Args>
        std::shared_ptr<CFGNode> branch_single(std::string name, Args&&... args) {
            auto node = std::make_shared<CFGNode>(name);
            auto instr = std::make_shared<Instr>(node, std::forward<Args>(args)...);
            instructions.push_back(instr);
            out_edges.push_back(node);
            return node;
        }

        template<typename Instr, typename ...Args>
        std::pair<std::shared_ptr<CFGNode>, std::shared_ptr<CFGNode>> branch(std::string next, std::string target, Args&&... args) {
            auto a = std::make_shared<CFGNode>(next);
            auto b = std::make_shared<CFGNode>(target);
            auto instr = std::make_shared<Instr>(b, std::forward<Args>(args)...);
            instructions.push_back(instr);
            out_edges.push_back(a);
            out_edges.push_back(b);
            return {a, b};
        }
    };

    struct Function {
        std::string name;
        static const constexpr size_t PADDING = 4;
        ssize_t stack_size = 0;
        size_t count = 0;

        // need to store all blocks here,
        // because cfg can have loop so edges are stored as weak_ptr
        // so blocks are stored here to keep it alive
        std::vector<std::shared_ptr<CFGNode>> blocks;
        std::shared_ptr<CFGNode> cursor;

        Function(std::string name);
        std::string next_name();

        std::shared_ptr<CFGNode> entry();

        template<typename Instr, typename ...Args>
        std::shared_ptr<VirtReg> append(Args&& ...args) {
            return cursor->template append<Instr, Args...>(std::forward<Args>(args)...);
        }

        std::shared_ptr<CFGNode> join(const std::shared_ptr<CFGNode>& x, const std::shared_ptr<CFGNode>& y) {
            auto node = std::make_shared<CFGNode>(next_name());
            if (blocks.back() != x) x->branch_existing<b>(node);
            if (blocks.back() != y) y->branch_existing<b>(node);
            blocks.push_back(node);
            switch_to(node);
            return node;
        }

        void add_phi(const std::shared_ptr<VirtReg>& x, const std::shared_ptr<VirtReg>& y) {
            cursor->add_phi(x, y);
        }

        template<typename Instr, typename ...Args>
        std::pair<std::shared_ptr<CFGNode>, std::shared_ptr<CFGNode>> branch(Args&&... args) {
            auto a = next_name();
            auto b = next_name();
            auto ret = cursor->template branch<Instr, Args...>(a, b, std::forward<Args>(args)...);
            blocks.push_back(ret.first);
            blocks.push_back(ret.second);
            switch_to(ret.first);
            return ret;
        }

        void switch_to(const std::shared_ptr<CFGNode> &target) {
            cursor = target;
        }

        std::vector<size_t> final_regs;
        void color();
        void output(std::ostream &) const;
    };

    template<class T>
    std::shared_ptr<vmips::Instruction>
    vmips::Ternary::create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1) {
        return std::make_shared<T>(std::move(lhs), std::move(op0), std::move(op1));
    }

    template<class T>
    std::shared_ptr<Instruction>
    Memory::create(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> addr, size_t offset) {
        return std::make_shared<T>(std::move(target), std::move(addr), offset);
    }

    template<class T>
    std::shared_ptr<Instruction>
    BinaryImm::create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs, ssize_t imm) {
        return std::make_shared<T>(std::move(lhs), std::move(rhs), imm);
    }

    template<class T>
    std::shared_ptr<Instruction> Binary::create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs) {
        return std::make_shared<T>(std::move(lhs), std::move(rhs));
    }

    template<class T>
    std::shared_ptr<Instruction> UnaryImm::create(std::shared_ptr<VirtReg> t, ssize_t imm) {
        return std::make_shared<T>(t, imm);
    }

    inline std::ostream &operator<<(std::ostream &out, const VirtReg &reg) {
        auto root = find_root(reg.parent.lock());
        if (root->allocated) {
            out << "$" << root->id.name;
        } else {
            out << "$undef(" << root->id.number << ")";
        }
        return out;
    }

}
#endif //BACKEND_VIRTUAL_MIPS_H
