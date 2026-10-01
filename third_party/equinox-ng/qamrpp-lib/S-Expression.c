#include "QaMRpp-Library.h"

static int sexpr_on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    (void)ctx;
    (void)host_api;
    return 0;
}

static const qamrpp_library_descriptor sexpr_descriptor = {
    QAMRPP_LIBRARY_API_VERSION,
    "sexpr",
    0,
    0,
    sexpr_on_load,
    0
};

QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(sexpr_descriptor)
