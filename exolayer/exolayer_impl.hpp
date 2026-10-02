#pragma once
#define ITK_ERROR_IMPLEMENTATION
#define FFI_LOADER_IMPLEMENTATION
#define ETK_DYNLOAD_IMPLEMENTATION
#include <ExtensionTk/etk_dynload.h>
#include <algorithm>
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct exl_library {
  etk_lib_handle handle{};
  size_t active_calls=0;
  ~exl_library() { if(handle.handle)etk_lib_close(&handle); }
};
struct exl_context {
  struct Function { exl_native_fn fn; void* userdata; std::shared_ptr<exl_library> library; };
  std::unordered_map<std::string,Function> functions;
  std::vector<std::shared_ptr<exl_library>> libraries;
  std::string error;
};
namespace {
int exl_fail(exl_context* c,int status,const char* text) noexcept {
  if(c)try{c->error=text;}catch(...){c->error.clear();}
  return status;
}
bool exl_valid_kind(exl_kind_t k,bool result) {
  return k==EXL_I64||k==EXL_F64||k==EXL_PTR||(result&&k==EXL_VOID);
}
}
extern "C" EXL_DEF exl_context_t* exl_context_create() {
  try{return new exl_context;}catch(...){return nullptr;}
}
extern "C" EXL_DEF void exl_context_destroy(exl_context_t* c) {
  if(!c)return;
  c->functions.clear();
  while(!c->libraries.empty())c->libraries.pop_back();
  delete c;
}
extern "C" EXL_DEF int exl_register(exl_context_t* c,const char* name,exl_native_fn fn,void* userdata) {
  if(!c||!name||!*name||!fn)return exl_fail(c,-1,"invalid callback registration");
  try {
    c->error.clear();
    if(!c->functions.emplace(name,exl_context::Function{fn,userdata,{}}).second)return exl_fail(c,-2,"duplicate symbol");
    return 0;
  }catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"callback registration failed");}
}
extern "C" EXL_DEF int exl_call(exl_context_t* c,const char* name,const exl_value_t* args,size_t count,exl_value_t* result) {
  // Copy arguments before clearing result: callers may use an argument as result.
  if(!c||!name||!*name||!result||(count&&!args)) {
    if(result)*result={EXL_VOID,{0}};
    return exl_fail(c,-1,"invalid callback invocation");
  }
  try {
    c->error.clear();
    for(size_t i=0;i<count;++i)if(!exl_valid_kind(args[i].kind,false)) {
      *result={EXL_VOID,{0}};return exl_fail(c,-1,"invalid callback argument kind");
    }
    std::vector<exl_value_t> copied;
    if(count)copied.assign(args,args+count);
    *result={EXL_VOID,{0}};
    auto it=c->functions.find(name);
    if(it==c->functions.end())return exl_fail(c,-2,"symbol not found");
    // Registry mutation in a reentrant call must not invalidate this invocation.
    const auto function=it->second;
    struct ActiveCall {
      std::shared_ptr<exl_library> library;
      ActiveCall(std::shared_ptr<exl_library> p):library(std::move(p)){if(library)++library->active_calls;}
      ~ActiveCall(){if(library)--library->active_calls;}
    } active(function.library);
    exl_value_t returned{EXL_VOID,{0}};
    function.fn(copied.data(),count,&returned,function.userdata);
    if(!exl_valid_kind(returned.kind,true))return exl_fail(c,-3,"invalid callback result kind");
    *result=returned;c->error.clear();return 0;
  }catch(const std::exception& e){*result={EXL_VOID,{0}};return exl_fail(c,-3,e.what());}
  catch(...){*result={EXL_VOID,{0}};return exl_fail(c,-3,"callback threw an exception");}
}
extern "C" EXL_DEF const char* exl_last_error(const exl_context_t* c) {
  return c?c->error.c_str():"invalid context";
}
extern "C" EXL_DEF exl_library_t* exl_library_open(exl_context_t* c,const char* path) {
  if(!c||!path||!*path){exl_fail(c,-1,"invalid extension path");return nullptr;}
  try {
    c->error.clear();auto library=std::make_shared<exl_library>();
    if(etk_lib_open(path,ETK_LIB_FLAG_NOW|ETK_LIB_FLAG_LOCAL,&library->handle)!=ETK_OK) {
      exl_fail(c,-3,library->handle.error.message);return nullptr;
    }
    auto* descriptor=static_cast<const exl_extension_descriptor_t*>(etk_lib_sym(&library->handle,"exl_extension_descriptor"));
    if(!descriptor||descriptor->abi_version!=EXL_EXTENSION_ABI_VERSION) {
      exl_fail(c,-3,"missing or incompatible exl_extension_descriptor");return nullptr;
    }
    if(descriptor->symbol_count>65536||(descriptor->symbol_count&&!descriptor->symbols)) {
      exl_fail(c,-1,"invalid extension symbol table");return nullptr;
    }
    // Transactional copies make duplicate/invalid exports and allocation failures
    // leave the original registry intact, while RAII closes the rejected library.
    auto functions=c->functions;auto libraries=c->libraries;
    for(size_t i=0;i<descriptor->symbol_count;++i) {
      const auto& s=descriptor->symbols[i];
      if(!s.name||!*s.name||!s.function){exl_fail(c,-1,"invalid extension export");return nullptr;}
      if(!functions.emplace(s.name,exl_context::Function{s.function,s.userdata,library}).second) {
        exl_fail(c,-2,"duplicate extension symbol");return nullptr;
      }
    }
    libraries.push_back(library);
    c->functions.swap(functions);c->libraries.swap(libraries);return library.get();
  }catch(const std::exception& e){exl_fail(c,-3,e.what());}catch(...){exl_fail(c,-3,"extension load failed");}
  return nullptr;
}
extern "C" EXL_DEF int exl_library_close(exl_context_t* c,exl_library_t* library) {
  if(!c)return -1;
  c->error.clear();
  auto it=std::find_if(c->libraries.begin(),c->libraries.end(),[&](auto& p){return p.get()==library;});
  if(it==c->libraries.end())return exl_fail(c,-1,"unknown extension library");
  if((*it)->active_calls)return exl_fail(c,-3,"extension library is active");
  std::erase_if(c->functions,[&](auto& item){return item.second.library.get()==library;});
  c->libraries.erase(it);return 0;
}
