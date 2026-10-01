# ISA Tooling Augmentation

Version 1 of the tooling augmentation adds a common, declarative metadata layer to the ISA descriptions.

## Goals

The additions are intentionally useful to several independent consumers:

- instruction scheduling
- register allocation
- instruction selection / legalization
- virtual-machine and code-generator construction
- disassembly
- binary lifting and reverse engineering

## Architecture-level `tooling`

Each architecture ISA now has a top-level `tooling` block containing six contracts:

- `scheduling`
- `register_allocation`
- `instruction_selection`
- `vmm`
- `disassembler`
- `reverse_engineering`

These describe the *shape* of the information required by a backend. Values that cannot be established from an ISA inventory are explicitly `unknown`, `target_dependent`, `abi_dependent`, or `architecture_specific` rather than being fabricated.

## Per-instruction `op.tooling`

Every real `op` receives:

- `dataflow`: explicit operands, derived uses/defs, flag effects, memory effect, and control-flow category.
- `scheduling`: latency/resource/throughput placeholders and serialization information.
- `register_allocation`: operand register-class constraints, fixed-register context, tied/early-clobber slots, spillability.
- `instruction_selection`: semantic pattern, opcode class, commutativity, immediate/addressing hints, pseudo status.
- `vmm`: terminator, control-flow kind, memory effect, trapping/atomic/serialization properties.
- `disassembly`: mnemonic, syntax, encoding reference, encoding width/opcode, operand order.
- `reverse_engineering`: semantic pattern, control-flow and memory classification, branch-target hints, semantic confidence.

## Important accuracy rule

This augmentation does **not** invent microarchitecture-specific scheduling numbers. Exact latency, reciprocal throughput, port/resource usage, bypass paths, issue/retire widths, and similar values belong in a microarchitecture profile and should be populated from authoritative processor documentation or measured data.

The existing ISA semantics remain the source of truth for instruction meaning. The augmentation is a backend-oriented index over those semantics and encodings.

## Grammar

`isa.peg` now accepts the top-level `tooling { ... }` declaration. Existing declarations remain unchanged.
