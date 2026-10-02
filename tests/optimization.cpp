#include "test.hpp"
#include "tunah.hpp"

int main(){return test_main([]{
  using namespace limestone;using namespace tunah;
  Session session;session.define_operator("add",2);
  CHECK(take(session.load_rules("(rule zero (add ?x 0) ?x)"))==1);
  auto optimized=take(session.saturate("(add (add x 0) 0)"));
  CHECK(optimized.expression=="x"&&optimized.cost==1&&optimized.saturated&&optimized.rewrites>=2&&!optimized.trace.empty());
  for(int i=0;i<5;++i)CHECK(take(session.saturate("(add (add x 0) 0)")).expression==optimized.expression);
  CHECK(take(session.saturate("(add 2 3)")).expression=="(add 2 3)");
  auto before=session.rules().size();fails(session.load_rules("(rule unbound (add ?x 0) ?y)"),Error::Code::InvalidArgument);CHECK(session.rules().size()==before);
  fails(session.load_rules("(rule zero (add ?x 0) ?x)"),Error::Code::Conflict);
  fails(session.load_rules("(rule bad (unknown ?x) ?x)"),Error::Code::InvalidArgument);
  fails(session.saturate("(add x)"),Error::Code::InvalidArgument);
  fails(session.saturate("(add x 0"),Error::Code::Parse);
  fails(session.saturate("x y"),Error::Code::Parse);
  fails(session.saturate("?x"),Error::Code::InvalidArgument);
  Limits none;none.iterations=0;auto unchanged=take(session.saturate("(add x 0)",none));CHECK(unchanged.expression=="(add x 0)"&&unchanged.limit_reached&&!unchanged.saturated);
  Limits cancelled;cancelled.cancelled=[] {return true;};auto stopped=take(session.saturate("(add x 0)",cancelled));CHECK(stopped.expression=="(add x 0)"&&stopped.limit_reached);
  Limits too_small;too_small.nodes=1;fails(session.saturate("(add x 0)",too_small),Error::Code::ResourceLimit);
  Session expanding;expanding.define_operator("wrap",1);expanding.define_operator("pair",2);take(expanding.load_rules("(rule grow (wrap ?x) (wrap (pair ?x ?x)))"));
  Limits bounded;bounded.nodes=20;bounded.classes=20;auto limited=take(expanding.saturate("(wrap x)",bounded));CHECK(limited.expression=="(wrap x)"&&limited.limit_reached&&limited.nodes<=20);
});}
