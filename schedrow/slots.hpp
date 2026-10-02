#pragma once
#include "schedrow.hpp"
namespace limestone::schedrow::detail {
bool slots_feasible(uint32_t width,const std::vector<std::pair<const Instruction*,std::optional<uint32_t>>>&);
}
