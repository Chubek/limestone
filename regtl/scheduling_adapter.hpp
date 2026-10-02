#pragma once
#include "regtl.hpp"
#include "schedrow/schedrow.hpp"
#include <map>

namespace limestone::regtl {
struct SpillClass {
  std::string klass,load_opcode,store_opcode,address_space;
  uint32_t size=0,alignment=0;
  std::vector<PReg> scratch;
};
struct AllocatedRegion {
  schedrow::Region region;
  std::vector<uint32_t> order;
  Allocation allocation;
  Program problem;
  std::vector<SpillSlot> slots;
  uint64_t frame_size=0;
  // New short-lived virtual value -> original spilled value.
  std::map<VReg,VReg> value_sources;
  std::vector<schedrow::Scheduled> scheduled;
};
// Reserve target-supplied scratch storage, including every direct alias, before
// allocation. Spills use a stable private frame, never guessed stack offsets.
Result<Program> reserve_spill_registers(const Program&,std::span<const SpillClass>);
Result<AllocatedRegion> materialize_spills(const schedrow::Region&,std::span<const uint32_t> order,
  const Program&,const Allocation&,std::span<const SpillClass>,std::span<const VReg> outputs={});
// Add post-allocation storage hazards without changing semantic operands.
Result<schedrow::Region> allocated_dependencies(const schedrow::Region&,const Allocation&,
  std::span<const std::pair<PReg,PReg>> aliases={});
}
