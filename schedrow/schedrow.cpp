#include "schedrow.hpp"
#include <cmath>
#include <limits>
#include <map>

namespace limestone::schedrow {
namespace {
Result<int> validate(const Region& r,const MachineModel& m) {
  std::unordered_set<InstrId> ids;
  for(auto&[name,cap]:m.resource_capacity)if(name.empty()||!cap)return Result<int>::err({Error::Code::InvalidArgument,"invalid resource capacity"});
  for(auto& i:r.instructions) {
    if(!ids.insert(i.id).second)return Result<int>::err({Error::Code::InvalidArgument,"duplicate instruction id"});
    if(!std::isfinite(i.throughput)||i.throughput<=0)return Result<int>::err({Error::Code::InvalidArgument,"invalid throughput"});
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
bool reserve(const Instruction& i,const MachineModel& m,uint64_t cycle,Usage& used,size_t index=0,const std::function<bool()>& accept={}) {
  if(index==i.resources.size())return !accept||accept();
  const auto& u=i.resources[index];
  auto names=u.alternatives.empty()?std::vector<std::string>{u.resource}:u.alternatives;
  std::sort(names.begin(),names.end());
  for(auto& name:names) {
    bool fits=true;
    for(uint64_t k=0;k<u.duration;++k)if(used[name][cycle+u.offset+k]+u.quantity>m.resource_capacity.at(name)+1e-9){fits=false;break;}
    if(!fits)continue;
    for(uint64_t k=0;k<u.duration;++k)used[name][cycle+u.offset+k]+=u.quantity;
    if(reserve(i,m,cycle,used,index+1,accept))return true;
    for(uint64_t k=0;k<u.duration;++k)used[name][cycle+u.offset+k]-=u.quantity;
  }
  return false;
}
}

Result<Region> dependencies(const Region& input) {
  Region r=input;
  std::unordered_set<InstrId> ids;
  for(auto& i:r.instructions)if(!ids.insert(i.id).second)return Result<Region>::err({Error::Code::InvalidArgument,"duplicate instruction id"});
  auto edge=[&](InstrId a,InstrId b,DepKind kind,uint32_t latency,bool scheduling=false) {
    if(a==b)return;
    auto it=std::find_if(r.deps.begin(),r.deps.end(),[&](auto& d){return d.producer==a&&d.consumer==b&&d.kind==kind&&d.distance==0&&d.scheduler_only==scheduling;});
    if(it==r.deps.end())r.deps.push_back({a,b,kind,latency,0,scheduling});else it->latency=std::max(it->latency,latency);
  };
  std::unordered_map<ValueId,const Instruction*> writer;
  std::unordered_map<ValueId,std::vector<InstrId>> readers;
  for(size_t index=0;index<r.instructions.size();++index) {
    const auto& i=r.instructions[index];auto uses=i.uses,defs=i.defs;
    uses.insert(uses.end(),i.implicit_uses.begin(),i.implicit_uses.end());
    defs.insert(defs.end(),i.implicit_defs.begin(),i.implicit_defs.end());
    for(auto v:uses) { if(writer.contains(v))edge(writer[v]->id,i.id,DepKind::True,writer[v]->latency);readers[v].push_back(i.id); }
    for(auto v:defs) {
      if(writer.contains(v))edge(writer[v]->id,i.id,DepKind::Output,0);
      for(auto reader:readers[v])edge(reader,i.id,DepKind::Anti,0);
      readers[v].clear();writer[v]=&i;
    }
    if(i.barrier) {
      for(size_t k=0;k<index;++k)edge(r.instructions[k].id,i.id,DepKind::Ordering,0,true);
      for(size_t k=index+1;k<r.instructions.size();++k)edge(i.id,r.instructions[k].id,DepKind::Ordering,0,true);
    }
  }
  return Result<Region>::ok(std::move(r));
}

Result<std::vector<Scheduled>> schedule(const Region& input,const MachineModel& m) {
  auto augmented=dependencies(input);if(!augmented)return Result<std::vector<Scheduled>>::err(augmented.error());
  const auto& r=augmented.value();auto valid=validate(r,m);if(!valid)return Result<std::vector<Scheduled>>::err(valid.error());
  std::map<InstrId,const Instruction*> instructions;
  std::map<InstrId,size_t> indegree;std::map<InstrId,std::vector<const Dependency*>> out;
  for(auto& i:r.instructions){instructions[i.id]=&i;indegree[i.id]=0;
    Usage isolated;if(!reserve(i,m,0,isolated))return Result<std::vector<Scheduled>>::err({Error::Code::Unsatisfiable,"instruction reservations cannot fit: "+std::to_string(i.id)});
  }
  for(auto& d:r.deps){++indegree[d.consumer];out[d.producer].push_back(&d);}
  Usage used;std::map<uint64_t,uint32_t> issued;std::map<InstrId,uint32_t> cycles;
  std::vector<Scheduled> result;
  while(result.size()<r.instructions.size()) {
    const Instruction* best=nullptr;uint64_t best_cycle=std::numeric_limits<uint64_t>::max();
    for(auto [id,count]:indegree)if(!count&&!cycles.contains(id)) {
      const auto& i=*instructions[id];uint64_t c=0;
      for(auto& d:r.deps)if(d.consumer==id)c=std::max(c,uint64_t(cycles.at(d.producer))+d.latency);
      for(;;++c) {
        if(c>std::numeric_limits<uint32_t>::max())return Result<std::vector<Scheduled>>::err({Error::Code::ResourceLimit,"schedule cycle overflow"});
        if(m.issue_width&&issued[c]>=m.issue_width)continue;
        Usage trial=used;if(reserve(i,m,c,trial))break;
      }
      if(c<best_cycle){best=&i;best_cycle=c;}
    }
    if(!best)return Result<std::vector<Scheduled>>::err({Error::Code::Conflict,"cyclic scheduling dependencies"});
    reserve(*best,m,best_cycle,used);++issued[best_cycle];cycles[best->id]=static_cast<uint32_t>(best_cycle);
    result.push_back({best->id,static_cast<uint32_t>(best_cycle)});
    for(auto d:out[best->id])--indegree[d->consumer];
  }
  // Equal-cycle operations retain their deterministic topological issue order.
  std::stable_sort(result.begin(),result.end(),[](auto& a,auto& b){return a.cycle<b.cycle;});
  return Result<std::vector<Scheduled>>::ok(std::move(result));
}

Result<int> verify(const Region& input,const MachineModel& m,std::span<const Scheduled> schedule) {
  auto augmented=dependencies(input);if(!augmented)return Result<int>::err(augmented.error());
  auto& r=augmented.value();auto valid=validate(r,m);if(!valid)return valid;
  std::map<InstrId,uint32_t> cycles;
  for(auto& s:schedule)if(!cycles.emplace(s.id,s.cycle).second)return Result<int>::err({Error::Code::Conflict,"duplicate scheduled instruction"});
  if(cycles.size()!=r.instructions.size())return Result<int>::err({Error::Code::Conflict,"incomplete schedule"});
  for(auto& i:r.instructions)if(!cycles.contains(i.id))return Result<int>::err({Error::Code::Conflict,"unknown or missing scheduled instruction"});
  for(auto& d:r.deps)if(uint64_t(cycles.at(d.consumer))<uint64_t(cycles.at(d.producer))+d.latency)return Result<int>::err({Error::Code::Conflict,"dependency latency violation"});
  Usage used;std::map<uint32_t,uint32_t> issued;
  auto ordered=schedule;std::vector<Scheduled> sorted(ordered.begin(),ordered.end());
  std::sort(sorted.begin(),sorted.end(),[](auto& a,auto& b){return std::tie(a.cycle,a.id)<std::tie(b.cycle,b.id);});
  for(auto& s:sorted) {
    auto it=std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& i){return i.id==s.id;});
    if(it==r.instructions.end())return Result<int>::err({Error::Code::Conflict,"unknown scheduled instruction"});
    if(m.issue_width&&++issued[s.cycle]>m.issue_width)return Result<int>::err({Error::Code::Conflict,"issue width exceeded"});
  }
  // A schedule describes issue cycles, so verification must consider every
  // possible port assignment rather than commit greedily to the first port.
  std::function<bool(size_t)> reservations=[&](size_t index) {
    if(index==sorted.size())return true;
    auto& s=sorted[index];
    auto& i=*std::find_if(r.instructions.begin(),r.instructions.end(),[&](auto& x){return x.id==s.id;});
    return reserve(i,m,s.cycle,used,0,[&]{return reservations(index+1);});
  };
  if(!reservations(0))return Result<int>::err({Error::Code::Conflict,"resource capacity exceeded"});
  return Result<int>::ok(0);
}
std::string print(const Region& r) {
  std::string s="region "+r.name+" {\n";
  for(auto& i:r.instructions)s+="  "+std::to_string(i.id)+": "+i.opcode+"\n";
  for(auto& d:r.deps)s+="  dep "+std::to_string(d.producer)+" -> "+std::to_string(d.consumer)+" latency="+std::to_string(d.latency)+" distance="+std::to_string(d.distance)+(d.scheduler_only?" scheduling":" semantic")+"\n";
  return s+"}\n";
}
}
