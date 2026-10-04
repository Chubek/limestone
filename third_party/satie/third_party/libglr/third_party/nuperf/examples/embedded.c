#include <nuperf/nuperf-api.h>

#include <stdio.h>

int main(void) {
    nuperf_status_t st = nuperf_init();
    if (st != NUPERF_OK) {
        fprintf(stderr, "init failed: %s\n", nuperf_strerror(st));
        return 1;
    }

    nuperf_keyset_t *ks = NULL;
    nuperf_table_t *tbl = NULL;

    st = nuperf_keyset_create(&ks);
    if (st == NUPERF_OK) st = nuperf_table_create(&tbl);
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(ks, "alpha");
    if (st == NUPERF_OK) st = nuperf_keyset_add_string(ks, "beta");
    if (st == NUPERF_OK) st = nuperf_table_set_keyset(tbl, ks);
    if (st == NUPERF_OK) st = nuperf_table_build(tbl);
    if (st == NUPERF_OK) st = nuperf_table_emit_file(tbl, "embedded_output.h");

    if (st != NUPERF_OK) {
        fprintf(stderr, "error: %s\n", nuperf_strerror(st));
    }

    nuperf_table_destroy(tbl);
    nuperf_keyset_destroy(ks);
    nuperf_shutdown();
    return st == NUPERF_OK ? 0 : 1;
}
