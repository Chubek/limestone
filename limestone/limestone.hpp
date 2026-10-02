#pragma once
#include "../limestone/foundation.hpp"
#include "../unisel/unisel.hpp"
#include "../regtl/regtl.hpp"
namespace limestone {
// Physical allocation requires an explicit target register model.
struct PipelineOptions { bool optimize=true, schedule=true, allocate=false; };
struct InstructionModel {
  std::optional<uint32_t> latency;
  double throughput=1;
  std::vector<schedrow::ResourceUse> resources;
  std::string opcode_class, semantic_class;
  bool barrier=false, memory=false;
  std::vector<uint32_t> implicit_defs, implicit_uses;
};
struct PipelineTarget {
  std::string name;
  std::vector<unisel::Pattern> patterns;
  schedrow::MachineModel scheduling;
  std::unordered_map<std::string,InstructionModel> instructions;
  std::vector<regtl::RegClass> register_classes;
  std::vector<std::pair<regtl::PReg,regtl::PReg>> aliases;
  std::unordered_map<uint32_t,std::string> value_classes;
  std::unordered_map<uint32_t,regtl::Constraint> constraints;
  std::string default_register_class;
  // A verified IL adapter owns optimization semantics; orchestration invokes it.
  std::function<Result<unisel::Program>(const unisel::Program&)> optimizer;
};
struct Module {
  std::string name; std::string machine_ir;
  schedrow::Region selected;
  std::vector<schedrow::Scheduled> scheduled;
  regtl::Program allocation_problem;
  std::optional<regtl::Allocation> allocation;
  std::vector<std::string> stages;
};
// Source convenience entry: closed TraceML integer computations -> portable IR.
Result<Module> run_pipeline(std::string_view input, const PipelineOptions& options = PipelineOptions{});
// Explicit machine-independent graph -> target selection/scheduling/allocation.
Result<Module> run_pipeline(const unisel::Program&, const PipelineTarget&, const PipelineOptions& = {});
}
