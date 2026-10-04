#include "limestone/traceml.h"
#include <assert.h>
#include <string.h>
int main(void) {
  limestone_error error;limestone_traceml_options options;limestone_traceml_options_default(&options);options.record_guards=1;
  limestone_traceml_runtime *runtime=limestone_traceml_prepare("(lambda x (lambda y (add x (if y 2 0))))",1000,&error);assert(runtime);
  limestone_traceml_value *forty=limestone_traceml_integer(40,&error),*one=limestone_traceml_integer(1,&error);const limestone_traceml_value *args[]={forty};
  limestone_traceml_result *result=limestone_traceml_invoke(runtime,args,1,&options,&error);assert(result);
  limestone_traceml_value *function=limestone_traceml_result_value(result,&error);assert(limestone_traceml_value_callable(function));int64_t value=7;assert(limestone_traceml_value_integer(function,&value,&error)==LIMESTONE_UNSUPPORTED&&value==7);
  limestone_traceml_runtime_destroy(runtime);limestone_traceml_result_destroy(result);limestone_traceml_value_destroy(forty);
  args[0]=one;result=limestone_traceml_apply(function,args,1,&options,&error);assert(result&&limestone_traceml_result_guard_count(result)==1&&strstr(limestone_traceml_result_trace(result),"taken"));
  limestone_traceml_guard *guard=limestone_traceml_result_guard(result,0,&error);assert(guard&&limestone_traceml_guard_expected(guard)==1);limestone_traceml_value *integer=limestone_traceml_result_value(result,&error);assert(limestone_traceml_value_integer(integer,&value,&error)==LIMESTONE_OK&&value==42);
  limestone_traceml_value_destroy(integer);limestone_traceml_value_destroy(function);limestone_traceml_value_destroy(one);limestone_traceml_result_destroy(result);
  result=limestone_traceml_resume(guard,0,NULL,&error);assert(result);integer=limestone_traceml_result_value(result,&error);assert(limestone_traceml_value_integer(integer,&value,&error)==LIMESTONE_OK&&value==40);
  options.step_limit=0;assert(!limestone_traceml_resume(guard,1,&options,&error)&&error.code==LIMESTONE_RESOURCE_LIMIT);
  limestone_traceml_value_destroy(integer);limestone_traceml_result_destroy(result);limestone_traceml_guard_destroy(guard);
  assert(!limestone_traceml_prepare(NULL,1000,&error)&&error.code==LIMESTONE_INVALID_ARGUMENT);return 0;
}
