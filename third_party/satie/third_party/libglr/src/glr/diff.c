#include <glr/diff.h>
#include <stdlib.h>
#include <string.h>

/* Find longest common prefix of two strings */
size_t glr_find_common_prefix(const char* a, const char* b, 
                               size_t len_a, size_t len_b) {
    size_t max_len = len_a < len_b ? len_a : len_b;
    size_t i = 0;
    while (i < max_len && a[i] == b[i]) {
        i++;
    }
    return i;
}

/* Find longest common suffix */
size_t glr_find_common_suffix(const char* a, const char* b,
                               size_t len_a, size_t len_b,
                               size_t prefix_len) {
    if (len_a <= prefix_len || len_b <= prefix_len) {
        return 0;
    }
    
    size_t i = 0;
    size_t max_i = (len_a - prefix_len) < (len_b - prefix_len) 
                   ? (len_a - prefix_len) : (len_b - prefix_len);
    
    while (i < max_i && a[len_a - 1 - i] == b[len_b - 1 - i]) {
        i++;
    }
    return i;
}

int glr_compute_edit(const char* old_content, size_t old_len,
                     const char* new_content, size_t new_len,
                     glr_edit_t* edit) {
    if (!old_content || !new_content || !edit) {
        return -1;
    }
    
    /* Find common prefix */
    size_t prefix_len = glr_find_common_prefix(old_content, new_content, 
                                                old_len, new_len);
    
    /* Find common suffix */
    size_t suffix_len = glr_find_common_suffix(old_content, new_content,
                                                old_len, new_len, prefix_len);
    
    /* Compute edit region */
    edit->old_start = prefix_len;
    edit->old_end = old_len - suffix_len;
    edit->new_start = prefix_len;
    edit->new_end = new_len - suffix_len;
    
    /* Determine edit type */
    size_t old_edit_len = edit->old_end - edit->old_start;
    size_t new_edit_len = edit->new_end - edit->new_start;
    
    if (old_edit_len == 0 && new_edit_len > 0) {
        edit->is_insertion = true;
        edit->is_deletion = false;
        edit->is_replacement = false;
    } else if (old_edit_len > 0 && new_edit_len == 0) {
        edit->is_insertion = false;
        edit->is_deletion = true;
        edit->is_replacement = false;
    } else if (old_edit_len > 0 && new_edit_len > 0) {
        edit->is_insertion = false;
        edit->is_deletion = false;
        edit->is_replacement = true;
    } else {
        /* No change */
        edit->is_insertion = false;
        edit->is_deletion = false;
        edit->is_replacement = false;
    }
    
    return 0;
}

bool glr_edit_is_empty(const glr_edit_t* edit) {
    if (!edit) return true;
    return !edit->is_insertion && !edit->is_deletion && !edit->is_replacement;
}

size_t glr_edit_old_length(const glr_edit_t* edit) {
    if (!edit) return 0;
    return edit->old_end - edit->old_start;
}

size_t glr_edit_new_length(const glr_edit_t* edit) {
    if (!edit) return 0;
    return edit->new_end - edit->new_start;
}
