#include "test.hpp"
#include "regtl.hpp"

int main(){return test_main([]{
  using namespace limestone;using namespace regtl;
  Program p{{{1,0,5,"G",{},true},{2,2,3,"G",{{},{},0},false}},{{"G",{0,1}}}};
  for(auto allocator:{linear_scan,greedy}) {
    auto a=take(allocator(p));take(verify(p,a));CHECK(a.regs.at(1)==1&&a.regs.at(2)==0);
    auto calls=p;calls.clobbers={{1,{1}}};auto c=take(allocator(calls));CHECK(c.spilled==std::vector<uint32_t>{1});take(verify(calls,c));
    Program pressure{{{1,0,3,"G",{},true},{2,1,2,"G",{},false}},{{"G",{0}}}};
    auto b=take(allocator(pressure));CHECK(b.regs.at(2)==0&&b.spilled==std::vector<uint32_t>{1});take(verify(pressure,b));
    Program overlap{{{1,0,1,"G",{},false},{2,1,2,"G",{},false}},{{"G",{0}}}};fails(allocator(overlap),Error::Code::Unsatisfiable);
    Program alias{{{1,0,1,"G",{{1},{},{}},false},{2,0,1,"G",{{2},{},{}},false}},{{"G",{0,1,2}}},{{0,1},{0,2}}};
    take(verify(alias,take(allocator(alias)))); // Overlap is not transitive.
    alias.aliases.push_back({1,2});fails(allocator(alias),Error::Code::Unsatisfiable);
  }
  auto bad=p;bad.ranges[0].end=0;bad.ranges[0].begin=1;fails(validate(bad),Error::Code::InvalidArgument);
  bad=p;bad.ranges[1].constraint.forbidden={0};fails(validate(bad),Error::Code::Unsatisfiable);
  bad=p;bad.ranges[1].constraint.fixed=99;fails(validate(bad),Error::Code::Unsatisfiable);
  auto valid=take(linear_scan(p));auto wrong=valid;wrong.regs[2]=1;fails(verify(p,wrong),Error::Code::Conflict);
  wrong=valid;wrong.spilled={99};fails(verify(p,wrong),Error::Code::Conflict);
  wrong=valid;wrong.spilled={2};wrong.regs.erase(2);fails(verify(p,wrong),Error::Code::Conflict);
  wrong=valid;wrong.regs[99]=0;fails(verify(p,wrong),Error::Code::Conflict);
});}
