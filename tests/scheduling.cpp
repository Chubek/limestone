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
  auto late_first=hs;std::swap(late_first[0],late_first[1]);fails(verify(hazards,{},late_first),Error::Code::Conflict);
  Region barrier{"barrier"};Instruction before{};before.id=4;Instruction fence{};fence.id=5;fence.barrier=true;Instruction after{};after.id=6;
  barrier.instructions={before,fence,after};auto fenced=take(dependencies(barrier));CHECK(fenced.deps.size()==2);CHECK(fenced.deps[0].scheduler_only);
  auto cycle=hazards;cycle.deps={{10,2,DepKind::Control,0},{2,10,DepKind::Control,0}};
  fails(schedule(cycle,{}),Error::Code::Conflict);fails(verify(cycle,{},std::vector<Scheduled>{{10,0},{2,0},{3,0}}),Error::Code::Conflict);
  auto loop=hazards;loop.deps={{10,2,DepKind::True,1,1}};fails(schedule(loop,{}),Error::Code::Unsupported);
  auto forged=hs;forged[0].id=99;fails(verify(hazards,{},forged),Error::Code::Conflict);
  Region mixed{"mixed-slots"};flexible.resources.clear();fixed.resources.clear();fixed.issue_slots={0};mixed.instructions={flexible,fixed};MachineModel dual{{},2};
  auto slots=take(schedule(mixed,dual));CHECK(slots[0].cycle==0&&slots[1].cycle==0);take(verify(mixed,dual,slots));
  fails(verify(mixed,dual,std::vector<Scheduled>{{flexible.id,0,0},{fixed.id,0,0}}),Error::Code::Conflict);
  Region ordering{"memory-fence"};before.access=MemoryAccess{true,false};fence.barrier=false;fence.access=MemoryAccess{};fence.access->ordering=MemoryOrdering::Sequential;after.access=MemoryAccess{true,false};ordering.instructions={before,fence,after};CHECK(take(dependencies(ordering)).deps.size()==2);
  Region separate{"latency-domains"};write.latency=4;write.implicit_defs={1};write.result_latency={{1,0}};read.implicit_uses={1};separate.instructions={write,read};auto timing=take(schedule(separate,{}));CHECK(timing[1].cycle==4);separate.instructions[0].implicit_result_latency={{1,2}};timing=take(schedule(separate,{}));CHECK(timing[1].cycle==2);take(verify(separate,{},timing));
  separate.instructions[0].implicit_result_latency={{99,2}};fails(schedule(separate,{}),Error::Code::Conflict);
  Region emission{"zero-latency-order"};Instruction a{};a.id=1;a.defs={7};a.latency=0;Instruction b{};b.id=2;b.uses={7};emission.instructions={a,b};const std::vector<uint32_t> legal_order{1,2},reverse_order{2,1};take(verify_order(emission,legal_order));fails(verify_order(emission,reverse_order),Error::Code::Conflict);
  take(verify(emission,dual,std::vector<Scheduled>{{1,0},{2,0}}));fails(verify(emission,dual,std::vector<Scheduled>{{2,0},{1,0}}),Error::Code::Conflict);
  take(verify_modulo(emission,dual,std::vector<Scheduled>{{1,0},{2,0}},1));fails(verify_modulo(emission,dual,std::vector<Scheduled>{{2,0},{1,0}},1),Error::Code::Conflict);
  Region recurrence{"loop-distance"};recurrence.instructions={a,b};recurrence.instructions[0].defs.clear();recurrence.instructions[1].uses.clear();recurrence.deps={{2,1,DepKind::True,1,1}};take(verify_modulo(recurrence,dual,std::vector<Scheduled>{{1,0},{2,0}},1));
  Region bundle{"joint-bundle"};Instruction first{},second{};first.id=8;first.latency=0;first.resources={{"",1,1,0,{"A","B"}}};second.id=2;second.latency=0;second.resources={{"A",1,1}};second.issue_slots={0};bundle.instructions={first,second};bundle.groups={{44,GroupKind::Bundle,{8,2},"dual"}};bundle.groups[0].issue_width=2;bundle.groups[0].issue_slots={0,1};
  auto packed=take(schedule(bundle,ports));CHECK(packed[0].id==8&&packed[1].id==2&&packed[0].cycle==packed[1].cycle);CHECK(packed[0].slot==1&&packed[1].slot==0&&packed[0].resources==std::vector<std::string>{"B"});take(verify(bundle,ports,packed));take(verify_order(bundle,std::vector<uint32_t>{8,2}));
  auto bad_bundle=bundle;bad_bundle.instructions[0].issue_slots={0};fails(schedule(bad_bundle,ports),Error::Code::Unsatisfiable);bad_bundle=bundle;bad_bundle.instructions[0].resources={{"A",1,1}};fails(schedule(bad_bundle,ports),Error::Code::Unsatisfiable);fails(schedule(bundle,MachineModel{{{"A",1},{"B",1}},1}),Error::Code::InvalidArgument);
  auto unbundled=bundle;unbundled.groups[0].issue_slots.clear();unbundled.groups[0].issue_width=1;fails(schedule(unbundled,ports),Error::Code::Unsatisfiable);
  bad_bundle=bundle;bad_bundle.instructions[0].defs={7};bad_bundle.instructions[0].latency=1;bad_bundle.instructions[1].uses={7};fails(schedule(bad_bundle,ports),Error::Code::Unsatisfiable);
  auto forged_bundle=packed;forged_bundle[1].cycle=2;fails(verify(bundle,ports,forged_bundle),Error::Code::Conflict);forged_bundle=packed;std::reverse(forged_bundle.begin(),forged_bundle.end());fails(verify(bundle,ports,forged_bundle),Error::Code::Conflict);
  bundle.deps={{8,8,DepKind::True,2,1}};auto periodic=take(schedule_modulo(bundle,ports,{2,4,10000}));CHECK(periodic[0].cycle==periodic[1].cycle&&periodic[0].slot==1);take(verify_modulo(bundle,ports,periodic,2));fails(schedule_modulo(bundle,ports,{1,4,10000}),Error::Code::Unsatisfiable);auto forged_periodic=periodic;forged_periodic[1].cycle+=2;fails(verify_modulo(bundle,ports,forged_periodic,2),Error::Code::Conflict);bundle.deps.clear();
  Region adjacent{"atomic-emission"};a.id=30;a.latency=4;a.defs={7};b.id=99;b.uses={7};Instruction middle{};middle.id=4;middle.priority=50;adjacent.instructions={a,middle,b};adjacent.groups={{5,GroupKind::Fusion,{30,99},"pair","fixture","produce-consume",100}};
  auto contiguous=take(schedule(adjacent,dual));CHECK(contiguous[0].id==30&&contiguous[1].id==99&&contiguous[2].id==4&&contiguous[1].cycle==4);take(verify(adjacent,dual,contiguous));fails(verify_order(adjacent,std::vector<uint32_t>{30,4,99}),Error::Code::Conflict);fails(verify(adjacent,dual,std::vector<Scheduled>{{30,0},{4,2},{99,4}}),Error::Code::Conflict);
  for(auto kind:{GroupKind::Adjacent,GroupKind::Atomic,GroupKind::Pair}){auto unit=adjacent;unit.groups[0].kind=kind;take(verify(unit,dual,take(schedule(unit,dual))));}
  auto ordered=adjacent;ordered.groups[0].kind=GroupKind::Ordered;take(verify(ordered,dual,std::vector<Scheduled>{{30,0},{4,2},{99,4}}));take(verify_order(ordered,std::vector<uint32_t>{30,4,99}));
  auto interleaved=adjacent;interleaved.deps={{30,4,DepKind::Ordering,0},{4,99,DepKind::Ordering,0}};fails(schedule(interleaved,dual),Error::Code::Unsatisfiable);
  auto same_cycle=interleaved;same_cycle.groups[0].kind=GroupKind::SameCycle;same_cycle.instructions[0].latency=0;MachineModel triple{{},3};auto coissued=take(schedule(same_cycle,triple));CHECK(coissued[0].id==30&&coissued[1].id==4&&coissued[2].id==99&&coissued[0].cycle==coissued[2].cycle);take(verify(same_cycle,triple,coissued));take(verify_modulo(same_cycle,triple,take(schedule_modulo(same_cycle,triple,{1,4,10000})),1));fails(schedule(same_cycle,dual),Error::Code::Unsatisfiable);same_cycle.deps[0].latency=1;fails(schedule(same_cycle,triple),Error::Code::Unsatisfiable);
  auto malformed=adjacent;malformed.groups[0].members.push_back(99);fails(schedule(malformed,dual),Error::Code::InvalidArgument);malformed=adjacent;malformed.groups.push_back({6,GroupKind::Adjacent,{4,99}});fails(schedule(malformed,dual),Error::Code::Unsupported);
  auto blocks=bundle;blocks.blocks={{7,"entry",{9}},{9,"exit"}};blocks.entry=7;for(auto& i:blocks.instructions)i.block=7;middle.block=9;blocks.instructions.push_back(middle);auto cfg_schedule=take(schedule_cfg(blocks,ports));take(verify_cfg(blocks,ports,cfg_schedule));
  auto mixed_blocks=cfg_schedule;std::swap(mixed_blocks[1],mixed_blocks[2]);fails(verify_cfg(blocks,ports,mixed_blocks),Error::Code::Conflict);
  auto reversed_blocks=cfg_schedule;std::rotate(reversed_blocks.begin(),reversed_blocks.end()-1,reversed_blocks.end());fails(verify(blocks,ports,reversed_blocks),Error::Code::Conflict);
  fails(schedule_modulo(blocks,ports),Error::Code::Unsupported);
  blocks.groups[0].members={8,4};fails(schedule_cfg(blocks,ports),Error::Code::Unsupported);
});}
