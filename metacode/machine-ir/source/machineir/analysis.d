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
    return ud;
}

bool isTerminator(const(MachineInstruction) i) {
    return i.effects.control != ControlFlowKind.fallthrough;
}

void recomputePredecessors(MachineFunction f) {
    foreach (b; f.blocks) b.predecessors = null;
    foreach (b; f.blocks)
        foreach (s; b.successors)
            foreach (t; f.blocks)
                if (t.id == s) { t.predecessors ~= b.id; break; }
}

bool verify(MachineFunction f, out string error) {
    if (!f.blocks.length) { error="function has no blocks"; return false; }
    foreach (b; f.blocks) {
        bool seenTerm;
        foreach (i, insn; b.instructions) {
            if (seenTerm) { error="instruction follows terminator in block " ~ b.label; return false; }
            if (isTerminator(insn)) seenTerm=true;
        }
        foreach (s; b.successors) {
            bool found=false; foreach (x; f.blocks) if (x.id==s) found=true;
            if (!found) { error="invalid successor"; return false; }
        }
    }
    return true;
}
