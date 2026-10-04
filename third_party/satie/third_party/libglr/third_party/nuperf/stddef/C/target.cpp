#include <nuperf/nuperf-target.h>
extern "C" const nuperf_target_t* nuperf_target_descriptor(void);
extern "C" const nuperf_target_t* nuperf_target_descriptor(void) { static const nuperf_target_t d{"C", "C target plugin placeholder", NUPERF_TARGET_FLAG_TEXT_OUTPUT, ".h", nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr}; return &d; }
