# MachineIR

MachineIR is an architecture-neutral machine intermediate representation for the
machines described by `../infobank/isa/*.isa`.

It deliberately separates:

* **machine description** — registers, register classes, aliases, encodings and
  target properties;
* **machine operations** — operands, explicit/implicit effects, memory/control
  effects, scheduling and allocation constraints;
* **semantic IR** — a small S-expression representation of instruction meaning;
* **program IR** — values, virtual/physical registers, memory operands,
  instructions, basic blocks, CFGs and functions;
* **backend contracts** — instruction selection, scheduling, register
  allocation, disassembly, reverse engineering and binary translation.

The `.isa` loader is intentionally lossless at the generic-value level. Unknown
metadata is retained in `MachineSpec.raw`, so adding a field to the Infobank
does not require changing MachineIR immediately.

## Layout

* `source/machineir/package.d` — public API
* `source/machineir/ir.d` — program and machine IR
* `source/machineir/isa.d` — normalized ISA model
* `source/machineir/parser.d` — parser for the Infobank `.isa` syntax
* `source/machineir/semantics.d` — semantic S-expression parser/printer
* `source/machineir/builder.d` — conversion from parsed `.isa` to `MachineSpec`
* `source/machineir/analysis.d` — use/def, CFG and basic machine analyses
* `source/machineir/serialize.d` — deterministic textual serialization
* `tests/` — loader and IR tests

## Example

```d
import machineir;

auto isa = loadISA("infobank/isa/riscv64.isa");
assert(isa.arch == "riscv64");
assert(isa.registerClasses["GPR"].registers.length == 32);

auto f = new MachineFunction("add");
auto bb = f.entry();
auto x = f.newVReg("GPR", 64);
auto y = f.newVReg("GPR", 64);
auto z = f.newVReg("GPR", 64);
bb.append(MachineInstruction(
    Opcode.generic("add"),
    [Operand.reg(x), Operand.reg(y)],
    [Operand.reg(z)]
));
```

MachineIR does not pretend that all machines have the same execution model.
`MachineKind`, `ExecutionDomain`, register classes, address spaces, resource
classes and effect sets are explicit and extensible.
