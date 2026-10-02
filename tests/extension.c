#include "exolayer.h"
#include "native_fixture.h"
#include <stdarg.h>
static void extension_sum(const exl_value_t* args,size_t count,exl_value_t* result,void* userdata) {
  (void)userdata;if(count!=2)return;result->kind=EXL_I64;result->as.i64=args[0].as.i64+args[1].as.i64;
}
static const exl_extension_symbol_t symbols[]={{"extension_sum",extension_sum,NULL}};
const exl_extension_descriptor_t exl_extension_descriptor={EXL_EXTENSION_ABI_VERSION,1,symbols};
long long exl_fixture_native_sum(long long a,long long b){return a+b;}
double exl_fixture_native_mixed(long long a,double b,void *p){return (double)a+b+(p?1.0:0.0);}
void *exl_fixture_native_pointer(void *p){return p;}
void exl_fixture_native_store(void *p,long long value){*(long long *)p=value;}
long long exl_fixture_native_reenter(void *p){struct native_fixture_state *state=(struct native_fixture_state *)p;return state->close(state->context,state->library);}
double exl_fixture_native_variadic(long long count,...) {
  va_list args;long long k;double result=0;va_start(args,count);
  for(k=0;k<count;++k){long long integer=va_arg(args,long long);double real=va_arg(args,double);void *pointer=va_arg(args,void *);result+=(double)integer+real+(pointer?1.0:0.0);}
  va_end(args);return result;
}
