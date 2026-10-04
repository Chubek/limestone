#include <nuperf/nuperf-method.h>
extern "C" const nuperf_method_t* nuperf_method_descriptor(void);
extern "C" const nuperf_method_t* nuperf_method_descriptor(void) { static const nuperf_method_t d{"FCH", "FCH plugin placeholder", 0, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}; return &d; }
