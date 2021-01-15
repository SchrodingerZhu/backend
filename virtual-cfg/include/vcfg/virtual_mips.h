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
        VirtReg();
        static std::shared_ptr<VirtReg> create();
        static std::shared_ptr<VirtReg> create_constant(const char * name);
    };
    
    class Instruction {
    public:
        virtual void collect_register(std::unordered_set<std::shared_ptr<VirtReg>> &) const;
        virtual const char * name() const;
        virtual std::shared_ptr<VirtReg> def() const;
        virtual bool used_register(const std::shared_ptr<VirtReg>& reg) const;
    };

    class Ternary : public Instruction {
    public:
        const std::shared_ptr<VirtReg> lhs;
        const std::shared_ptr<VirtReg> op0, op1;
        Ternary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);
        void collect_register(std::unordered_set<std::shared_ptr<VirtReg>>& set) const override;
        std::shared_ptr<VirtReg> def() const override;
        bool used_register(const std::shared_ptr<VirtReg>& reg) const override;
    };

    class BinaryImm : public Instruction {};
    class Binary : public Instruction {};
    class Unary : public Instruction {};
    class UnaryImm : public Instruction {};
    class Branch : public Instruction {};

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

    //TODO: fix la
    class LA : public Instruction {};


    class LI : public UnaryImm {};
    class LUI : public UnaryImm {};
    class MOVE : public Binary {};
    class NEGU: public Binary {};
    class SEB : public Binary {};
    class SEH : public Binary {};
    class SUB : public Ternary {};
    class SUBU : public Ternary {};

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

    // upward links are broken down

    struct CFGNode {
        std::string label;
        std::vector<std::shared_ptr<Instruction>> instructions;
        std::vector<std::shared_ptr<CFGNode>> out_edges; // at most two
        std::unordered_map<std::shared_ptr<VirtReg>, size_t> lives; // instructions.size  means live through

        void dfs_collect(std::unordered_set<std::shared_ptr<VirtReg>>& regs);
        void setup_living(const std::unordered_set<std::shared_ptr<VirtReg>>& reg);
        void generate_web(std::unordered_set<std::shared_ptr<VirtReg>>& liveness);
    };

    template<class T>
    std::shared_ptr<vmips::Instruction> vmips::Ternary::create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1) {
        return std::make_shared<T>(lhs, op0, op1);
    }
}
#endif //BACKEND_VIRTUAL_MIPS_H
