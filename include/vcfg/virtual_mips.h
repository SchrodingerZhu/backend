//
// Created by schrodinger on 1/15/21.
//

#ifndef BACKEND_VIRTUAL_MIPS_H
#define BACKEND_VIRTUAL_MIPS_H
#define REG_NUM 17
#define SAVE_START 9
#include <utility>
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

    enum class SpecialReg {
        zero,
        at,
        v0,
        v1,
        a0,
        a1,
        a2,
        a3,
        k0,
        k1,
        gp,
        sp,
        fp,
        ra,
    };

    struct CFGNode;
    struct Function;

    struct MemoryLocation {
        size_t identifier = -1;
        size_t offset {};
        size_t size {};
        std::shared_ptr<class VirtReg> base {};
        enum Status {
            Assigned,
            Undetermined,
            Static
        } status {};
    };

    // must be SSA
    class VirtReg {
        static std::atomic_size_t GLOBAL;
    public:
        std::weak_ptr<VirtReg> parent;
        size_t union_size = 1;
        std::shared_ptr<MemoryLocation> overlap_location = nullptr;
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

    extern char special_names[(size_t)SpecialReg::ra + 1][8];
    std::shared_ptr<VirtReg> get_special(SpecialReg reg);

    std::shared_ptr<VirtReg> find_root(std::shared_ptr<VirtReg> x);

    void unite(std::shared_ptr<VirtReg> x, std::shared_ptr<VirtReg> y);

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
        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;
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

    struct callfunc : public Instruction {
        Function *current;
        std::unordered_set<std::shared_ptr<VirtReg>> overlap_temp {};
        std::weak_ptr<Function> function;
        std::vector<std::shared_ptr<VirtReg>> call_with;
        std::shared_ptr<VirtReg> ret;
        bool scanned = false;

        callfunc(std::shared_ptr<VirtReg> ret, Function * current, std::weak_ptr<Function> function, std::vector<std::shared_ptr<VirtReg>> call_with);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &out) const override;

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

    class Memory : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;
        std::shared_ptr<MemoryLocation> location;

        Memory(std::shared_ptr<VirtReg> target, std::shared_ptr<MemoryLocation> location);

        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &set) const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        template<class T>
        static std::shared_ptr<Instruction>
        create(std::shared_ptr<VirtReg> target, std::shared_ptr<MemoryLocation> location);

        void output(std::ostream &) const override;
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

    DECLARE(j, Unconditional);

    DECLARE(beq, CmpBranch);

    DECLARE(syscall, Instruction);

    DECLARE(beqz, ZeroBranch);
    DECLARE(blez, ZeroBranch);
    DECLARE(ble, CmpBranch);
    DECLARE(bge, CmpBranch);

    class jr : public Unary {
    public:
        std::shared_ptr<VirtReg> def() const override;
        jr(std::shared_ptr<VirtReg> reg);
    };

    class text : public Instruction {
        std::string context;
    public:
        explicit text(std::string context);
        const char * name() const override;
        void output(std::ostream& out) const override;
    };

    // upward links are broken down

    struct CFGNode {
        Function* function;
        std::string label;
        bool visited = false;
        std::vector<std::shared_ptr<Instruction>> instructions {};
        std::vector<std::weak_ptr<CFGNode>> out_edges {}; // at most two
        std::unordered_map<std::shared_ptr<VirtReg>, size_t> lives {}; // instructions.size  means live through

        CFGNode(Function* function, std::string name);
        void dfs_collect(std::unordered_set<std::shared_ptr<VirtReg>> &regs);

        void dfs_reset();

        void setup_living(const std::unordered_set<std::shared_ptr<VirtReg>> &reg);

        void generate_web(std::unordered_set<std::shared_ptr<VirtReg>> &liveness);

        void spill(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<MemoryLocation>& location);

        size_t color(const std::shared_ptr<VirtReg> &sp);

        void scan_overlap(std::unordered_set<std::shared_ptr<VirtReg>> &liveness);

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
            auto a = std::make_shared<CFGNode>(function, next);
            auto b = std::make_shared<CFGNode>(function, target);
            auto instr = std::make_shared<Instr>(b, std::forward<Args>(args)...);
            instructions.push_back(instr);
            out_edges.push_back(a);
            out_edges.push_back(b);
            return {a, b};
        }
    };



    struct Function {
        std::string name;
        static const constexpr size_t PADDING = 8;
        static const constexpr size_t MASK = PADDING - 1;
        size_t count = 0;
        size_t memory_count = 0;
        MemoryLocation ra_location;
        std::vector<std::shared_ptr<MemoryLocation>> mem_blocks;
        bool has_sub = false;
        bool allocated = false;
        size_t sub_argc = 0;
        size_t save_regs = 0;
        size_t stack_size = 0;
        size_t argc;

        // need to store all blocks here,
        // because cfg can have loop so edges are stored as weak_ptr
        // so blocks are stored here to keep it alive
        std::vector<std::shared_ptr<CFGNode>> blocks;
        std::shared_ptr<CFGNode> cursor;

        Function(std::string name, size_t argc);
        std::string next_name();

        std::shared_ptr<MemoryLocation> new_memory(size_t size);

        std::shared_ptr<MemoryLocation> new_static_mem(size_t size, std::shared_ptr<VirtReg> reg, size_t);

        std::shared_ptr<CFGNode> entry();

        template<typename Instr, typename ...Args>
        std::shared_ptr<VirtReg> append(Args&& ...args) {
            return cursor->template append<Instr, Args...>(std::forward<Args>(args)...);
        }

        template<typename Instr, typename ...Args>
        void append_void(Args&& ...args) {
            auto instr = std::make_shared<Instr>(std::forward<Args>(args)...);
            cursor->instructions.push_back(instr);
        }

        template<typename Instr, typename ...Args>
        std::shared_ptr<CFGNode> new_section_branch(Args&& ...args) {
            auto node = std::make_shared<CFGNode>(this, next_name());
            cursor->branch_existing<Instr>(node, std::forward<Args>(args)...);
            this->blocks.push_back(node);
            switch_to(node);
            return node;
        }

        template<typename Instr, typename ...Args>
        std::shared_ptr<CFGNode> branch_exsiting(const std::shared_ptr<CFGNode>& node, Args&& ...args) {
            cursor->branch_existing<Instr>(node, std::forward<Args>(args)...);
            switch_to(node);
            return node;
        }


        std::shared_ptr<CFGNode> join(const std::shared_ptr<CFGNode>& x, const std::shared_ptr<CFGNode>& y);
        std::shared_ptr<CFGNode> new_section();
        void add_phi(const std::shared_ptr<VirtReg>& x, const std::shared_ptr<VirtReg>& y);

        template<typename Instr, typename ...Args>
        std::pair<std::shared_ptr<CFGNode>, std::shared_ptr<CFGNode>> branch(Args&&... args) {
            auto a = next_name();
            auto b = next_name();
            auto ret = cursor->template branch<Instr, Args...>(a, b, std::forward<Args>(args)...);
            if (cursor != blocks.back()) cursor->branch_existing<vmips::j>(ret.first);
            blocks.push_back(ret.first);
            blocks.push_back(ret.second);
            switch_to(ret.first);
            return ret;
        }

        void switch_to(const std::shared_ptr<CFGNode> &target);
        template<typename ...Args>
        std::shared_ptr<VirtReg> call(std::weak_ptr<Function> target, Args&&... args) {
            auto ret = VirtReg::create();
            has_sub = true;
            sub_argc = std::max(sub_argc, target.lock()->argc);
            auto calling = std::make_shared<callfunc>(ret, this, std::move(target), std::vector<std::shared_ptr<VirtReg>> {std::forward<Args>(args)...});
            cursor->instructions.push_back(calling);
            return ret;
        }

        template<typename ...Args>
        void call_void(std::weak_ptr<Function> target, Args&&... args) {
            has_sub = true;
            sub_argc = std::max(sub_argc, target.lock()->argc);
            auto calling = std::make_shared<callfunc>(nullptr, this, std::move(target), std::vector<std::shared_ptr<VirtReg>> {std::forward<Args>(args)...});
            cursor->instructions.push_back(calling);
        }

        std::vector<size_t> final_regs;

        size_t color();
        void scan_overlap();
        void output(std::ostream &) const;
        void handle_alloca();
        void add_ret();
        void assign_special(SpecialReg special, std::shared_ptr<VirtReg> reg);
        void assign_special(SpecialReg special, ssize_t value);
    };

    template<class T>
    std::shared_ptr<vmips::Instruction>
    vmips::Ternary::create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1) {
        return std::make_shared<T>(std::move(lhs), std::move(op0), std::move(op1));
    }

    template<class T>
    std::shared_ptr<Instruction>
    Memory::create(std::shared_ptr<VirtReg> target, std::shared_ptr<MemoryLocation> location) {
        return std::make_shared<T>(std::move(target), std::move(location));
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

    std::ostream &operator<<(std::ostream &out, const VirtReg &reg);

    std::ostream &operator<<(std::ostream &out, const MemoryLocation &location);

}
#endif //BACKEND_VIRTUAL_MIPS_H
