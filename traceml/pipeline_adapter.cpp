#include "traceml.hpp"

namespace limestone::traceml {
PortableLowering lower_graph(int64_t value) {
  PortableLowering lowering;
  lowering.program={{{1,"const",{},value,"i64"},{2,"ret",{1},{},"",0,true,true,false}},{1},{}};
  lowering.patterns={{1,"constant","const","const.i64",{},1},{2,"return","ret","ret",{"i64"},1,{},true}};
  schedrow::Instruction constant{};constant.opcode="const.i64";constant.opcode_class="pseudo";constant.semantic_class="integer";constant.latency=0;
  schedrow::Instruction ret{};ret.opcode="ret";ret.opcode_class="pseudo";ret.semantic_class="return";ret.barrier=true;ret.latency=0;
  lowering.instruction_models={std::move(constant),std::move(ret)};return lowering;
}
}
