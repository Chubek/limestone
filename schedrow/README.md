# Schedrow

Schedrow separates scheduling IL from its machine model and algorithms. Link
`Limestone::schedrow` for `schedrow.hpp`, `Limestone::schedrow_text` for `text.hpp`,
or `Limestone::il` for opaque C handles in `limestone/il.h`.

## Scheduling and verification

```cpp
auto schedule = limestone::schedrow::schedule(region, model);
if (!schedule) return schedule.error();
auto checked = limestone::schedrow::verify(region, model, schedule.value());
```

`validate_region` checks structure/effects independently of timing.
`dependencies` augments explicit edges with virtual and architectural register
hazards, memory alias/order constraints, barriers, calls, and trap/terminator
ordering. `scheduler_only` distinguishes artificial edges from semantic edges;
iteration distance is retained. `verify_order` validates sequential emission,
including zero-latency dependencies and declared block layout.
`verify` also checks issue-vector dependency order and group adjacency, so
issue assignments remain consistent with sequential emission. Modulo verification
checks intra-iteration issue order separately from loop-carried dependencies.

The list scheduler supports issue width/slots, alternative execution resources,
fractional quantities, multi-cycle occupancy, reservation offsets, explicit
priorities, and optional critical-path priority. Opcode throughput is preserved
as metadata; hard issue-rate limits are expressed through reservations and issue
width/slots. Result latency overrides are keyed by SSA value in
`Instruction::result_latency`.
Architectural IDs have the distinct `implicit_result_latency` map. Neither map
changes the instruction's default latency for unrelated results.

CFG scheduling handles each block independently, with block-local cycles and
resources. Declaration order is code layout, entry comes first, and the second
target of a conditional branch is fallthrough. Cross-block motion is an adapter.
CFG verification checks the complete issue vector against that layout and
sequential dependencies, including interleaved or reordered blocks.
After physical allocation, use `regtl::allocated_dependencies` from
`Limestone::regtl_schedrow` before final scheduling.

`schedule_modulo(region, model, options)` searches a bounded periodic schedule.
`verify_modulo` checks resources, slots, groups and loop-carried latency for
the supplied initiation interval. This result needs loop expansion before it is
a sequential executable region.

## Textual IL

```text
machine_model fixture {
  issue { width = 2; }
  resources { ALU = 1; }
  timing { add { latency = 3; resources = [{resource = ALU; duration = 2;}]; } }
}
region test {
  instruction %1 { opcode = add; use %input; def %value; }
  instruction %2 { opcode = ret; latency = 0; use %value; control_flow = return; }
}
```

`load_schedrow` returns regions and a separate machine model. Each instruction
needs explicit latency or opcode timing. Numeric and named identities are stable
within their own instruction/virtual/physical domains. Metadata is retained by
canonical `print_schedrow` serialization. CFGs use region `blocks` and `entry`
metadata plus instruction block/control targets.

```sh
build/limestone-cli --schedule-il tests/fixtures/scheduling.schedrow
# A loop document with explicit distance edges can use --modulo N.
```

The implemented memory contract includes read/write/volatile/atomic, ordering,
address space, alias sets, size, and alignment. Speculation/motion classifications
beyond the implemented flags, register banks, and ranged/operand-to-operand
latency require explicit adapters and are rejected
by textual loading. ISA ingestion does not manufacture a microarchitecture model.

## Groups, bundles and fusion

`Region::groups` holds stable identities and ordered member lists. The supported
contracts are:

| Kind | Hard scheduling contract |
| --- | --- |
| `ordered` | Listed emission order; other instructions may intervene |
| `adjacent`, `atomic` | Contiguous emission in listed order, with cycles chosen legally |
| `same_cycle` | Listed order and equal cycles; dependent zero-latency instructions may intervene |
| `bundle` | Same cycle and contiguous listed emission, with optional width/slot constraints |
| `fusion` | Contiguous listed emission plus an explicit pattern name and benefit hint |
| `pair` | Exactly two adjacent members in listed order |

Atomic groups are indivisible scheduler units; memory atomicity stays in the
instruction's effect contract. Fusion benefits affect priority without changing
resources, timing, or semantics. All groups stay within a block. Non-ordered
groups have disjoint membership; ordered groups may overlap them. Bundles jointly
solve resource alternatives and slots. Intervening dependency paths in a
same-cycle group are co-issued, or rejected when positive latency, capacity, or
adjacency makes that impossible. List, CFG, modulo, sequential-order and exchange
verifiers enforce these contracts.

```text
machine_model dual { issue { width = 2; } }
region example {
  bundle %4 {
    issue { width = 2; slots = [0, 1]; }
    instruction %9 { opcode = A; latency = 0; issue { slots = [1]; } }
    instruction %2 { opcode = B; latency = 0; issue { slots = [0]; } }
  }
}
```

Region `groups = [{id=4; kind=adjacent; members=[%9,%2];}]` and matching instruction
`group = {id=4; kind=pair;}` annotations are also accepted. Canonical printing uses
region groups. `limestone_schedule_group*` and Python `Schedule.groups` expose
owning-result inspection. The pipeline's `grouping_adapter` resolves target groups
after selection; spill materialization places transfers outside protected units.
`tests/fixtures/grouping.schedrow` exercises joint ports/slots and fusion.
