module negative;
import machineir;
import std.exception : assertThrown;

unittest {
    auto strings=parseISA(`arch strings { value="\u0000\b\f\/\uD83D\uDE00"; }`);
    auto value=strings.declarations[0].fields[0].value;
    assert(value.text=="\0\b\f/\xf0\x9f\x98\x80");
    auto roundtrip=parseISA("arch strings { value="~value.toString()~"; }");
    assert(roundtrip.declarations[0].fields[0].value.text==value.text);
    auto expression=parseSExpr(`(intrinsic "\u0000\b\f\/\uD83D\uDE00")`);
    assert(expression.children[1].text==value.text);
    assert(parseSExpr(expression.toString()).children[1].text==value.text);
    auto semanticDocument=parseISA(`arch strings {} op quoted { semantics=(intrinsic "\u0000\b\f\/\uD83D\uDE00"); }`);
    auto semanticText=semanticDocument.declarations[1].fields[0].value.text;
    assert(parseSExpr(semanticText).children[1].text==value.text);
    foreach(invalid;[`arch strings { value="\q"; }`,`arch strings { value="\uD800"; }`,`arch strings { value="\uDC00"; }`,`arch strings { value="\uZZZZ"; }`,"arch strings { value=\"line\nbreak\"; }","arch strings { value=\"\xff\"; }"])
        assertThrown!ParseError(parseISA(invalid));
    foreach(invalid;[`(intrinsic "\q")`,`(intrinsic "\uD800")`,`(intrinsic "\uDC00")`,"(intrinsic \"line\nbreak\")","(intrinsic \"\xff\")"])
        assertThrown!SemanticError(parseSExpr(invalid));
}

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
    assert(!verify(f,error)); // A return cannot declare an outgoing CFG edge.
    block.instructions[$-1].effects.control=ControlFlowKind.unconditionalBranch;
    block.instructions[$-1].operands=[Operand.block(exit.id)];
    assert(verify(f,error),error);exit.predecessors=null;assert(!verify(f,error));recomputePredecessors(f);assert(verify(f,error),error);
    f.blocks~=block;assert(!verify(f,error));
}
