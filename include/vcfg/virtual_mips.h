/**
 * Virtual MIPS Intermediate Representation.
 * An static single assignment IR with infinite register for code generation, it does the following:
 * 1. control data and text section allocation
 * 2. manage stack frame
 * 3. manage register allocation
 */
#ifndef BACKEND_VIRTUAL_MIPS_H
#define BACKEND_VIRTUAL_MIPS_H
#define REG_NUM 17
#define SAVE_START 9
#define EXTRA_STACK 16

#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <ostream>
#include <algorithm>
#include <sstream>
#include <phmap.h>

namespace vmips {

    template<class T>
    using unordered_set = phmap::parallel_flat_hash_set<T>;

    template<class Key, class Value, class H = std::hash<Key>>
    using unordered_map = phmap::parallel_flat_hash_map<Key, Value, H>;

    /*!
     * The data class. Represents a data section in the virtual MIPS IR.
     */
    struct Data {
        /*!
         * Name of the data section
         */
        std::string name;
        /*!
         * Read-write property
         */
        bool read_only;

        /*!
         * Data constructor.
         * @param name name of the data section
         * @param read_only read-write property
         */
        Data(std::string name, bool read_only);

        /*!
         * Get the type label of the data
         * @return an assembler derivative representing the type
         */
        virtual const char *type_label() const = 0;

        /*!
         * Display the data section
         * @param out output stream
         */
        virtual void output(std::ostream &out) const = 0;

        /*!
         * Factory function to create a data section instance.
         * @tparam Type subclass type
         * @tparam Args subclass creation arguments
         * @param read_only read-write property
         * @param args subclass creation arguments
         * @return a shared pointer to the subclass instance
         */
        template<class Type, class ...Args>
        inline static std::shared_ptr<Data> create(bool read_only, Args &&... args) {
            static std::atomic_size_t counter{0};
            std::stringstream ss;
            ss << "data_section_$$" << counter.fetch_add(1);
            return std::make_shared<Type>(ss.str(), read_only, std::forward<Args>(args)...);
        }
    };

    /*!
     * The SpecialReg enum class.
     */
    enum class SpecialReg {
        zero, /**< zero register */
        at,   /**< at register   */
        v0,   /**< v0 register   */
        v1,   /**< v1 register   */
        a0,   /**< a0 register   */
        a1,   /**< a1 register   */
        a2,   /**< a2 register   */
        a3,   /**< a3 register   */
        k0,   /**< k0 register   */
        k1,   /**< k1 register   */
        gp,   /**< gp register   */
        sp,   /**< sp register   */
        fp,   /**< fp register   */
        ra,   /**< ra register   */
        s8,   /**< s8 register   */
    };

    struct CFGNode;
    struct Function;
    /*!
     * The MemoryLocation class. Represents a memory location on stack to be determined statically.
     */
    struct MemoryLocation {
        /*!
         * Identifier for pretty print.
         */
        size_t identifier = -1;
        /*!
         * Memory offset on the stack.
         */
        size_t offset{};
        /*!
         * Size of the memory area.
         */
        size_t size{};
        /*!
         * Pointer to the function that owns the area.
         */
        Function *function;
        /*!
         * Base register for locating.
         */
        std::shared_ptr<class VirtReg> base{};
        /*!
         * Status of the allocation.
         */
        enum Status {
            Assigned,     /**< Already assigned. */
            Undetermined, /**< To be determined. */
            Static,       /**< Manually assigned. */
            Argument      /**< Argument section. */
        } status{};
    };

    /*!
     * The register life time can be merged using union find algorithm.
     * This operation is the union find operation to collapse the whole equivalent class
     * of the register and locate the root representative of the register.
     * @param x target register
     * @return representative of the equivalent class
     */
    std::shared_ptr<class VirtReg> find_root(std::shared_ptr<class VirtReg> x);

    /*!
     * The VirtReg class. Represents the virtual registers in the IR.
     */
    class VirtReg {
        /*!
         * Global monotonic counter used to generate identifier.
         */
        static std::atomic_size_t GLOBAL;
    public:
        /*!
         * Parent point (used in union find algorithm).
         */
        std::weak_ptr<VirtReg> parent;
        /*!
         * Size of the total union (used for heuristic optimal merging of two unions).
         */
        size_t union_size = 1;
        /*!
         * If a temporal register life time is overlapped with a subroutine call, we need to assign
         * a stack section for it to recover it after the call.
         */
        std::shared_ptr<MemoryLocation> overlap_location = nullptr;
        /*!
         * Reachable neighbours in the lifetime graph (a.k.a. Web). Neighbours cannot use the same color.
         */
        unordered_set<std::shared_ptr<VirtReg>> neighbors;
        /*!
         * Real identifier in the generated code. If the register is assigned successfully, it will use the name of
         * a real MIPS register; otherwise we just use the global identifier number to distinguish the registers.
         */
        union {
            size_t number = 0;
            char name[sizeof(size_t)];
        } id;
        /*!
         * Whether the current register is already assigned with a real MIPS register.
         */
        bool allocated;
        /*!
         * Whether the current register has already been spilled.
         */
        bool spilled;

        /*!
         * Virtual Register Constructor.
         */
        VirtReg();

        /*!
         * Factory function to create an unassigned virtual register.
         * @return a new register instance.
         */
        static std::shared_ptr<VirtReg> create();

        /*!
         * Factory function to create a manually assigned register.
         * @param name name of the real mips register.
         * @return a new register instance.
         */
        static std::shared_ptr<VirtReg> create_constant(const char *name);

        /*!
         * Display the virtual register.
         * @return the original stream with the register displayed.
         */
        friend std::ostream &operator<<(std::ostream &, const VirtReg &);

        /*!
         * Compare register through equivalent class.
         * @param that comparison operand.
         * @return comparison result.
         */
        bool operator==(const VirtReg &that) const {
            return id.number == that.id.number || find_root(parent.lock()) == find_root(that.parent.lock());
        }
    };

    /*!
     * Special register names.
     */
    extern char special_names[(size_t) SpecialReg::s8 + 1][8];

    /*!
     * Get singleton of a special MIPS register.
     * @param reg special register enum.
     * @return singleton of the register.
     */
    std::shared_ptr<VirtReg> get_special(SpecialReg reg);

    /*!
     * Union find operation to merge two lifetime nodes.
     * @param x register to be merged.
     * @param y register to be merged.
     */
    void unite(std::shared_ptr<VirtReg> x, std::shared_ptr<VirtReg> y);

    /*!
     * The Instruction class. Represents an instruction line of MIPS.
     */
    class Instruction {
    public:
        /*!
         * Collect all used register that need to be colored.
         * @param collection set of register (accumulator).
         */
        virtual void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                                      &collection) const;

        /*!
         * Get MIPS assembly name of the instruction.
         * @return name of the instruction.
         */
        virtual const char *name() const;

        /*!
         * Get new register defined at this instruction.
         * @return the defined register; null if nothing is newly defined.
         */
        virtual std::shared_ptr<VirtReg> def() const;

        /*!
         * Check whether this instruction used a target register.
         * @param reg register to be checked.
         * @return usage.
         */
        virtual bool used_register(const std::shared_ptr<VirtReg> &reg) const;

        /*!
         * Replace the original register with a new one.
         * @param reg original register.
         * @param target alternative register.
         */
        virtual void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target);

        /*!
         * Display the instruction (codegen).
         * @param out output stream.
         */
        virtual void output(std::ostream &out) const = 0;

        /*!
         * Branch at this instruction.
         * @return the new CFGNode caused by the branch.
         */
        virtual std::shared_ptr<CFGNode> branch();
    };

    /*!
     * The phi class. SSA Phi Node Instruction. This is used to
     * declare the joint of register lifetime through different
     * CFG branches. Typically used in if-else branch and for-loops.
     */
    class phi : public Instruction {
    public:
        /*!
         * Registers whose lifetime to be joint.
         */
        std::shared_ptr<VirtReg> op0, op1;

        /*!
         * The phi class constructor.
         * @param op0 a register whose lifetime to be joint.
         * @param op1 a register whose lifetime to be joint.
         */
        phi(std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &out) const override;
    };

    /*!
     * The Ternary class. Represents instructions that take three registers as operands.
     */
    class Ternary : public Instruction {
    public:
        std::shared_ptr<VirtReg> lhs;
        std::shared_ptr<VirtReg> op0, op1;

        Ternary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        template<class T>
        static std::shared_ptr<Instruction>
        create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                              &set)
        const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;


    };

    /*!
     * The BinaryImm class. Represents instructions that take two registers and an immediate value as operands.
     */
    class BinaryImm : public Instruction {
    public:
        std::shared_ptr<VirtReg> lhs;
        std::shared_ptr<VirtReg> rhs;
        ssize_t imm;

        BinaryImm(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs, ssize_t imm);

        template<class T>
        static std::shared_ptr<Instruction>
        create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs, ssize_t imm);

        void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                              &set)
        const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;
    };

    /*!
     * The Binary class. Represents instructions that take two registers as operands.
     */
    class Binary : public Instruction {
    public:
        std::shared_ptr<VirtReg> lhs;
        std::shared_ptr<VirtReg> rhs;

        Binary(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> rhs);

        void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                              &set)
        const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;
    };

    /*!
     * The Unary class. Represents instructions that take one register as operands.
     */
    class Unary : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;

        Unary(std::shared_ptr<VirtReg> t);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> t);

        void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                              &set)
        const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;

    };

    template<class T>
    std::shared_ptr<Instruction> Unary::create(std::shared_ptr<VirtReg> t) {
        return std::make_shared<T>(t);
    }

    /*!
     * The UnaryImm class. Represents instructions that take one register and an immediate value as operands.
     */
    class UnaryImm : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;
        ssize_t imm;

        UnaryImm(std::shared_ptr<VirtReg> t, ssize_t imm);

        template<class T>
        static std::shared_ptr<Instruction> create(std::shared_ptr<VirtReg> t, ssize_t imm);

        void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                              &set)
        const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &) const override;
    };

    /*!
     * The callfunc class. Represents function call virtual instruction. This instruction will be expanded into
     * preparations before the call, jal calling and recovery operations after the call.
     */
    struct callfunc : public Instruction {
        Function *current;
        unordered_set<std::shared_ptr<VirtReg>> overlap_temp{};
        std::weak_ptr<Function> function;
        std::vector<std::shared_ptr<VirtReg>> call_with;
        std::shared_ptr<VirtReg> ret;
        bool scanned = false;

        callfunc(std::shared_ptr<VirtReg> ret, Function *current, std::weak_ptr<Function> function,
                 std::vector<std::shared_ptr<VirtReg>> call_with);

        void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                              &set)
        const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        void output(std::ostream &out) const override;

    };

    /*!
     * The Unconditional class. Represents the unconditionally branch and jump instructions.
     */
    class Unconditional : public Instruction {
    public:
        std::weak_ptr<CFGNode> block;

        explicit Unconditional(std::weak_ptr<CFGNode> block);

        void output(std::ostream &) const override;

        std::shared_ptr<CFGNode> branch() override;

        std::shared_ptr<VirtReg> def() const override;
    };

    /*!
     * The ZeroBranch class. Represents the branch and jump instructions based the comparison with zero.
     */
    class ZeroBranch : public Unary {
    public:
        std::weak_ptr<CFGNode> block;

        ZeroBranch(std::weak_ptr<CFGNode> block, std::shared_ptr<VirtReg> check);

        void output(std::ostream &) const override;

        std::shared_ptr<CFGNode> branch() override;

        std::shared_ptr<VirtReg> def() const override;
    };

    /*!
     * The CmpBranch class. Represents the branch and jump instructions based the comparison of two operands.
     */
    class CmpBranch : public Binary {
    public:
        std::weak_ptr<CFGNode> block;

        CmpBranch(std::weak_ptr<CFGNode> block,
                  std::shared_ptr<VirtReg> op0, std::shared_ptr<VirtReg> op1);

        void output(std::ostream &) const override;

        std::shared_ptr<CFGNode> branch() override;

        std::shared_ptr<VirtReg> def() const override;
    };

    /*!
     * The Memory class. Represents memory loading and storing MIPS instructions.
     */
    class Memory : public Instruction {
    public:
        std::shared_ptr<VirtReg> target;
        std::shared_ptr<MemoryLocation> location;

        Memory(std::shared_ptr<VirtReg> target, std::shared_ptr<MemoryLocation> location);

        void collect_register(unordered_set<std::shared_ptr<VirtReg>>

                              &set)
        const override;

        std::shared_ptr<VirtReg> def() const override;

        bool used_register(const std::shared_ptr<VirtReg> &reg) const override;

        void replace(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<VirtReg> &target) override;

        template<class T>
        static std::shared_ptr<Instruction>
        create(std::shared_ptr<VirtReg> target, std::shared_ptr<MemoryLocation> location);

        void output(std::ostream &) const override;
    };

/*! Macro to generate a constructor of subclass instruction. */
#define BASE_INIT(S, B) \
template <typename ...Args> \
explicit S(Args&& ...args) : B(std::forward<Args>(args)...) {}

/*! Macro to define a new instruction. */
#define DECLARE(S, B) \
/*! The S class. MIPS S instruction. Derived from B. */\
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

    /*!
     * The jr class. MIPS jump return instruction.
     */
    class jr : public Unary {
    public:
        std::shared_ptr<VirtReg> def() const override;

        jr(std::shared_ptr<VirtReg> reg);
    };

    /*!
     * The text class. This is used to manual insert a instruction.
     */
    class text : public Instruction {
        std::string context;
    public:
        explicit text(std::string context);

        const char *name() const override;

        void output(std::ostream &out) const override;
    };

    /*!
     * The la class. Pseudo MIPS instruction to load data address.
     */
    class la : public Unary {
        std::shared_ptr<Data> data;
    public:
        explicit la(std::shared_ptr<VirtReg> reg, std::shared_ptr<Data> data);

        const char *name() const override;

        void output(std::ostream &out) const override;
    };

    /*!
     * The address class. Virtual instruction to load data offset from stack.
     */
    class address : public Unary {
        std::shared_ptr<MemoryLocation> data;
    public:
        explicit address(std::shared_ptr<VirtReg> reg, std::shared_ptr<MemoryLocation> data);

        void output(std::ostream &out) const override;
    };

    // upward links are broken down
    /*!
     * The CFGNode class. Control Flow Graph Node.
     */
    struct CFGNode {
        /*!
         * Pointer to the function that owns the node.
         */
        Function *function;
        /*!
         * Label of the node.
         */
        std::string label;
        /*!
         * Flag used in the DFS walk of the graph.
         */
        bool visited = false;
        /*!
         * All instructions in the basic block.
         */
        std::vector<std::shared_ptr<Instruction>> instructions{};
        /*!
         * Out edges from this basic block.
         */
        std::vector<std::weak_ptr<CFGNode>> out_edges{}; // at most two
        /*!
         * Set used in the DFS walk of the graph to record lifetime information of the register.
         */
        unordered_map<std::shared_ptr<VirtReg>, size_t> lives{}; // instructions.size  means live through
        /*!
         * CFGNode constructor.
         * @param function pointer to the function that owns the node.
         * @param name name of the node.
         */
        CFGNode(Function *function, std::string name);

        /*!
         * DFS walk to initialize the register information of the coloring graph.
         * @param regs register accumulator.
         */
        void dfs_collect(unordered_set<std::shared_ptr<VirtReg>> &regs);

        /*!
         * DFS walk to reset the node state.
         */
        void dfs_reset();

        /*!
         * DFS walk to calculate the lifetime information of the coloring graph.
         * @param reg register accumulator.
         */
        void setup_living(const unordered_set<std::shared_ptr<VirtReg>>

                          &reg);

        /*!
         * DFS walk to construct coloring graph.
         * @param liveness register accumulator.
         */
        void generate_web(unordered_set<std::shared_ptr<VirtReg>> &liveness);

        /*!
         * DFS walk to spill conflicted register.
         * @param reg register to be spilled.
         * @param location fallback storage of the register.
         */
        void spill(const std::shared_ptr<VirtReg> &reg, const std::shared_ptr<MemoryLocation> &location);

        /*!
         * Perform graph coloring algorithm to assign the registers.
         * @param sp stack frame pointer.
         */
        size_t color(const std::shared_ptr<VirtReg> &sp);

        /*!
         * DFS walk to mark overlapped lifetime.
         * @param liveness register accumulator.
         */
        void scan_overlap(unordered_set<std::shared_ptr<VirtReg>> &liveness);

        /*!
         * Display of the node (codegen).
         * @param out output stream.
         */
        void output(std::ostream &out);

        /*!
         * Add a new instruction.
         * @tparam Instr instruction class.
         * @tparam Args parameters to build the instruction.
         * @param args parameters to build the instruction.
         * @return new register defined in this instruction.
         */
        template<typename Instr, typename ...Args>
        std::shared_ptr<VirtReg> append(Args &&...args) {
            auto ret = VirtReg::create();
            auto instr = std::make_shared<Instr>(ret, std::forward<Args>(args)...);
            instructions.push_back(instr);
            return ret;
        }

        /*!
         * Create a phi node and append it to this node.
         * @param x register to be joint
         * @param y register to be joint
         */
        void add_phi(std::shared_ptr<VirtReg> x, std::shared_ptr<VirtReg> y) {
            instructions.push_back(std::make_shared<phi>(std::move(x), std::move(y)));
        }

        /*!
         * Branch to an existing CFGNode.
         * @tparam Instr instruction class.
         * @tparam Args parameters to build the instruction.
         * @param node target CFGNode.
         * @param args parameters to build the instruction.
         */
        template<typename Instr, typename ...Args>
        void branch_existing(const std::shared_ptr<CFGNode> &node, Args &&... args) {
            out_edges.push_back(node);
            auto instr = std::make_shared<Instr>(node, std::forward<Args>(args)...);
            instructions.push_back(instr);
        }

        /*!
         * Single target branch.
         * @tparam Instr  instruction class (should be b, j).
         * @tparam Args parameters to build the instruction.
         * @param name name of the new node.
         * @param args parameters to build the instruction.
         * @return a new CFGNode.
         */
        template<typename Instr, typename ...Args>
        std::shared_ptr<CFGNode> branch_single(std::string name, Args &&... args) {
            auto node = std::make_shared<CFGNode>(name);
            auto instr = std::make_shared<Instr>(node, std::forward<Args>(args)...);
            instructions.push_back(instr);
            out_edges.push_back(node);
            return node;
        }

        /*!
         * Multiple targets branch.
         * @tparam Instr instruction class.
         * @tparam Args parameters to build the instruction.
         * @param next name of the next node.
         * @param target name of the target node.
         * @param args parameters to build the instruction.
         * @return a pair of new CFGNodes.
         */
        template<typename Instr, typename ...Args>
        std::pair<std::shared_ptr<CFGNode>, std::shared_ptr<CFGNode>>
        branch(std::string next, std::string target, Args &&... args) {
            auto a = std::make_shared<CFGNode>(function, next);
            auto b = std::make_shared<CFGNode>(function, target);
            auto instr = std::make_shared<Instr>(b, std::forward<Args>(args)...);
            instructions.push_back(instr);
            out_edges.push_back(a);
            out_edges.push_back(b);
            return {a, b};
        }
    };

    /*!
     * The Function class. Represents a function in the program.
     */
    struct Function {
        /*!
         * Function name.
         */
        std::string name;
        /*!
         * Stack padding amount.
         */
        static const constexpr size_t PADDING = 8;
        /*!
         * Stack padding mask.
         */
        static const constexpr size_t MASK = PADDING - 1;
        /*!
         * Basic blocks counter.
         */
        size_t count = 0;
        /*!
         * Memory Region counter.
         */
        size_t memory_count = 0;
        /*!
         * Memory region to store RA register.
         */
        MemoryLocation ra_location;
        /*!
         * Memory region to store PIC information.
         */
        MemoryLocation pic_location;
        /*!
         * Memory region to store frame register.
         */
        MemoryLocation s8_location;
        /*!
         * All data blocks.
         */
        std::vector<std::shared_ptr<struct Data>> data_blocks;
        /*!
         * All memory blocks.
         */
        std::vector<std::shared_ptr<MemoryLocation>> mem_blocks;
        /*!
         * Whether the function has subroutine call.
         */
        bool has_sub = false;
        /*!
         * Whether memory sections are assigned.
         */
        bool allocated = false;
        /*!
         * Maximum argument count of subroutine call.
         */
        size_t sub_argc = 0;
        /*!
         * Maximum usage of save registers.
         */
        size_t save_regs = 0;
        /*!
         * Size of the stack frame.
         */
        size_t stack_size = 0;
        /*!
         * Number of arguments.
         */
        size_t argc;

        // need to store all blocks here,
        // because cfg can have loop so edges are stored as weak_ptr
        // so blocks are stored here to keep it alive
        /*!
         * All CFGNodes.
         */
        std::vector<std::shared_ptr<CFGNode>> blocks;
        /*!
         * Current codegen point.
         */
        std::shared_ptr<CFGNode> cursor;

        /*!
         * Function constructor.
         * @param name name of the function.
         * @param argc number of arguments.
         */
        Function(std::string name, size_t argc);

        /*!
         * Allocate a unique name for a new basic block.
         * @return unique name.
         */
        std::string next_name();

        /*!
         * Create a new memory region.
         * @param size size of the memory region.
         * @return a new memory region instance.
         */
        std::shared_ptr<MemoryLocation> new_memory(size_t size);

        /*!
         * Get a memory location represents an argument (in the callee stack frame).
         * @param index the index of the argument.
         * @return the memory location
         */
        std::shared_ptr<MemoryLocation> argument(size_t index);

        /*!
         * Create a manually assigned static memory region.
         * @param size size of the memory region.
         * @param reg base register.
         * @param offset shift amount from the base register.
         * @return the memory location.
         */
        std::shared_ptr<MemoryLocation> new_static_mem(size_t size, std::shared_ptr<VirtReg> reg, size_t offset);

        /*!
         * Start adding instruction to the function.
         * @return
         */
        std::shared_ptr<CFGNode> entry();

        /*!
         * Add a new instruction to the cursor pointed location.
         * @tparam Instr instruction class.
         * @tparam Args instruction class constructor arguments.
         * @param args instruction class constructor arguments.
         * @return new virtual register defined in the instruction.
         */
        template<typename Instr, typename ...Args>
        std::shared_ptr<VirtReg> append(Args &&...args) {
            return cursor->template append<Instr, Args...>(std::forward<Args>(args)...);
        }

        /*!
         * Add a new instruction the cursor pointed location that does not define new register.
         * @tparam Instr instruction class.
         * @tparam Args instruction class constructor arguments.
         * @param args instruction class constructor arguments.
         */
        template<typename Instr, typename ...Args>
        void append_void(Args &&...args) {
            auto instr = std::make_shared<Instr>(std::forward<Args>(args)...);
            cursor->instructions.push_back(instr);
        }

        /*!
         * Create a new CFG Node via a branch instruction.
         * @tparam Instr instruction class.
         * @tparam Args instruction class constructor arguments.
         * @param args instruction class constructor arguments.
         * @return a new CFGNode.
         */
        template<typename Instr, typename ...Args>
        std::shared_ptr<CFGNode> new_section_branch(Args &&...args) {
            auto node = std::make_shared<CFGNode>(this, next_name());
            cursor->branch_existing<Instr>(node, std::forward<Args>(args)...);
            this->blocks.push_back(node);
            switch_to(node);
            return node;
        }

        template<typename Instr, typename ...Args>
        std::shared_ptr<CFGNode> branch_existing(const std::shared_ptr<CFGNode> &node, Args &&...args) {
            cursor->branch_existing<Instr>(node, std::forward<Args>(args)...);
            switch_to(node);
            return node;
        }


        std::shared_ptr<CFGNode> join(const std::shared_ptr<CFGNode> &x, const std::shared_ptr<CFGNode> &y);

        std::shared_ptr<CFGNode> new_section();

        void add_phi(const std::shared_ptr<VirtReg> &x, const std::shared_ptr<VirtReg> &y);

        template<typename Instr, typename ...Args>
        std::pair<std::shared_ptr<CFGNode>, std::shared_ptr<CFGNode>> branch(Args &&... args) {
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
        std::shared_ptr<VirtReg> call(std::weak_ptr<Function> target, Args &&... args) {
            auto ret = VirtReg::create();
            has_sub = true;
            sub_argc = std::max(sub_argc, target.lock()->argc);
            auto calling = std::make_shared<callfunc>(ret, this, std::move(target),
                                                      std::vector<std::shared_ptr<VirtReg >>{
                                                              std::forward<Args>(args)...});
            cursor->instructions.push_back(calling);
            return ret;
        }

        template<typename ...Args>
        void call_void(std::weak_ptr<Function> target, Args &&... args) {
            has_sub = true;
            sub_argc = std::max(sub_argc, target.lock()->argc);
            auto calling = std::make_shared<callfunc>(nullptr, this, std::move(target),
                                                      std::vector<std::shared_ptr<VirtReg >>{
                                                              std::forward<Args>(args)...});
            cursor->instructions.push_back(calling);
        }

        /*!
         * Register allocation.
         * @return saved register count.
         */
        size_t color();

        /*!
         * DFS walk to record the overlapped lifetime information.
         */
        void scan_overlap();

        /*!
         * Output the generated code.
         * @param out output stream.
         */
        void output(std::ostream &out) const;

        /*!
         * Allocate all memory locations.
         */
        void handle_alloca();

        /*!
         * Jump to the epilogue and return.
         */
        void add_ret();

        /*!
         * Add an assignment operation to a special register.
         * @param special target.
         * @param reg source.
         */
        void assign_special(SpecialReg special, std::shared_ptr<VirtReg> reg);

        /*!
         * Add an assignment operation to a special register.
         * @param special target.
         * @param value value.
         */
        void assign_special(SpecialReg special, ssize_t value);

        /*!
         * Factory function to create a data section.
         * @tparam Type data type.
         * @tparam Args data creation arguments.
         * @param read_only whether the data is read only.
         * @param args data creation arguments.
         * @return data section instance.
         */
        template<class Type, typename ...Args>
        std::shared_ptr<struct Data> create_data(bool read_only, Args &&... args) {
            auto data = Data::create<Type>(read_only,
                                           std::vector<typename Type::data_type>{std::forward<Args>(args)...});
            data_blocks.push_back(data);
            return data;
        }
    };

    template<class T>
    std::shared_ptr<vmips::Instruction>
    vmips::Ternary::create(std::shared_ptr<VirtReg> lhs, std::shared_ptr<VirtReg> op0,
                           std::shared_ptr<VirtReg> op1) {
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

    /*!
     * Convert string to output form.
     * @param out output stream.
     * @param s inner string.
     */
    static inline void escaped_string(std::ostream &out, const std::string &s) {
        for (auto ch : s) {
            switch (ch) {
                case '\'':
                    out << "\\'";
                    break;

                case '\"':
                    out << "\\\"";
                    break;

                case '\?':
                    out << "\\?";
                    break;

                case '\\':
                    out << "\\\\";
                    break;

                case '\a':
                    out << "\\a";
                    break;

                case '\b':
                    out << "\\b";
                    break;

                case '\f':
                    out << "\\f";
                    break;

                case '\n':
                    out << "\\n";
                    break;

                case '\r':
                    out << "\\r";
                    break;

                case '\t':
                    out << "\\t";
                    break;

                case '\v':
                    out << "\\v";
                    break;

                default:
                    out << ch;
            }
        }
    }

    /*!
     * Convert char to output format.
     * @param out output stream.
     * @param x target char.
     */
    static inline void char_wrap(std::ostream &out, char x) {
        out << "'";
        char data[2] = {x, 0};
        escaped_string(out, data);
        out << "'";
    }

    /*!
     * Convert string to output string
     * @param out output stream.
     * @param data string data
     */
    static inline void str_wrap(std::ostream &out, const std::string &data) {
        out << '"';
        escaped_string(out, data);
        out << '"';
    }

    /*!
     * Identity output.
     * @tparam T output type.
     * @param out output stream.
     * @param x output data.
     */
    template<class T>
    static inline void normal(std::ostream &out, const T &x) {
        out << x;
    }

/*!
 * Declare a MIPS data type.
 */
#define DECLARE_DATA(Name, ValueType) \
    /*! The Name class. MIPS Data field containing ValueType.  */                          \
    struct Name : public Data {       \
         using data_type = ValueType;                             \
         std::vector<ValueType> value;              \
         template <typename ...Args>\
         explicit Name(const std::string& name, bool read_only, Args&&... args) : Data(name, read_only), value(std::forward<Args>(args)...) { \
         }                                                                                                       \
         const char * type_label() const override; \
         void output(std::ostream &out) const override; \
    };

#define IMPLEMENT_DATA(Name, Align, Process) \
         const char * vmips::Name::type_label() const { \
            return "." #Name;                   \
         }                             \
         void vmips::Name::output(std::ostream &out) const {                                                                                    \
                                              \
            out << (read_only ? "\t.rdata" : "\t.data") << std::endl;                                                                       \
            if (Align > 0) {                                                                                                                \
                out << "\t.align " << Align << std::endl;                                                                                   \
            }                                 \
            out << name << ": " << std::endl;\
            out << "\t" << type_label() << " ";         \
            for(auto i = 0; i < value.size(); ++i) {                                 \
                Process(out, value[i]);         \
                if(i + 1 < value.size()) out << " ";                             \
            }                                 \
            out << std::endl;                                          \
         }\


    DECLARE_DATA(byte, char);

    DECLARE_DATA(ascii, std::string);

    DECLARE_DATA(asciiz, std::string);

    DECLARE_DATA(word, int32_t);

    DECLARE_DATA(hword, int64_t);

    DECLARE_DATA(space, size_t);

    /*!
     * The Module class. Module is the toplevel structure for code generation.
     * It contains the global data and function definitions.
     */
    struct Module {
        /*!
         * Global data section. Shared by all functions.
         */
        std::vector<std::shared_ptr<Data>> global_data_section;
        /*!
         * Functions defined in the module.
         */
        std::vector<std::shared_ptr<Function>> functions;
        /*!
         * Function prototypes.
         */
        std::vector<std::shared_ptr<Function>> externs;
        std::string name;

        /*!
         * Module constructor.
         * @param name name of the module.
         */
        explicit Module(std::string name);

        /*!
         * Code generation.
         * @param out output stream.
         */
        void output(std::ostream &out) const;

        /*!
         * Create a new function.
         * @param fname function name.
         * @param argc function argument number.
         * @return a new function instance.
         */
        std::shared_ptr<Function> create_function(std::string fname, size_t argc);

        /*!
         * Create a new extern function refernece.
         * @param fname function name.
         * @param argc function argument number.
         * @return a new function instance.
         */
        std::shared_ptr<Function> create_extern(std::string fname, size_t argc);

        /*!
         * Allocate memory and registers for all defined functions.
         */
        void finalize() {
            for (auto &i : functions) {
                i->color();
                i->scan_overlap();
                i->handle_alloca();
            }
        }

        /*!
         * Factory function to create a new data section.
         * @tparam Type data class type.
         * @tparam Args data creation arguments.
         * @param read_only data section read-write property.
         * @param args data creation arguments.
         * @return a new data section instance.
         */
        template<class Type, typename ...Args>
        std::shared_ptr<struct Data> create_data(bool read_only, Args &&... args) {
            auto data = Data::create<Type>(read_only, std::forward<Args>(args)...);
            global_data_section.push_back(data);
            return data;
        }

    };
};
#endif //BACKEND_VIRTUAL_MIPS_H
