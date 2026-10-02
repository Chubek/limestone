#include "scheduling_adapter.hpp"
#include <limits>
#include <set>

namespace limestone::regtl {
Result<schedrow::Region> allocated_dependencies(const schedrow::Region& input,const Allocation& allocation,std::span<const std::pair<PReg,PReg>> aliases) {
  auto shadow=input;
  for(auto& i:shadow.instructions){for(auto v:i.uses){if(!allocation.regs.contains(v))return Result<schedrow::Region>::err({Error::Code::Conflict,"missing post-allocation input"});i.implicit_uses.push_back(allocation.regs.at(v));}for(auto v:i.defs){if(!allocation.regs.contains(v))return Result<schedrow::Region>::err({Error::Code::Conflict,"missing post-allocation definition"});auto physical=allocation.regs.at(v);auto latency=i.result_latency.contains(v)?i.result_latency.at(v):i.latency;bool defined=std::find(i.implicit_defs.begin(),i.implicit_defs.end(),physical)!=i.implicit_defs.end();if(defined)latency=std::max(latency,i.implicit_result_latency.contains(physical)?i.implicit_result_latency.at(physical):i.latency);i.implicit_result_latency[physical]=latency;i.implicit_defs.push_back(physical);}}
  auto augmented=schedrow::dependencies(shadow,aliases);if(!augmented)return augmented;auto result=input;
  for(auto d:augmented.value().deps)if(std::none_of(input.deps.begin(),input.deps.end(),[&](auto& e){return e.producer==d.producer&&e.consumer==d.consumer&&e.kind==d.kind&&e.latency>=d.latency&&e.distance==d.distance;})){d.scheduler_only=true;result.deps.push_back(d);}
  return Result<schedrow::Region>::ok(std::move(result));
}
namespace {
bool aliases(const Program& p,PReg a,PReg b) {
  return a==b||std::any_of(p.aliases.begin(),p.aliases.end(),[&](auto pair){return pair==std::pair{a,b}||pair==std::pair{b,a};});
}
Result<int> validate_spills(const Program& p,std::span<const SpillClass> classes) {
  std::set<std::string> names;std::set<PReg> physical;for(auto& c:p.classes)physical.insert(c.members.begin(),c.members.end());
  for(auto& c:classes){auto klass=std::find_if(p.classes.begin(),p.classes.end(),[&](auto& k){return k.name==c.klass;});if(!names.insert(c.klass).second||klass==p.classes.end()||c.load_opcode.empty()||c.store_opcode.empty()||c.address_space.empty()||!c.size||!c.alignment||(c.alignment&(c.alignment-1))||c.scratch.empty())return Result<int>::err({Error::Code::InvalidArgument,"invalid spill class contract: "+c.klass});std::set<PReg> seen;for(auto r:c.scratch)if(!seen.insert(r).second||std::find(klass->members.begin(),klass->members.end(),r)==klass->members.end())return Result<int>::err({Error::Code::InvalidArgument,"invalid spill scratch register"});}
  return Result<int>::ok(0);
}
}
Result<Program> reserve_spill_registers(const Program& input,std::span<const SpillClass> classes) {
  auto checked=validate(input);if(!checked)return Result<Program>::err(checked.error());checked=validate_spills(input,classes);if(!checked)return Result<Program>::err(checked.error());auto result=input;
  for(auto& c:classes)result.reserved.insert(result.reserved.end(),c.scratch.begin(),c.scratch.end());std::sort(result.reserved.begin(),result.reserved.end());result.reserved.erase(std::unique(result.reserved.begin(),result.reserved.end()),result.reserved.end());
  checked=validate(result);if(!checked)return Result<Program>::err(checked.error());return Result<Program>::ok(std::move(result));
}
Result<AllocatedRegion> materialize_spills(const schedrow::Region& input,std::span<const uint32_t> order,const Program& problem,const Allocation& assignment,std::span<const SpillClass> classes,std::span<const VReg> outputs) {
  using Output=Result<AllocatedRegion>;
  auto checked=verify(problem,assignment);if(!checked)return Output::err(checked.error());checked=validate_spills(problem,classes);if(!checked)return Output::err(checked.error());
  checked=schedrow::verify_order(input,order,problem.aliases);if(!checked)return Output::err(checked.error());
  std::map<VReg,const LiveRange*> ranges;std::map<std::string,const SpillClass*> models;std::map<uint32_t,const schedrow::Instruction*> instructions;uint64_t next_value=0,next_instruction=0;
  for(auto& r:problem.ranges){ranges[r.value]=&r;next_value=std::max(next_value,uint64_t(r.value)+1);}for(auto& c:classes)models[c.klass]=&c;
  for(auto& i:input.instructions){if(!instructions.emplace(i.id,&i).second)return Output::err({Error::Code::Conflict,"duplicate spill input instruction"});next_instruction=std::max(next_instruction,uint64_t(i.id)+1);}
  if(order.size()!=instructions.size())return Output::err({Error::Code::Conflict,"incomplete spill input order"});std::set<uint32_t> seen;for(auto id:order)if(!instructions.contains(id)||!seen.insert(id).second)return Output::err({Error::Code::Conflict,"invalid spill input order"});
  Function source;source.classes=problem.classes;source.aliases=problem.aliases;
  for(auto& range:problem.ranges)source.values.push_back({range.value,range.klass,range.constraint,range.spillable});
  std::map<uint32_t,Block> source_blocks;
  if(input.blocks.empty())source_blocks[0]={0};else for(auto& b:input.blocks)source_blocks[b.id]={b.id,{},b.successors,b.live_out};
  for(auto id:order){auto& i=*instructions.at(id);source_blocks.at(input.blocks.empty()?0:i.block).instructions.push_back({i.id,i.defs,i.uses,i.early_defs,i.implicit_defs,i.ties,i.implicit_defs,i.implicit_uses});}
  for(auto& [id,block]:source_blocks)source.blocks.push_back(std::move(block));auto physical_state=analyze(source);if(!physical_state)return Output::err(physical_state.error());
  std::set<VReg> spilled(assignment.spilled.begin(),assignment.spilled.end()),defined;
  for(auto& i:input.instructions)defined.insert(i.defs.begin(),i.defs.end());
  for(auto v:spilled)if(!defined.contains(v))return Output::err({Error::Code::Unsupported,"spilled live-in needs a calling-convention transfer"});
  for(auto v:outputs)if(spilled.contains(v))return Output::err({Error::Code::Unsupported,"spilled output needs a calling-convention transfer"});
  for(auto& b:input.blocks)for(auto v:b.live_out)if(spilled.contains(v))return Output::err({Error::Code::Unsupported,"spilled explicit live-out needs a boundary transfer"});
  for(auto [value,physical]:assignment.regs)for(auto& c:classes)for(auto scratch:c.scratch)if(aliases(problem,physical,scratch))return Output::err({Error::Code::Conflict,"spill scratch overlaps allocated storage"});
  AllocatedRegion result;result.region=input;result.region.instructions.clear();result.allocation=assignment;result.allocation.spilled.clear();std::map<VReg,size_t> slots;
  for(auto v:spilled){auto& r=*ranges.at(v);if(!models.contains(r.klass))return Output::err({Error::Code::Unsupported,"missing spill class for v"+std::to_string(v)});auto& c=*models.at(r.klass);auto padding=(c.alignment-result.frame_size%c.alignment)%c.alignment;if(padding>UINT64_MAX-result.frame_size||c.size>UINT64_MAX-result.frame_size-padding)return Output::err({Error::Code::ResourceLimit,"spill frame overflow"});result.frame_size+=padding;slots[v]=result.slots.size();result.slots.push_back({v,r.klass,result.frame_size,c.size,c.alignment});result.frame_size+=c.size;}
  auto identity=[&](uint64_t& next)->Result<uint32_t>{if(next>UINT32_MAX)return Result<uint32_t>::err({Error::Code::ResourceLimit,"spill temporary identity overflow"});return Result<uint32_t>::ok(static_cast<uint32_t>(next++));};
  std::map<uint32_t,size_t> positions;for(size_t k=0;k<order.size();++k)positions[order[k]]=k;
  std::vector<std::pair<size_t,size_t>> intervals,units;
  for(auto& group:input.groups)if(group.kind!=schedrow::GroupKind::Ordered)intervals.emplace_back(positions.at(group.members.front()),positions.at(group.members.back()));
  std::sort(intervals.begin(),intervals.end());for(auto interval:intervals){if(!units.empty()&&interval.first<=units.back().second)units.back().second=std::max(units.back().second,interval.second);else units.push_back(interval);}
  std::map<size_t,size_t> boundaries(units.begin(),units.end());
  std::map<VReg,std::set<uint32_t>> consumers;for(auto& i:input.instructions)for(auto v:i.uses)if(spilled.contains(v))consumers[v].insert(i.id);
  for(size_t position=0;position<order.size();) {
    auto end=boundaries.contains(position)?boundaries.at(position)+1:position+1;
    std::vector<uint32_t> unit(order.begin()+position,order.begin()+end);position=end;std::set<uint32_t> unit_ids(unit.begin(),unit.end());
    std::vector<schedrow::Instruction> operations;std::map<VReg,VReg> temporary;
    std::set<VReg> involved,unit_defs,reloads,stores;
    for(auto id:unit){auto& i=*instructions.at(id);operations.push_back(i);involved.insert(i.uses.begin(),i.uses.end());involved.insert(i.defs.begin(),i.defs.end());for(auto v:i.uses)if(spilled.contains(v)&&!unit_defs.contains(v))reloads.insert(v);unit_defs.insert(i.defs.begin(),i.defs.end());}
    for(auto v:unit_defs)if(spilled.contains(v)&&std::any_of(consumers[v].begin(),consumers[v].end(),[&](auto id){return !unit_ids.contains(id);}))stores.insert(v);
    for(auto v:involved)if(!ranges.contains(v))return Output::err({Error::Code::Conflict,"spill instruction references unknown value"});
    for(auto v:involved)if(spilled.contains(v)){auto fresh=identity(next_value);if(!fresh)return Output::err(fresh.error());temporary[v]=fresh.value();result.value_sources[fresh.value()]=v;}
    if(!temporary.empty()) {
      Function local;local.classes=problem.classes;local.aliases=problem.aliases;Block block{0};block.live_out.assign(stores.begin(),stores.end());
      for(auto& i:operations)block.instructions.push_back({i.id,i.defs,i.uses,i.early_defs,i.implicit_defs,i.ties,i.implicit_defs,i.implicit_uses});
      for(auto v:involved){auto& r=*ranges.at(v);Constraint constraint;if(temporary.contains(v)){constraint=r.constraint;constraint.allowed=models.at(r.klass)->scratch;
          for(auto reg:constraint.allowed)if(!r.constraint.allowed.empty()&&std::find(r.constraint.allowed.begin(),r.constraint.allowed.end(),reg)==r.constraint.allowed.end())constraint.forbidden.push_back(reg);
          // Unit temporaries survive until their boundary transfers. Protect all
          // architectural state visible inside the unit, including aliased views.
          for(auto reg:constraint.allowed)for(auto id:unit)for(auto& state:{physical_state.value().physical_before.at(id),physical_state.value().physical_after.at(id)})for(auto live:state)if(aliases(problem,reg,live))constraint.forbidden.push_back(reg);
        }else constraint.fixed=assignment.regs.at(v);local.values.push_back({v,r.klass,std::move(constraint),false});}
      local.blocks.push_back(std::move(block));auto analysis=analyze(local);if(!analysis)return Output::err(analysis.error());auto allocation=constraint_allocate(analysis.value().problem);if(!allocation)return Output::err(allocation.error());for(auto [v,tmp]:temporary)result.allocation.regs[tmp]=allocation.value().regs.at(v);
      auto replace=[&](VReg v){return temporary.contains(v)?temporary.at(v):v;};
      for(auto& i:operations){for(auto& v:i.uses)v=replace(v);for(auto& v:i.defs)v=replace(v);for(auto& v:i.early_defs)v=replace(v);for(auto& [a,b]:i.ties){a=replace(a);b=replace(b);}std::unordered_map<VReg,std::string> renamed;for(auto& [v,klass]:i.register_classes)renamed[replace(v)]=klass;i.register_classes=std::move(renamed);std::unordered_map<VReg,uint32_t> latencies;for(auto [v,latency]:i.result_latency)latencies[replace(v)]=latency;i.result_latency=std::move(latencies);}
    }
    auto transfer=[&](VReg source,bool load)->Result<schedrow::Instruction>{auto fresh=identity(next_instruction);if(!fresh)return Result<schedrow::Instruction>::err(fresh.error());auto& slot=result.slots[slots.at(source)];if(slot.offset>INT64_MAX)return Result<schedrow::Instruction>::err({Error::Code::ResourceLimit,"spill offset exceeds immediate range"});auto& c=*models.at(slot.klass);schedrow::Instruction t{};t.id=fresh.value();t.block=operations.front().block;t.opcode=load?c.load_opcode:c.store_opcode;t.register_classes[temporary.at(source)]=slot.klass;if(load)t.defs={temporary.at(source)};else t.uses={temporary.at(source)};t.immediates={{source,static_cast<int64_t>(slot.offset)}};t.access=schedrow::MemoryAccess{load,!load,false,false,schedrow::MemoryOrdering::Relaxed,c.address_space,{},c.size,c.alignment};t.memory=true;t.speculative=false;t.origin=(load?"reload v":"spill v")+std::to_string(source);return Result<schedrow::Instruction>::ok(std::move(t));};
    for(auto v:reloads){auto t=transfer(v,true);if(!t)return Output::err(t.error());result.region.instructions.push_back(std::move(t.value()));}
    if(!stores.empty()&&operations.back().terminator)return Output::err({Error::Code::Unsupported,"spilled terminating unit needs a control-flow transfer adapter"});
    result.region.instructions.insert(result.region.instructions.end(),operations.begin(),operations.end());
    for(auto v:stores){auto t=transfer(v,false);if(!t)return Output::err(t.error());result.region.instructions.push_back(std::move(t.value()));}
  }
  std::map<uint32_t,uint32_t> prior;
  for(auto& i:result.region.instructions){result.order.push_back(i.id);if(prior.contains(i.block))result.region.deps.push_back({prior.at(i.block),i.id,schedrow::DepKind::Ordering,0,0,true});prior[i.block]=i.id;}
  checked=schedrow::verify_order(result.region,result.order,problem.aliases);if(!checked)return Output::err(checked.error());
  Function function;function.classes=problem.classes;function.aliases=problem.aliases;std::map<uint32_t,Block> blocks;
  if(input.blocks.empty())blocks[0]={0,{}, {},std::vector<VReg>(outputs.begin(),outputs.end())};else for(auto& b:input.blocks){blocks[b.id]={b.id,{},b.successors,b.live_out};if(b.successors.empty())blocks[b.id].live_out.insert(blocks[b.id].live_out.end(),outputs.begin(),outputs.end());}
  for(auto& i:result.region.instructions)blocks.at(input.blocks.empty()?0:i.block).instructions.push_back({i.id,i.defs,i.uses,i.early_defs,i.implicit_defs,i.ties,i.implicit_defs,i.implicit_uses});
  for(auto& [id,b]:blocks)function.blocks.push_back(std::move(b));
  for(auto [value,physical]:result.allocation.regs){auto source=result.value_sources.contains(value)?result.value_sources.at(value):value;function.values.push_back({value,ranges.at(source)->klass,{{},{},physical},false});}
  auto analysis=analyze(function);if(!analysis)return Output::err(analysis.error());result.problem=std::move(analysis.value().problem);checked=verify(result.problem,result.allocation);if(!checked)return Output::err(checked.error());return Output::ok(std::move(result));
}
}
