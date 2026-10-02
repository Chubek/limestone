#pragma once
#include "tunah.hpp"
#include "bin2bin/bin2bin.hpp"

namespace limestone::tunah {
struct BinaryAdapterOptions {
  Limits limits;
  CostModel costs;
  // Called at ingress and after extraction. Prove types, architectural effects,
  // widths and operand legality in the host's ISA semantic vocabulary.
  std::function<Result<bool>(const bin2bin::LiftedInstruction&,const Term&)> legality;
};
// Snapshot the session and options. The context identity covers host semantics,
// predicates and legality analysis; rules/costs/budgets are encoded automatically.
// Translation owns layout/encoding and preserves instruction/control boundaries.
Result<bin2bin::SemanticTransform> binary_transform(const Session&,std::string context_identity,const BinaryAdapterOptions&);
}
