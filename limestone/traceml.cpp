#include "traceml.h"
#include "c_api_internal.hpp"
#include "traceml/traceml.hpp"

struct limestone_traceml_runtime { limestone::traceml::RuntimeProgram program; };
struct limestone_traceml_value { limestone::traceml::RuntimeValue value; };
struct limestone_traceml_guard { limestone::traceml::GuardSnapshot guard; };
struct limestone_traceml_result {
  limestone::traceml::RuntimeResult result;
  std::string trace;
  explicit limestone_traceml_result(limestone::traceml::RuntimeResult value):result(std::move(value)),trace(limestone::traceml::print_trace(result.execution)){}
};
namespace {
using namespace limestone;
using namespace limestone::c_api_internal;
traceml::ExecutionOptions options(const limestone_traceml_options *input) {
  traceml::ExecutionOptions result;if(!input)return result;
  for(auto flag:{input->record_trace,input->record_guards})if(flag!=0&&flag!=1)throw Error{Error::Code::InvalidArgument,"TraceLambda options must be Boolean"};
  result.step_limit=input->step_limit;result.event_limit=input->event_limit;result.record_trace=input->record_trace;result.record_guards=input->record_guards;
  if(input->cancelled)result.cancelled=[input]{return input->cancelled(input->userdata)!=0;};return result;
}
std::vector<traceml::RuntimeValue> arguments(const limestone_traceml_value *const *input,size_t count) {
  if((count&&!input)||count>65536)throw Error{Error::Code::InvalidArgument,"invalid TraceLambda runtime arguments"};std::vector<traceml::RuntimeValue> result;result.reserve(count);
  for(size_t k=0;k<count;++k){if(!input[k])throw Error{Error::Code::InvalidArgument,"null TraceLambda runtime argument"};result.push_back(input[k]->value);}return result;
}
}
extern "C" void limestone_traceml_options_default(limestone_traceml_options *output){if(output)*output={100000,100000,1,0,nullptr,nullptr};}
extern "C" limestone_traceml_runtime *limestone_traceml_prepare(const char *source,size_t nodes,limestone_error *error){return boundary(error,[&]()->limestone_traceml_runtime*{if(!source)throw Error{Error::Code::InvalidArgument,"null TraceML source"};return new limestone_traceml_runtime{checked(traceml::lower_runtime(checked(traceml::compile(source)),nodes))};});}
extern "C" void limestone_traceml_runtime_destroy(limestone_traceml_runtime *runtime){delete runtime;}
extern "C" limestone_traceml_value *limestone_traceml_integer(int64_t value,limestone_error *error){return boundary(error,[&]{return new limestone_traceml_value{traceml::RuntimeValue::integer(value)};});}
extern "C" void limestone_traceml_value_destroy(limestone_traceml_value *value){delete value;}
extern "C" limestone_status limestone_traceml_value_integer(const limestone_traceml_value *value,int64_t *output,limestone_error *error){return boundary(error,[&]{if(!value||!output)throw Error{Error::Code::InvalidArgument,"null TraceLambda integer inspection"};auto integer=value->value.integer();if(!integer)throw Error{Error::Code::Unsupported,"TraceLambda value is a function"};*output=*integer;return LIMESTONE_OK;});}
extern "C" int limestone_traceml_value_callable(const limestone_traceml_value *value){return value&&value->value.callable();}
extern "C" limestone_traceml_result *limestone_traceml_invoke(const limestone_traceml_runtime *runtime,const limestone_traceml_value *const *input,size_t count,const limestone_traceml_options *config,limestone_error *error){return boundary(error,[&]()->limestone_traceml_result*{if(!runtime)throw Error{Error::Code::InvalidArgument,"null TraceLambda runtime"};return new limestone_traceml_result(checked(traceml::run_runtime(runtime->program,arguments(input,count),options(config))));});}
extern "C" limestone_traceml_result *limestone_traceml_apply(const limestone_traceml_value *value,const limestone_traceml_value *const *input,size_t count,const limestone_traceml_options *config,limestone_error *error){return boundary(error,[&]()->limestone_traceml_result*{if(!value)throw Error{Error::Code::InvalidArgument,"null TraceLambda function"};return new limestone_traceml_result(checked(traceml::apply_runtime(value->value,arguments(input,count),options(config))));});}
extern "C" limestone_traceml_value *limestone_traceml_result_value(const limestone_traceml_result *result,limestone_error *error){return boundary(error,[&]()->limestone_traceml_value*{if(!result)throw Error{Error::Code::InvalidArgument,"null TraceLambda result"};return new limestone_traceml_value{result->result.value};});}
extern "C" size_t limestone_traceml_result_guard_count(const limestone_traceml_result *result){return result?result->result.guards.size():0;}
extern "C" limestone_traceml_guard *limestone_traceml_result_guard(const limestone_traceml_result *result,size_t index,limestone_error *error){return boundary(error,[&]()->limestone_traceml_guard*{if(!result||index>=result->result.guards.size())throw Error{Error::Code::InvalidArgument,"TraceLambda guard index out of range"};return new limestone_traceml_guard{result->result.guards[index]};});}
extern "C" const char *limestone_traceml_result_trace(const limestone_traceml_result *result){return result?result->trace.c_str():nullptr;}
extern "C" void limestone_traceml_result_destroy(limestone_traceml_result *result){delete result;}
extern "C" int limestone_traceml_guard_expected(const limestone_traceml_guard *guard){return guard?int(guard->guard.expected()):-1;}
extern "C" void limestone_traceml_guard_destroy(limestone_traceml_guard *guard){delete guard;}
extern "C" limestone_traceml_result *limestone_traceml_resume(const limestone_traceml_guard *guard,int64_t condition,const limestone_traceml_options *config,limestone_error *error){return boundary(error,[&]()->limestone_traceml_result*{if(!guard)throw Error{Error::Code::InvalidArgument,"null TraceLambda guard"};return new limestone_traceml_result(checked(traceml::resume_guard(guard->guard,condition,options(config))));});}
