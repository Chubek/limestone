%module nuperf

%{
#include <nuperf/nuperf-api.h>
%}

%include "stdint.i"

#ifdef NUPERF_XFEAT_ENUM_MAPPING
%inline %{
const char *nuperf_status_name(nuperf_status_t st) {
    return nuperf_strerror(st);
}
%}
#endif

%include "nuperf/nuperf-types.h"
%include "nuperf/nuperf-api.h"
