#pragma once
#define ITK_ERROR_IMPLEMENTATION
#define FFI_LOADER_IMPLEMENTATION
#define ETK_DYNLOAD_IMPLEMENTATION
#define FFI_CIF_IMPLEMENTATION
#include <ExtensionTk/etk_dynload.h>
#include <FFItk/ffi_cif.h>
#include "native_internal.h"
#include "data_types_internal.hpp"
#include <algorithm>
#include <cstring>
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
struct exl_native_binding {
  exl_native_address_t address{};
  std::unique_ptr<exl_ffi_handle,decltype(&exl_ffi_destroy)> prepared{nullptr,exl_ffi_destroy};
  ffi_cif signature{};
  itk_type result{};
  std::vector<itk_type> arguments;
};
struct exl_data_binding {
  exl_native_address_t address{};
  std::shared_ptr<const exl_data_type_snapshot> result;
  std::vector<std::shared_ptr<const exl_data_type_snapshot>> arguments;
  std::unique_ptr<exl_ffi_data_handle,decltype(&exl_ffi_data_destroy)> prepared{nullptr,exl_ffi_data_destroy};
};
struct exl_context {
  struct Function {
    exl_native_fn fn; void* userdata; std::shared_ptr<exl_library> library;
    bool typed=false;exl_kind_t result=EXL_VOID;std::vector<exl_kind_t> arguments;
    std::shared_ptr<exl_native_binding> native;
    std::shared_ptr<exl_data_binding> data;
  };
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
struct exl_active_call {
  std::shared_ptr<exl_library> library;
  explicit exl_active_call(std::shared_ptr<exl_library> p):library(std::move(p)){if(library)++library->active_calls;}
  ~exl_active_call(){if(library)--library->active_calls;}
};
int exl_add_data(exl_context* c,const char* name,exl_native_address_t address,const exl_data_signature_t* signature,exl_callconv_t convention,std::shared_ptr<exl_library> library={}) {
  if(!c||!name||!*name||!address||!signature||!signature->result_type||signature->argument_count>EXL_NATIVE_MAX_ARGS||(signature->argument_count&&!signature->argument_types)||convention<EXL_CC_HOST||convention>EXL_CC_AAPCS)return exl_fail(c,-1,"invalid native data registration");
  c->error.clear();if(c->functions.contains(name))return exl_fail(c,-2,"duplicate native symbol");
  auto binding=std::make_shared<exl_data_binding>();binding->address=address;binding->result=signature->result_type->snapshot;
  if(binding->result->array)return exl_fail(c,-1,"native array result requires a pointer or enclosing struct");
  std::vector<exl_ffi_data_type*> arguments;
  for(size_t k=0;k<signature->argument_count;++k){auto handle=signature->argument_types[k];if(!handle)return exl_fail(c,-1,"null native argument type");auto type=handle->snapshot;if(type->array||(type->scalar&&type->kind==EXL_NATIVE_VOID))return exl_fail(c,-1,"native data argument must be a non-VOID scalar or struct");arguments.push_back(type->native.get());binding->arguments.push_back(std::move(type));}
  exl_ffi_data_handle* prepared=nullptr;auto status=exl_ffi_data_prepare(binding->result->native.get(),arguments.data(),arguments.size(),convention,&prepared);
  if(status)return exl_fail(c,status,status==-4?"native aggregate backend or calling convention is unavailable":"native data signature preparation failed");
  binding->prepared.reset(prepared);exl_context::Function function{nullptr,nullptr,std::move(library)};function.data=std::move(binding);c->functions.emplace(name,std::move(function));return 0;
}
itk_type exl_native_type(exl_kind_t kind) {
  switch(kind){case EXL_VOID:return itk_type_prim(ITK_KIND_VOID);case EXL_I64:return itk_type_prim(ITK_KIND_I64);case EXL_F64:return itk_type_prim(ITK_KIND_DOUBLE);case EXL_PTR:return itk_type_prim(ITK_KIND_PTR);}
  return itk_type_prim(ITK_KIND_VOID);
}
int exl_add_native(exl_context* c,const char* name,exl_native_address_t address,const exl_signature_t* signature,exl_callconv_t convention,std::shared_ptr<exl_library> library={},bool variadic=false,size_t fixed_count=0) {
  static_assert(EXL_NATIVE_MAX_ARGS==FFI_MAX_ARGS);
  if(!c||!name||!*name||!address||!signature||!exl_valid_kind(signature->return_kind,true)||signature->argument_count>EXL_NATIVE_MAX_ARGS||(signature->argument_count&&!signature->argument_kinds)||convention<EXL_CC_HOST||convention>EXL_CC_AAPCS)return exl_fail(c,-1,"invalid native registration");
  if(variadic&&(!fixed_count||fixed_count>signature->argument_count))return exl_fail(c,-1,"invalid variadic fixed-argument count");
  if(variadic&&(convention==EXL_CC_STDCALL||convention==EXL_CC_FASTCALL))return exl_fail(c,-4,"variadic calling convention is unsupported");
  c->error.clear();
  if(c->functions.contains(name))return exl_fail(c,-2,"duplicate native symbol");
  auto native=std::make_shared<exl_native_binding>();native->address=address;native->result=exl_native_type(signature->return_kind);
  exl_context::Function function{nullptr,nullptr,std::move(library)};function.typed=true;function.result=signature->return_kind;
  for(size_t k=0;k<signature->argument_count;++k){auto kind=signature->argument_kinds[k];if(!exl_valid_kind(kind,false))return exl_fail(c,-1,"invalid native argument kind");function.arguments.push_back(kind);native->arguments.push_back(exl_native_type(kind));}
  const itk_type* arguments[FFI_MAX_ARGS];for(size_t k=0;k<native->arguments.size();++k)arguments[k]=&native->arguments[k];
  const itk_callconv conventions[]{ITK_CALLCONV_DEFAULT,ITK_CALLCONV_SYSV64,ITK_CALLCONV_WIN64,ITK_CALLCONV_CDECL,ITK_CALLCONV_STDCALL,ITK_CALLCONV_FASTCALL,ITK_CALLCONV_AAPCS64,ITK_CALLCONV_AAPCS};
  auto signature_status=variadic?ffi_cif_prepare_var(&native->signature,conventions[convention],&native->result,arguments,fixed_count,native->arguments.size()):ffi_cif_prepare(&native->signature,conventions[convention],&native->result,arguments,native->arguments.size());
  if(signature_status!=FFI_OK)return exl_fail(c,-1,"FFItk rejected native signature");
  exl_ffi_handle* prepared=nullptr;auto status=exl_ffi_prepare(signature,convention,variadic,fixed_count,&prepared);
  if(status)return exl_fail(c,status,status==-4?"native backend, calling convention, or scalar layout is unavailable":"native call preparation failed");
  native->prepared.reset(prepared);function.native=std::move(native);c->functions.emplace(name,std::move(function));return 0;
}
int exl_bind_library(exl_context* c,exl_library* library,const char* name,const char* symbol,const exl_signature_t* signature,exl_callconv_t convention,bool variadic=false,size_t fixed_count=0) {
  if(!c||!name||!*name||!symbol||!*symbol)return exl_fail(c,-1,"invalid native symbol binding");
  c->error.clear();auto found=std::find_if(c->libraries.begin(),c->libraries.end(),[&](auto& p){return p.get()==library;});
  if(found==c->libraries.end())return exl_fail(c,-1,"unknown native library");
  auto address=etk_lib_sym(&(*found)->handle,symbol);if(!address)return exl_fail(c,-2,(*found)->handle.error.message);
  exl_native_address_t function=nullptr;
  if constexpr(sizeof(function)==sizeof(address))std::memcpy(&function,&address,sizeof(function));
  else return exl_fail(c,-4,"loader function-pointer representation is unsupported");
  return exl_add_native(c,name,function,signature,convention,*found,variadic,fixed_count);
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
extern "C" EXL_DEF int exl_register_typed(exl_context_t* c,const char* name,exl_native_fn fn,void* userdata,const exl_signature_t* signature) {
  if(!c||!name||!*name||!fn||!signature||!exl_valid_kind(signature->return_kind,true)||signature->argument_count>65536||(signature->argument_count&&!signature->argument_kinds))return exl_fail(c,-1,"invalid typed callback registration");
  try {
    c->error.clear();exl_context::Function function{fn,userdata,{}};function.typed=true;function.result=signature->return_kind;
    for(size_t k=0;k<signature->argument_count;++k){if(!exl_valid_kind(signature->argument_kinds[k],false))return exl_fail(c,-1,"invalid callback signature argument");function.arguments.push_back(signature->argument_kinds[k]);}
    if(!c->functions.emplace(name,std::move(function)).second)return exl_fail(c,-2,"duplicate symbol");return 0;
  }catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"typed callback registration failed");}
}
extern "C" EXL_DEF int exl_register_native(exl_context_t* c,const char* name,exl_native_address_t function,const exl_signature_t* signature,exl_callconv_t convention) {
  try{return exl_add_native(c,name,function,signature,convention);}catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"native registration failed");}
}
extern "C" EXL_DEF int exl_register_native_variadic(exl_context_t* c,const char* name,exl_native_address_t function,const exl_signature_t* signature,size_t fixed_count,exl_callconv_t convention) {
  try{return exl_add_native(c,name,function,signature,convention,{},true,fixed_count);}catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"variadic native registration failed");}
}
extern "C" EXL_DEF int exl_register_native_data(exl_context_t* c,const char* name,exl_native_address_t function,const exl_data_signature_t* signature,exl_callconv_t convention) {
  try{return exl_add_data(c,name,function,signature,convention);}catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"native data registration failed");}
}
extern "C" EXL_DEF int exl_call(exl_context_t* c,const char* name,const exl_value_t* args,size_t count,exl_value_t* result) {
  // Copy arguments before clearing result: callers may use an argument as result.
  if(!c||!name||!*name||!result||(count&&!args)||count>65536) {
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
    if(function.data)return exl_fail(c,-1,"native data function requires exl_call_data");
    if(function.typed) {
      if(function.arguments.size()!=count)return exl_fail(c,-1,"callback argument count mismatch");
      for(size_t k=0;k<count;++k)if(copied[k].kind!=function.arguments[k])return exl_fail(c,-1,"callback argument type mismatch");
    }
    exl_active_call active(function.library);
    exl_value_t returned{EXL_VOID,{0}};
    if(function.native){auto status=exl_ffi_invoke(function.native->prepared.get(),function.native->address,copied.data(),&returned);if(status)return exl_fail(c,status,"native invocation failed");}
    else function.fn(copied.data(),count,&returned,function.userdata);
    if(!exl_valid_kind(returned.kind,true))return exl_fail(c,-3,"invalid callback result kind");
    if(function.typed&&returned.kind!=function.result)return exl_fail(c,-3,"callback result type mismatch");
    *result=returned;c->error.clear();return 0;
  }catch(const std::exception& e){*result={EXL_VOID,{0}};return exl_fail(c,-3,e.what());}
  catch(...){*result={EXL_VOID,{0}};return exl_fail(c,-3,"callback threw an exception");}
}
extern "C" EXL_DEF int exl_call_data(exl_context_t* c,const char* name,const exl_data_argument_t* arguments,size_t count,void* result,size_t result_size) {
  if(!c||!name||!*name||count>EXL_NATIVE_MAX_ARGS||(count&&!arguments))return exl_fail(c,-1,"invalid native data invocation");
  try {
    c->error.clear();auto it=c->functions.find(name);if(it==c->functions.end())return exl_fail(c,-2,"native data symbol not found");
    const auto function=it->second;if(!function.data)return exl_fail(c,-1,"symbol is not a native data function");
    if(count!=function.data->arguments.size())return exl_fail(c,-1,"native data argument count mismatch");
    exl_active_call active(function.library);
    auto status=exl_ffi_data_invoke(function.data->prepared.get(),function.data->address,arguments,count,result,result_size);
    if(status)return exl_fail(c,status,status==-1?"native data argument/result buffer size mismatch":"native data invocation failed");
    c->error.clear();return 0;
  }catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"native data invocation failed");}
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
extern "C" EXL_DEF exl_library_t* exl_library_open_native(exl_context_t* c,const char* path) {
  if(!c||!path||!*path){exl_fail(c,-1,"invalid native library path");return nullptr;}
  try {
    c->error.clear();auto library=std::make_shared<exl_library>();
    if(etk_lib_open(path,ETK_LIB_FLAG_NOW|ETK_LIB_FLAG_LOCAL,&library->handle)!=ETK_OK){exl_fail(c,-3,library->handle.error.message);return nullptr;}
    c->libraries.push_back(library);return library.get();
  }catch(const std::exception& e){exl_fail(c,-3,e.what());}catch(...){exl_fail(c,-3,"native library load failed");}return nullptr;
}
extern "C" EXL_DEF int exl_library_bind(exl_context_t* c,exl_library_t* library,const char* name,const char* symbol,const exl_signature_t* signature,exl_callconv_t convention) {
  try{return exl_bind_library(c,library,name,symbol,signature,convention);}catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"native symbol binding failed");}
}
extern "C" EXL_DEF int exl_library_bind_variadic(exl_context_t* c,exl_library_t* library,const char* name,const char* symbol,const exl_signature_t* signature,size_t fixed_count,exl_callconv_t convention) {
  try{return exl_bind_library(c,library,name,symbol,signature,convention,true,fixed_count);}catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"variadic native symbol binding failed");}
}
extern "C" EXL_DEF int exl_library_bind_data(exl_context_t* c,exl_library_t* library,const char* name,const char* symbol,const exl_data_signature_t* signature,exl_callconv_t convention) {
  if(!c||!name||!*name||!symbol||!*symbol)return exl_fail(c,-1,"invalid native data symbol binding");
  try {
    c->error.clear();auto found=std::find_if(c->libraries.begin(),c->libraries.end(),[&](auto& p){return p.get()==library;});if(found==c->libraries.end())return exl_fail(c,-1,"unknown native library");
    auto address=etk_lib_sym(&(*found)->handle,symbol);if(!address)return exl_fail(c,-2,(*found)->handle.error.message);
    exl_native_address_t function=nullptr;if constexpr(sizeof(function)==sizeof(address))std::memcpy(&function,&address,sizeof(function));else return exl_fail(c,-4,"loader function-pointer representation is unsupported");
    return exl_add_data(c,name,function,signature,convention,*found);
  }catch(const std::exception& e){return exl_fail(c,-3,e.what());}catch(...){return exl_fail(c,-3,"native data symbol binding failed");}
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
