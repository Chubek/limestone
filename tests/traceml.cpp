#include "test.hpp"
#include "traceml.hpp"

int main(){return test_main([]{
  using namespace limestone;
  auto eval=[](std::string_view source){return take(traceml::evaluate(take(traceml::compile(source))));};
  CHECK(eval("(add 20 22)")==42);CHECK(eval("(mul -3 -7)")==21);CHECK(eval("0")==0);
  CHECK(eval("(((lambda x (lambda y (add x y))) 10) 7)")==17);
  CHECK(eval("((lambda x ((lambda x x) 5)) 7)")==5);
  CHECK(eval("((lambda f (f 20 22)) add)")==42);
  CHECK(eval("((lambda x 42) (add 9223372036854775807 1))")==42);
  CHECK(eval("(if 0 (add 9223372036854775807 1) 8)")==8);
  CHECK(eval("((if 1 (lambda x (add x 1)) (lambda x 0)) 9)")==10);
  CHECK(eval("(begin (lambda x x) (add 2 3))")==5);
  CHECK(eval("; comment\n (add 1 2)\n 9")==9);
  CHECK(eval("(sub -9223372036854775808 0)")==std::numeric_limits<int64_t>::min());
  for(auto source:{"(add 9223372036854775807 1)","(sub -9223372036854775808 1)","(mul -9223372036854775808 -1)","(neg -9223372036854775808)","(1 2)","(add (lambda x x) 1)"})fails(traceml::evaluate(take(traceml::compile(source))),Error::Code::InvalidArgument);
  for(auto source:{"(lambda 1 2)","(add 1)","(if 1 2)","(begin)","unknown"})fails(traceml::compile(source),Error::Code::InvalidArgument);
  for(auto source:{"", "()", "(add 1 2", "9223372036854775808", ")"})fails(traceml::compile(source),Error::Code::Parse);
  auto located=take(traceml::parse("\n  (add 1 2)"));CHECK(located->line==2&&located->column==3);
  auto divergence=take(traceml::compile("((lambda x (x x)) (lambda x (x x)))"));fails(traceml::evaluate(divergence),Error::Code::ResourceLimit);
  fails(traceml::evaluate(take(traceml::compile("1")),0),Error::Code::ResourceLimit);
  fails(traceml::lower_checked(take(traceml::compile("(lambda x x)"))),Error::Code::Unsupported);
  CHECK(take(traceml::lower_checked(take(traceml::compile("(add 1 2)")))).find("const.i64 3")!=std::string::npos);
  auto cyclic=std::make_shared<traceml::Expr>();cyclic->kind=traceml::Expr::Kind::Begin;cyclic->children={cyclic};
  fails(traceml::verify({{cyclic}}),Error::Code::InvalidArgument);cyclic->children.clear();
  auto traced=take(traceml::compile("((lambda x (if (lt x 0) (neg x) (add x 2))) -3)"));
  auto execution=take(traceml::execute(traced));CHECK(execution.value==3&&execution.result_value&&execution.steps>0);
  CHECK(execution.trace==take(traceml::execute(traced)).trace);CHECK(traceml::print_trace(execution).find("taken")!=std::string::npos);
  auto lowering=take(traceml::lower_trace(execution));CHECK(std::count_if(lowering.program.nodes.begin(),lowering.program.nodes.end(),[](auto& n){return n.op=="guard_nonzero";})==1);
  CHECK(std::count_if(lowering.program.nodes.begin(),lowering.program.nodes.end(),[](auto& n){return n.op=="add";})==0);
  auto false_path=take(traceml::execute(take(traceml::compile("(if 0 100 (add 2 3))"))));CHECK(false_path.value==5);take(traceml::lower_trace(false_path));
  auto corrupt=execution;for(auto& event:corrupt.trace)if(event.kind==traceml::TraceEvent::Kind::Primitive){event.result=1000;break;}fails(traceml::lower_trace(corrupt),Error::Code::Conflict);
  traceml::ExecutionOptions trace_options;trace_options.event_limit=1;fails(traceml::execute(traced,trace_options),Error::Code::ResourceLimit);
  trace_options.event_limit=100;trace_options.cancelled=[] {return true;};fails(traceml::execute(traced,trace_options),Error::Code::ResourceLimit);
  trace_options.cancelled={};trace_options.record_trace=false;size_t observed=0;trace_options.observer=[&](auto&){++observed;};CHECK(take(traceml::execute(traced,trace_options)).trace.empty()&&observed>0);
  auto lazy_trace=take(traceml::execute(take(traceml::compile("((lambda x 42) (add 9223372036854775807 1))"))));CHECK(lazy_trace.value==42);CHECK(std::none_of(lazy_trace.trace.begin(),lazy_trace.trace.end(),[](auto& e){return e.kind==traceml::TraceEvent::Kind::Primitive;}));
});}
