#include "QaMRpp-Library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEQVSG_MAX_RULES 1024u
#define LEQVSG_MAX_REWRITE_PASSES 1024u

typedef struct {
    char* pattern;
    char* replacement;
} leqvsg_rule;

static int leqvsg_trace_enabled = 0;
static char* leqvsg_expr_definition = 0;
static char* leqvsg_rules_source = 0;
static leqvsg_rule* leqvsg_rules = 0;
static size_t leqvsg_rule_count = 0;

static void leqvsg_set_error(qamrpp_context* ctx, qamrpp_error_code code, const char* message) {
    if (ctx) {
        qamrpp_set_error(ctx, code, message);
        return;
    }
    fprintf(stderr, "LEQVSG ERROR [%d]: %s\n", (int)code, message ? message : "(null)");
}

static void leqvsg_free_string(char** s) {
    if (s && *s) {
        free(*s);
        *s = 0;
    }
}

static void leqvsg_clear_rules(void) {
    size_t i;
    for (i = 0; i < leqvsg_rule_count; ++i) {
        free(leqvsg_rules[i].pattern);
        free(leqvsg_rules[i].replacement);
    }
    free(leqvsg_rules);
    leqvsg_rules = 0;
    leqvsg_rule_count = 0;
}

static char* leqvsg_strdup_slice(const char* src, size_t len) {
    char* out = (char*)malloc(len + 1u);
    if (!out) {
        return 0;
    }
    if (len) {
        memcpy(out, src, len);
    }
    out[len] = '\0';
    return out;
}

static int leqvsg_read_file(const char* path, char** out_buf, size_t* out_len) {
    FILE* f;
    long end;
    size_t nread;
    char* buf;

    if (!path || !out_buf) {
        return 0;
    }

    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }

    end = ftell(f);
    if (end < 0) {
        fclose(f);
        return 0;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    buf = (char*)malloc((size_t)end + 1u);
    if (!buf) {
        fclose(f);
        return 0;
    }

    nread = fread(buf, 1u, (size_t)end, f);
    fclose(f);

    if (nread != (size_t)end) {
        free(buf);
        return 0;
    }

    buf[nread] = '\0';
    *out_buf = buf;
    if (out_len) {
        *out_len = nread;
    }

    return 1;
}

static const char* leqvsg_skip_space(const char* s, const char* end) {
    while (s < end && (*s == ' ' || *s == '\t' || *s == '\r')) {
        ++s;
    }
    return s;
}

static const char* leqvsg_rtrim_end(const char* begin, const char* end) {
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        --end;
    }
    return end;
}

static int leqvsg_parse_rules(const char* src) {
    const char* p = src;

    leqvsg_clear_rules();

    while (*p) {
        const char* line_start = p;
        const char* line_end;
        const char* lhs_start;
        const char* lhs_end;
        const char* rhs_start;
        const char* rhs_end;
        const char* arrow;
        leqvsg_rule* grown;

        while (*p && *p != '\n') {
            ++p;
        }
        line_end = p;
        if (*p == '\n') {
            ++p;
        }

        lhs_start = leqvsg_skip_space(line_start, line_end);
        if (lhs_start >= line_end || *lhs_start == '#') {
            continue;
        }

        arrow = 0;
        {
            const char* q;
            for (q = lhs_start; q + 1 < line_end; ++q) {
                if (q[0] == '=' && q[1] == '>') {
                    arrow = q;
                    break;
                }
            }
        }

        if (!arrow) {
            return 0;
        }

        lhs_end = leqvsg_rtrim_end(lhs_start, arrow);
        rhs_start = leqvsg_skip_space(arrow + 2, line_end);
        rhs_end = leqvsg_rtrim_end(rhs_start, line_end);

        if (lhs_end <= lhs_start) {
            return 0;
        }

        if (leqvsg_rule_count >= LEQVSG_MAX_RULES) {
            return 0;
        }

        grown = (leqvsg_rule*)realloc(leqvsg_rules, (leqvsg_rule_count + 1u) * sizeof(leqvsg_rule));
        if (!grown) {
            return 0;
        }
        leqvsg_rules = grown;
        leqvsg_rules[leqvsg_rule_count].pattern = leqvsg_strdup_slice(lhs_start, (size_t)(lhs_end - lhs_start));
        leqvsg_rules[leqvsg_rule_count].replacement = leqvsg_strdup_slice(rhs_start, (size_t)(rhs_end - rhs_start));

        if (!leqvsg_rules[leqvsg_rule_count].pattern || !leqvsg_rules[leqvsg_rule_count].replacement) {
            return 0;
        }

        leqvsg_rule_count += 1u;
    }

    return 1;
}

static char* leqvsg_replace_once(const char* input, const leqvsg_rule* rule, int* changed) {
    const char* pos;
    size_t in_len;
    size_t pat_len;
    size_t rep_len;
    size_t out_len;
    char* out;

    *changed = 0;
    if (!input || !rule || !rule->pattern || !rule->replacement) {
        return 0;
    }

    pat_len = strlen(rule->pattern);
    rep_len = strlen(rule->replacement);
    in_len = strlen(input);

    if (pat_len == 0u) {
        out = leqvsg_strdup_slice(input, in_len);
        return out;
    }

    pos = strstr(input, rule->pattern);
    if (!pos) {
        out = leqvsg_strdup_slice(input, in_len);
        return out;
    }

    out_len = in_len - pat_len + rep_len;
    out = (char*)malloc(out_len + 1u);
    if (!out) {
        return 0;
    }

    memcpy(out, input, (size_t)(pos - input));
    memcpy(out + (size_t)(pos - input), rule->replacement, rep_len);
    memcpy(out + (size_t)(pos - input) + rep_len, pos + pat_len, in_len - (size_t)(pos - input) - pat_len);
    out[out_len] = '\0';

    *changed = 1;
    return out;
}

static qamrpp_value* eqvsg_define_expr_native(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    size_t name_len = 0;
    size_t schema_len = 0;
    const char* name;
    const char* schema;
    size_t total_len;
    char* combined;

    if (argc != 2) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_define_expr expects 2 arguments: name and schema");
        return 0;
    }

    name = qamrpp_value_to_string(argv[0], &name_len);
    schema = qamrpp_value_to_string(argv[1], &schema_len);
    if (!name || !schema) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_define_expr expects two string arguments");
        return 0;
    }

    total_len = name_len + 1u + schema_len;
    combined = (char*)malloc(total_len + 1u);
    if (!combined) {
        leqvsg_set_error(ctx, QAMRPP_ERR_IO, "eqvsg_define_expr: out of memory");
        return 0;
    }

    memcpy(combined, name, name_len);
    combined[name_len] = ':';
    memcpy(combined + name_len + 1u, schema, schema_len);
    combined[total_len] = '\0';

    leqvsg_free_string(&leqvsg_expr_definition);
    leqvsg_expr_definition = combined;

    return qamrpp_make_nil(ctx);
}

static qamrpp_value* eqvsg_define_rewriters_native(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    (void)argv;
    if (argc != 1) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_define_rewriters expects 1 argument");
        return 0;
    }

    leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
        "eqvsg_define_rewriters(table) is not implemented; use eqvsg_load_rewrite_rules(path)");
    return 0;
}

static qamrpp_value* eqvsg_load_expr_definition_native(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    const char* path;
    char* loaded = 0;
    size_t loaded_len = 0;

    if (argc != 1) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_load_expr_definition expects 1 argument: path");
        return 0;
    }

    path = qamrpp_value_to_string(argv[0], 0);
    if (!path) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_load_expr_definition expects a string path");
        return 0;
    }

    if (!leqvsg_read_file(path, &loaded, &loaded_len)) {
        leqvsg_set_error(ctx, QAMRPP_ERR_IO,
            "eqvsg_load_expr_definition failed to read file");
        return 0;
    }

    (void)loaded_len;
    leqvsg_free_string(&leqvsg_expr_definition);
    leqvsg_expr_definition = loaded;

    return qamrpp_make_nil(ctx);
}

static qamrpp_value* eqvsg_load_rewrite_rules_native(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    const char* path;
    char* loaded = 0;

    if (argc != 1) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_load_rewrite_rules expects 1 argument: path");
        return 0;
    }

    path = qamrpp_value_to_string(argv[0], 0);
    if (!path) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_load_rewrite_rules expects a string path");
        return 0;
    }

    if (!leqvsg_read_file(path, &loaded, 0)) {
        leqvsg_set_error(ctx, QAMRPP_ERR_IO,
            "eqvsg_load_rewrite_rules failed to read file");
        return 0;
    }

    if (!leqvsg_parse_rules(loaded)) {
        free(loaded);
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_load_rewrite_rules parse error: expected 'lhs => rhs' lines");
        return 0;
    }

    leqvsg_free_string(&leqvsg_rules_source);
    leqvsg_rules_source = loaded;

    return qamrpp_make_nil(ctx);
}

static qamrpp_value* eqvsg_rewrite_native(qamrpp_context* ctx, qamrpp_value** argv, size_t argc) {
    size_t input_len = 0;
    const char* input = 0;
    char* current;
    size_t pass;

    if (argc != 1) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_rewrite expects 1 argument: input_sexpr_string");
        return 0;
    }

    input = qamrpp_value_to_string(argv[0], &input_len);
    if (!input) {
        leqvsg_set_error(ctx, QAMRPP_ERR_BAD_ARGUMENT,
            "eqvsg_rewrite expects a string input");
        return 0;
    }

    current = leqvsg_strdup_slice(input, input_len);
    if (!current) {
        leqvsg_set_error(ctx, QAMRPP_ERR_IO, "eqvsg_rewrite: out of memory");
        return 0;
    }

    for (pass = 0; pass < LEQVSG_MAX_REWRITE_PASSES; ++pass) {
        size_t i;
        int changed_any = 0;

        for (i = 0; i < leqvsg_rule_count; ++i) {
            int changed = 0;
            char* next = leqvsg_replace_once(current, &leqvsg_rules[i], &changed);
            if (!next) {
                free(current);
                leqvsg_set_error(ctx, QAMRPP_ERR_IO, "eqvsg_rewrite: out of memory");
                return 0;
            }
            free(current);
            current = next;
            if (changed) {
                changed_any = 1;
                break;
            }
        }

        if (!changed_any) {
            qamrpp_value* out = qamrpp_make_string(ctx, current, strlen(current));
            free(current);
            return out;
        }
    }

    free(current);
    leqvsg_set_error(ctx, QAMRPP_ERR_IO,
        "eqvsg_rewrite exceeded rewrite pass limit");
    return 0;
}

static const qamrpp_native_binding leqvsg_functions[] = {
    { "eqvsg_define_expr",          eqvsg_define_expr_native },
    { "eqvsg_define_rewriters",     eqvsg_define_rewriters_native },
    { "eqvsg_load_expr_definition", eqvsg_load_expr_definition_native },
    { "eqvsg_load_rewrite_rules",   eqvsg_load_rewrite_rules_native },
    { "eqvsg_rewrite",              eqvsg_rewrite_native }
};

static int leqvsg_on_load(qamrpp_context* ctx, const qamrpp_host_api* host_api) {
    leqvsg_trace_enabled = 0;

    if (host_api && host_api->get_global) {
        qamrpp_value* trace = host_api->get_global(ctx, "debug.trace_leqvsg");
        if (trace && qamrpp_value_type_of(trace) == QAMRPP_TYPE_BOOL) {
            leqvsg_trace_enabled = qamrpp_value_to_bool(trace);
        }
    }

    if (leqvsg_trace_enabled) {
        fprintf(stderr, "[leqvsg] loaded\n");
    }

    return 0;
}

static void leqvsg_on_unload(qamrpp_context* ctx) {
    (void)ctx;
    if (leqvsg_trace_enabled) {
        fprintf(stderr, "[leqvsg] unloading\n");
    }

    leqvsg_free_string(&leqvsg_expr_definition);
    leqvsg_free_string(&leqvsg_rules_source);
    leqvsg_clear_rules();
}

static const qamrpp_library_descriptor leqvsg_descriptor = {
    QAMRPP_LIBRARY_API_VERSION,
    "leqvsg",
    leqvsg_functions,
    QAMRPP_ARRAY_COUNT(leqvsg_functions),
    leqvsg_on_load,
    leqvsg_on_unload
};

QAMRPP_LIBRARY_EXPORT_DESCRIPTOR(leqvsg_descriptor)
