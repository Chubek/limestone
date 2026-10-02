#include "schedrow.hpp"
#include "slots.hpp"
#include "groups.hpp"
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace limestone::schedrow {
namespace {
Result<int> validate(const Region& r,const MachineModel& m) {
  std::unordered_set<InstrId> ids;
  for(auto&[name,cap]:m.resource_capacity)if(name.empty()||!cap)return Result<int>::err({Error::Code::InvalidArgument,"invalid resource capacity"});
  for(auto& i:r.instructions) {
    if(!ids.insert(i.id).second)return Result<int>::err({Error::Code::InvalidArgument,"duplicate instruction id"});
    if(!std::isfinite(i.throughput)||i.throughput<=0)return Result<int>::err({Error::Code::InvalidArgument,"invalid throughput"});
    for(auto slot:i.issue_slots)if(!m.issue_width||slot>=m.issue_width)return Result<int>::err({Error::Code::InvalidArgument,"issue slot outside machine issue width"});
    if(i.access&&i.access->alignment&&(i.access->alignment&(i.access->alignment-1)))return Result<int>::err({Error::Code::InvalidArgument,"memory alignment is not a power of two"});
    for(auto& u:i.resources) {
      if(!u.duration||!std::isfinite(u.quantity)||u.quantity<=0)return Result<int>::err({Error::Code::InvalidArgument,"invalid resource reservation"});
      std::vector<std::string> names=u.alternatives.empty()?std::vector<std::string>{u.resource}:u.alternatives;
      bool feasible=false;
      for(auto& name:names) {
        auto it=m.resource_capacity.find(name);
        if(it==m.resource_capacity.end())return Result<int>::err({Error::Code::InvalidArgument,"unknown resource: "+name});
        feasible|=u.quantity<=it->second;
      }
      if(!feasible)return Result<int>::err({Error::Code::Unsatisfiable,"reservation exceeds resource capacity"});
    }
  }
  for(auto& d:r.deps) {
    if(!ids.contains(d.producer)||!ids.contains(d.consumer))return Result<int>::err({Error::Code::InvalidArgument,"dependency references unknown instruction"});
    if(d.distance)return Result<int>::err({Error::Code::Unsupported,"loop-carried dependencies require a modulo scheduler"});
    if(d.producer==d.consumer)return Result<int>::err({Error::Code::Conflict,"self-dependent instruction"});
  }
  std::map<InstrId,size_t> indegree;std::map<InstrId,std::vector<InstrId>> edges;
  for(auto id:ids)indegree[id]=0;
  for(auto& d:r.deps){++indegree[d.consumer];edges[d.producer].push_back(d.consumer);}
  std::vector<InstrId> ready;for(auto [id,count]:indegree)if(!count)ready.push_back(id);
  size_t visited=0;
  while(!ready.empty()){auto id=ready.back();ready.pop_back();++visited;for(auto child:edges[id])if(!--indegree[child])ready.push_back(child);}
  if(visited!=ids.size())return Result<int>::err({Error::Code::Conflict,"cyclic scheduling dependencies"});
  return Result<int>::ok(0);
}
using Usage=std::map<std::string,std::map<uint64_t,double>>;
// Backtracking handles alternatives jointly, including multiple uses of one port.
bool reserve(const Instruction& i,const MachineModel& m,uint64_t cycle,Usage& used,size_t index=0,const std::function<bool()>& accept={},std::vector<std::string>* chosen=nullptr) {
  if(index==i.resources.size())return !accept||accept();
  const auto& u=i.resources[index];
  auto names=u.alternatives.empty()?std::vector<std::string>{u.resource}:u.alternatives;
  std::sort(names.begin(),names.end());
  for(auto& name:names) {
    bool fits=true;
    for(uint64_t k=0;k<u.duration;++k)if(used[name][cycle+u.offset+k]+u.quantity>m.resource_capacity.at(name)+1e-9){fits=false;break;}
    if(!fits)continue;
    for(uint64_t k=0;k<u.duration;++k)used[name][cycle+u.offset+k]+=u.quantity;
    if(chosen)chosen->push_back(name);
    if(reserve(i,m,cycle,used,index+1,accept,chosen))return true;
    if(chosen)chosen->pop_back();
    for(uint64_t k=0;k<u.duration;++k)used[name][cycle+u.offset+k]-=u.quantity;
  }
  return false;
}
using Issues=std::map<uint64_t,uint32_t>;
using Slots=std::map<uint64_t,std::set<uint32_t>>;
// A bundle's slots and alternative resources must be solved jointly. Committing
// the flexible first member greedily can make a legal later member impossible.
bool pack(std::span<const Instruction* const> members,const MachineModel& machine,uint64_t cycle,Usage& usage,Issues& issued,Slots& slots,std::vector<Scheduled>& plan,size_t index=0) {
  if(index==members.size())return true;
  const auto& instruction=*members[index];
  if(machine.issue_width&&issued[cycle]>=machine.issue_width)return false;
  auto allowed=instruction.issue_slots;
  if(allowed.empty()&&machine.issue_width)for(uint64_t slot=0;slot<machine.issue_width;++slot)allowed.push_back(uint32_t(slot));
  if(!machine.issue_width)allowed={0};std::sort(allowed.begin(),allowed.end());
  for(auto slot:allowed)if(!machine.issue_width||!slots[cycle].contains(slot)) {
    Scheduled entry{instruction.id,uint32_t(cycle)};if(machine.issue_width)entry.slot=slot;
    ++issued[cycle];if(machine.issue_width)slots[cycle].insert(slot);
    if(reserve(instruction,machine,cycle,usage,0,[&]{plan.push_back(entry);if(pack(members,machine,cycle,usage,issued,slots,plan,index+1))return true;plan.pop_back();return false;},&entry.resources))return true;
    --issued[cycle];if(machine.issue_width)slots[cycle].erase(slot);
  }
  return false;
}
Result<std::vector<Scheduled>> grouped(const Region& region,const MachineModel& machine) {
  using Output=Result<std::vector<Scheduled>>;
  struct Unit {std::vector<const Instruction*> members;bool same=false,constrained=false;int64_t priority=0;uint64_t height=0;size_t source=0;bool relaxed=false;};
  std::map<InstrId,const Instruction*> instructions;std::map<InstrId,size_t> owner,source;
  for(size_t k=0;k<region.instructions.size();++k){auto& i=region.instructions[k];instructions[i.id]=&i;source[i.id]=k;Usage isolated;if(!reserve(i,machine,0,isolated))return Output::err({Error::Code::Unsatisfiable,"instruction reservations cannot fit: "+std::to_string(i.id)});}
  std::vector<Unit> units;
  for(auto& group:region.groups)if(group.kind!=GroupKind::Ordered) {
    Unit unit;unit.same=detail::same_cycle(group.kind);unit.priority=group.benefit;unit.source=source.at(group.members.front());
    unit.relaxed=group.kind==GroupKind::SameCycle;
    for(auto id:group.members){owner[id]=units.size();unit.members.push_back(instructions.at(id));}
    units.push_back(std::move(unit));
  }
  for(auto& i:region.instructions)if(!owner.contains(i.id)){owner[i.id]=units.size();units.push_back({{&i},false,false,0,0,source.at(i.id)});}
  std::map<InstrId,uint64_t> heights;std::map<InstrId,std::vector<const Dependency*>> instruction_edges;
  for(auto& d:region.deps)instruction_edges[d.producer].push_back(&d);
  std::function<uint64_t(InstrId)> height=[&](InstrId id){if(heights.contains(id))return heights.at(id);uint64_t h=instructions.at(id)->latency;for(auto edge:instruction_edges[id])h=std::max(h,uint64_t(edge->latency)+height(edge->consumer));return heights[id]=h;};
  for(auto& unit:units){int priority=INT32_MIN;for(auto i:unit.members){priority=std::max(priority,i->priority);unit.constrained|=!i->issue_slots.empty();if(machine.critical_path)unit.height=std::max(unit.height,height(i->id));}unit.priority+=priority;}
  // Same-cycle members can surround zero-latency dependent instructions. Such
  // paths force the intervening units into that cycle, but preserve adjacency
  // inside any bundles/atomic units. Only same_cycle permits this interleaving.
  std::vector<std::vector<size_t>> graph(units.size()),components;
  for(auto& d:region.deps){auto a=owner.at(d.producer),b=owner.at(d.consumer);if(a!=b)graph[a].push_back(b);}
  std::vector<size_t> indices(units.size(),SIZE_MAX),low(units.size()),stack;std::vector<bool> active(units.size());size_t next=0;
  std::function<void(size_t)> component=[&](size_t v){indices[v]=low[v]=next++;stack.push_back(v);active[v]=true;for(auto w:graph[v]){if(indices[w]==SIZE_MAX){component(w);low[v]=std::min(low[v],low[w]);}else if(active[w])low[v]=std::min(low[v],indices[w]);}if(low[v]==indices[v]){std::vector<size_t> members;for(;;){auto w=stack.back();stack.pop_back();active[w]=false;members.push_back(w);if(w==v)break;}components.push_back(std::move(members));}};
  for(size_t k=0;k<units.size();++k)if(indices[k]==SIZE_MAX)component(k);
  std::vector<Unit> merged;
  for(auto& members:components) {
    if(members.size()==1){merged.push_back(std::move(units[members[0]]));continue;}
    if(std::none_of(members.begin(),members.end(),[&](auto k){return units[k].relaxed;}))return Output::err({Error::Code::Unsatisfiable,"grouping requires interleaving adjacent scheduling units"});
    std::set<size_t> inside(members.begin(),members.end());std::vector<std::vector<const Instruction*>> blocks;std::map<InstrId,size_t> local_owner;
    Unit unit;unit.same=true;unit.source=SIZE_MAX;unit.priority=INT64_MIN;
    for(auto k:members){auto& original=units[k];unit.source=std::min(unit.source,original.source);unit.priority=std::max(unit.priority,original.priority);unit.height=std::max(unit.height,original.height);unit.constrained|=original.constrained;if(original.relaxed)for(auto i:original.members){local_owner[i->id]=blocks.size();blocks.push_back({i});}else {for(auto i:original.members)local_owner[i->id]=blocks.size();blocks.push_back(original.members);}}
    std::vector<size_t> incoming(blocks.size());std::vector<std::vector<size_t>> edges(blocks.size());
    for(auto& d:region.deps)if(inside.contains(owner.at(d.producer))&&inside.contains(owner.at(d.consumer))){if(d.latency)return Output::err({Error::Code::Unsatisfiable,"same-cycle path has positive latency"});auto a=local_owner.at(d.producer),b=local_owner.at(d.consumer);if(a!=b){++incoming[b];edges[a].push_back(b);}}
    std::vector<bool> emitted(blocks.size());size_t count=0;
    while(count<blocks.size()){std::optional<size_t> ready;for(size_t k=0;k<blocks.size();++k)if(!emitted[k]&&!incoming[k]&&(!ready||source.at(blocks[k].front()->id)<source.at(blocks[*ready].front()->id)))ready=k;if(!ready)return Output::err({Error::Code::Unsatisfiable,"same-cycle path breaks an adjacent scheduling unit"});unit.members.insert(unit.members.end(),blocks[*ready].begin(),blocks[*ready].end());emitted[*ready]=true;++count;for(auto child:edges[*ready])--incoming[child];}
    merged.push_back(std::move(unit));
  }
  units=std::move(merged);owner.clear();for(size_t k=0;k<units.size();++k)for(auto i:units[k].members)owner[i->id]=k;
  std::vector<size_t> indegree(units.size());std::vector<std::vector<size_t>> outgoing(units.size());
  for(auto& d:region.deps){auto a=owner.at(d.producer),b=owner.at(d.consumer);if(a!=b){++indegree[b];outgoing[a].push_back(b);}else if(units[a].same&&d.latency)return Output::err({Error::Code::Unsatisfiable,"same-cycle group has a positive-latency dependency"});}
  for(auto& unit:units)if(unit.same){Usage usage;Issues issues;Slots slots;std::vector<Scheduled> plan;if(!pack(unit.members,machine,0,usage,issues,slots,plan))return Output::err({Error::Code::Unsatisfiable,"same-cycle group cannot fit resources or issue slots"});}
  Usage usage;Issues issued;Slots slots;std::map<InstrId,uint32_t> cycles;std::vector<bool> done(units.size());
  std::vector<Scheduled> result;uint64_t floor=0;size_t completed=0;
  while(completed<units.size()) {
    std::optional<size_t> best;std::vector<Scheduled> best_plan;Usage best_usage;Issues best_issued;Slots best_slots;
    for(size_t k=0;k<units.size();++k)if(!done[k]&&!indegree[k]) {
      auto& unit=units[k];Usage trial=usage;Issues trial_issued=issued;Slots trial_slots=slots;std::vector<Scheduled> plan;auto local_cycles=cycles;
      auto earliest=[&](InstrId id){uint64_t cycle=floor;for(auto& d:region.deps)if(d.consumer==id&&local_cycles.contains(d.producer))cycle=std::max(cycle,uint64_t(local_cycles.at(d.producer))+d.latency);return cycle;};
      if(unit.same) {
        uint64_t cycle=floor;for(auto i:unit.members)cycle=std::max(cycle,earliest(i->id));
        for(;;++cycle){if(cycle>UINT32_MAX)return Output::err({Error::Code::ResourceLimit,"group schedule cycle overflow"});if(pack(unit.members,machine,cycle,trial,trial_issued,trial_slots,plan))break;}
      }else for(auto i:unit.members) {
        uint64_t cycle=earliest(i->id);if(!plan.empty())cycle=std::max(cycle,uint64_t(plan.back().cycle));
        Scheduled entry{i->id,0};
        for(;;++cycle) {
          if(cycle>UINT32_MAX)return Output::err({Error::Code::ResourceLimit,"group schedule cycle overflow"});
          if(machine.issue_width&&trial_issued[cycle]>=machine.issue_width)continue;
          auto allowed=i->issue_slots;if(allowed.empty()&&machine.issue_width)for(uint64_t slot=0;slot<machine.issue_width;++slot)allowed.push_back(uint32_t(slot));std::sort(allowed.begin(),allowed.end());
          std::optional<uint32_t> slot;for(auto s:allowed)if(!trial_slots[cycle].contains(s)){slot=s;break;}if(machine.issue_width&&!slot)continue;
          if(!reserve(*i,machine,cycle,trial,0,{},&entry.resources))continue;
          entry.cycle=uint32_t(cycle);entry.slot=slot;++trial_issued[cycle];if(slot)trial_slots[cycle].insert(*slot);break;
        }
        local_cycles[i->id]=entry.cycle;plan.push_back(std::move(entry));
      }
      auto score=[&](size_t n){return std::tuple{units[n].priority,units[n].height,units[n].constrained};};
      if(!best||plan.front().cycle<best_plan.front().cycle||(plan.front().cycle==best_plan.front().cycle&&(score(k)>score(*best)||(score(k)==score(*best)&&unit.source<units[*best].source)))){best=k;best_plan=std::move(plan);best_usage=std::move(trial);best_issued=std::move(trial_issued);best_slots=std::move(trial_slots);}
    }
    if(!best)return Output::err({Error::Code::Unsatisfiable,"grouping requires interleaving dependent scheduling units"});
    usage=std::move(best_usage);issued=std::move(best_issued);slots=std::move(best_slots);floor=best_plan.back().cycle;
    for(auto& s:best_plan)cycles[s.id]=s.cycle;result.insert(result.end(),best_plan.begin(),best_plan.end());done[*best]=true;++completed;for(auto next:outgoing[*best])--indegree[next];
  }
  return Output::ok(std::move(result));
}
}

Result<Region> dependencies(const Region& input,std::span<const std::pair<uint32_t,uint32_t>> physical_aliases) {
  auto valid=validate_region(input);if(!valid)return Result<Region>::err(valid.error());
  if(!input.blocks.empty()) {
    Region combined=input;
    for(auto& block:input.blocks){auto local=block_region(input,block.id);if(!local)return local;auto result=dependencies(local.value(),physical_aliases);if(!result)return result;for(auto& d:result.value().deps)if(std::none_of(combined.deps.begin(),combined.deps.end(),[&](auto& e){return e.producer==d.producer&&e.consumer==d.consumer&&e.kind==d.kind&&e.distance==d.distance&&e.latency>=d.latency&&e.scheduler_only==d.scheduler_only;}))combined.deps.push_back(d);}
    return Result<Region>::ok(std::move(combined));
  }
  Region r=input;
  std::unordered_set<InstrId> ids;
  for(auto& i:r.instructions)if(!ids.insert(i.id).second)return Result<Region>::err({Error::Code::InvalidArgument,"duplicate instruction id"});
  auto edge=[&](InstrId a,InstrId b,DepKind kind,uint32_t latency,bool scheduling=false) {
    if(a==b)return;
    auto it=std::find_if(r.deps.begin(),r.deps.end(),[&](auto& d){return d.producer==a&&d.consumer==b&&d.kind==kind&&d.distance==0&&d.scheduler_only==scheduling;});
    if(it==r.deps.end())r.deps.push_back({a,b,kind,latency,0,scheduling});else it->latency=std::max(it->latency,latency);
  };
  for(auto& group:r.groups) {
    for(size_t k=1;k<group.members.size();++k)edge(group.members[k-1],group.members[k],DepKind::Ordering,0,true);
    if(!group.issue_slots.empty())for(auto id:group.members){auto& i=*std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& x){return x.id==id;});if(i.issue_slots.empty())i.issue_slots=group.issue_slots;else{std::vector<uint32_t> allowed;for(auto slot:i.issue_slots)if(std::find(group.issue_slots.begin(),group.issue_slots.end(),slot)==group.issue_slots.end())continue;else allowed.push_back(slot);if(allowed.empty())return Result<Region>::err({Error::Code::Unsatisfiable,"instruction and group have disjoint issue slots"});i.issue_slots=std::move(allowed);}}
  }
  std::unordered_map<ValueId,const Instruction*> writer;
  std::unordered_map<ValueId,std::vector<InstrId>> readers;
  // Physical implicit effects have a separate identity space from virtual values.
  std::unordered_map<ValueId,const Instruction*> physical_writer;
  std::unordered_map<ValueId,std::vector<InstrId>> physical_readers;
   auto hazards=[&](const Instruction& i,const std::vector<ValueId>& uses,const std::vector<ValueId>& defs,auto& writers,auto& reads,bool implicit) {
    for(auto v:uses) {
      if(writers.contains(v)) {
        const auto& producer=*writers[v];auto latency=producer.latency;
         auto& latencies=implicit?producer.implicit_result_latency:producer.result_latency;
         if(auto found=latencies.find(v);found!=latencies.end())latency=found->second;
        edge(producer.id,i.id,DepKind::True,latency);
      }
      reads[v].push_back(i.id);
    }
    for(auto v:defs) {
      if(writers.contains(v))edge(writers[v]->id,i.id,DepKind::Output,0);
      for(auto reader:reads[v])edge(reader,i.id,DepKind::Anti,0);
      reads[v].clear();writers[v]=&i;
    }
  };
  auto memory=[](const Instruction& i){return i.access.value_or(MemoryAccess{i.memory,i.memory});};
  auto alias=[](const MemoryAccess& a,const MemoryAccess& b) {
    if(!a.address_space.empty()&&!b.address_space.empty()&&a.address_space!=b.address_space)return false;
    if(a.alias_sets.empty()||b.alias_sets.empty())return true;
    for(auto x:a.alias_sets)if(std::find(b.alias_sets.begin(),b.alias_sets.end(),x)!=b.alias_sets.end())return true;
    return false;
  };
  for(size_t index=0;index<r.instructions.size();++index) {
    const auto& i=r.instructions[index];
     hazards(i,i.uses,i.defs,writer,readers,false);
     hazards(i,i.implicit_uses,i.implicit_defs,physical_writer,physical_readers,true);
    auto current=memory(i);
    for(size_t k=0;k<index;++k) {
      const auto& prior=r.instructions[k];auto previous=memory(prior);
      auto overlaps=[&](uint32_t a,uint32_t b){return a==b||std::find(physical_aliases.begin(),physical_aliases.end(),std::pair{a,b})!=physical_aliases.end()||std::find(physical_aliases.begin(),physical_aliases.end(),std::pair{b,a})!=physical_aliases.end();};
       for(auto a:prior.implicit_defs){auto latency=prior.implicit_result_latency.contains(a)?prior.implicit_result_latency.at(a):prior.latency;for(auto b:i.implicit_uses)if(a!=b&&overlaps(a,b))edge(prior.id,i.id,DepKind::True,latency);for(auto b:i.implicit_defs)if(a!=b&&overlaps(a,b))edge(prior.id,i.id,DepKind::Output,0);}
      for(auto a:prior.implicit_uses)for(auto b:i.implicit_defs)if(a!=b&&overlaps(a,b))edge(prior.id,i.id,DepKind::Anti,0);
      bool a=previous.read||previous.write||previous.ordering!=MemoryOrdering::Relaxed||previous.volatile_access;
      bool b=current.read||current.write||current.ordering!=MemoryOrdering::Relaxed||current.volatile_access;
      if(a&&b) {
        bool ordering=previous.volatile_access||current.volatile_access||previous.ordering==MemoryOrdering::Acquire||previous.ordering==MemoryOrdering::AcquireRelease||previous.ordering==MemoryOrdering::Sequential||current.ordering==MemoryOrdering::Release||current.ordering==MemoryOrdering::AcquireRelease||current.ordering==MemoryOrdering::Sequential;
        if(ordering||((previous.write||current.write)&&alias(previous,current)))edge(prior.id,i.id,ordering?DepKind::Ordering:DepKind::Memory,0);
      }
      if((prior.may_trap&&!i.speculative)||(i.may_trap&&!prior.speculative))edge(prior.id,i.id,DepKind::Ordering,0);
    }
    if(i.barrier||i.call) {
      for(size_t k=0;k<index;++k)edge(r.instructions[k].id,i.id,DepKind::Ordering,0,true);
      for(size_t k=index+1;k<r.instructions.size();++k)edge(i.id,r.instructions[k].id,DepKind::Ordering,0,true);
    }
    if(i.terminator)for(size_t k=0;k<index;++k)edge(r.instructions[k].id,i.id,DepKind::Control,0);
  }
  return Result<Region>::ok(std::move(r));
}

Result<std::vector<Scheduled>> schedule(const Region& input,const MachineModel& m) {
  if(!input.blocks.empty())return schedule_cfg(input,m);
  auto augmented=dependencies(input,m.register_aliases);if(!augmented)return Result<std::vector<Scheduled>>::err(augmented.error());
  const auto& r=augmented.value();auto valid=validate(r,m);if(!valid)return Result<std::vector<Scheduled>>::err(valid.error());
  if(!r.groups.empty())return grouped(r,m);
  std::map<InstrId,const Instruction*> instructions;
  std::map<InstrId,size_t> indegree;std::map<InstrId,std::vector<const Dependency*>> out;
  for(auto& i:r.instructions){instructions[i.id]=&i;indegree[i.id]=0;
    Usage isolated;if(!reserve(i,m,0,isolated))return Result<std::vector<Scheduled>>::err({Error::Code::Unsatisfiable,"instruction reservations cannot fit: "+std::to_string(i.id)});
  }
  for(auto& d:r.deps){++indegree[d.consumer];out[d.producer].push_back(&d);}
  Usage used;std::map<uint64_t,uint32_t> issued;std::map<InstrId,uint32_t> cycles;
  std::map<uint64_t,std::unordered_set<uint32_t>> slots;
  std::map<InstrId,uint64_t> height;
  std::function<uint64_t(InstrId)> rank=[&](InstrId id){if(height.contains(id))return height[id];uint64_t h=instructions[id]->latency;for(auto d:out[id])h=std::max(h,uint64_t(d->latency)+rank(d->consumer));return height[id]=h;};
  if(m.critical_path)for(auto [id,i]:instructions)rank(id);
  std::vector<Scheduled> result;
  while(result.size()<r.instructions.size()) {
    const Instruction* best=nullptr;uint64_t best_cycle=std::numeric_limits<uint64_t>::max();
    for(auto [id,count]:indegree)if(!count&&!cycles.contains(id)) {
      const auto& i=*instructions[id];uint64_t c=0;
      for(auto& d:r.deps)if(d.consumer==id)c=std::max(c,uint64_t(cycles.at(d.producer))+d.latency);
      for(;;++c) {
        if(c>std::numeric_limits<uint32_t>::max())return Result<std::vector<Scheduled>>::err({Error::Code::ResourceLimit,"schedule cycle overflow"});
        if(m.issue_width&&issued[c]>=m.issue_width)continue;
        if(!i.issue_slots.empty()&&std::all_of(i.issue_slots.begin(),i.issue_slots.end(),[&](auto slot){return slots[c].contains(slot);}))continue;
        Usage trial=used;if(reserve(i,m,c,trial))break;
      }
      auto constrained=[](const Instruction& x){return !x.issue_slots.empty();};
      if(c<best_cycle||(c==best_cycle&&best&&std::tuple{i.priority,height[id],constrained(i)}>std::tuple{best->priority,height[best->id],constrained(*best)})){best=&i;best_cycle=c;}
    }
    if(!best)return Result<std::vector<Scheduled>>::err({Error::Code::Conflict,"cyclic scheduling dependencies"});
    Scheduled entry{best->id,static_cast<uint32_t>(best_cycle)};
    reserve(*best,m,best_cycle,used,0,{},&entry.resources);++issued[best_cycle];cycles[best->id]=static_cast<uint32_t>(best_cycle);
    if(m.issue_width){auto allowed=best->issue_slots;if(allowed.empty())for(uint32_t slot=0;slot<m.issue_width;++slot)allowed.push_back(slot);std::sort(allowed.begin(),allowed.end());for(auto slot:allowed)if(!slots[best_cycle].contains(slot)){entry.slot=slot;slots[best_cycle].insert(slot);break;}}
    result.push_back(std::move(entry));
    for(auto d:out[best->id])--indegree[d->consumer];
  }
  // Equal-cycle operations retain their deterministic topological issue order.
  std::stable_sort(result.begin(),result.end(),[](auto& a,auto& b){return a.cycle<b.cycle;});
  return Result<std::vector<Scheduled>>::ok(std::move(result));
}

Result<int> verify(const Region& input,const MachineModel& m,std::span<const Scheduled> schedule) {
  if(!input.blocks.empty())return verify_cfg(input,m,schedule);
  auto augmented=dependencies(input,m.register_aliases);if(!augmented)return Result<int>::err(augmented.error());
  auto& r=augmented.value();auto valid=validate(r,m);if(!valid)return valid;
  std::map<InstrId,uint32_t> cycles;std::map<InstrId,size_t> positions;
  for(size_t k=0;k<schedule.size();++k){auto& s=schedule[k];if(!cycles.emplace(s.id,s.cycle).second)return Result<int>::err({Error::Code::Conflict,"duplicate scheduled instruction"});positions[s.id]=k;}
  if(cycles.size()!=r.instructions.size())return Result<int>::err({Error::Code::Conflict,"incomplete schedule"});
  for(auto& i:r.instructions)if(!cycles.contains(i.id))return Result<int>::err({Error::Code::Conflict,"unknown or missing scheduled instruction"});
  for(auto& d:r.deps){if(uint64_t(cycles.at(d.consumer))<uint64_t(cycles.at(d.producer))+d.latency)return Result<int>::err({Error::Code::Conflict,"dependency latency violation"});if(positions.at(d.consumer)<=positions.at(d.producer))return Result<int>::err({Error::Code::Conflict,"issue order violates a dependency"});}
  std::vector<InstrId> emission;for(auto& s:schedule)emission.push_back(s.id);valid=verify_group_order(r,emission);if(!valid)return valid;
  Usage used;std::map<uint32_t,uint32_t> issued;
  std::map<uint32_t,std::unordered_set<uint32_t>> slots;
  auto ordered=schedule;std::vector<Scheduled> sorted(ordered.begin(),ordered.end());
  std::sort(sorted.begin(),sorted.end(),[](auto& a,auto& b){return std::tie(a.cycle,a.id)<std::tie(b.cycle,b.id);});
  for(auto& s:sorted) {
    auto it=std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& i){return i.id==s.id;});
    if(it==r.instructions.end())return Result<int>::err({Error::Code::Conflict,"unknown scheduled instruction"});
    if(m.issue_width&&++issued[s.cycle]>m.issue_width)return Result<int>::err({Error::Code::Conflict,"issue width exceeded"});
    if(s.slot&&(!m.issue_width||*s.slot>=m.issue_width||(!it->issue_slots.empty()&&std::find(it->issue_slots.begin(),it->issue_slots.end(),*s.slot)==it->issue_slots.end())||!slots[s.cycle].insert(*s.slot).second))return Result<int>::err({Error::Code::Conflict,"invalid or duplicate issue slot"});
    if(!it->issue_slots.empty()&&!s.slot)return Result<int>::err({Error::Code::Conflict,"missing issue slot assignment"});
    if(!s.resources.empty()&&s.resources.size()!=it->resources.size())return Result<int>::err({Error::Code::Conflict,"incomplete resource assignment"});
  }
  if(m.issue_width) {
    std::map<uint32_t,std::vector<std::pair<const Instruction*,std::optional<uint32_t>>>> groups;
    for(auto& s:sorted){auto i=std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& x){return x.id==s.id;});groups[s.cycle].push_back({&*i,s.slot});}
    for(auto& [cycle,instructions]:groups)if(!detail::slots_feasible(m.issue_width,instructions))return Result<int>::err({Error::Code::Conflict,"issue slot constraints are infeasible"});
  }
  // A schedule describes issue cycles, so verification must consider every
  // possible port assignment rather than commit greedily to the first port.
  std::function<bool(size_t)> reservations=[&](size_t index) {
    if(index==sorted.size())return true;
    auto& s=sorted[index];
    auto& i=*std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& x){return x.id==s.id;});
    if(s.resources.empty())return reserve(i,m,s.cycle,used,0,[&]{return reservations(index+1);});
    auto fixed=i;
    for(size_t k=0;k<i.resources.size();++k) {
      auto& u=i.resources[k];auto& selected=s.resources[k];
      if((u.alternatives.empty()&&selected!=u.resource)||(!u.alternatives.empty()&&std::find(u.alternatives.begin(),u.alternatives.end(),selected)==u.alternatives.end()))return false;
      fixed.resources[k].resource=selected;fixed.resources[k].alternatives.clear();
    }
    return reserve(fixed,m,s.cycle,used,0,[&]{return reservations(index+1);});
  };
  if(!reservations(0))return Result<int>::err({Error::Code::Conflict,"resource capacity exceeded"});
  return verify_groups(r,schedule);
}
namespace detail {
bool slots_feasible(uint32_t width,const std::vector<std::pair<const Instruction*,std::optional<uint32_t>>>& instructions) {
  std::map<uint32_t,size_t> occupied;
  std::set<size_t> fixed;
  for(size_t k=0;k<instructions.size();++k)if(instructions[k].second){if(!occupied.emplace(*instructions[k].second,k).second)return false;fixed.insert(k);}
  std::function<bool(size_t,std::set<uint32_t>&)> assign=[&](size_t index,std::set<uint32_t>& seen) {
    auto attempt=[&](uint32_t slot) {
      if(!seen.insert(slot).second)return false;
      auto current=occupied.find(slot);
      if(current==occupied.end()||(!fixed.contains(current->second)&&assign(current->second,seen))){occupied[slot]=index;return true;}return false;
    };
    auto& allowed=instructions[index].first->issue_slots;
    if(allowed.empty()){for(uint64_t slot=0;slot<width;++slot)if(attempt(static_cast<uint32_t>(slot)))return true;}
    else for(auto slot:allowed)if(attempt(slot))return true;
    return false;
  };
  for(size_t k=0;k<instructions.size();++k)if(!fixed.contains(k)){std::set<uint32_t> seen;if(!assign(k,seen))return false;}
  return true;
}
}
std::string print(const Region& r) {
  std::string s="region "+r.name+" {\n";
  for(auto& i:r.instructions)s+="  "+std::to_string(i.id)+": "+i.opcode+"\n";
  for(auto& d:r.deps)s+="  dep "+std::to_string(d.producer)+" -> "+std::to_string(d.consumer)+" latency="+std::to_string(d.latency)+" distance="+std::to_string(d.distance)+(d.scheduler_only?" scheduling":" semantic")+"\n";
  for(auto& group:r.groups){s+="  group "+std::to_string(group.id)+" "+group.name+" kind="+std::to_string(static_cast<int>(group.kind))+" members=";for(auto id:group.members)s+=std::to_string(id)+" ";s+="\n";}
  return s+"}\n";
}
}
