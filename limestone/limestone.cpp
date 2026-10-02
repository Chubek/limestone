#include "limestone.hpp"
#include "../traceml/traceml.hpp"
#include <limits>
#include <map>
#include <sstream>

namespace limestone {
namespace {
Error stage(std::string_view name,const Error& error) {
  return {error.code,std::string(name)+": "+error.message};
}
Result<regtl::Program> allocation_problem(const unisel::Program& source,const PipelineTarget& target,const schedrow::Region& region,const std::vector<uint32_t>& order) {
  regtl::Program problem;problem.classes=target.register_classes;problem.aliases=target.aliases;
  std::map<uint32_t,const schedrow::Instruction*> instructions;
  for(auto& instruction:region.instructions)instructions[instruction.id]=&instruction;
  std::map<uint32_t,std::pair<uint32_t,uint32_t>> intervals;
  for(auto& n:source.nodes)if(!n.required&&n.produces_value)intervals[n.id]={0,0};
  if(order.size()>std::numeric_limits<uint32_t>::max()/2)return Result<regtl::Program>::err({Error::Code::ResourceLimit,"too many allocation positions"});
  for(size_t k=0;k<order.size();++k) {
    auto& instruction=*instructions.at(order[k]);auto position=static_cast<uint32_t>(2*k);
    // Explicit and implicit physical effects need a target allocation adapter;
    // they are never silently reinterpreted as virtual values here.
    if(!instruction.implicit_defs.empty()||!instruction.implicit_uses.empty())return Result<regtl::Program>::err({Error::Code::Unsupported,"implicit register effects need a target allocation adapter"});
    for(auto value:instruction.uses) {
      auto it=intervals.find(value);
      if(it==intervals.end())return Result<regtl::Program>::err({Error::Code::Conflict,"use before definition of v"+std::to_string(value)});
      it->second.second=std::max(it->second.second,position);
    }
    for(auto value:instruction.defs)if(!intervals.emplace(value,std::pair{position,position}).second)return Result<regtl::Program>::err({Error::Code::Conflict,"multiple definitions of v"+std::to_string(value)});
  }
  for(auto value:source.outputs)if(auto it=intervals.find(value);it!=intervals.end())it->second.second=static_cast<uint32_t>(2*order.size());
  for(auto [value,interval]:intervals) {
    auto it=target.value_classes.find(value);auto klass=it==target.value_classes.end()?target.default_register_class:it->second;
    if(klass.empty())return Result<regtl::Program>::err({Error::Code::Unsupported,"missing target register class for v"+std::to_string(value)});
    auto constraint=target.constraints.find(value);
    problem.ranges.push_back({value,interval.first,interval.second,klass,constraint==target.constraints.end()?regtl::Constraint{}:constraint->second,true});
  }
  return Result<regtl::Program>::ok(std::move(problem));
}
std::string emit(const Module& module,const std::vector<uint32_t>& order) {
  std::ostringstream out;out<<"module "<<module.name<<" {\n  function main {\n    block entry {\n";
  if(module.allocation) {
    std::map<uint32_t,uint32_t> registers(module.allocation->regs.begin(),module.allocation->regs.end());
    for(auto [value,physical]:registers)out<<"      ; v"<<value<<" -> physical "<<physical<<'\n';
    for(auto value:module.allocation->spilled)out<<"      ; v"<<value<<" -> spill (materialization required)\n";
  }
  std::map<uint32_t,uint32_t> cycles;
  for(auto scheduled:module.scheduled)cycles[scheduled.id]=scheduled.cycle;
  for(auto id:order) {
    const auto& i=*std::find_if(module.selected.instructions.begin(),module.selected.instructions.end(),[&](auto& x){return x.id==id;});
    out<<"      ";for(size_t n=0;n<i.defs.size();++n){if(n)out<<", ";out<<'%'<<i.defs[n];}
    if(!i.defs.empty())out<<" = ";out<<i.opcode;
    for(auto value:i.uses)out<<" %"<<value;
    for(auto [value,constant]:i.immediates)out<<" #"<<constant;
    if(cycles.contains(id))out<<" ; cycle "<<cycles.at(id);
    out<<'\n';
  }
  return out.str()+"    }\n  }\n}\n";
}
}
Result<Module> run_pipeline(const unisel::Program& input,const PipelineTarget& target,const PipelineOptions& options) {
  if(target.name.empty())return Result<Module>::err({Error::Code::InvalidArgument,"target has no identity"});
  Module module;module.name="module";unisel::Program program=input;
  auto valid=unisel::validate(program,target.patterns);if(!valid)return Result<Module>::err(stage("ingest",valid.error()));
  module.stages.push_back("ingest");
  if(options.optimize&&target.optimizer) {
    auto optimized=target.optimizer(program);if(!optimized)return Result<Module>::err(stage("optimize",optimized.error()));
    program=std::move(optimized.value());valid=unisel::validate(program,target.patterns);
    if(!valid)return Result<Module>::err(stage("optimize",valid.error()));module.stages.push_back("optimize");
  }
  auto selected=unisel::solve(program,target.patterns);if(!selected)return Result<Module>::err(stage("select",selected.error()));
  auto region=unisel::emit_scheduler(program,target.patterns,selected.value());if(!region)return Result<Module>::err(stage("select",region.error()));
  module.selected=std::move(region.value());module.stages.push_back("select");
  for(auto& i:module.selected.instructions) {
    auto it=target.instructions.find(i.opcode);
    if(it==target.instructions.end()) {
      if(options.schedule)return Result<Module>::err({Error::Code::Unsupported,"schedule: missing instruction model for "+i.opcode});
      continue;
    }
    auto& model=it->second;
    if(options.schedule&&!model.latency)return Result<Module>::err({Error::Code::Unsupported,"schedule: unknown target latency for "+i.opcode});
    if(model.latency)i.latency=*model.latency;
    i.throughput=model.throughput;i.resources=model.resources;i.opcode_class=model.opcode_class;i.semantic_class=model.semantic_class;
    i.barrier=model.barrier;i.memory=model.memory;i.implicit_defs=model.implicit_defs;i.implicit_uses=model.implicit_uses;
  }
  std::vector<uint32_t> order;
  if(options.schedule) {
    auto scheduled=schedrow::schedule(module.selected,target.scheduling);if(!scheduled)return Result<Module>::err(stage("schedule",scheduled.error()));
    module.scheduled=std::move(scheduled.value());auto checked=schedrow::verify(module.selected,target.scheduling,module.scheduled);
    if(!checked)return Result<Module>::err(stage("schedule",checked.error()));
    for(auto i:module.scheduled)order.push_back(i.id);module.stages.push_back("schedule");
  }else for(auto& i:module.selected.instructions)order.push_back(i.id);
  if(options.allocate) {
    auto problem=allocation_problem(program,target,module.selected,order);if(!problem)return Result<Module>::err(stage("allocate",problem.error()));
    module.allocation_problem=std::move(problem.value());auto allocated=regtl::linear_scan(module.allocation_problem);
    if(!allocated)return Result<Module>::err(stage("allocate",allocated.error()));
    module.allocation=std::move(allocated.value());module.stages.push_back("allocate");
  }
  module.machine_ir=emit(module,order);module.stages.push_back("machine-ir");return Result<Module>::ok(std::move(module));
}
Result<Module> run_pipeline(std::string_view input,const PipelineOptions& options) {
  auto compiled=traceml::compile(input);if(!compiled)return Result<Module>::err(stage("frontend",compiled.error()));
  auto value=traceml::evaluate(compiled.value());if(!value)return Result<Module>::err(stage("frontend",value.error()));
  // Portable IR pseudo-instruction selection belongs to the frontend adapter.
  auto lowered=traceml::lower_graph(value.value());PipelineTarget target;target.name="portable-machineir";
  target.patterns=std::move(lowered.patterns);
  for(auto& i:lowered.instruction_models)target.instructions.emplace(i.opcode,InstructionModel{i.latency,i.throughput,i.resources,i.opcode_class,i.semantic_class,i.barrier,i.memory});
  auto result=run_pipeline(lowered.program,target,options);
  if(result)result.value().stages.insert(result.value().stages.begin(),{"frontend","trace-lambda"});
  return result;
}
}
