#include "test.hpp"
#include "limestone.hpp"
#include "limestone.h"

int main(){return test_main([]{
  using namespace limestone;
  auto result=take(run_pipeline("((lambda x (add x 2)) 40)"));
  CHECK(result.machine_ir.find("const.i64 #42")!=std::string::npos&&result.machine_ir.find("ret %1")!=std::string::npos);
  CHECK(result.selected.instructions.size()==2&&result.scheduled.size()==2&&result.stages.back()=="machine-ir");
  CHECK(take(run_pipeline("(add 1 2)",{true,false,false})).scheduled.empty());
  fails(run_pipeline("hello"),Error::Code::InvalidArgument);fails(run_pipeline("(add 1)"),Error::Code::InvalidArgument);
  fails(run_pipeline("3",{true,true,true}),Error::Code::Unsupported);
  unisel::Program graph{{{10,"const",{},1,"i64"},{20,"const",{},2,"i64"},{1,"add",{10,20},{},"i64"}},{1}};
  PipelineTarget target;target.name="fixture";target.patterns={{1,"constant","const","CONST",{},1},{2,"add","add","ADD",{"v","v"},1}};
  target.instructions={{"CONST",{0,1,{},"integer","integer"}},{"ADD",{2,1,{{"ALU",1,1}},"integer","integer"}}};
  target.scheduling={{{"ALU",1}},1};target.register_classes={{"G",{0,1,2}}};target.default_register_class="G";
  auto allocated=take(run_pipeline(graph,target,{true,true,true}));
  CHECK(allocated.allocation&&allocated.allocation->regs.size()==3&&allocated.allocation->spilled.empty());
  take(regtl::verify(allocated.allocation_problem,*allocated.allocation));take(schedrow::verify(allocated.selected,target.scheduling,allocated.scheduled));
  CHECK(allocated.machine_ir.find("ADD %10 %20")!=std::string::npos);
  CHECK(allocated.machine_ir==take(run_pipeline(graph,target,{true,true,true})).machine_ir);
  bool optimized=false;target.optimizer=[&](const unisel::Program& p){optimized=true;return Result<unisel::Program>::ok(p);};
  take(run_pipeline(graph,target));CHECK(optimized);optimized=false;take(run_pipeline(graph,target,{false,true,false}));CHECK(!optimized);
  auto missing=target;missing.instructions.erase("ADD");fails(run_pipeline(graph,missing),Error::Code::Unsupported);
  auto unknown=target;unknown.instructions.at("ADD").latency.reset();fails(run_pipeline(graph,unknown),Error::Code::Unsupported);
  target.optimizer=[](const unisel::Program&){return Result<unisel::Program>::err({Error::Code::Conflict,"adapter rejected input"});};fails(run_pipeline(graph,target),Error::Code::Conflict);
  limestone_error error{};auto* module=limestone_compile_checked("(mul 6 7)",nullptr,&error);CHECK(module&&error.code==LIMESTONE_OK);
  CHECK(std::string(limestone_module_text(module)).find("#42")!=std::string::npos);limestone_module_destroy(module);
  CHECK(!limestone_compile_checked(nullptr,nullptr,&error)&&error.code==LIMESTONE_INVALID_ARGUMENT);
  CHECK(!limestone_compile_checked("(",nullptr,&error)&&error.code==LIMESTONE_PARSE&&error.message[0]);
  limestone_options invalid{2,1,0};CHECK(!limestone_compile_checked("1",&invalid,&error)&&error.code==LIMESTONE_INVALID_ARGUMENT);
});}
