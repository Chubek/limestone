#include "data_types_internal.hpp"
#include <algorithm>

namespace {
int exl_make_aggregate(const exl_native_type_t* const* fields,size_t count,bool array,exl_native_type_t** output) {
  if(!output||!fields||!count||count>EXL_NATIVE_MAX_TYPE_NODES)return -1;
  auto snapshot=std::make_shared<exl_data_type_snapshot>();snapshot->scalar=false;snapshot->array=array;
  std::vector<exl_ffi_data_type*> views;views.reserve(count);snapshot->children.reserve(count);
  for(size_t k=0;k<count;++k) {
    if(!fields[k]||!fields[k]->snapshot||(fields[k]->snapshot->scalar&&fields[k]->snapshot->kind==EXL_NATIVE_VOID))return -1;
    auto child=fields[k]->snapshot;
    if(child->nodes>EXL_NATIVE_MAX_TYPE_NODES-snapshot->nodes)return -3;
    snapshot->nodes+=child->nodes;snapshot->depth=std::max(snapshot->depth,child->depth+1);
    if(snapshot->depth>EXL_NATIVE_MAX_TYPE_DEPTH)return -3;
    views.push_back(child->native.get());snapshot->children.push_back(std::move(child));
  }
  exl_ffi_data_type* descriptor=nullptr;auto status=exl_ffi_data_aggregate(views.data(),views.size(),array,&descriptor);if(status)return status;
  snapshot->native.reset(descriptor);auto handle=std::make_unique<exl_native_type>();handle->snapshot=std::move(snapshot);*output=handle.release();return 0;
}
}
extern "C" EXL_DEF int exl_native_type_scalar(exl_native_kind_t kind,exl_native_type_t** output) {
  if(!output||kind<EXL_NATIVE_VOID||kind>EXL_NATIVE_PTR)return -1;
  try {
    auto snapshot=std::make_shared<exl_data_type_snapshot>();snapshot->kind=kind;
    exl_ffi_data_type* descriptor=nullptr;auto status=exl_ffi_data_scalar(kind,&descriptor);if(status)return status;
    snapshot->native.reset(descriptor);auto handle=std::make_unique<exl_native_type>();handle->snapshot=std::move(snapshot);*output=handle.release();return 0;
  }catch(...){return -3;}
}
extern "C" EXL_DEF int exl_native_type_struct(const exl_native_type_t* const* fields,size_t count,exl_native_type_t** output) {
  try{return exl_make_aggregate(fields,count,false,output);}catch(...){return -3;}
}
extern "C" EXL_DEF int exl_native_type_array(const exl_native_type_t* element,size_t count,exl_native_type_t** output) {
  if(!element||!count||count>EXL_NATIVE_MAX_TYPE_NODES||!output)return -1;
  try {std::vector<const exl_native_type_t*> fields(count,element);return exl_make_aggregate(fields.data(),count,true,output);}catch(...){return -3;}
}
extern "C" EXL_DEF void exl_native_type_destroy(exl_native_type_t* type){delete type;}
extern "C" EXL_DEF int exl_native_type_layout(const exl_native_type_t* type,size_t* size,size_t* alignment) {
  if(!type||!size||!alignment)return -1;return exl_ffi_data_layout(type->snapshot->native.get(),size,alignment);
}
extern "C" EXL_DEF int exl_native_type_offset(const exl_native_type_t* type,size_t index,size_t* offset) {
  if(!type||!offset)return -1;return exl_ffi_data_offset(type->snapshot->native.get(),index,offset);
}
