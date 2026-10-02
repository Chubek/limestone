#include "limestone.h"
#include "limestone.hpp"
#include <cstring>
#include <exception>

struct limestone_module { limestone::Module module; };
namespace {
void diagnostic(limestone_error* out,limestone_status code,const char* message) noexcept {
  if(!out)return;out->code=code;
  std::strncpy(out->message,message,sizeof(out->message)-1);out->message[sizeof(out->message)-1]=0;
}
}
extern "C" void limestone_options_default(limestone_options* options) {
  if(options)*options={1,1,0};
}
extern "C" limestone_module* limestone_compile_checked(const char* input,const limestone_options* options,limestone_error* error) {
  diagnostic(error,LIMESTONE_OK,"");
  if(!input){diagnostic(error,LIMESTONE_INVALID_ARGUMENT,"null source input");return nullptr;}
  try {
    limestone::PipelineOptions configuration;
    if(options) {
      for(auto value:{options->optimize,options->schedule,options->allocate})if(value!=0&&value!=1){diagnostic(error,LIMESTONE_INVALID_ARGUMENT,"pipeline options must be Boolean");return nullptr;}
      configuration={bool(options->optimize),bool(options->schedule),bool(options->allocate)};
    }
    auto result=limestone::run_pipeline(input,configuration);
    if(!result){diagnostic(error,static_cast<limestone_status>(static_cast<int>(result.error().code)+1),result.error().message.c_str());return nullptr;}
    return new limestone_module{std::move(result.value())};
  }catch(const std::exception& e){diagnostic(error,LIMESTONE_INTERNAL,e.what());}
  catch(...){diagnostic(error,LIMESTONE_INTERNAL,"compiler exception");}
  return nullptr;
}
extern "C" limestone_module* limestone_compile(const char* input) {
  return limestone_compile_checked(input,nullptr,nullptr);
}
extern "C" const char* limestone_module_text(const limestone_module* module) {
  return module?module->module.machine_ir.c_str():nullptr;
}
extern "C" void limestone_module_destroy(limestone_module* module) { delete module; }
