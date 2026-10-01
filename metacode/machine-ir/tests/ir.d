module ir;

import std.stdio : writeln;
import machineir;

void main() {
    auto e = parseSExpr("(set rd (add rs1 rs2))");
    assert(e.toString() == "(set rd (add rs1 rs2))");

    auto f = new MachineFunction("test");
    auto b0 = f.entry();
    auto b1 = f.newBlock("exit");
    auto x = f.newVReg("GPR", 64);
    auto y = f.newVReg("GPR", 64);
    auto z = f.newVReg("GPR", 64);
    b0.append(MachineInstruction(
        Opcode.generic("add"),
        [Operand.reg(x), Operand.reg(y)],
        [Operand.reg(z)]
    ));
    f.connect(b0, b1);
    string error;
    assert(verify(f, error), error);
    writeln("machine-ir: ok");
}
