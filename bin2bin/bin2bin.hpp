#pragma once
#include "../limestone/foundation.hpp"
#include "../metacode/metacode.hpp"
namespace limestone::bin2bin {
enum class Status { Supported, Unsupported, Fallback, ArchitectureSpecific, Privileged, EnvironmentDependent, Ambiguous };
struct Instruction { uint64_t address; uint8_t opcode; std::vector<uint8_t> bytes; std::string mnemonic; Status status=Status::Supported; };
// This decoder adapter describes fixed, operand-free, one-byte instructions.
// Richer native encodings require a separate decoder/encoder adapter.
struct Architecture {
  std::string name; std::unordered_map<uint8_t,std::string> opcodes;
  std::unordered_map<uint8_t,std::string> semantics;
  std::unordered_map<uint8_t,Status> status;
  std::string version, description, infobank_version, execution_domain, state_model;
};
struct TranslationRule { std::string source, target; std::vector<uint8_t> bytes; };
struct CacheStorage;
struct TranslationCache {
  std::unordered_map<std::string,std::vector<uint8_t>> entries;
  std::shared_ptr<CacheStorage> storage;
};
struct TranslationOptions {
  std::string rule_version, optimization_configuration, translator_configuration, plugin_versions, runtime_configuration;
};
struct LiftedInstruction { uint64_t address; std::string semantics; Status status; };
Result<Architecture> from_metacode(const metacode::Architecture&);
// Open an LMDB environment directory; failures never downgrade to memory caching.
Result<int> open_cache(TranslationCache&, const std::string& path, size_t map_size=64*1024*1024);
Result<std::vector<Instruction>> decode(const Architecture&,std::span<const uint8_t> bytes,uint64_t address=0);
Result<std::vector<LiftedInstruction>> lift(const Architecture&,std::span<const uint8_t>,uint64_t address=0);
Result<std::vector<uint8_t>> translate(const Architecture&,const Architecture&,std::span<const uint8_t>,TranslationCache* cache=nullptr,const TranslationOptions& = {});
std::string disassemble(const std::vector<Instruction>&);
Result<std::string> decompile(const Architecture&,std::span<const uint8_t>,uint64_t address=0);
}
