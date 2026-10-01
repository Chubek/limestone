#ifndef ETK_VERSION_H
#define ETK_VERSION_H
/**
 * @file etk_version.h
 * @brief Semantic versioning (MAJOR.MINOR.PATCH[-prerelease]) parsing, comparison,
 * and constraint checking ("^1.2.0", ">=2.0.0 <3.0.0", "1.x") so hosts can
 * gate extension compatibility without pulling in a dependency.
 * @stability stable
 * @depends ExtensionTk::platform, ExtensionTk::types
 */
#include "etk_platform.h"
#include "etk_types.h"

/**
 * @brief Semantic version with a borrowed prerelease slice.
 * @var etk_version::major Major component, bounded by UINT_MAX.
 * @var etk_version::minor Minor component, bounded by UINT_MAX.
 * @var etk_version::patch Patch component, bounded by UINT_MAX.
 * @var etk_version::prerelease Borrowed identifier text, or NULL for a release.
 * @var etk_version::prerelease_len Identifier byte count, excluding '-'.
 * @note Keep the source string alive while using this value. Build metadata
 * is validated by the parser but discarded; it does not affect precedence.
 */
typedef struct etk_version {
    unsigned major, minor, patch;
    const char *prerelease;
    size_t prerelease_len;
} etk_version;

/**
 * @brief Parse a complete semantic version without allocation.
 * @param text NUL-terminated MAJOR.MINOR.PATCH[-prerelease][+build] string.
 * @param out Destination; unchanged on failure.
 * @return ETK_OK, ETK_EINVAL for NULL arguments, or ETK_EVERSION for invalid
 * syntax, leading zeros, or numeric overflow.
 * @note No whitespace is accepted. Independent outputs are thread safe.
 */
ETK_DEF etk_status etk_version_parse(const char *text, etk_version *out);
/**
 * @brief Compare version precedence, including numeric prerelease identifiers.
 * @param a First parsed version.
 * @param b Second parsed version.
 * @return -1, 0, or 1; NULL arguments compare equal for legacy compatibility.
 * @note Read-only and thread safe while the borrowed text remains immutable.
 */
ETK_DEF int etk_version_compare(const etk_version *a, const etk_version *b);
/**
 * @brief Format a version, preserving prerelease but omitting build metadata.
 * @param v Valid version descriptor.
 * @param buf Output buffer, disjoint from borrowed prerelease text.
 * @param cap Buffer capacity including the NUL terminator.
 * @return ETK_OK, ETK_EINVAL for invalid arguments, ETK_EVERSION for invalid
 * prerelease text, or ETK_EFAIL for insufficient space (buffer terminated).
 * @note No allocation. Independent buffers are thread safe.
 */
ETK_DEF etk_status etk_version_fmt(const etk_version *v, char *buf, size_t cap);
/**
 * @brief Check a whitespace-separated conjunction of version constraints.
 * @param v Parsed version to test.
 * @param constraint NUL-terminated expression: exact versions, =, <, <=, >,
 * >=, ^, ~, or trailing x/X/star wildcards (including bare major/minor ranges).
 * @return ETK_TRUE only when every comparator matches; invalid or empty
 * expressions return ETK_FALSE. OR, hyphen ranges, and commas are unsupported.
 * @note Prereleases require an explicit prerelease comparator with the same
 * major/minor/patch. Caret fixes the first nonzero component; tilde fixes
 * major/minor. Operator operands must be complete versions. No allocation;
 * read-only and thread safe with immutable input.
 */
ETK_DEF etk_bool etk_version_satisfies(const etk_version *v, const char *constraint);

#ifdef ETK_VERSION_IMPLEMENTATION
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int _etk_version_digit(char c) { return c >= '0' && c <= '9'; }
static int _etk_version_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\f' || c == '\v';
}
static int _etk_version_number(const char **cursor, const char *end, unsigned *out)
{
    const char *p = *cursor, *start = p;
    unsigned value = 0;
    if (p == end || !_etk_version_digit(*p)) return 0;
    while (p < end && _etk_version_digit(*p)) {
        unsigned digit = (unsigned)(*p++ - '0');
        if (value > (UINT_MAX - digit) / 10u) return 0;
        value = value * 10u + digit;
    }
    if (p - start > 1 && *start == '0') return 0;
    *out = value;
    *cursor = p;
    return 1;
}
static int _etk_version_identifiers(const char *p, const char *end, int prerelease)
{
    if (p == end) return 0;
    while (p < end) {
        const char *start = p;
        int numeric = 1;
        while (p < end && *p != '.') {
            char c = *p++;
            if (!_etk_version_digit(c)) {
                numeric = 0;
                if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c != '-')
                    return 0;
            }
        }
        if (p == start || (prerelease && numeric && p - start > 1 && *start == '0'))
            return 0;
        if (p < end && ++p == end) return 0;
    }
    return 1;
}
static int _etk_version_parse(const char *p, const char *end, etk_version *out)
{
    etk_version result;
    if (!_etk_version_number(&p, end, &result.major) || p == end || *p++ != '.' ||
        !_etk_version_number(&p, end, &result.minor) || p == end || *p++ != '.' ||
        !_etk_version_number(&p, end, &result.patch)) return 0;
    result.prerelease = NULL;
    result.prerelease_len = 0;
    if (p < end && *p == '-') {
        const char *start = ++p;
        while (p < end && *p != '+') ++p;
        if (!_etk_version_identifiers(start, p, 1)) return 0;
        result.prerelease = start;
        result.prerelease_len = (size_t)(p - start);
    }
    if (p < end && *p == '+') {
        if (!_etk_version_identifiers(++p, end, 0)) return 0;
        p = end;
    }
    if (p != end) return 0;
    *out = result;
    return 1;
}
ETK_DEF etk_status etk_version_parse(const char *text, etk_version *out)
{
    if (text == NULL || out == NULL) return ETK_EINVAL;
    return _etk_version_parse(text, text + strlen(text), out) ? ETK_OK : ETK_EVERSION;
}
ETK_DEF int etk_version_compare(const etk_version *a, const etk_version *b)
{
    size_t ai = 0, bi = 0;
    if (a == NULL || b == NULL) return 0;
    if (a->major != b->major) return a->major > b->major ? 1 : -1;
    if (a->minor != b->minor) return a->minor > b->minor ? 1 : -1;
    if (a->patch != b->patch) return a->patch > b->patch ? 1 : -1;
    if (!a->prerelease_len || !b->prerelease_len)
        return a->prerelease_len ? -1 : b->prerelease_len ? 1 : 0;
    while (ai < a->prerelease_len && bi < b->prerelease_len) {
        size_t as = ai, bs = bi, an, bn, n;
        int ad = 1, bd = 1, cmp;
        while (ai < a->prerelease_len && a->prerelease[ai] != '.')
            if (!_etk_version_digit(a->prerelease[ai++])) ad = 0;
        while (bi < b->prerelease_len && b->prerelease[bi] != '.')
            if (!_etk_version_digit(b->prerelease[bi++])) bd = 0;
        an = ai - as; bn = bi - bs;
        if (ad != bd) return ad ? -1 : 1;
        if (ad && an != bn) return an > bn ? 1 : -1;
        n = an < bn ? an : bn;
        cmp = memcmp(a->prerelease + as, b->prerelease + bs, n);
        if (cmp) return cmp > 0 ? 1 : -1;
        if (an != bn) return an > bn ? 1 : -1;
        if (ai == a->prerelease_len || bi == b->prerelease_len)
            return ai < a->prerelease_len ? 1 : bi < b->prerelease_len ? -1 : 0;
        ++ai; ++bi;
    }
    return 0;
}
ETK_DEF etk_status etk_version_fmt(const etk_version *v, char *buf, size_t cap)
{
    int n;
    size_t used;
    if (v == NULL || buf == NULL || cap == 0) return ETK_EINVAL;
    buf[0] = '\0';
    if (v->prerelease_len && (v->prerelease == NULL ||
        !_etk_version_identifiers(v->prerelease, v->prerelease + v->prerelease_len, 1)))
        return ETK_EVERSION;
    n = snprintf(buf, cap, "%u.%u.%u", v->major, v->minor, v->patch);
    if (n < 0 || (size_t)n >= cap) return ETK_EFAIL;
    used = (size_t)n;
    if (v->prerelease_len) {
        if (cap - used < 2 || v->prerelease_len > cap - used - 2) return ETK_EFAIL;
        buf[used++] = '-';
        memcpy(buf + used, v->prerelease, v->prerelease_len);
        buf[used + v->prerelease_len] = '\0';
    }
    return ETK_OK;
}

/* A bare partial version or trailing wildcard denotes a prefix range. */
static int _etk_version_prefix(const char *p, const char *end,
                               const etk_version *v, int *match)
{
    unsigned parts[3] = {0, 0, 0};
    size_t count = 0, fixed = 3;
    while (p < end && count < 3) {
        if (*p == '*' || *p == 'x' || *p == 'X') {
            if (fixed == 3) fixed = count;
            ++p;
        } else {
            if (fixed != 3 || !_etk_version_number(&p, end, &parts[count])) return 0;
        }
        ++count;
        if (p == end) break;
        if (*p++ != '.' || p == end) return 0;
    }
    if (!count || p != end) return 0;
    if (fixed == 3) fixed = count;
    *match = (fixed < 1 || parts[0] == v->major) &&
             (fixed < 2 || parts[1] == v->minor) &&
             (fixed < 3 || parts[2] == v->patch);
    return 1;
}
ETK_DEF etk_bool etk_version_satisfies(const etk_version *v, const char *constraint)
{
    const char *p = constraint;
    int seen = 0, all = 1, prerelease_allowed = 0;
    if (v == NULL || p == NULL) return ETK_FALSE;
    while (*p) {
        char op = 0;
        int equal = 0, cmp, match = 0;
        const char *start, *end;
        etk_version want;
        while (_etk_version_space(*p)) ++p;
        if (!*p) break;
        if (*p == '<' || *p == '>' || *p == '=' || *p == '^' || *p == '~') {
            op = *p++;
            if ((op == '<' || op == '>') && *p == '=') { equal = 1; ++p; }
            while (_etk_version_space(*p)) ++p;
        }
        start = p;
        while (*p && !_etk_version_space(*p)) ++p;
        end = p;
        if (start == end) return ETK_FALSE;
        if (!_etk_version_parse(start, end, &want)) {
            if (op || !_etk_version_prefix(start, end, v, &match)) return ETK_FALSE;
        } else {
            cmp = etk_version_compare(v, &want);
            if (want.prerelease_len && v->major == want.major &&
                v->minor == want.minor && v->patch == want.patch) prerelease_allowed = 1;
            switch (op) {
            case '<': match = cmp < 0 || (equal && cmp == 0); break;
            case '>': match = cmp > 0 || (equal && cmp == 0); break;
            case '^':
                match = cmp >= 0 && v->major == want.major &&
                    (want.major || (v->minor == want.minor &&
                     (want.minor || v->patch == want.patch)));
                break;
            case '~': match = cmp >= 0 && v->major == want.major && v->minor == want.minor; break;
            default: match = cmp == 0; break;
            }
        }
        seen = 1;
        if (!match) all = 0;
    }
    return (etk_bool)(seen && all && (!v->prerelease_len || prerelease_allowed));
}
#endif /* ETK_VERSION_IMPLEMENTATION */
#endif /* ETK_VERSION_H */
