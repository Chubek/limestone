#include "schedrow.hpp"
#include <queue>
namespace limestone::schedrow {
Result<std::vector<Scheduled>> schedule(const Region&r,const MachineModel&m){
 std::unordered_map<InstrId,uint32_t> indeg; std::unordered_map<InstrId,std::vector<const Dependency*>> out;
 for(auto&i:r.instructions){if(indeg.contains(i.id))return Result<std::vector<Scheduled>>::err({Error::Code::InvalidArgument,"duplicate instruction id"});indeg[i.id]=0;}
 for(auto&d:r.deps){if(!indeg.contains(d.producer)||!indeg.contains(d.consumer))return Result<std::vector<Scheduled>>::err({Error::Code::InvalidArgument,"dependency references unknown instruction"});++indeg[d.consumer];out[d.producer].push_back(&d);}
 std::vector<InstrId> ready;for(auto&[id,n]:indeg)if(!n)ready.push_back(id);std::sort(ready.begin(),ready.end());
 std::unordered_map<InstrId,uint32_t> cycle;std::vector<Scheduled> result;
 std::unordered_map<std::string,std::unordered_map<uint32_t,uint32_t>> used;
 auto can=[&](const Instruction&i,uint32_t c){for(auto&u:i.resources){auto cap=m.resource_capacity.find(u.resource);if(cap!=m.resource_capacity.end()&&used[u.resource][c]+u.quantity>cap->second)return false;}return true;};
 while(!ready.empty()){InstrId id=ready.front();ready.erase(ready.begin());const Instruction* ins=nullptr;for(auto&i:r.instructions)if(i.id==id){ins=&i;break;}uint32_t c=0;for(auto&d:r.deps)if(d.consumer==id)c=std::max(c,cycle[d.producer]+d.latency);while(!can(*ins,c))++c;cycle[id]=c;result.push_back({id,c});for(auto&u:ins->resources)for(uint32_t k=0;k<u.duration;++k)used[u.resource][c+k]+=u.quantity;for(auto*d:out[id])if(--indeg[d->consumer]==0){ready.push_back(d->consumer);std::sort(ready.begin(),ready.end());}}
 if(result.size()!=r.instructions.size())return Result<std::vector<Scheduled>>::err({Error::Code::Conflict,"cyclic scheduling dependencies"});
 return Result<std::vector<Scheduled>>::ok(std::move(result));
}
std::string print(const Region&r){std::string s="region "+r.name+" {\n";for(auto&i:r.instructions)s+="  "+std::to_string(i.id)+": "+i.opcode+"\n";for(auto&d:r.deps)s+="  dep "+std::to_string(d.producer)+" -> "+std::to_string(d.consumer)+"\n";return s+"}\n";}
}
