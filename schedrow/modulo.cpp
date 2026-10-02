#include "schedrow.hpp"
#include "slots.hpp"
#include "groups.hpp"
#include <cmath>
#include <map>
#include <set>

namespace limestone::schedrow {
namespace {
using Usage=std::map<std::pair<std::string,uint32_t>,double>;
bool latency(const Dependency& d,uint32_t producer,uint32_t consumer,uint32_t interval) {
  uint64_t required=uint64_t(producer)+d.latency;
  return consumer>=required||uint64_t(d.distance)*interval>=required-consumer;
}
Result<Region> checked(const Region& input,const MachineModel& machine,uint32_t interval,std::vector<InstrId>* order=nullptr) {
  if(!interval)return Result<Region>::err({Error::Code::InvalidArgument,"zero modulo initiation interval"});
  if(input.blocks.size()>1)return Result<Region>::err({Error::Code::Unsupported,"multi-block modulo scheduling needs a loop-region adapter"});
  auto augmented=dependencies(input,machine.register_aliases);if(!augmented)return augmented;
  auto straight=augmented.value();std::erase_if(straight.deps,[](auto& edge){return edge.distance!=0;});
  // Reuse list-scheduler validation without changing the loop's dependency model.
  auto valid=schedule(straight,machine);if(!valid)return Result<Region>::err(valid.error());
  if(order)for(auto& s:valid.value())order->push_back(s.id);
  std::set<InstrId> ids;for(auto& i:input.instructions)ids.insert(i.id);
  for(auto& d:augmented.value().deps)if(!ids.contains(d.producer)||!ids.contains(d.consumer))return Result<Region>::err({Error::Code::InvalidArgument,"unknown modulo dependency endpoint"});
  return augmented;
}
bool reserve(const Instruction& i,const MachineModel& m,uint32_t cycle,uint32_t interval,Usage& usage,std::vector<std::string>& choices,size_t k,const std::function<bool()>& accept) {
  if(k==i.resources.size())return accept();
  auto& u=i.resources[k];auto names=u.alternatives.empty()?std::vector<std::string>{u.resource}:u.alternatives;std::sort(names.begin(),names.end());
  for(auto& name:names) {
    std::map<uint32_t,double> reservations;
    auto full=u.duration/interval,remainder=u.duration%interval;
    if(full)for(uint64_t slot=0;slot<interval;++slot)reservations[static_cast<uint32_t>(slot)]=full*u.quantity;
    for(uint64_t offset=0;offset<remainder;++offset)reservations[static_cast<uint32_t>((uint64_t(cycle)+u.offset+offset)%interval)]+=u.quantity;
    bool fits=true;for(auto [slot,amount]:reservations)if(usage[{name,slot}]+amount>m.resource_capacity.at(name)+1e-9)fits=false;
    if(!fits)continue;
    for(auto [slot,amount]:reservations)usage[{name,slot}]+=amount;
    choices.push_back(name);
    if(reserve(i,m,cycle,interval,usage,choices,k+1,accept))return true;
    choices.pop_back();for(auto [slot,amount]:reservations)usage[{name,slot}]-=amount;
  }
  return false;
}
}
Result<std::vector<Scheduled>> schedule_modulo(const Region& input,const MachineModel& m,ModuloOptions options) {
  std::vector<InstrId> identities;auto valid=checked(input,m,options.initiation_interval,&identities);if(!valid)return Result<std::vector<Scheduled>>::err(valid.error());auto& r=valid.value();
  std::vector<const Instruction*> order;for(auto id:identities)order.push_back(&*std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& i){return i.id==id;}));
  Usage used;std::map<InstrId,uint32_t> cycles;std::map<uint32_t,uint32_t> issued;std::map<uint32_t,std::set<uint32_t>> slots;
  std::vector<Scheduled> result;size_t visits=0;bool exhausted=false;
  std::function<bool(size_t)> search=[&](size_t k) {
    if(k==order.size())return bool(verify_groups(r,result));const auto& i=*order[k];
    for(uint64_t c=0;c<=options.max_cycle;++c) {
      if(++visits>options.search_limit){exhausted=true;return false;}
      auto cycle=static_cast<uint32_t>(c),phase=cycle%options.initiation_interval;bool legal=true;
      for(auto& d:r.deps) {
        if(d.producer==i.id&&d.consumer==i.id)legal&=latency(d,cycle,cycle,options.initiation_interval);
        else if(d.producer==i.id&&cycles.contains(d.consumer))legal&=latency(d,cycle,cycles.at(d.consumer),options.initiation_interval);
        else if(d.consumer==i.id&&cycles.contains(d.producer))legal&=latency(d,cycles.at(d.producer),cycle,options.initiation_interval);
      }
      for(auto& group:r.groups)if(auto member=std::find(group.members.begin(),group.members.end(),i.id);member!=group.members.end())for(auto other=group.members.begin();other!=group.members.end();++other)if(cycles.contains(*other)){auto assigned=cycles.at(*other);legal&=detail::same_cycle(group.kind)?cycle==assigned:(other<member?assigned<=cycle:cycle<=assigned);}
      if(!legal||(m.issue_width&&issued[phase]>=m.issue_width))continue;
      auto allowed=i.issue_slots;if(allowed.empty()&&m.issue_width)for(uint32_t slot=0;slot<m.issue_width;++slot)allowed.push_back(slot);
      if(!m.issue_width)allowed={0};std::sort(allowed.begin(),allowed.end());
      for(auto slot:allowed)if(!m.issue_width||!slots[phase].contains(slot)) {
        Scheduled entry{i.id,cycle};if(m.issue_width)entry.slot=slot;
        cycles[i.id]=cycle;++issued[phase];if(m.issue_width)slots[phase].insert(slot);
        bool success=reserve(i,m,cycle,options.initiation_interval,used,entry.resources,0,[&]{result.push_back(entry);if(search(k+1))return true;result.pop_back();return false;});
        if(success)return true;
        cycles.erase(i.id);--issued[phase];if(m.issue_width)slots[phase].erase(slot);
        if(exhausted)return false;
      }
    }
    return false;
  };
  if(!search(0))return Result<std::vector<Scheduled>>::err({exhausted?Error::Code::ResourceLimit:Error::Code::Unsatisfiable,exhausted?"modulo search budget exhausted":"no modulo schedule within cycle bound"});
  std::stable_sort(result.begin(),result.end(),[](auto& a,auto& b){return a.cycle<b.cycle;});
  auto verified=verify_modulo(input,m,result,options.initiation_interval);if(!verified)return Result<std::vector<Scheduled>>::err(verified.error());
  return Result<std::vector<Scheduled>>::ok(std::move(result));
}
Result<int> verify_modulo(const Region& input,const MachineModel& m,std::span<const Scheduled> entries,uint32_t interval) {
  auto valid=checked(input,m,interval);if(!valid)return Result<int>::err(valid.error());auto& r=valid.value();
  std::map<InstrId,const Scheduled*> schedule;std::map<InstrId,size_t> positions;std::map<uint32_t,uint32_t> issued;std::map<uint32_t,std::set<uint32_t>> slots;
  for(size_t k=0;k<entries.size();++k){auto& s=entries[k];if(!schedule.emplace(s.id,&s).second)return Result<int>::err({Error::Code::Conflict,"duplicate modulo instruction"});positions[s.id]=k;}
  if(schedule.size()!=r.instructions.size())return Result<int>::err({Error::Code::Conflict,"incomplete modulo schedule"});
  std::vector<Instruction> fixed;
  std::map<uint32_t,std::vector<std::pair<const Instruction*,std::optional<uint32_t>>>> groups;
  for(auto& i:r.instructions) {
    if(!schedule.contains(i.id))return Result<int>::err({Error::Code::Conflict,"unknown or missing modulo instruction"});auto& s=*schedule.at(i.id);auto phase=s.cycle%interval;
    if(m.issue_width&&++issued[phase]>m.issue_width)return Result<int>::err({Error::Code::Conflict,"modulo issue width exceeded"});
    if(s.slot&&(!m.issue_width||*s.slot>=m.issue_width||(!i.issue_slots.empty()&&std::find(i.issue_slots.begin(),i.issue_slots.end(),*s.slot)==i.issue_slots.end())||!slots[phase].insert(*s.slot).second))return Result<int>::err({Error::Code::Conflict,"invalid modulo slot"});
    if(!i.issue_slots.empty()&&!s.slot)return Result<int>::err({Error::Code::Conflict,"missing modulo slot"});
    if(m.issue_width)groups[phase].push_back({&i,s.slot});
    auto instruction=i;
    if(!s.resources.empty()) {
      if(s.resources.size()!=i.resources.size())return Result<int>::err({Error::Code::Conflict,"incomplete modulo resources"});
      for(size_t k=0;k<i.resources.size();++k) {
        const auto& u=i.resources[k];auto& name=s.resources[k];
        if((u.alternatives.empty()&&u.resource!=name)||(!u.alternatives.empty()&&std::find(u.alternatives.begin(),u.alternatives.end(),name)==u.alternatives.end()))return Result<int>::err({Error::Code::Conflict,"illegal modulo resource"});
        instruction.resources[k].resource=name;instruction.resources[k].alternatives.clear();
      }
    }
    fixed.push_back(std::move(instruction));
  }
  for(auto& [phase,instructions]:groups)if(!detail::slots_feasible(m.issue_width,instructions))return Result<int>::err({Error::Code::Conflict,"modulo slot constraints are infeasible"});
  for(auto& d:r.deps){if(!latency(d,schedule.at(d.producer)->cycle,schedule.at(d.consumer)->cycle,interval))return Result<int>::err({Error::Code::Conflict,"loop dependency latency violation"});if(!d.distance&&positions.at(d.producer)>=positions.at(d.consumer))return Result<int>::err({Error::Code::Conflict,"modulo issue order violates an intra-iteration dependency"});}
  std::vector<InstrId> emission;for(auto& s:entries)emission.push_back(s.id);auto group_order=verify_group_order(r,emission);if(!group_order)return group_order;
  Usage used;std::vector<std::string> choices;
  std::function<bool(size_t)> reservations=[&](size_t k){if(k==fixed.size())return true;return reserve(fixed[k],m,schedule.at(fixed[k].id)->cycle,interval,used,choices,0,[&]{return reservations(k+1);});};
  if(!reservations(0))return Result<int>::err({Error::Code::Conflict,"modulo resource capacity exceeded"});
  return verify_groups(r,entries);
}
}
