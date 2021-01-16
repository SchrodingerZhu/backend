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
namespace vmips {
    // must be SSA
    class VirtReg {
        static std::atomic_size_t GLOBAL;
    public:
        std::unordered_set<std::shared_ptr<VirtReg>> neighbors;
        union {
            size_t number = 0;
            char name[sizeof(size_t)];
        } id;
        bool allocated;
        bool spilled;
        VirtReg();
        static std::shared_ptr<VirtReg> create();
        static std::shared_ptr<VirtReg> create_constant(const char * name);
        friend std::ostream& operator<<(std::ostream &, const VirtReg&);
    };
    
    class Instruction {
    public:
        virtual void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &) const;
        virtual const char * name() const;
        virtual std::shared_ptr<VirtReg> def() const;
        virtual bool used_register(const std::shared_ptr<VirtReg>& reg) const;
        virtual void replace(const std::shared_ptr<VirtReg>& reg, const std::shared_ptr<VirtReg>& target);
        virtual void output(std::ostream &) const = 0;
    };

    class Ternary : public Instruction {
    public:
        std::shared_ptr<VirtReg> lhs;
        std::shared_ptr<VirtReg> op0, op1;
        Ternary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);
        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>>& set) const override;
        std::shared_ptr<VirtReg> def() const override;
        bool used_register(const std::shared_ptr<VirtReg>& reg) const override;
        void replace(const std::shared_ptr<VirtReg>& reg, const std::shared_ptr<VirtReg>& target) override;
        void output(std::ostream &) const override;
    };

    class BinaryImm : public Instruction {};
    class Binary : public Instruction {};
    class Unary : public Instruction {};
    class UnaryImm : public Instruction {};
    class Branch : public Instruction {};
    class Memory : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;
        std::shared_ptr<VirtReg> addr;
        ssize_t offset;
        Memory(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> addr, ssize_t offset);
        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>>& set) const override;
        std::shared_ptr<VirtReg> def() const override;
        bool used_register(const std::shared_ptr<VirtReg>& reg) const override;
        void replace(const std::shared_ptr<VirtReg>& reg, const std::shared_ptr<VirtReg>& target) override;
        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> addr, size_t offset);
        void output(std::ostream &) const override;
    };

    //self defined marker
    class Syscall : public Instruction {};

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
    class la : public Instruction {};

    DECLARE(li, UnaryImm);
    DECLARE(lui, UnaryImm);
    DECLARE(move, Binary);
    DECLARE(negu, Binary);
    DECLARE(seb, Binary);
    DECLARE(seh, Binary);
    DECLARE(sub, Ternary);
    DECLARE(subu, Ternary);


    class ROTR : public BinaryImm {};
    class ROTRV : public Ternary {};
    class SLL : public BinaryImm {};
    class SLLV : public Ternary {};
    class SRA : public BinaryImm {};
    class SRAV : public Ternary {};
    class SRL : public BinaryImm {};
    class SRLV : public Ternary {};

    class AND : public Ternary {};
    class ANDI : public BinaryImm {};

    DECLARE(lw, Memory);
    DECLARE(sw, Memory);

    // upward links are broken down

    struct CFGNode {
        std::string label;
        bool visited;
        std::vector<std::shared_ptr<Instruction>> instructions;
        std::vector<std::shared_ptr<CFGNode>> out_edges; // at most two
        std::unordered_map<std::shared_ptr<VirtReg>, size_t> lives; // instructions.size  means live through

        void dfs_collect(std::unordered_set<std::shared_ptr<VirtReg>>& regs);
        void dfs_reset();
        void setup_living(const std::unordered_set<std::shared_ptr<VirtReg>>& reg);
        void generate_web(std::unordered_set<std::shared_ptr<VirtReg>>& liveness);
        void spill(const std::shared_ptr<VirtReg>& reg, const std::shared_ptr<VirtReg>& sp, ssize_t stack_location);
        void color(const std::shared_ptr<VirtReg>& sp, ssize_t& current_stack_shift);
        void output(std::ostream &out);
    };

    template<class T>
    std::shared_ptr<vmips::Instruction> vmips::Ternary::create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1) {
        return std::make_shared<T>(lhs, op0, op1);
    }

    template<class T>
    std::shared_ptr<Instruction>
    Memory::create(std::shared_ptr<VirtReg> target, std::shared_ptr<VirtReg> addr, size_t offset) {
        return std::make_shared<T>(target, addr, offset);
    }

    inline std::ostream& operator<<(std::ostream & out, const VirtReg& reg) {
        if (reg.allocated) {
            out << "$" << reg.id.name;
        } else {
            out << "$undef(" << reg.id.number << ")";
        }
        return out;
    }

    inline void CFGNode::output(std::ostream &out) {
        if (visited) return;
        visited = true;
        out << label << ":" << std::endl;
        for (auto & i : instructions) {
            out << "\t";
            i->output(out);
            out << "\n";
        }
        out << std::endl;
        for (auto & i : out_edges) {
            i->output(out);
        }
        visited = false;
    }
}
#endif //BACKEND_VIRTUAL_MIPS_H
