# AGENTS.md: DomMEMTk Implementation Guide

This document defines architecture, design specifications, and implementation instructions for autonomous agents and engineers constructing **DomMEMTk**, a modern C++20 memory management toolkit (inspired by MMTk / Memory Management Toolkit) built leveraging the header-only domain-specific language engine `DomDSL.hpp`.

**Note**: DomDSL is in `/mnt/warble/domweave/domlibs/domdsl/DomDSL.hpp`.

---

## 1. Executive Overview & Design Philosophy

**DomMEMTk** is a high-performance, modular, composable Memory Management Toolkit in C++20. Like the Rust-based and Java-based MMTk frameworks, DomMEMTk decouples virtual machine / runtime semantics from memory management algorithms (GC algorithms, spaces, allocators, plan policies, and barrier implementations).

### Key Architectural Tenets
1. **Zero Runtime Overhead via CRTP & Concepts**: Elimination of virtual dispatch on hot allocation/barrier paths using compile-time composition via `dsl::DSL<Derived, Features...>`.
2. **Explicit Fact vs Proposed Distinction**: Clear demarcation between verified constructs existing in `DomDSL.hpp` and newly proposed DomMEMTk architectural layers.
3. **Formal Verification & Type Safety**: Compile-time space boundaries, allocation semantics, and barrier pipelines.
4. **Modularity & Plan Composition**: Flexible GC plan assembly (MarkSweep, SemiSpace, Immix, Generational, MarkCompact, CMS) composed from orthogonal memory spaces.

---

## 2. Observed Facts in `DomDSL.hpp` (Source Reference Inventory)

All references below reflect the exact declarations and definitions in `/mnt/data/DomDSL.hpp` (total 3,582 lines, standard C++20):

* **Namespace & Core Base**:
  * `namespace dsl` (Line 423)
  * `dsl::DSL<Derived, Features...>` CRTP base class (Lines 435–455). Feature mixins are attached via multiple inheritance through `Features::template Mixin<Derived>`.
  * `dsl::FixedString<std::size_t N>` compile-time string parameter for NTTP (Lines 387–400).
* **Feature Tags & Mixin Engine**:
  * `dsl::Pipeline` (Lines 403, 578–596) providing `wrap()` and `operator|` via `dsl::pipe()` and `dsl::PipeStage` (Lines 533–573).
  * `dsl::Operators` (Lines 404, 612–627) providing `make_pred()` and `Predicate<F>` with `&`, `|`, `!` operators (Lines 469–528).
  * `dsl::PatternMatch` (Lines 405, 972–979) providing `dsl::match`, `dsl::when`, `dsl::otherwise` (Lines 762–959) and regex-like compile-time string match `dsl::pattern<FixedString>` (Lines 644–759).
  * `dsl::AST` (Lines 406, 1192–1218) and `dsl::ASTNode` (Lines 986–1119), `dsl::leaf<FixedString>` (Lines 1121–1154), `dsl::node<FixedString>` (Lines 1157–1183).
  * `dsl::Rewrite` (Lines 407, 1391–1410) and `dsl::RewriteSet` / `dsl::rule<FixedString>` (Lines 1220–1389).
  * `dsl::ExprTemplates` (Lines 408, 1525–1568) evaluating binary/unary expressions lazily via `BinExpr` / `UnaryExpr` and `.eval()` (Lines 1416–1523).
  * `dsl::CustomLiterals` (Lines 409, 1734–1750) and `dsl::LiteralSet` / `dsl::lit<FixedString>` (Lines 1603–1732).
  * `dsl::Memoization` (Lines 410, 1759–1770) and `dsl::memoize` (Lines 1808–1868).
  * `dsl::LazyFeature` (Lines 411, 1772–1777) and `dsl::Lazy<T>` (Lines 1873–1910).
  * `dsl::Monadic` (Lines 412, 1779–1784) and `dsl::Maybe<T>` (Lines 1920–1979).
  * `dsl::ResultFeature` (Lines 413, 1786–1791) and `dsl::Result<T, E>` / `Ok()` / `Err()` (Lines 2012–2125).
  * `dsl::CombinatorParser` (Lines 414, 1793–1798), `dsl::ParsecInput` (Lines 2133–2177), `dsl::Parser<T>` (Lines 2229–2401), combinators `ch`, `satisfy`, `run_parser`, etc. (Lines 2403–2453).
  * `dsl::TaskPipeline` (Lines 415, 1800–1805), `dsl::TaskState` (Lines 2462–2466), `dsl::Task` (Lines 2471–2480), `dsl::TaskChain` / `operator|` / `run()` / `suspend()` (Lines 2484–2528).
  * `dsl::PEGDefinition`, `dsl::PEGRule`, `dsl::PEGMatch`, `dsl::PEGMatcher`, `dsl::PEGChannel` (Lines 2602–3555).
  * Utility Concepts: `dsl::HasFeature` (Line 3562), `dsl::Pipeable` (Line 3566), `dsl::ExprLike` (Line 3569), `dsl::HasLiterals` (Line 3574), `dsl::Rewritable` (Line 3580).

---

## 3. Proposed DomMEMTk Architecture

DomMEMTk bridges MMTk concepts to C++20 idioms using `DomDSL.hpp` feature mixins.

```
+-------------------------------------------------------------------------+
|                              DomMEMTk                                   |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  | User / VM Binding API: Plan, MutatorContext, CollectorContext     |  |
|  +-------------------------------------------------------------------+  |
|                                  |                                      |
|  +-------------------------------------------------------------------+  |
|  | Policies & Plans (MarkSweep, Immix, SemiSpace, GenCopy, CMS)     |  |
|  | (Configured via MatchTable, Literals, TaskPipeline)               |  |
|  +-------------------------------------------------------------------+  |
|         |                                              |                |
|  +-----------------------------+             +-----------------------+  |
|  | Spaces & Allocators         |             | Barrier Pipelines     |  |
|  | - BumpPointer (dsl::DSL)    |             | - dsl::Pipeline       |  |
|  | - FreeList / ImmixBlock     |             | - Read/Write Filters  |  |
|  | - LargeObjectSpace (LOS)    |             | - Card/SATB Enqueue   |  |
|  +-----------------------------+             +-----------------------+  |
|         |                                              |                |
|  +-------------------------------------------------------------------+  |
|  | GC Phases & Work Packet Scheduler (TaskPipeline & TaskState)      |  |
|  +-------------------------------------------------------------------+  |
|                                  |                                      |
|  +-------------------------------------------------------------------+  |
|  | Raw Memory & VM Mapping Layer (OS mmap, Address, Chunk/Page Map)  |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 4. Mapping MMTk Core Concepts to DomDSL Constructs

| MMTk Architectural Concept | Proposed DomMEMTk Component | DomDSL Mechanism & Source Line Reference |
| :--- | :--- | :--- |
| **GC Configuration & Units** | `MemorySizeDSL`, Heap Sizing | `dsl::CustomLiterals` (`LiteralSet`, `lit<FixedString>`, lines 1603–1750) |
| **Barrier Composition** | `WriteBarrierPipeline`, `SATB` | `dsl::Pipeline` (`dsl::pipe`, `PipeStage`, `wrap`, lines 533–596) |
| **Filtering & Space Validation** | Region checks, In-nursery checks | `dsl::Operators` (`dsl::predicate`, `Predicate<&, |, !>`, lines 469–528, 612–627) |
| **Object Tag & GC State Dispatch** | Header decoding, GC state transitions | `dsl::PatternMatch` (`dsl::match`, `dsl::when`, `dsl::otherwise`, lines 762–979) |
| **GC Work & Phase Scheduling** | Stop-The-World, RootScan, Marking, Sweep | `dsl::TaskPipeline` (`dsl::Task`, `TaskChain`, `TaskState`, lines 2462–2528) |
| **Allocation & Remset Operations** | Allocation result, OOM Handling | `dsl::ResultFeature` (`dsl::Result<T, E>`, `Ok`, `Err`, lines 2012–2125) |
| **GC Plan & Config Parsing** | CLI options, GC tuning script | `dsl::CombinatorParser` (lines 2229–2453) & PEG (lines 2602–3555) |
| **Heap Metrics & Accounting** | Live byte counting, fragmentation math | `dsl::ExprTemplates` (`BinExpr`, `UnaryExpr`, lines 1416–1568) |
| **Static Metadata Tree Optimization** | Space layout descriptor rewrites | `dsl::AST` & `dsl::Rewrite` (`ASTNode`, `rewrite_set`, lines 986–1410) |

---

## 5. Detailed Component Specifications & Code Guidance

### 5.1 Memory Units & Heap Sizing (CustomLiterals)
* **Goal**: Provide expressive, constexpr heap size definitions (e.g., `64_MB`, `4_GB`, `16_KB`).
* **Implementation pattern**:
```cpp
#include "DomDSL.hpp"
#include <cstdint>

namespace DomMEMTk {

struct MemoryUnitsDSL : dsl::DSL<MemoryUnitsDSL, dsl::CustomLiterals> {
  static constexpr auto literals = dsl::literal_set(
    dsl::lit<"_B">  ([](long double v){ return static_cast<std::size_t>(v); }),
    dsl::lit<"_KB"> ([](long double v){ return static_cast<std::size_t>(v * 1024.0L); }),
    dsl::lit<"_MB"> ([](long double v){ return static_cast<std::size_t>(v * 1024.0L * 1024.0L); }),
    dsl::lit<"_GB"> ([](long double v){ return static_cast<std::size_t>(v * 1024.0L * 1024.0L * 1024.0L); })
  );
};

inline namespace literals {
  constexpr std::size_t operator""_B (unsigned long long v) { return static_cast<std::size_t>(v); }
  constexpr std::size_t operator""_KB(unsigned long long v) { return static_cast<std::size_t>(v * 1024ULL); }
  constexpr std::size_t operator""_MB(unsigned long long v) { return static_cast<std::size_t>(v * 1024ULL * 1024ULL); }
  constexpr std::size_t operator""_GB(unsigned long long v) { return static_cast<std::size_t>(v * 1024ULL * 1024ULL * 1024ULL); }
}

} // namespace DomMEMTk
```

### 5.2 Address, ObjectReference & Predicate-Based Filters (Operators)
* **Goal**: Define type-safe address spaces and compose region predicates using `dsl::Operators`.
* **Implementation pattern**:
```cpp
namespace DomMEMTk {

using Address = std::uintptr_t;

struct AddressRange {
  Address start;
  Address end;
  constexpr bool contains(Address addr) const { return addr >= start && addr < end; }
};

struct SpaceFilterDSL : dsl::DSL<SpaceFilterDSL, dsl::Operators> {
  static auto in_nursery(AddressRange nursery) {
    return make_pred([nursery](Address addr) { return nursery.contains(addr); });
  }
  static auto in_los(AddressRange los) {
    return make_pred([los](Address addr) { return los.contains(addr); });
  }
  static auto is_non_null() {
    return make_pred([](Address addr) { return addr != 0; });
  }
};

// Predicate Composition:
// auto needs_remembered_set = SpaceFilterDSL::is_non_null() &
//                             SpaceFilterDSL::in_nursery(nursery_range) &
//                             !SpaceFilterDSL::in_los(los_range);

} // namespace DomMEMTk
```

### 5.3 Write/Read Barrier Pipelines (Pipeline)
* **Goal**: Construct zero-overhead barrier sequences using `dsl::Pipeline` (`operator|`).
* **Implementation pattern**:
```cpp
namespace DomMEMTk {

struct Slot {
  Address* slot_addr;
  Address target_obj;
};

struct BarrierDSL : dsl::DSL<BarrierDSL, dsl::Pipeline> {};

inline auto filter_cross_region(AddressRange src_region, AddressRange target_region) {
  return dsl::pipe([=](Slot s) -> std::optional<Slot> {
    if (src_region.contains(reinterpret_cast<Address>(s.slot_addr)) &&
        target_region.contains(s.target_obj)) {
      return s;
    }
    return std::nullopt;
  });
}

inline auto record_in_modbuf(std::vector<Slot>& modbuf) {
  return dsl::pipe([&modbuf](std::optional<Slot> s) {
    if (s.has_value()) {
      modbuf.push_back(*s);
    }
  });
}

// Composition:
// BarrierDSL b;
// b.wrap(Slot{slot, obj}) | filter_cross_region(mature_space, nursery_space) | record_in_modbuf(local_buf);

} // namespace DomMEMTk
```

### 5.4 Object Header & GC State Dispatch (PatternMatch)
* **Goal**: Match object states, forwarding pointers, or GC phase transitions at compile time without dynamic tables.
* **Implementation pattern**:
```cpp
namespace DomMEMTk {

enum class GCState : std::uint8_t {
  Unmarked = 0,
  Marked   = 1,
  Forwarded= 2,
  Pinned   = 3
};

struct TraceDispatcher : dsl::DSL<TraceDispatcher, dsl::PatternMatch> {
  static constexpr auto trace_object = dsl::match(
    dsl::when<GCState::Unmarked>([](Address obj) {
      // Mark object, push children to trace queue
      return GCState::Marked;
    }),
    dsl::when<GCState::Marked>([](Address) {
      // Already marked, no-op
      return GCState::Marked;
    }),
    dsl::when<GCState::Forwarded>([](Address) {
      // Return forwarding pointer
      return GCState::Forwarded;
    }),
    dsl::otherwise([](Address) {
      return GCState::Pinned;
    })
  );
};

} // namespace DomMEMTk
```

### 5.5 Phase & Work-Packet Scheduler (TaskPipeline)
* **Goal**: Express GC collection cycles (Stop-The-World, Prepare, RootScan, Closure, Sweep, Release) as composable task pipelines supporting suspension and error propagation.
* **Implementation pattern**:
```cpp
namespace DomMEMTk {

inline dsl::Task make_stop_the_world_task() {
  return dsl::Task{
    "StopTheWorld",
    [](dsl::TaskState& state) -> dsl::Result<std::string, std::string> {
      // Synchronize mutators at safepoint
      return dsl::Result<std::string, std::string>::from_ok("STW Achieved");
    }
  };
}

inline dsl::Task make_root_scan_task() {
  return dsl::Task{
    "RootScan",
    [](dsl::TaskState& state) -> dsl::Result<std::string, std::string> {
      // Scan registers, thread stacks, global roots
      return dsl::Result<std::string, std::string>::from_ok("Roots Queued");
    }
  };
}

inline dsl::Task make_transitive_closure_task() {
  return dsl::Task{
    "TransitiveClosure",
    [](dsl::TaskState& state) -> dsl::Result<std::string, std::string> {
      // Process work packets concurrently
      return dsl::Result<std::string, std::string>::from_ok("Closure Finished");
    }
  };
}

// Plan GC Execution:
// auto gc_cycle = make_stop_the_world_task() | make_root_scan_task() | make_transitive_closure_task();
// dsl::TaskState state;
// dsl::run(state, gc_cycle);

} // namespace DomMEMTk
```

### 5.6 Safe Allocation Flows (ResultFeature)
* **Goal**: Return explicit `dsl::Result<Address, AllocError>` from allocators without exception overhead on hot paths.
* **Implementation pattern**:
```cpp
namespace DomMEMTk {

enum class AllocError { OutOfMemory, LargeObjectExceeded, AllocationFailed };

template <typename SpacePolicy>
class BumpPointerAllocator : public dsl::DSL<BumpPointerAllocator<SpacePolicy>, dsl::ResultFeature> {
  Address cursor_;
  Address limit_;
public:
  BumpPointerAllocator(Address start, Address end) : cursor_(start), limit_(end) {}

  dsl::Result<Address, AllocError> allocate(std::size_t bytes, std::size_t align) {
    Address aligned_cursor = (cursor_ + (align - 1)) & ~(align - 1);
    if (aligned_cursor + bytes > limit_) {
      return dsl::Result<Address, AllocError>::from_err(AllocError::OutOfMemory);
    }
    cursor_ = aligned_cursor + bytes;
    return dsl::Result<Address, AllocError>::from_ok(aligned_cursor);
  }
};

} // namespace DomMEMTk
```

---

## 6. Directory & Module Layout for DomMEMTk

Agents implementing DomMEMTk must organize the repository cleanly:

```
dommemtk/
├── include/
│   └── dommemtk/
│       ├── DomDSL.hpp            # Attached DSL toolkit (C++20, header-only)
│       ├── DomMEMTk.hpp          # Umbrella include
│       ├── core/
│       │   ├── types.hpp         # Address, ObjectReference, Size types
│       │   ├── units.hpp         # MemoryUnitsDSL (CustomLiterals)
│       │   └── error.hpp         # AllocError, GCError (ResultFeature)
│       ├── space/
│       │   ├── space.hpp         # Base space CRTP definition
│       │   ├── bump_pointer.hpp  # Fast-path BumpAllocator
│       │   ├── free_list.hpp     # FreeList / MarkSweep space
│       │   ├── immix_space.hpp   # Block / Line allocators
│       │   └── los_space.hpp     # Large Object Space
│       ├── barrier/
│       │   ├── barrier.hpp       # BarrierDSL & Pipeline stages
│       │   ├── card_table.hpp    # Card table / ModBuf filters
│       │   └── satb.hpp          # Snapshot-At-The-Beginning barrier
│       ├── scheduler/
│       │   ├── work_packet.hpp   # Work packet structures
│       │   ├── task_pipeline.hpp # GC phase chains (TaskPipeline)
│       │   └── coordinator.hpp   # STW / Mutator synchronization
│       └── plan/
│           ├── plan.hpp          # Plan base CRTP
│           ├── marksweep.hpp     # MarkSweep Plan
│           ├── semispace.hpp     # SemiSpace (Copying) Plan
│           ├── immix.hpp         # Immix Plan (Mark-Region)
│           └── generational.hpp  # GenCopy / GenImmix Plans
├── tests/
│   ├── test_units.cpp
│   ├── test_allocator.cpp
│   ├── test_barrier.cpp
│   ├── test_dispatcher.cpp
│   ├── test_scheduler.cpp
│   └── test_plans.cpp
└── benchmarks/
    ├── bench_alloc.cpp
    └── bench_barrier.cpp
```

---

## 7. Implementation Checklist for Agents

1. **Verify C++20 Standard Compliance**:
   * Ensure build options specify `-std=c++20` or `-std=gnu++20`.
   * Enforce zero runtime cost for all `dsl::DSL` derived abstractions.
2. **Implement Core Spaces & Allocators**:
   * Implement `BumpPointerAllocator` for nursery and semispace copying.
   * Implement `FreeListSpace` utilizing segregated free-lists for MarkSweep.
   * Implement `ImmixSpace` with 32KB blocks and 256B lines.
3. **Construct Barrier Pipelines**:
   * Connect mutator field stores to `BarrierDSL` pipelines.
   * Benchmark inlined `operator|` stages against raw handwritten pointers to verify zero overhead under `-O3`.
4. **Assemble GC Phase Work Pipelines**:
   * Model GC phases as chains of `dsl::Task`.
   * Maintain phase state inside `dsl::TaskState`.
5. **Support VM Bindings**:
   * Provide clean C++ APIs for mutator thread registration, safepoint polls, and stack scanning callbacks.
5. **Write Documentation**:
   * Generate a twenty-two chapter manual for DomMEMTk under `/mnt/warble/domweave/docs/DomMEMTk`. Each chapter must be a separate Markdown file. It must have a `README.md` as index. Chapters must be expansive and cover their subject carefully. Do not talk about how DomMEMTk is implemented. Do it a little, but not too much! Talk about how to *use* DomMEMTk. 

---
*End of AGENTS.md*
