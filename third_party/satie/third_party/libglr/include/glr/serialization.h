#ifndef GLR_SERIALIZATION_H
#define GLR_SERIALIZATION_H

#include <glr/forest.h>
#include <glr/grammar.h>
#include <glr/stack.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file serialization.h
 * @brief Binary serialization for parse forests and graph-structured stack nodes.
 *
 * The cache layer stores parser artifacts as compact byte streams. This header
 * defines the stable wire headers and conversion helpers used by @ref cache.h
 * and incremental parsing code. Serialized data is allocated by the serializer;
 * callers free returned buffers with the C library allocator used by libglr.
 *
 * @see cache.h
 * @see forest.h
 * @see stack.h
 */

/** @def GLR_SERIAL_TAG_FOREST
 *  @brief Four-byte magic tag identifying a serialized forest payload ("GLRF"). */
#define GLR_SERIAL_TAG_FOREST       0x474C5246

/** @def GLR_SERIAL_TAG_GSS_NODE
 *  @brief Four-byte magic tag identifying a serialized GSS node payload ("GLSG"). */
#define GLR_SERIAL_TAG_GSS_NODE     0x474C5347

/** @def GLR_SERIAL_TAG_FOREST_NODE
 *  @brief Four-byte magic tag identifying a serialized single SPPF node payload ("GLND"). */
#define GLR_SERIAL_TAG_FOREST_NODE  0x474C4E44

/**
 * @struct glr_serialized_node_header_t
 * @brief Packed header placed before a serialized @ref glr_forest_node_t.
 */
typedef struct __attribute__((packed)) {
    uint32_t tag;             /**< Format tag for node records. */
    uint32_t size;            /**< Total record size including this header. */
    uint32_t node_type;       /**< Stored @ref glr_forest_node_type_t value. */
    int32_t  symbol_id;       /**< Grammar symbol represented by the node. */
    uint64_t position;        /**< Input position associated with the node. */
    uint32_t child_count;     /**< Number of child indices following the header. */
    uint32_t children_offset; /**< Byte offset to the child-index array. */
    uint32_t data_offset;     /**< Byte offset to optional node payload data. */
    uint32_t data_size;       /**< Size of optional node payload data in bytes. */
} glr_serialized_node_header_t;

/**
 * @struct glr_serialized_gss_node_t
 * @brief Packed record for one graph-structured stack node.
 */
typedef struct __attribute__((packed)) {
    uint32_t tag;           /**< @ref GLR_SERIAL_TAG_GSS_NODE. */
    uint32_t size;          /**< Total record size including this header. */
    uint32_t state_id;      /**< Parser automaton state stored in the stack node. */
    uint64_t position;      /**< Input byte position associated with the state. */
    uint32_t parent_count;  /**< Number of parent references in the record. */
    uint32_t parent_offset; /**< Byte offset to serialized parent indices. */
} glr_serialized_gss_node_t;

/**
 * @struct glr_serialized_forest_header_t
 * @brief Packed top-level header for a serialized parse forest.
 */
typedef struct __attribute__((packed)) {
    uint32_t tag;          /**< @ref GLR_SERIAL_TAG_FOREST. */
    uint32_t version;      /**< Serialization format version. */
    uint32_t size;         /**< Total payload size in bytes. */
    uint64_t node_count;   /**< Number of serialized node records. */
    uint64_t edge_count;   /**< Number of serialized edge records. */
    uint32_t nodes_offset; /**< Byte offset to serialized nodes. */
    uint32_t edges_offset; /**< Byte offset to serialized edges. */
} glr_serialized_forest_header_t;

/**
 * @brief Serialize a parse forest to binary format.
 * @param forest Forest to serialize.
 * @param out_data Output buffer allocated by the function.
 * @param out_len Output buffer length in bytes.
 * @return 0 on success, -1 on invalid input or serialization failure.
 */
int glr_serialize_forest(const glr_forest_t* forest, uint8_t** out_data, size_t* out_len);

/**
 * @brief Deserialize a parse forest from binary format.
 * @param data Input buffer produced by @ref glr_serialize_forest.
 * @param len Input buffer length in bytes.
 * @param out_forest Output forest owned by the caller on success.
 * @return 0 on success, -1 on malformed input or allocation failure.
 */
int glr_deserialize_forest(const uint8_t* data, size_t len, glr_forest_t** out_forest);

/**
 * @brief Serialize a graph-structured stack node.
 * @param node Stack node to serialize.
 * @param out_data Output buffer allocated by the function.
 * @param out_len Output buffer length in bytes.
 * @return 0 on success, -1 on invalid input or serialization failure.
 */
int glr_serialize_stack_node(const glr_stack_node_t* node, uint8_t** out_data, size_t* out_len);

/**
 * @brief Deserialize a graph-structured stack node.
 * @param data Input buffer produced by @ref glr_serialize_stack_node.
 * @param len Input buffer length in bytes.
 * @param out_node Output stack node owned by the caller on success.
 * @return 0 on success, -1 on malformed input or allocation failure.
 */
int glr_deserialize_stack_node(const uint8_t* data, size_t len, glr_stack_node_t** out_node);

/**
 * @brief Serialize a single SPPF node.
 * @param node Forest node to serialize.
 * @param out_data Output buffer allocated by the function.
 * @param out_len Output buffer length in bytes.
 * @return 0 on success, -1 on invalid input or serialization failure.
 */
int glr_serialize_forest_node(const glr_forest_node_t* node, uint8_t** out_data, size_t* out_len);

/**
 * @brief Deserialize a single SPPF node.
 * @param data Input buffer produced by @ref glr_serialize_forest_node.
 * @param len Input buffer length in bytes.
 * @param out_node Output node owned by the caller on success.
 * @return 0 on success, -1 on malformed input or allocation failure.
 */
int glr_deserialize_forest_node(const uint8_t* data, size_t len, glr_forest_node_t** out_node);

  /**
   * @brief Kind of event reported while streaming a parse forest as XML.
   *
   * The stream is a document: it opens once, reports one start or leaf event
   * per node reachable from the root in depth-first order, and closes once. A
   * terminal is reported as a single GLR_XML_EVENT_LEAF carrying its text, so
   * the leaf text needs no separate side channel.
   */
  typedef enum
  {
    GLR_XML_EVENT_DOCUMENT_START, /**< Emitted once before any node. */
    GLR_XML_EVENT_NODE_START,    /**< Opening tag of a non-terminal/constructor. */
    GLR_XML_EVENT_LEAF,          /**< Complete terminal, with @c text set. */
    GLR_XML_EVENT_NODE_END,      /**< Closing tag matching the open element. */
    GLR_XML_EVENT_DOCUMENT_END   /**< Emitted once after the last node. */
  } glr_xml_event_t;

  /**
   * @brief Payload delivered with each XML event.
   */
  typedef struct
  {
    const glr_forest_node_t *node; /**< Node the event refers to; NULL for document events. */
    const char *text;               /**< Terminal text; only set for GLR_XML_EVENT_LEAF. */
    size_t text_length;             /**< Length of @c text in bytes. */
    size_t depth;                   /**< Nesting depth; 0 at document events and the root. */
  } glr_xml_event_info_t;

  /**
   * @brief Sink invoked for every XML event.
   * @param event Kind of event.
   * @param info Event payload; never NULL.
   * @param user_data Caller context.
   */
  typedef void (*glr_xml_event_fn) (glr_xml_event_t event,
                                    const glr_xml_event_info_t *info,
                                    void *user_data);

  /**
   * @brief Stream a parse forest as an XML event sequence.
   *
   * Walks the forest rooted at @c forest->root in depth-first order and
   * reports one event per node, so a caller can render XML, count nodes, or
   * forward the stream over a pipe without the library materializing a
   * document. Shared nodes are visited once.
   *
   * @param forest Forest to stream (NULL yields the document events only).
   * @param grammar Grammar used to resolve symbol ids to names (may be NULL,
   *                 in which case only ids are reported).
   * @param input Source text the forest was built from (may be NULL, in which
   *              case terminals report no text).
   * @param input_length Length of @p input in bytes.
   * @param emit Sink invoked per event (required).
   * @param user_data Context passed to @p emit.
   * @return Number of node events emitted, or 0 on invalid input.
   *
   * @see glr_forest_to_xml
   */
  size_t glr_forest_write_xml_events (const glr_forest_t *forest,
                                      const glr_grammar_t *grammar,
                                      const char *input, size_t input_length,
                                      glr_xml_event_fn emit, void *user_data);

  /**
   * @brief Stream one subtree as an XML event sequence.
   *
   * The same event sequence as @ref glr_forest_write_xml_events, but rooted at
   * a single node, so a caller can serialize the part of a forest it cares
   * about without building a throwaway container first.
   *
   * @param root Node to start from (may be NULL).
   * @param grammar Grammar used to resolve symbol names (may be NULL).
   * @param input Source text (may be NULL).
   * @param input_length Length of @p input in bytes.
   * @param emit Sink invoked per event (required).
   * @param user_data Context passed to @p emit.
   * @return Number of node events emitted, or 0 on invalid input.
   */
  size_t glr_forest_node_write_xml_events (const glr_forest_node_t *root,
                                           const glr_grammar_t *grammar,
                                           const char *input,
                                           size_t input_length,
                                           glr_xml_event_fn emit,
                                           void *user_data);

  /**
   * @brief Render a parse forest as an XML document.
   *
   * Produces the same event sequence as @ref glr_forest_write_xml_events as a
   * single XML string:
   *
   * @code
   * <?xml version="1.0" encoding="UTF-8"?>
   * <parse-forest>
   *   <node type="nonterminal" symbol="Expr" start="0" end="3">
   *     <node type="constructor" production="1" start="0" end="3">
   *       <leaf type="terminal" symbol="n" start="0" end="1">n</leaf>
   *     </node>
   *   </node>
   * </parse-forest>
   * @endcode
   *
   * @param forest Forest to render (may be NULL).
   * @param grammar Grammar used to resolve symbol names (may be NULL).
   * @param input Source text, used for leaf content (may be NULL).
   * @param input_length Length of @p input in bytes.
   * @param out Output buffer allocated by the function, NUL-terminated; the
   *            caller frees it.
   * @param out_length Receives the length excluding the terminator; may be NULL.
   * @return 0 on success, -1 on invalid input or allocation failure.
   */
  int glr_forest_to_xml (const glr_forest_t *forest,
                         const glr_grammar_t *grammar, const char *input,
                         size_t input_length, char **out, size_t *out_length);

#ifdef __cplusplus
}
#endif

#endif /* GLR_SERIALIZATION_H */
