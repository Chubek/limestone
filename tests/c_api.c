#include "limestone.h"
#include "exolayer.h"
#include <assert.h>
#include <string.h>
extern int c_header_other(void);
static void identity(const exl_value_t *args,size_t count,exl_value_t *result,void *userdata) {
  (void)userdata;assert(count==1);*result=args[0];
}
int main(void) {
  limestone_options options;limestone_options_default(&options);assert(options.optimize==1&&options.allocate==0);
  limestone_error error;limestone_module* module=limestone_compile_checked("(add 1 2)",&options,&error);
  assert(module&&error.code==LIMESTONE_OK);assert(strstr(limestone_module_text(module),"#3"));limestone_module_destroy(module);
  assert(limestone_module_text(NULL)==NULL);limestone_module_destroy(NULL);
  exl_context_t* context=exl_context_create();assert(context);
  assert(exl_register(context,"id",identity,NULL)==0);
  exl_value_t value;value.kind=EXL_I64;value.as.i64=42;
  assert(exl_call(context,"id",&value,1,&value)==0&&value.as.i64==42);
  assert(exl_call(context,"id",NULL,1,&value)<0&&value.kind==EXL_VOID);
  exl_context_destroy(context);assert(c_header_other()==EXL_I64);return 0;
}
