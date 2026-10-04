#define ITK_DEF extern
#define ETK_VERSION_IMPLEMENTATION
#define ETK_REGISTRY_IMPLEMENTATION
#include "ExtensionTk/etk_registry.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static int events[256];
static size_t event_count;
static etk_status activate(void *ctx)
{
    int id = *(int *)ctx;
    events[event_count++] = id;
    return id == 3 ? ETK_EVERSION : ETK_OK;
}
static void deactivate(void *ctx) { events[event_count++] = -*(int *)ctx; }
static int versions(void)
{
    const char *invalid[] = {"", "1", "1.2", "01.2.3", "1.02.3", "1.2.03",
        "-1.2.3", "1.2.3junk", "1.2.3-", "1.2.3-01", "1.2.3-a..b",
        "1.2.3+", "1.2.3+a..b", "1.2.3 ", " 1.2.3", "1.2.3+a+b"};
    const char *ordered[] = {"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta",
        "1.0.0-beta", "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1", "1.0.0"};
    etk_version v, w;
    char text[128];
    size_t i;
    CHECK(etk_version_parse("1.2.3", &v) == ETK_OK);
    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        CHECK(etk_version_parse(invalid[i], &v) == ETK_EVERSION);
        CHECK(v.major == 1 && v.minor == 2 && v.patch == 3);
    }
    snprintf(text, sizeof(text), "%u0.0.0", UINT_MAX);
    CHECK(etk_version_parse(text, &v) == ETK_EVERSION);
    for (i = 1; i < sizeof(ordered)/sizeof(ordered[0]); ++i) {
        CHECK(etk_version_parse(ordered[i-1], &v) == ETK_OK);
        CHECK(etk_version_parse(ordered[i], &w) == ETK_OK);
        CHECK(etk_version_compare(&v, &w) < 0);
        CHECK(etk_version_compare(&w, &v) > 0);
    }
    CHECK(etk_version_parse("1.2.3-rc.2+build.01", &v) == ETK_OK);
    CHECK(etk_version_fmt(&v, text, sizeof(text)) == ETK_OK);
    CHECK(strcmp(text, "1.2.3-rc.2") == 0);
    CHECK(etk_version_fmt(&v, text, 2) == ETK_EFAIL && text[1] == '\0');
    CHECK(!etk_version_satisfies(&v, "*"));
    CHECK(etk_version_satisfies(&v, ">=1.2.3-rc.1 <2.0.0"));
    CHECK(etk_version_parse("1.2.3", &v) == ETK_OK);
    CHECK(etk_version_satisfies(&v, "^1.2.0"));
    CHECK(etk_version_satisfies(&v, ">= 1.2.0 <2.0.0"));
    CHECK(etk_version_satisfies(&v, "1.x"));
    CHECK(etk_version_satisfies(&v, "1.2"));
    CHECK(etk_version_satisfies(&v, "~1.2.0"));
    CHECK(etk_version_satisfies(&v, "=1.2.3"));
    CHECK(!etk_version_satisfies(&v, "1.2.2"));
    CHECK(!etk_version_satisfies(&v, "1.2.3 garbage"));
    CHECK(!etk_version_satisfies(&v, ""));
    CHECK(!etk_version_satisfies(&v, "1.x.2"));
    CHECK(!etk_version_satisfies(&v, ">=1.2.3 || <2.0.0"));
    CHECK(etk_version_parse("0.2.4", &v) == ETK_OK);
    CHECK(etk_version_satisfies(&v, "^0.2.3"));
    CHECK(!etk_version_satisfies(&v, "^0.0.3"));
    CHECK(etk_version_parse("0.3.0", &v) == ETK_OK);
    CHECK(!etk_version_satisfies(&v, "^0.2.3"));
    return 0;
}
static int registry_test(void)
{
    etk_registry r;
    int ids[] = {1,2,3};
    const char *deps[] = {"base"};
    const char *cycle[] = {"child"};
    etk_extension child = {"child", {1,0,0,NULL,0}, deps, 1, activate, deactivate, &ids[1], 0};
    etk_extension base = {"base", {1,0,0,NULL,0}, NULL, 0, activate, deactivate, &ids[0], 0};
    etk_extension fail = {"fail", {1,0,0,NULL,0}, cycle, 1, activate, deactivate, &ids[2], 0};
    etk_registry_init(&r);
    CHECK(etk_registry_register(&r, &child) == ETK_OK);
    CHECK(etk_registry_activate(&r) == ETK_ENOTFOUND && event_count == 0);
    CHECK(etk_registry_register(&r, &base) == ETK_OK);
    CHECK(etk_registry_count(&r) == 2);
    CHECK(etk_registry_register(&r, &base) == ETK_EFAIL);
    CHECK(etk_registry_activate(&r) == ETK_OK);
    CHECK(events[0] == 1 && events[1] == 2);
    CHECK(etk_registry_activate(&r) == ETK_OK && event_count == 2);
    CHECK(etk_registry_register(&r, &fail) == ETK_OK);
    CHECK(etk_registry_activate(&r) == ETK_EVERSION && r.active_count == 2);
    etk_registry_deactivate_all(&r);
    CHECK(events[3] == -2 && events[4] == -1);
    etk_registry_deactivate_all(&r);
    CHECK(event_count == 5);
    event_count = 0;
    CHECK(etk_registry_activate(&r) == ETK_EVERSION && r.active_count == 0);
    CHECK(event_count == 5 && events[0] == 1 && events[1] == 2 &&
          events[2] == 3 && events[3] == -2 && events[4] == -1);
    CHECK(!r.entries[0].active && !r.entries[1].active);
    etk_registry_init(&r);
    base.dependencies = cycle; base.dependency_count = 1;
    CHECK(etk_registry_register(&r, &child) == ETK_OK);
    CHECK(etk_registry_register(&r, &base) == ETK_OK);
    event_count = 0;
    CHECK(etk_registry_activate(&r) == ETK_EFAIL && event_count == 0);
    return 0;
}
int main(void) { return versions() || registry_test(); }
