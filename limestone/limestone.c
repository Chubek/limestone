#include "limestone.h"
#include <stdlib.h>
#include <string.h>
struct limestone_module { char *text; };
limestone_module *limestone_compile(const char *input){if(!input)return NULL;limestone_module*m=(limestone_module*)calloc(1,sizeof*m);m->text=(char*)malloc(strlen(input)+1);strcpy(m->text,input);return m;}
const char *limestone_module_text(const limestone_module*m){return m?m->text:NULL;}
void limestone_module_destroy(limestone_module*m){if(m){free(m->text);free(m);}}
