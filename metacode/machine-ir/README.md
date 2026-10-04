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
Quoted metadata uses JSON string escapes, including Unicode surrogate pairs.
Malformed escapes, invalid UTF-8, and unescaped control characters are rejected;
printing preserves decoded strings, including embedded control characters.

## Layout

* `source/machineir/package.d` — public API
* `source/machineir/ir.d` — program and machine IR
* `source/machineir/isa.d` — normalized ISA model
* `source/machineir/parser.d` — parser for the Infobank `.isa` syntax
* `source/machineir/semantics.d` — semantic S-expression parser/printer
* `source/machineir/builder.d` — conversion from parsed `.isa` to `MachineSpec`
* `source/machineir/analysis.d` — verification, use/def, dominance and CFG liveness
* `source/machineir/serialize.d` — deterministic textual serialization
* `source/machineir/exchange.d` — owning, validated C++/D region exchange
* `bridge.hpp` / `bridge.cpp` — C++ exchange adapter (`Limestone::machineir_bridge`)
* `tests/` — loader and IR tests

## Example

```d
import machineir;

auto isa = loadISA("../infobank/isa/riscv64.isa"); // from the D package directory
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

## Analyses and C++/D exchange

`verify` checks register/operand/effect classifications, declared identities,
terminators, block operands, CFG reciprocity, memory alignment, and parallel
transfers. `recomputePredecessors`, `analyzeDominance`, `dominates`,
`analyzeLiveness`, and use/def helpers operate on the architecture-neutral program.
Physical and virtual identities remain distinct; semantics are parsed without
evaluation.

The `limestone.machineir.region` schema exports a compiler region and its owning
selection/timing/allocation envelope. Version 1 handles simple regions; version 2
adds CFG/control targets, spill frames, and architectural-result latency.
Version 3 adds scheduling groups, fusion hints, and bundle issue constraints.
Groups may overlap; each contract is checked independently, including shared
members' adjacency, cycle and slot requirements.

```d
import machineir;
import std.file : readText, write;

auto exchange = readExchange(readText("region.json"));
auto function_ = exchange.program.functions[0];
auto live = analyzeLiveness(function_);
auto contract = exchange.instructionContract(0);
auto assigned = exchange.physicalRegister(1); // Nullable!uint
write("returned.json", writeExchange(exchange));
```

C++ exposes `Module::machine_ir_exchange` and `machineir_bridge::deserialize`;
C exposes `limestone_module_exchange`. Both exchange boundaries reject
duplicate/unknown fields, noncanonical keys, invalid SSA/dominance/availability,
malformed resources/slots, order/latency violations, and inconsistent CFG/frame
data. JSON is bounded to 4 MiB and 256 nesting levels. Throughput and resource
quantities are decimal strings. Complete resource/alias/allocation verification
still consumes target metadata through the subsystem verifiers.
Potentially aliasing atomic reads retain their source order even with relaxed
ordering, including when the envelope supplies no explicit memory dependency.

`writeExchange` preserves the envelope while admitting immediate, naming, and
source-provenance edits. Structural, opcode, dataflow, effect, and machine-config
changes need adapters. Timing/resource changes require clearing and recomputing
schedules. Frames are inspectable through `frameSize` and `spillFrameContract`.
Groups remain in the owning envelope, exposed through `groupCount` and
`groupContract`; both exchange boundaries check their order, adjacency, cycles,
and slot restrictions.
Issue vectors preserve intra-block dependency order and declared CFG layout,
including dependencies that issue in different cycles. Reversing dependent
co-issued instructions or interleaving/reversing block issue vectors is rejected.
The actual C++ -> D -> C++ CTest re-encodes and independently executes edited CFG
and spill programs; `dub test` runs the package's unit and Infobank-loader suites.
