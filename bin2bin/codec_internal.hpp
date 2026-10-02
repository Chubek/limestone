#pragma once
#include "bin2bin.hpp"
namespace limestone::bin2bin::detail {
Result<int> translation_contract(const metacode::Value::Object*,EncodingForm&);
Result<std::string> transformed_semantics(const TranslationOptions&,const LiftedInstruction&);
Result<Architecture> load_masked(const metacode::Architecture&,Architecture);
Result<std::vector<Instruction>> decode_masked(const Architecture&,std::span<const uint8_t>,uint64_t);
Result<std::string> semantics(const Architecture&,const Instruction&);
Result<std::vector<uint8_t>> translate_masked(const Architecture&,const Architecture&,std::span<const uint8_t>,const TranslationOptions&);
}
