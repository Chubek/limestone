#include "test.hpp"
#include "exolayer.h"
#include <cstring>

void throws(const exl_value_t*,size_t,exl_value_t*,void*) {throw std::runtime_error("callback failure");}
void no_op(const exl_value_t*,size_t,exl_value_t*,void*) {}
void reentrant(const exl_value_t*,size_t,exl_value_t* result,void* user) {
  auto* context=static_cast<exl_context_t*>(user);
  for(int i=0;i<128;++i)CHECK(exl_register(context,("registered_"+std::to_string(i)).c_str(),no_op,nullptr)==0);
  result->kind=EXL_I64;result->as.i64=42;
}
int main(int argc,char** argv){return test_main([&]{
  CHECK(argc==3);
  auto* context=exl_context_create();CHECK(context);
  std::unique_ptr<exl_context_t,decltype(&exl_context_destroy)> owned(context,exl_context_destroy);
  CHECK(exl_register(context,"throws",throws,nullptr)==0);
  exl_value_t result{EXL_I64,{7}};
  CHECK(exl_call(context,"throws",nullptr,0,&result)==-3&&result.kind==EXL_VOID);
  CHECK(std::string(exl_last_error(context)).find("callback failure")!=std::string::npos);
  CHECK(exl_register(context,"noop",no_op,nullptr)==0&&std::strlen(exl_last_error(context))==0);
  CHECK(exl_register(context,"noop",no_op,nullptr)==-2);
  CHECK(exl_call(context,"absent",nullptr,0,&result)==-2&&result.kind==EXL_VOID);
  CHECK(exl_call(context,"noop",nullptr,1,&result)==-1);
  CHECK(exl_register(context,"",no_op,nullptr)==-1&&std::strlen(exl_last_error(context))>0);
  CHECK(exl_register(context,"reentrant",reentrant,context)==0);
  CHECK(exl_call(context,"reentrant",nullptr,0,&result)==0&&result.as.i64==42);
  auto* library=exl_library_open(context,argv[1]);CHECK(library);
  exl_value_t args[]={{EXL_I64,{20}},{EXL_I64,{22}}};
  CHECK(exl_call(context,"extension_sum",args,2,&result)==0&&result.as.i64==42);
  CHECK(!exl_library_open(context,argv[1])); // Duplicate export is transactional.
  CHECK(exl_call(context,"extension_sum",args,2,&result)==0&&result.as.i64==42);
  CHECK(!exl_library_open(context,argv[2]));CHECK(std::string(exl_last_error(context)).find("incompatible")!=std::string::npos);
  CHECK(!exl_library_open(context,"/nonexistent/exolayer-extension.so"));
  CHECK(exl_library_close(context,library)==0);CHECK(exl_call(context,"extension_sum",args,2,&result)==-2);
  CHECK(exl_library_close(context,nullptr)==-1);
  CHECK(exl_library_open(context,argv[1])); // Context destruction owns remaining handles.
});}
