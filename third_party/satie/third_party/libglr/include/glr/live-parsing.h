#ifndef GLR_LIVE_PARSING_H
#define GLR_LIVE_PARSING_H

#include <glr/forest.h>
#include <glr/grammar.h>
#include <glr/parser.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file live-parsing.h
 * @brief Editor-facing incremental parsing: report an edit, update the tree.
 *
 * A live parser owns the text, the grammar, the parse forest, and the
 * metadata that ties them together. The intended flow is four calls:
 *
 * 1. @ref glr_live_parser_create parses the initial text and caches the tree.
 * 2. @ref glr_live_parser_edit reports an edit as a line/column range. The
 *    text is spliced, the branches that the edit can reach are marked, and
 *    the affected region is remembered. Nothing is re-parsed yet, so an
 *    application can batch several keystrokes into one update.
 * 3. @ref glr_live_parser_update re-parses exactly the marked region and
 *    rebuilds the tree, reusing the parts of the old tree that the edit
 *    cannot reach.
 * 4. @ref glr_live_parser_forest hands the current tree to the caller.
 *
 * Positions are given as 1-based line numbers and 1-based columns counted in
 * bytes from the start of the line, which is what editors report. The
 * conversion to byte offsets is the session's job, so callers never have to
 * keep a second copy of the line index.
 *
 * Marking walks the cached tree twice: down to find the nodes whose span
 * overlaps the edit, and up from each of those to collect every ancestor that
 * has to be reconsidered. The two passes produce the smallest set of branches
 * whose re-parse can change the answer, which is what @ref
 * glr_live_parser_dirty_count and @ref glr_live_parser_dirty_node report.
 *
 * @see glr_live_parser_stats_t
 * @see cache.h for the optional persistent cache
 */

/**
 * @brief An opaque live parsing session.
 *
 * Owns the text buffer, the parse forest, and the edit metadata. Create with
 * @ref glr_live_parser_create and release with @ref glr_live_parser_destroy.
 */
typedef struct glr_live_parser_t glr_live_parser_t;

/**
 * @brief A text edit reported by the application.
 *
 * The edited region runs from (@ref start_line, @ref start_column) up to but not
 * including (@ref end_line, @ref end_column), and @ref replacement takes its
 * place. That makes the three cases distinguishable: a pure insertion has the
 * two positions equal with a non-empty @ref replacement, a pure deletion has
 * an empty @ref replacement with distinct positions, and a replacement has
 * both. Line and column numbers are 1-based and columns count bytes from the
 * start of the line.
 */
typedef struct
{
    uint32_t start_line;      /**< 1-based line where the edit starts. */
    uint32_t start_column;    /**< 1-based byte column where the edit starts. */
    uint32_t end_line;        /**< 1-based line where the edited region ends. */
    uint32_t end_column;      /**< 1-based byte column just past the last edited byte. */
    const char *replacement;  /**< Text inserted in place of the region; may be NULL. */
    size_t replacement_length; /**< Length of @ref replacement in bytes. */
} glr_live_edit_t;

/**
 * @brief Counters describing what the session has done so far.
 *
 * The interesting pair is @c bytes_reparsed and @c bytes_reused: together
 * they show how much of the text the last update actually had to look at.
 */
typedef struct
{
    uint64_t edit_count;      /**< Number of edits reported so far. */
    uint64_t update_count;    /**< Number of completed updates. */
    uint64_t bytes_reparsed;  /**< Bytes re-parsed by the last update. */
    uint64_t bytes_reused;    /**< Bytes served from the cached tree by the last update. */
    size_t dirty_node_count;  /**< Nodes marked for edit after the last reported edit. */
    size_t total_nodes;       /**< Nodes in the current forest. */
    size_t input_length;      /**< Current text length in bytes. */
    bool last_parse_succeeded;/**< Whether the last update produced a complete parse. */
} glr_live_parser_stats_t;

/**
 * @brief Create a live parsing session and parse the initial text.
 *
 * The grammar is not owned by the session and must outlive it. The text is
 * copied, so the caller may free or modify its buffer immediately.
 *
 * @param grammar Grammar to parse with (required).
 * @param text Initial text (may be NULL only when @p length is 0).
 * @param length Length of @p text in bytes.
 * @param error Optional error buffer; receives a message on failure.
 * @param error_size Size of @p error in bytes.
 * @return New session, or NULL on invalid input, parse failure, or OOM.
 *
 * @note A parse failure at creation is reported as NULL: there is no tree to
 *       update yet. Once a session exists, a later failure leaves the previous
 *       tree in place and is reported through
 *       @ref glr_live_parser_stats_t::last_parse_succeeded.
 */
glr_live_parser_t *glr_live_parser_create (glr_grammar_t *grammar,
                                           const char *text, size_t length,
                                           char *error, size_t error_size);

/**
 * @brief Destroy a live parsing session and everything it owns.
 * @param live Session to destroy (NULL is a no-op).
 */
void glr_live_parser_destroy (glr_live_parser_t *live);

/**
 * @brief Replace the whole text and re-parse it.
 *
 * The cache is refreshed and every branch is marked, so a subsequent
 * @ref glr_live_parser_update is a no-op until the next edit.
 *
 * @param live Session (required).
 * @param text New text (may be NULL when @p length is 0).
 * @param length Length of @p text in bytes.
 * @param error Optional error buffer.
 * @param error_size Size of @p error in bytes.
 * @return 0 on success, -1 on invalid input or allocation failure.
 */
int glr_live_parser_set_text (glr_live_parser_t *live, const char *text,
                              size_t length, char *error, size_t error_size);

/**
 * @brief Report a text edit and mark the branches it can reach.
 *
 * The text is spliced immediately, so @ref glr_live_parser_text reports the
 * new content right away, while the tree still describes the old content.
 * That gap is the point: the application can report several edits and then
 * call @ref glr_live_parser_update once.
 *
 * Positions outside the text are rejected rather than clamped, because a
 * silently clamped edit would produce a tree that does not match the text.
 *
 * @param live Session (required).
 * @param edit Edit description (required).
 * @param error Optional error buffer.
 * @param error_size Size of @p error in bytes.
 * @return 0 on success, -1 on invalid input or allocation failure.
 *
 * @see glr_live_parser_update
 */
int glr_live_parser_edit (glr_live_parser_t *live, const glr_live_edit_t *edit,
                          char *error, size_t error_size);

/**
 * @brief Whether edits have been reported that no update has consumed.
 * @param live Session (may be NULL).
 * @return true when @ref glr_live_parser_update has work to do.
 */
bool glr_live_parser_has_pending_edits (const glr_live_parser_t *live);

/**
 * @brief Re-parse the marked region and update the tree.
 *
 * The update re-parses the smallest text region that can contain the edit
 * together with its surrounding nodes, and keeps the untouched parts of the
 * previous tree. The forest handed out afterwards is a new, self-contained
 * tree; any pointer returned by an earlier
 * @ref glr_live_parser_forest call is invalid.
 *
 * @param live Session (required).
 * @param error Optional error buffer.
 * @param error_size Size of @p error in bytes.
 * @return 0 on success, -1 on invalid input or allocation failure.
 *
 * @note When the edit makes the input unparseable, the previous tree is
 *       dropped and the session reports failure through
 *       @ref glr_live_parser_stats_t::last_parse_succeeded and through
 *       @ref glr_live_parser_error.
 */
int glr_live_parser_update (glr_live_parser_t *live, char *error,
                            size_t error_size);

/**
 * @brief Get the current parse forest.
 * @param live Session (may be NULL).
 * @return Borrowed forest, or NULL when the last parse failed.
 */
const glr_forest_t *glr_live_parser_forest (const glr_live_parser_t *live);

/**
 * @brief Get the current text.
 * @param live Session (may be NULL).
 * @param length Receives the length in bytes; may be NULL.
 * @return Borrowed text owned by the session, or NULL for a NULL session.
 */
const char *glr_live_parser_text (const glr_live_parser_t *live,
                                  size_t *length);

/**
 * @brief Get the error of the last update.
 * @param live Session (may be NULL).
 * @return Static error string, or NULL when the last update succeeded.
 */
const char *glr_live_parser_error (const glr_live_parser_t *live);

/**
 * @brief Get counters for the session.
 * @param live Session (may be NULL).
 * @param stats Receives the counters (required).
 * @return 0 on success, -1 on invalid input.
 */
int glr_live_parser_get_stats (const glr_live_parser_t *live,
                               glr_live_parser_stats_t *stats);

/**
 * @brief Number of nodes currently marked for edit.
 * @param live Session (may be NULL).
 * @return Marked node count.
 */
size_t glr_live_parser_dirty_count (const glr_live_parser_t *live);

/**
 * @brief Copy the marked node at an index.
 *
 * Marked nodes are stored in the order the marking pass found them: the
 * nodes overlapping the edit first, then their ancestors, so the list doubles
 * as an explanation of what the update intends to redo.
 *
 * @param live Session (may be NULL).
 * @param index Index in the marked list.
 * @param out_span Receives the node's start and end offsets; may be NULL.
 * @return Borrowed node, or NULL when @p index is out of range.
 */
const glr_forest_node_t *glr_live_parser_dirty_node (
    const glr_live_parser_t *live, size_t index, size_t *out_start,
    size_t *out_end);

/**
 * @brief Get the byte range the session last edited.
 *
 * This is the region the next @ref glr_live_parser_update will consider, in
 * the coordinates of the current text. It stays available after the update so
 * an application can highlight what it last touched.
 *
 * @param live Session (may be NULL).
 * @param start Receives the start offset; may be NULL.
 * @param end Receives the end offset (exclusive); may be NULL.
 * @return true when an edit has been reported, false otherwise.
 */
bool glr_live_parser_last_edit_range (const glr_live_parser_t *live,
                                      size_t *start, size_t *end);

/**
 * @brief Convert a line/column pair to a byte offset in a buffer.
 *
 * Exposed because applications that keep their own position bookkeeping need
 * the same conversion the session uses.
 *
 * @param text Text to measure (may be NULL when @p length is 0).
 * @param length Length of @p text in bytes.
 * @param line 1-based line number.
 * @param column 1-based byte column within the line.
 * @param out_offset Receives the byte offset.
 * @return 0 on success, -1 when the position is out of range.
 */
int glr_live_offset_of (const char *text, size_t length, uint32_t line,
                        uint32_t column, size_t *out_offset);

/**
 * @brief Convert a byte offset to a line/column pair.
 *
 * @param text Text to measure (may be NULL when @p length is 0).
 * @param length Length of @p text in bytes.
 * @param offset Byte offset; must be within the text.
 * @param out_line Receives the 1-based line number.
 * @param out_column Receives the 1-based byte column.
 * @return 0 on success, -1 when the offset is out of range.
 */
int glr_live_line_column_of (const char *text, size_t length, size_t offset,
                             uint32_t *out_line, uint32_t *out_column);

#ifdef __cplusplus
}
#endif

#endif /* GLR_LIVE_PARSING_H */
