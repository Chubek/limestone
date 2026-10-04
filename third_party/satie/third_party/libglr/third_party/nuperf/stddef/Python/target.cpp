#include <nuperf/nuperf-target.h>
extern "C" const nuperf_target_t* nuperf_target_descriptor(void);
extern "C" const nuperf_target_t* nuperf_target_descriptor(void) { static const nuperf_target_t d{"Python", "Python target plugin placeholder", NUPERF_TARGET_FLAG_TEXT_OUTPUT, ".py", nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr}; return &d; }
