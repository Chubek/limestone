module negative;
import machineir;
import std.exception : assertThrown;

unittest {
    assertThrown!ParseError(parseISA("arch a { x = 1; x = 2; }"));
    assertThrown!ParseError(parseISA("arch a { x = [1,; }"));
    assertThrown!ParseError(parseISA("arch a { x = foo(1; }"));
    assertThrown!ParseError(buildMachineSpec(parseISA("arch a { word_size = invalid; }")));
    assertThrown!ParseError(buildMachineSpec(parseISA("arch a {} regclass G { r0(0)=0, }")));
    assertThrown!ParseError(buildMachineSpec(parseISA("arch a {} arch b {}")));
    assertThrown!SemanticError(parseSExpr("(add 1"));
    assertThrown!SemanticError(parseSExpr("x y"));
    auto spec=buildMachineSpec(parseISA("arch unknown {} op x { semantics = (add a b); tooling = { future = { values = [1,2]; }; }; }"));
    assert(spec.kind==MachineKind.unknown&&spec.executionDomain==ExecutionDomain.unknown);
    assert(spec.instructions["x"].semantics=="(add a b)");
    assert(spec.instructions["x"].tooling[0].values["values"]=="[1,2]");
    assert(spec.raw.length==2);
}
unittest {
    string error;
    assert(!verify(null,error));
    auto f=new MachineFunction("verification");auto block=f.entry();auto x=f.newVReg("G",64);
    auto call=MachineInstruction(Opcode.generic("call"));call.effects.control=ControlFlowKind.call;
    call.effects.implicitDefs=["flags"];block.append(call);
    block.append(MachineInstruction(Opcode.generic("copy"),[Operand.reg(x)],[Operand.reg(x)]));
    assert(verify(f,error),error);assert(useDef(call).defs[0].name=="flags");
    block.instructions[1].operands=[Operand.reg(RegisterRef.vreg("unknown","G",64))];assert(!verify(f,error));
    block.instructions[1].operands=[Operand.reg(x)];
    auto ret=MachineInstruction(Opcode.generic("ret"));ret.effects.control=ControlFlowKind.return_;
    block.append(ret);block.append(call);assert(!verify(f,error));block.instructions.length--;
    auto exit=f.newBlock("exit");f.connect(block,exit);f.connect(block,exit);assert(exit.predecessors.length==1);
    assert(verify(f,error),error);exit.predecessors=null;assert(!verify(f,error));recomputePredecessors(f);assert(verify(f,error),error);
    f.blocks~=block;assert(!verify(f,error));
}
