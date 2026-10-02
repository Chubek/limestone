#include "test.hpp"
#include "schedrow.hpp"
#include <map>

int main(){return test_main([]{
  using namespace limestone;
  using namespace schedrow;
  Region region{"ports"};
  for(uint32_t id=1;id<=3;++id){Instruction i{};i.id=id;i.opcode="op";i.resources={{"ALU",3,0.5}};region.instructions.push_back(i);}
  MachineModel model{{{"ALU",1}},2};auto scheduled=take(schedule(region,model));
  CHECK(scheduled[0].cycle==0&&scheduled[1].cycle==0&&scheduled[2].cycle==3);take(verify(region,model,scheduled));
  auto invalid=scheduled;invalid[2].cycle=1;fails(verify(region,model,invalid),Error::Code::Conflict);
  auto impossible=region;impossible.instructions[0].resources={{"ALU",1,0.7},{"ALU",1,0.7}};fails(schedule(impossible,model),Error::Code::Unsatisfiable);
  auto unknown=region;unknown.instructions[0].resources[0].resource="missing";fails(schedule(unknown,model),Error::Code::InvalidArgument);
  auto nan=region;nan.instructions[0].resources[0].quantity=std::numeric_limits<double>::quiet_NaN();fails(schedule(nan,model),Error::Code::InvalidArgument);
  Region alternatives{"alternatives"};Instruction flexible{};flexible.id=1;flexible.resources={{"",1,1,0,{"A","B"}}};
  Instruction fixed{};fixed.id=2;fixed.resources={{"A",1,1}};alternatives.instructions={flexible,fixed};
  MachineModel ports{{{"A",1},{"B",1}},2};std::vector<Scheduled> together{{1,0},{2,0}};take(verify(alternatives,ports,together));
  take(verify(alternatives,ports,take(schedule(alternatives,ports))));
  Region hazards{"hazards"};Instruction write{};write.id=10;write.defs={1};write.latency=3;
  Instruction read{};read.id=2;read.uses={1};Instruction overwrite{};overwrite.id=3;overwrite.defs={1};
  hazards.instructions={write,read,overwrite};auto augmented=take(dependencies(hazards));
  CHECK(augmented.deps.size()==3);
  CHECK(std::any_of(augmented.deps.begin(),augmented.deps.end(),[](auto& d){return d.producer==10&&d.consumer==2&&d.kind==DepKind::True&&d.latency==3&&!d.scheduler_only;}));
  auto hs=take(schedule(hazards,{}));CHECK(hs[0].id==10&&hs[1].cycle>=3);take(verify(hazards,{},hs));
  Region barrier{"barrier"};Instruction before{};before.id=4;Instruction fence{};fence.id=5;fence.barrier=true;Instruction after{};after.id=6;
  barrier.instructions={before,fence,after};auto fenced=take(dependencies(barrier));CHECK(fenced.deps.size()==2);CHECK(fenced.deps[0].scheduler_only);
  auto cycle=hazards;cycle.deps={{10,2,DepKind::Control,0},{2,10,DepKind::Control,0}};
  fails(schedule(cycle,{}),Error::Code::Conflict);fails(verify(cycle,{},std::vector<Scheduled>{{10,0},{2,0},{3,0}}),Error::Code::Conflict);
  auto loop=hazards;loop.deps={{10,2,DepKind::True,1,1}};fails(schedule(loop,{}),Error::Code::Unsupported);
  auto forged=hs;forged[0].id=99;fails(verify(hazards,{},forged),Error::Code::Conflict);
});}
