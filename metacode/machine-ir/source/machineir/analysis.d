module machineir.analysis;

import machineir.ir;
import std.algorithm : sort;
import std.array : array;

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
    foreach(name;insn.effects.uses)
        ud.uses~=RegisterRef(RegKind.specialRegister,name,"effect",0,0);
    foreach(name;insn.effects.defs)
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

bool verify(const(MachineFunction) f, out string error) {
    if(f is null){error="null function";return false;}
    if (!f.blocks.length) { error="function has no blocks"; return false; }
    size_t[uint] blocks; RegisterRef[string] registers;
    foreach(k,b;f.blocks) {
        if(b is null){error="null basic block";return false;}
        if(b.id in blocks){error="duplicate block identity";return false;}blocks[b.id]=k;
    }
    foreach(r;f.virtualRegisters) {
        if(r.kind!=RegKind.virtualRegister||!r.name.length||r.name in registers){error="invalid virtual register declaration";return false;}
        registers[r.name]=r;
    }
    foreach (b; f.blocks) {
        bool seenTerm;
        foreach (i, insn; b.instructions) {
            if(!insn.opcode.name.length){error="empty instruction opcode";return false;}
            if (seenTerm) { error="instruction follows terminator in block " ~ b.label; return false; }
            if (isTerminator(insn)) seenTerm=true;
            if(insn.effects.control<ControlFlowKind.fallthrough||insn.effects.control>ControlFlowKind.unknown||insn.effects.memory<MemoryEffect.none||insn.effects.memory>MemoryEffect.unknown){error="invalid instruction effect classification";return false;}
            foreach(m;insn.effects.memoryAccesses) {
                if(m.alignment.length){uint alignment;try{import std.conv : to;alignment=to!uint(m.alignment);}catch(Exception){error="invalid memory alignment";return false;}if(alignment&&(alignment&(alignment-1))){error="memory alignment is not a power of two";return false;}}
                if(m.ordering.length&&m.ordering!="relaxed"&&m.ordering!="acquire"&&m.ordering!="release"&&m.ordering!="acq_rel"&&m.ordering!="seq_cst"){error="invalid memory ordering";return false;}
            }
            foreach(o;insn.operands~insn.results) {
                if(o.kind==Operand.Kind.reg) {
                    auto r=o.registerRef;
                    if(!r.name.length){error="invalid register operand";return false;}
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
            if(isTerminator(insn)) {
                bool[uint] targets,expected;foreach(o;insn.operands)if(o.kind==Operand.Kind.block){if(o.blockId in targets){error="duplicate terminator target";return false;}targets[o.blockId]=true;}
                foreach(s;b.successors)expected[s]=true;
                if(targets!=expected){error="terminator targets disagree with successors";return false;}
                if((insn.effects.control==ControlFlowKind.unconditionalBranch&&targets.length!=1)||(insn.effects.control==ControlFlowKind.conditionalBranch&&targets.length!=2)||((insn.effects.control==ControlFlowKind.return_||insn.effects.control==ControlFlowKind.trap)&&targets.length)){error="invalid terminator target count";return false;}
            }
        }
        foreach(r;b.liveOut){auto declared=r.name in registers;if(declared is null||*declared!=r){error="invalid live-out register";return false;}}
        bool[uint] successors,predecessors;
        foreach (s; b.successors) {
            auto target=s in blocks;
            if(target is null||s in successors){error="invalid or duplicate successor";return false;}
            successors[s]=true; bool reciprocal=false;foreach(p;f.blocks[*target].predecessors)if(p==b.id)reciprocal=true;
            if(!reciprocal){error="nonreciprocal CFG edge";return false;}
        }
        foreach(p;b.predecessors) {
            auto source=p in blocks;
            if(source is null||p in predecessors){error="invalid or duplicate predecessor";return false;}
            predecessors[p]=true;bool reciprocal=false;foreach(s;f.blocks[*source].successors)if(s==b.id)reciprocal=true;
            if(!reciprocal){error="nonreciprocal predecessor edge";return false;}
        }
    }
    error="";
    return true;
}

/** Analyses reject structurally invalid functions before returning results. */
class AnalysisError : Exception {
    this(string message) { super(message); }
}

struct BlockLiveness {
    RegisterRef[] liveIn, liveOut;
    RegisterRef[][] before, after;
}
struct Interference {
    RegisterRef first, second;
}
struct Liveness {
    BlockLiveness[uint] blocks;
    Interference[] interference;
}
private alias RegisterSet = bool[RegisterRef];
private bool lessRegister(RegisterRef a, RegisterRef b) {
    if(a.kind != b.kind) return a.kind < b.kind;
    if(a.className != b.className) return a.className < b.className;
    if(a.name != b.name) return a.name < b.name;
    if(a.number != b.number) return a.number < b.number;
    return a.width < b.width;
}
private RegisterRef[] sortedRegisters(RegisterSet set) {
    return set.keys.sort!lessRegister.array;
}

/** Backwards fixed-point liveness includes loops, disconnected blocks and effects.
 *  Registers used before a definition are live-ins; SSA is not assumed.
 */
Liveness analyzeLiveness(MachineFunction f) {
    string error;
    if(!verify(f,error)) throw new AnalysisError(error);
    RegisterSet[uint] uses, defs, inputs, outputs;
    foreach(b;f.blocks) {uses[b.id]=null;defs[b.id]=null;inputs[b.id]=null;outputs[b.id]=null;}
    foreach(b;f.blocks) foreach(insn;b.instructions) {
        auto ud=useDef(insn);
        foreach(r;ud.uses) if(!(r in defs[b.id])) uses[b.id][r]=true;
        foreach(r;ud.defs) defs[b.id][r]=true;
    }
    bool changed=true;
    while(changed) {
        changed=false;
        foreach_reverse(b;f.blocks) {
            RegisterSet nextOut;foreach(r;b.liveOut)nextOut[r]=true;
            foreach(s;b.successors) foreach(r;inputs[s].keys) nextOut[r]=true;
            auto nextIn=uses[b.id].dup;
            foreach(r;nextOut.keys) if(!(r in defs[b.id])) nextIn[r]=true;
            if(nextIn != inputs[b.id] || nextOut != outputs[b.id]) {
                inputs[b.id]=nextIn;outputs[b.id]=nextOut;changed=true;
            }
        }
    }
    Liveness result;
    bool[RegisterRef][RegisterRef] edges;
    void clique(RegisterSet set) {
        auto regs=sortedRegisters(set);
        foreach(i,a;regs) foreach(b;regs[i+1..$]) edges[a][b]=true;
    }
    foreach(b;f.blocks) {
        BlockLiveness block;
        block.liveIn=sortedRegisters(inputs[b.id]);block.liveOut=sortedRegisters(outputs[b.id]);
        block.before.length=b.instructions.length;block.after.length=b.instructions.length;
        auto live=outputs[b.id].dup;clique(live);
        foreach_reverse(k,insn;b.instructions) {
            block.after[k]=sortedRegisters(live);
            auto ud=useDef(insn);auto occupied=live.dup;
            foreach(r;ud.defs) occupied[r]=true;
            clique(occupied);
            foreach(r;ud.defs) live.remove(r);
            foreach(r;ud.uses) live[r]=true;
            clique(live);block.before[k]=sortedRegisters(live);
        }
        result.blocks[b.id]=block;
    }
    foreach(a;edges.keys.sort!lessRegister) foreach(b;edges[a].keys.sort!lessRegister)
        result.interference~=Interference(a,b);
    return result;
}

struct Dominance {
    uint entry;
    uint[][uint] dominators;
    uint[uint] immediate;
    uint[] unreachable;
}
/** Dominance of the reachable CFG, rooted at the function's first block. */
Dominance analyzeDominance(MachineFunction f) {
    string error;
    if(!verify(f,error)) throw new AnalysisError(error);
    MachineBasicBlock[uint] blocks;
    foreach(b;f.blocks) blocks[b.id]=b;
    bool[uint] reachable;uint[] work=[f.blocks[0].id];
    while(work.length) {
        auto id=work[$-1];work.length--;
        if(id in reachable) continue;
        reachable[id]=true;work~=blocks[id].successors;
    }
    Dominance result;result.entry=f.blocks[0].id;
    bool[uint][uint] sets;
    foreach(id;blocks.keys.sort) {
        if(!(id in reachable)) {result.unreachable~=id;continue;}
        sets[id]=id==result.entry?([id:true]):reachable.dup;
    }
    bool changed=true;
    while(changed) {
        changed=false;
        foreach(id;reachable.keys.sort) {
            if(id==result.entry) continue;
            bool[uint] intersection;bool first=true;
            foreach(p;blocks[id].predecessors) if(p in reachable) {
                if(first) {intersection=sets[p].dup;first=false;}
                else foreach(d;intersection.keys) if(!(d in sets[p])) intersection.remove(d);
            }
            intersection[id]=true;
            if(intersection!=sets[id]) {sets[id]=intersection;changed=true;}
        }
    }
    foreach(id;reachable.keys.sort) {
        result.dominators[id]=sets[id].keys.sort.array;
        if(id==result.entry) continue;
        uint best;size_t depth=0;
        foreach(d;result.dominators[id]) if(d!=id&&sets[d].length>depth) {best=d;depth=sets[d].length;}
        result.immediate[id]=best;
    }
    return result;
}
bool dominates(const ref Dominance result,uint a,uint b) {
    auto set=b in result.dominators;if(set is null)return false;
    foreach(id;*set)if(id==a)return true;return false;
}
