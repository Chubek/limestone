module analysis;
import machineir;
import std.exception : assertThrown;

unittest {
    auto f=new MachineFunction("loop");auto entry=f.entry();auto loop=f.newBlock("loop");auto exit=f.newBlock("exit");auto unreachable=f.newBlock("unreachable");
    auto x=f.newVReg("G",64);auto y=f.newVReg("G",64);auto z=f.newVReg("G",64);
    entry.append(MachineInstruction(Opcode.generic("input"),[],[Operand.reg(x)]));
    loop.append(MachineInstruction(Opcode.generic("add"),[Operand.reg(x),Operand.reg(y)],[Operand.reg(z)]));
    exit.append(MachineInstruction(Opcode.generic("use"),[Operand.reg(z)]));
    f.connect(entry,loop);f.connect(loop,loop);f.connect(loop,exit);
    auto live=analyzeLiveness(f);
    assert(live.blocks[loop.id].liveIn==[x,y]);
    assert(live.blocks[loop.id].liveOut==[x,y,z]);
    assert(live.interference.length==3);
    auto dom=analyzeDominance(f);assert(dominates(dom,entry.id,exit.id));assert(dominates(dom,loop.id,exit.id));
    assert(dom.immediate[exit.id]==loop.id&&dom.unreachable==[unreachable.id]);
    assert(!dominates(dom,entry.id,unreachable.id));
    auto serialized=dump(f);auto saved=f.blocks.dup;f.blocks=[exit,loop,entry,unreachable];assert(dump(f)==serialized);f.blocks=saved;
    assertThrown!AnalysisError(analyzeLiveness(null));
    loop.successors~=[100];assertThrown!AnalysisError(analyzeDominance(f));
}

unittest {
    auto f=new MachineFunction("diamond");auto entry=f.entry();auto a=f.newBlock("left");auto b=f.newBlock("right");auto merge=f.newBlock("merge");
    f.connect(entry,a);f.connect(entry,b);f.connect(a,merge);f.connect(b,merge);
    auto dom=analyzeDominance(f);assert(dom.immediate[merge.id]==entry.id);assert(!dominates(dom,a.id,merge.id));
    auto flags=MachineInstruction(Opcode.generic("compare"));flags.effects.flagsWritten=["ZF"];a.append(flags);
    auto branch=MachineInstruction(Opcode.generic("branch"));branch.effects.flagsRead=["ZF"];merge.append(branch);
    auto live=analyzeLiveness(f);assert(live.blocks[b.id].liveOut[0].name=="ZF");assert(live.blocks[a.id].liveIn.length==0);
}
