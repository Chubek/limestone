#include "exolayer.h"
static void extension_sum(const exl_value_t* args,size_t count,exl_value_t* result,void* userdata) {
  (void)userdata;if(count!=2)return;result->kind=EXL_I64;result->as.i64=args[0].as.i64+args[1].as.i64;
}
static const exl_extension_symbol_t symbols[]={{"extension_sum",extension_sum,NULL}};
const exl_extension_descriptor_t exl_extension_descriptor={EXL_EXTENSION_ABI_VERSION,1,symbols};
