#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/QaMRpp-Library.h"

static const qamrpp_host_api* g_api = 0;

static qamrpp_value* lib_print(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    for (size_t i = 0; i < argc; ++i) {
        if (i) putchar(' ');
        qamrpp_value_type t = g_api->value_get_type(argv[i]);
        if (t == QAMRPP_TYPE_STRING) {
            size_t n = 0; const char* s = g_api->value_as_string(argv[i], &n); fwrite(s ? s : "", 1, n, stdout);
        } else if (t == QAMRPP_TYPE_INT) {
            printf("%lld", (long long)g_api->value_as_int(argv[i]));
        } else if (t == QAMRPP_TYPE_FLOAT) {
            printf("%g", g_api->value_as_float(argv[i]));
        } else if (t == QAMRPP_TYPE_BOOL) {
            fputs(g_api->value_as_bool(argv[i]) ? "true" : "false", stdout);
        } else {
            fputs("nil", stdout);
        }
    }
    putchar('\n');
    return g_api->value_nil(ctx);
}

static qamrpp_value* lib_type(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    const char* t = "nil";
    if (argc > 0) {
        switch (g_api->value_get_type(argv[0])) {
            case QAMRPP_TYPE_BOOL: t = "bool"; break;
            case QAMRPP_TYPE_INT: t = "int"; break;
            case QAMRPP_TYPE_FLOAT: t = "float"; break;
            case QAMRPP_TYPE_STRING: t = "string"; break;
            case QAMRPP_TYPE_FUNCTION: t = "function"; break;
            case QAMRPP_TYPE_USERDATA: t = "userdata"; break;
            default: t = "nil"; break;
        }
    }
    return g_api->value_string(ctx, t, strlen(t));
}

static qamrpp_value* lib_tostring(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    char buf[64];
    if (argc == 0) return g_api->value_string(ctx, "", 0);
    switch (g_api->value_get_type(argv[0])) {
        case QAMRPP_TYPE_STRING: {
            size_t len = 0; const char* s = g_api->value_as_string(argv[0], &len); return g_api->value_string(ctx, s ? s : "", len);
        }
        case QAMRPP_TYPE_INT: {
            int n = snprintf(buf, sizeof(buf), "%lld", (long long)g_api->value_as_int(argv[0]));
            return g_api->value_string(ctx, buf, (size_t)(n > 0 ? n : 0));
        }
        case QAMRPP_TYPE_FLOAT: {
            int n = snprintf(buf, sizeof(buf), "%g", g_api->value_as_float(argv[0]));
            return g_api->value_string(ctx, buf, (size_t)(n > 0 ? n : 0));
        }
        case QAMRPP_TYPE_BOOL:
            return g_api->value_string(ctx, g_api->value_as_bool(argv[0]) ? "true" : "false", g_api->value_as_bool(argv[0]) ? 4 : 5);
        default:
            return g_api->value_string(ctx, "nil", 3);
    }
}

static qamrpp_value* lib_tonumber(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc == 0) return g_api->value_nil(ctx);
    if (g_api->value_get_type(argv[0]) == QAMRPP_TYPE_INT) return g_api->value_int(ctx, g_api->value_as_int(argv[0]));
    if (g_api->value_get_type(argv[0]) == QAMRPP_TYPE_FLOAT) return g_api->value_float(ctx, g_api->value_as_float(argv[0]));
    if (g_api->value_get_type(argv[0]) == QAMRPP_TYPE_STRING) {
        size_t len = 0; const char* s = g_api->value_as_string(argv[0], &len);
        if (!s) return g_api->value_nil(ctx);
        return g_api->value_float(ctx, atof(s));
    }
    return g_api->value_nil(ctx);
}

static qamrpp_value* lib_assert(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc == 0 || !g_api->value_as_bool(argv[0])) {
        g_api->set_error(ctx, QAMRPP_ERR_GENERIC, "assertion failed");
        return g_api->value_nil(ctx);
    }
    return argv[0];
}

static qamrpp_value* lib_error(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc > 0 && g_api->value_get_type(argv[0]) == QAMRPP_TYPE_STRING) {
        size_t len = 0; const char* s = g_api->value_as_string(argv[0], &len); (void)len;
        g_api->set_error(ctx, QAMRPP_ERR_GENERIC, s ? s : "error");
    } else {
        g_api->set_error(ctx, QAMRPP_ERR_GENERIC, "error");
    }
    return g_api->value_nil(ctx);
}

static qamrpp_value* lib_rawequal(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc < 2) return g_api->value_bool(ctx, 0);
    qamrpp_value_type t1 = g_api->value_get_type(argv[0]);
    qamrpp_value_type t2 = g_api->value_get_type(argv[1]);
    if (t1 != t2) return g_api->value_bool(ctx, 0);
    if (t1 == QAMRPP_TYPE_INT) return g_api->value_bool(ctx, g_api->value_as_int(argv[0]) == g_api->value_as_int(argv[1]));
    if (t1 == QAMRPP_TYPE_FLOAT) return g_api->value_bool(ctx, g_api->value_as_float(argv[0]) == g_api->value_as_float(argv[1]));
    if (t1 == QAMRPP_TYPE_BOOL) return g_api->value_bool(ctx, g_api->value_as_bool(argv[0]) == g_api->value_as_bool(argv[1]));
    if (t1 == QAMRPP_TYPE_STRING) {
        size_t a=0,b=0; const char* sa=g_api->value_as_string(argv[0],&a); const char* sb=g_api->value_as_string(argv[1],&b);
        return g_api->value_bool(ctx, a==b && sa && sb && memcmp(sa,sb,a)==0);
    }
    return g_api->value_bool(ctx, argv[0] == argv[1]);
}

static qamrpp_value* lib_select(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    if (argc == 0) return g_api->value_int(ctx, 0);
    return g_api->value_int(ctx, (int64_t)(argc - 1));
}

static qamrpp_value* lib_pcall(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_bool(ctx, 0); }
static qamrpp_value* lib_xpcall(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_bool(ctx, 0); }
static qamrpp_value* lib_nil(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) { (void)argv; (void)argc; return g_api->value_nil(ctx); }

static qamrpp_native_binding kBindings[] = {
    {"assert", lib_assert}, {"collectgarbage", lib_nil}, {"dofile", lib_nil}, {"error", lib_error},
    {"getmetatable", lib_nil}, {"ipairs", lib_nil}, {"load", lib_nil}, {"loadfile", lib_nil},
    {"next", lib_nil}, {"pairs", lib_nil}, {"pcall", lib_pcall}, {"print", lib_print},
    {"rawequal", lib_rawequal}, {"rawget", lib_nil}, {"rawlen", lib_nil}, {"rawset", lib_nil},
    {"require", lib_nil}, {"select", lib_select}, {"setmetatable", lib_nil}, {"tonumber", lib_tonumber},
    {"tostring", lib_tostring}, {"type", lib_type}, {"xpcall", lib_xpcall}
};

static int on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    g_api = host_api;
    g_api->set_global(ctx, "_VERSION", g_api->value_string(ctx, "Lua 5.4", 7));
    return 0;
}

static const qamrpp_library_descriptor kDescriptor = {
    QAMRPP_LIBRARY_API_VERSION,
    "core",
    kBindings,
    sizeof(kBindings) / sizeof(kBindings[0]),
    on_load,
    0
};

QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(kDescriptor)
