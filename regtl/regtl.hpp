#pragma once
#include "../limestone/foundation.hpp"
namespace limestone::regtl {
using VReg=uint32_t; using PReg=uint32_t;
struct RegClass { std::string name; std::vector<PReg> members; };
struct Constraint { std::vector<PReg> allowed, forbidden; std::optional<PReg> fixed; };
struct LiveRange { VReg value; uint32_t begin,end; std::string klass; Constraint constraint; bool spillable=true; };
struct Allocation { std::unordered_map<VReg,PReg> regs; std::vector<VReg> spilled; };
struct Program { std::vector<LiveRange> ranges; std::vector<RegClass> classes; };
Result<Allocation> linear_scan(const Program&);
Result<int> verify(const Program&, const Allocation&);
std::string print(const Program&);
}
