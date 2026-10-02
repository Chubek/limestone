module machineir.ir;

import std.string : format;

enum MachineKind { mpu, mcu, gpu, virtualMachine, bytecode, dsp, accelerator, unknown }
enum ExecutionDomain { nativeMachineCode, bytecode, virtualISA, shaderIR, interpreter, jit, unknown }
enum Endianness { little, big, bi, unknown }
enum RegKind { virtualRegister, physicalRegister, specialRegister }

struct RegisterRef {
    RegKind kind;
    string name;
    string className;
    uint number;
    uint width;

    static RegisterRef vreg(string n, string cls, uint w=0) {
        return RegisterRef(RegKind.virtualRegister,n,cls,0,w);
    }
    static RegisterRef preg(string n, string cls, uint number=0, uint w=0) {
        return RegisterRef(RegKind.physicalRegister,n,cls,number,w);
    }
}

struct AddressSpace {
    uint id;
    string name;
    uint pointerWidth;
    string properties;
}

struct MemoryOperand {
    uint addressSpace;
    uint width;
    bool isLoad;
    bool isStore;
    bool volatileAccess;
    bool atomic;
    string ordering;
    string alignment;
    string addressExpression;
    // Symbolic address spaces and alias facts remain valid before target layout.
    string addressSpaceName;
    uint[] aliasSets;
    uint size;
}

struct Operand {
    enum Kind { reg, immediate, memory, block, symbol, literal }
    Kind kind;
    string text;
    long integer;
    RegisterRef registerRef;
    MemoryOperand memory;
    uint blockId;

    static Operand reg(RegisterRef r) {
        Operand o; o.kind=Kind.reg; o.registerRef=r; return o;
    }
    static Operand imm(long x) {
        Operand o; o.kind=Kind.immediate; o.integer=x; return o;
    }
    static Operand symbol(string s) {
        Operand o; o.kind=Kind.symbol; o.text=s; return o;
    }
    static Operand block(uint id) {
        Operand o; o.kind=Kind.block; o.blockId=id; return o;
    }
}

struct Opcode {
    string name;
    string encoding;
    string semanticClass;
    bool pseudo;
    static Opcode generic(string n) { return Opcode(n,"","",false); }
}

enum MemoryEffect { none, read, write, readWrite, unknown }
enum ControlFlowKind { fallthrough, conditionalBranch, unconditionalBranch, call,
                       return_, indirectBranch, trap, syscall, barrier, unknown }

struct EffectSet {
    string[] uses;
    string[] defs;
    string[] implicitUses;
    string[] implicitDefs;
    string[] flagsRead;
    string[] flagsWritten;
    MemoryEffect memory = MemoryEffect.none;
    ControlFlowKind control = ControlFlowKind.fallthrough;
    bool mayTrap;
    bool atomic;
    bool serializing;
    bool privileged;
    MemoryOperand[] memoryAccesses;
}

struct MachineInstruction {
    Opcode opcode;
    Operand[] operands;
    Operand[] results;
    EffectSet effects;
    string semantic;
    string[] constraints;
    string sourceName;

    this(Opcode op, Operand[] ops=[], Operand[] defs=[]) {
        opcode=op; operands=ops; results=defs;
    }
}

class MachineBasicBlock {
    uint id;
    string label;
    MachineInstruction[] instructions;
    uint[] successors;
    uint[] predecessors;
    // Explicit values exported beyond this region's CFG boundary.
    RegisterRef[] liveOut;

    this(uint id, string label="") { this.id=id; this.label=label; }
    void append(MachineInstruction i) { instructions ~= i; }
    void addSuccessor(uint id_) {
        foreach (x; successors) if (x == id_) return;
        successors ~= id_;
    }
}

class MachineFunction {
    string name;
    MachineBasicBlock[] blocks;
    private uint nextBlock;
    private uint nextVReg;
    RegisterRef[] virtualRegisters;

    this(string name) { this.name=name; }
    MachineBasicBlock entry() {
        if (!blocks.length) newBlock("entry");
        return blocks[0];
    }
    MachineBasicBlock newBlock(string label="") {
        auto b=new MachineBasicBlock(nextBlock++,label);
        blocks ~= b; return b;
    }
    /** Import a stable block identifier, keeping later IDs disjoint. */
    MachineBasicBlock importBlock(uint id,string label="") {
        foreach(b;blocks)if(b.id==id)throw new Exception("duplicate imported block");
        if(id==uint.max)throw new Exception("block identity exhausted");
        auto b=new MachineBasicBlock(id,label);blocks~=b;if(nextBlock<=id)nextBlock=id+1;return b;
    }
    RegisterRef newVReg(string cls, uint width) {
        auto r=RegisterRef.vreg(format("v%u",nextVReg++),cls,width);
        virtualRegisters ~= r; return r;
    }
    void connect(MachineBasicBlock a, MachineBasicBlock b) {
        a.addSuccessor(b.id);
        foreach(id;b.predecessors)if(id==a.id)return;
        b.predecessors ~= a.id;
    }
}

struct Resource {
    string name;
    uint units = 1;
}

struct MachineConfig {
    string name;
    MachineKind kind = MachineKind.unknown;
    ExecutionDomain executionDomain = ExecutionDomain.unknown;
    uint wordSize;
    uint addressSize;
    Endianness endian = Endianness.unknown;
    uint alignment;
    string encodingModel;
    string executionModel;
    string registerModel;
    string[] extensions;
    string[] operationClasses;
    Resource[] resources;
}

struct MachinePipeline {
    string[] stages;
    string[] resources;
    uint issueWidth;
    uint retireWidth;
}

struct MachineModule {
    string name;
    MachineConfig config;
    MachineFunction[] functions;
}
