module machineir.analysis;

import machineir.ir;

struct UseDef {
    RegisterRef[] uses;
    RegisterRef[] defs;
}

UseDef useDef(const(MachineInstruction) insn) {
    UseDef ud;
    foreach (o; insn.operands)
        if (o.kind == Operand.Kind.reg) ud.uses ~= o.registerRef;
    foreach (o; insn.results)
        if (o.kind == Operand.Kind.reg) ud.defs ~= o.registerRef;
    foreach(name;insn.effects.implicitUses~insn.effects.flagsRead)
        ud.uses~=RegisterRef(RegKind.specialRegister,name,"effect",0,0);
    foreach(name;insn.effects.implicitDefs~insn.effects.flagsWritten)
        ud.defs~=RegisterRef(RegKind.specialRegister,name,"effect",0,0);
    return ud;
}

bool isTerminator(const(MachineInstruction) i) {
    return i.effects.control==ControlFlowKind.conditionalBranch||i.effects.control==ControlFlowKind.unconditionalBranch||
        i.effects.control==ControlFlowKind.return_||i.effects.control==ControlFlowKind.indirectBranch||i.effects.control==ControlFlowKind.trap;
}

void recomputePredecessors(MachineFunction f) {
    foreach (b; f.blocks) b.predecessors = null;
    foreach (b; f.blocks)
        foreach (s; b.successors)
            foreach (t; f.blocks)
                if (t.id == s) { t.predecessors ~= b.id; break; }
}

bool verify(MachineFunction f, out string error) {
    if(f is null){error="null function";return false;}
    if (!f.blocks.length) { error="function has no blocks"; return false; }
    MachineBasicBlock[uint] blocks; RegisterRef[string] registers;
    foreach(b;f.blocks) {
        if(b is null){error="null basic block";return false;}
        if(b.id in blocks){error="duplicate block identity";return false;}blocks[b.id]=b;
    }
    foreach(r;f.virtualRegisters) {
        if(r.kind!=RegKind.virtualRegister||!r.name.length||!r.className.length||!r.width||r.name in registers){error="invalid virtual register declaration";return false;}
        registers[r.name]=r;
    }
    foreach (b; f.blocks) {
        bool seenTerm;
        foreach (i, insn; b.instructions) {
            if(!insn.opcode.name.length){error="empty instruction opcode";return false;}
            if (seenTerm) { error="instruction follows terminator in block " ~ b.label; return false; }
            if (isTerminator(insn)) seenTerm=true;
            foreach(o;insn.operands~insn.results) {
                if(o.kind==Operand.Kind.reg) {
                    auto r=o.registerRef;
                    if(!r.name.length||!r.className.length||!r.width){error="invalid register operand";return false;}
                    if(r.kind==RegKind.virtualRegister) {
                        auto declared=r.name in registers;
                        if(declared is null||*declared!=r){error="undeclared or inconsistent virtual register";return false;}
                    }
                } else if(o.kind==Operand.Kind.block) {
                    if(!(o.blockId in blocks)){error="invalid block operand";return false;}
                } else if(o.kind==Operand.Kind.memory) {
                    if(!o.memory.width||!o.memory.addressExpression.length||(!o.memory.isLoad&&!o.memory.isStore)){error="invalid memory operand";return false;}
                }
            }
        }
        bool[uint] successors,predecessors;
        foreach (s; b.successors) {
            auto target=s in blocks;
            if(target is null||s in successors){error="invalid or duplicate successor";return false;}
            successors[s]=true; bool reciprocal=false;foreach(p;(*target).predecessors)if(p==b.id)reciprocal=true;
            if(!reciprocal){error="nonreciprocal CFG edge";return false;}
        }
        foreach(p;b.predecessors) {
            auto source=p in blocks;
            if(source is null||p in predecessors){error="invalid or duplicate predecessor";return false;}
            predecessors[p]=true;bool reciprocal=false;foreach(s;(*source).successors)if(s==b.id)reciprocal=true;
            if(!reciprocal){error="nonreciprocal predecessor edge";return false;}
        }
    }
    error="";
    return true;
}
