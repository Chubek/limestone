#pragma once
#include "../limestone/foundation.hpp"
namespace limestone::schedrow {
using InstrId=uint32_t; using ValueId=uint32_t;
enum class DepKind { True, Anti, Output, Memory, Control, Ordering };
struct Dependency { InstrId producer, consumer; DepKind kind; uint32_t latency=0, distance=0; };
struct ResourceUse { std::string resource; uint32_t duration=1; double quantity=1; };
struct Instruction {
 InstrId id; std::string opcode, opcode_class, semantic_class;
 std::vector<ValueId> defs, uses; std::vector<ResourceUse> resources;
 uint32_t latency=1; double throughput=1.0; bool barrier=false, speculative=true, memory=false;
};
struct Region { std::string name; std::vector<Instruction> instructions; std::vector<Dependency> deps; };
struct MachineModel { std::unordered_map<std::string,uint32_t> resource_capacity; };
struct Scheduled { InstrId id; uint32_t cycle; };
Result<std::vector<Scheduled>> schedule(const Region&, const MachineModel&);
std::string print(const Region&);
}
