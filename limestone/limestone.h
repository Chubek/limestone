#ifndef LIMESTONE_H
#define LIMESTONE_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct limestone_module limestone_module;
limestone_module *limestone_compile(const char *input);
const char *limestone_module_text(const limestone_module *);
void limestone_module_destroy(limestone_module *);
#ifdef __cplusplus
}
#endif
#endif
