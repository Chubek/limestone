#include "test.hpp"
#include "unisel.hpp"
#include "limeburg.hpp"
#include <set>

int main(){return test_main([]{
  using namespace limestone;
  unisel::Program p{{{1,"const",{},100,"i64"},{2,"const",{},7,"i64"},{3,"add",{1,2},{},"i64"}},{3}};
  unisel::Pattern fused{3,"immediate","add","ADDI",{},6};
  fused.tree=unisel::PatternTree{"add","","i64",{{"","lhs","i64"},{"const","","i64",{},std::pair<int64_t,int64_t>{-8,7}}}};
  std::vector<unisel::Pattern> patterns{{1,"constant","const","CONST",{},0},{2,"add","add","ADD",{"v","v"},7},fused};
  auto model=take(unisel::build_model(p,patterns));CHECK(model.candidates.size()==4);
  auto solved=take(unisel::solve(p,patterns));CHECK(solved.cost==6&&solved.selected.size()==2);
  CHECK(take(unisel::solve_greedy(p,patterns)).cost==7);
  auto emitted=take(unisel::emit_scheduler(p,patterns,solved));CHECK(emitted.instructions.size()==2);
  CHECK(emitted.instructions[0].id==1&&emitted.instructions[0].immediates==std::vector<std::pair<uint32_t,int64_t>>{{1,100}});
  CHECK(emitted.instructions[1].opcode=="ADDI"&&emitted.instructions[1].uses==std::vector<uint32_t>{1});
  CHECK(emitted.instructions[1].immediates==std::vector<std::pair<uint32_t,int64_t>>{{2,7}});
  auto schedule=take(schedrow::schedule(emitted,{}));CHECK(schedule.size()==2);take(schedrow::verify(emitted,{},schedule));
  auto shared=p;shared.outputs.push_back(2);CHECK(take(unisel::solve(shared,patterns)).cost==7);
  auto wrong_type=p;wrong_type.nodes[1].type="f64";CHECK(take(unisel::solve(wrong_type,patterns)).cost==7);
  auto out_of_range=p;out_of_range.nodes[1].constant=8;CHECK(take(unisel::solve(out_of_range,patterns)).cost==7);
  auto forged=solved;forged.selected[0].covered.clear();fails(unisel::emit_scheduler(p,patterns,forged),Error::Code::InvalidArgument);
  fails(unisel::solve(p,{patterns[0]}),Error::Code::Unsatisfiable);
  auto cycle=p;cycle.nodes[0].inputs={3};fails(unisel::solve(cycle,patterns),Error::Code::Conflict);
  auto missing=p;missing.nodes[2].inputs[0]=99;fails(unisel::solve(missing,patterns),Error::Code::InvalidArgument);
  auto effect=p;effect.nodes[2].side_effect=true;fails(unisel::solve(effect,patterns),Error::Code::Unsatisfiable);
  unisel::Program repeated{{{1,"input",{}, {},"i64",0,false},{2,"add",{1,1},{},"i64"}},{2}};
  auto tied=fused;tied.id=9;tied.cost=1;tied.instruction="DOUBLE";
  tied.tree=unisel::PatternTree{"add","","i64",{{"","x","i64"},{"","x","i64"}}};
  CHECK(take(unisel::solve(repeated,{tied})).cost==1);
  repeated.nodes.insert(repeated.nodes.begin(),{0,"input",{},{},"i64",0,false});repeated.nodes.back().inputs[1]=0;
  fails(unisel::solve(repeated,{tied}),Error::Code::Unsatisfiable);
  // Exhaustive exact-cover reference exercises Boolean weighted-cost carries.
  for(int constant_cost=0;constant_cost<4;++constant_cost)for(int fused_cost=0;fused_cost<4;++fused_cost) {
    auto ps=patterns;ps[0].cost=constant_cost;ps[1].cost=3;ps[2].cost=fused_cost;
    auto candidates=unisel::match(p,ps);int best=100000;
    for(unsigned mask=0;mask<(1u<<candidates.size());++mask) {
      std::map<uint32_t,unsigned> coverage;int cost=0;
      for(size_t i=0;i<candidates.size();++i)if(mask&(1u<<i)){cost+=candidates[i].cost;for(auto id:candidates[i].covered)++coverage[id];}
      if(coverage[1]==1&&coverage[2]==1&&coverage[3]==1)best=std::min(best,cost);
    }
    CHECK(take(unisel::solve(p,ps)).cost==best);
  }
  limeburg::RuleSet rules{{{1,"reg","const","reg",{},4,"CONST",0},{2,"imm","const","imm",{},0,"",0},{3,"reg","add","reg",{"reg","imm"},1,"ADDI",0},{4,"reg","add","reg",{"reg","reg"},1,"ADD",0}},{{"reg",1},{"imm",2}}};
  rules.rules[1].immediate=std::pair<int64_t,int64_t>{-8,7};
  std::vector<limeburg::Node> tree{{1,"add","i64",{20,10}},{20,"const","i64",{},100,true},{10,"const","i64",{},7,true}};
  auto burs=take(limeburg::select(tree,1,rules,"reg"));CHECK(burs.cost==5&&burs.chosen.at(1).rule==3);
  CHECK(burs.chosen.at(20).rule==1&&burs.chosen.at(10).rule==2);
  auto sir=take(limeburg::emit_scheduler(tree,rules,burs));CHECK(sir.instructions.size()==2&&sir.instructions[0].id==20);
  CHECK(sir.instructions[1].uses==std::vector<uint32_t>{20}&&sir.instructions[1].immediates==std::vector<std::pair<uint32_t,int64_t>>{{10,7}});
  take(schedrow::verify(sir,{},take(schedrow::schedule(sir,{}))));
  auto forged_burs=burs;forged_burs.chosen.at(20).rule=2;fails(limeburg::emit_scheduler(tree,rules,forged_burs),Error::Code::Conflict);
  auto tied_rules=rules;tied_rules.rules.push_back({5,"reg","add","reg",{"reg","imm"},1,"ADDI_SECOND",0});std::reverse(tied_rules.rules.begin(),tied_rules.rules.end());
  CHECK(take(limeburg::select(tree,1,tied_rules,"reg")).chosen.at(1).rule==3);
  auto dag=tree;dag[0].children={10,10};fails(limeburg::select(dag,1,rules,"reg"),Error::Code::Unsupported);
  auto cyclic=tree;cyclic[1].children={1};fails(limeburg::select(cyclic,1,rules,"reg"),Error::Code::Conflict);
  auto bad_rules=rules;bad_rules.rules[0].cost=-1;fails(limeburg::validate(bad_rules),Error::Code::InvalidArgument);
  fails(limeburg::select(tree,1,rules,"unknown"),Error::Code::InvalidArgument);
});}
