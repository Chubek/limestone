#include <glr/grammar.h>
#include <glr/serialization.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SERIAL_VERSION 1

/* Helper: Write data to buffer with automatic expansion */
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} write_buffer_t;

static int buffer_init(write_buffer_t* buf, size_t initial_capacity) {
    buf->data = malloc(initial_capacity);
    if (!buf->data) return -1;
    buf->size = 0;
    buf->capacity = initial_capacity;
    return 0;
}

static void buffer_free(write_buffer_t* buf) {
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

static int buffer_write(write_buffer_t* buf, const void* data, size_t len) {
    if (buf->size + len > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        while (new_capacity < buf->size + len) {
            new_capacity *= 2;
        }
        uint8_t* new_data = realloc(buf->data, new_capacity);
        if (!new_data) return -1;
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    return 0;
}

static size_t buffer_tell(write_buffer_t* buf) {
    return buf->size;
}

/* Serialize a single forest node (recursive) */
static int serialize_node_recursive(const glr_forest_node_t* node, 
                                     write_buffer_t* buf) {
    if (!node) return 0;
    
    glr_serialized_node_header_t header;
    header.tag = GLR_SERIAL_TAG_FOREST_NODE;
    header.node_type = node->type;
    header.symbol_id = node->symbol_id;
    header.position = node->position;
    header.child_count = node->child_count;
    header.data_size = 0;  /* User data not serialized for now */
    
    size_t header_pos = buffer_tell(buf);
    if (buffer_write(buf, &header, sizeof(header)) < 0) return -1;
    
    /* Write children offsets (will be filled later) */
    size_t children_offset_pos = buffer_tell(buf);
    header.children_offset = children_offset_pos;
    
    if (node->child_count > 0) {
        uint32_t* child_offsets = calloc(node->child_count, sizeof(uint32_t));
        if (!child_offsets) return -1;
        
        if (buffer_write(buf, child_offsets, node->child_count * sizeof(uint32_t)) < 0) {
            free(child_offsets);
            return -1;
        }
        
        /* Serialize each child and record offset */
        for (size_t i = 0; i < node->child_count; i++) {
            child_offsets[i] = buffer_tell(buf);
            if (serialize_node_recursive(node->children[i], buf) < 0) {
                free(child_offsets);
                return -1;
            }
        }
        
        /* Update child offsets in buffer */
        memcpy(buf->data + children_offset_pos, child_offsets, 
               node->child_count * sizeof(uint32_t));
        free(child_offsets);
    } else {
        header.children_offset = 0;
    }
    
    /* Update header size */
    header.size = buffer_tell(buf) - header_pos;
    memcpy(buf->data + header_pos, &header, sizeof(header));
    
    return 0;
}

int glr_serialize_forest_node(const glr_forest_node_t* node,
                              uint8_t** out_data,
                              size_t* out_len) {
    if (!node || !out_data || !out_len) return -1;
    
    write_buffer_t buf;
    if (buffer_init(&buf, 4096) < 0) return -1;
    
    if (serialize_node_recursive(node, &buf) < 0) {
        buffer_free(&buf);
        return -1;
    }
    
    *out_data = buf.data;
    *out_len = buf.size;
    return 0;
}

int glr_serialize_forest(const glr_forest_t* forest, 
                         uint8_t** out_data, 
                         size_t* out_len) {
    if (!forest || !out_data || !out_len) return -1;
    
    write_buffer_t buf;
    if (buffer_init(&buf, 8192) < 0) return -1;
    
    /* Write header */
    glr_serialized_forest_header_t header;
    header.tag = GLR_SERIAL_TAG_FOREST;
    header.version = SERIAL_VERSION;
    header.node_count = forest->node_count;
    header.edge_count = forest->edge_count;
    
    size_t header_pos = buffer_tell(&buf);
    if (buffer_write(&buf, &header, sizeof(header)) < 0) {
        buffer_free(&buf);
        return -1;
    }
    
    /* Write node position offsets */
    header.nodes_offset = buffer_tell(&buf);
    uint32_t* node_offsets = calloc(forest->node_count, sizeof(uint32_t));
    if (!node_offsets) {
        buffer_free(&buf);
        return -1;
    }
    
    if (buffer_write(&buf, node_offsets, forest->node_count * sizeof(uint32_t)) < 0) {
        free(node_offsets);
        buffer_free(&buf);
        return -1;
    }
    
    /* Serialize all nodes at each position */
    for (size_t pos = 0; pos < forest->node_count; pos++) {
        node_offsets[pos] = buffer_tell(&buf);
        
        glr_forest_node_t* node = forest->nodes[pos];
        uint32_t node_count_at_pos = 0;
        
        /* Count nodes at this position */
        glr_forest_node_t* temp = node;
        while (temp) {
            node_count_at_pos++;
            temp = temp->next;
        }
        
        /* Write node count */
        if (buffer_write(&buf, &node_count_at_pos, sizeof(uint32_t)) < 0) {
            free(node_offsets);
            buffer_free(&buf);
            return -1;
        }
        
        /* Serialize each node in the linked list */
        while (node) {
            if (serialize_node_recursive(node, &buf) < 0) {
                free(node_offsets);
                buffer_free(&buf);
                return -1;
            }
            node = node->next;
        }
    }
    
    /* Update node offsets in header */
    memcpy(buf.data + header.nodes_offset, node_offsets, 
           forest->node_count * sizeof(uint32_t));
    free(node_offsets);
    
    /* Edges are not serialized for now (can be reconstructed) */
    header.edges_offset = 0;
    
    /* Update header */
    header.size = buf.size;
    memcpy(buf.data + header_pos, &header, sizeof(header));
    
    *out_data = buf.data;
    *out_len = buf.size;
    return 0;
}

/* Deserialization */
static int deserialize_node_recursive(const uint8_t* data, size_t len,
                                       size_t offset,
                                       glr_forest_node_t** out_node) {
    if (offset + sizeof(glr_serialized_node_header_t) > len) return -1;
    
    const glr_serialized_node_header_t* header = 
        (const glr_serialized_node_header_t*)(data + offset);
    
    if (header->tag != GLR_SERIAL_TAG_FOREST_NODE) return -1;
    
    glr_forest_node_t* node = calloc(1, sizeof(glr_forest_node_t));
    if (!node) return -1;
    
    node->type = header->node_type;
    node->symbol_id = header->symbol_id;
    node->position = header->position;
    node->child_count = header->child_count;
    node->capacity = header->child_count;
    node->data = NULL;
    node->next = NULL;
    
    if (node->child_count > 0) {
        node->children = calloc(node->child_count, sizeof(glr_forest_node_t*));
        if (!node->children) {
            free(node);
            return -1;
        }
        
        const uint32_t* child_offsets = (const uint32_t*)(data + header->children_offset);
        
        for (size_t i = 0; i < node->child_count; i++) {
            if (deserialize_node_recursive(data, len, child_offsets[i], 
                                          &node->children[i]) < 0) {
                /* Cleanup */
                for (size_t j = 0; j < i; j++) {
                    free(node->children[j]);
                }
                free(node->children);
                free(node);
                return -1;
            }
        }
    }
    
    *out_node = node;
    return 0;
}

int glr_deserialize_forest_node(const uint8_t* data,
                                size_t len,
                                glr_forest_node_t** out_node) {
    if (!data || !out_node) return -1;
    return deserialize_node_recursive(data, len, 0, out_node);
}

int glr_deserialize_forest(const uint8_t* data, 
                           size_t len, 
                           glr_forest_t** out_forest) {
    if (!data || !out_forest || len < sizeof(glr_serialized_forest_header_t)) {
        return -1;
    }
    
    const glr_serialized_forest_header_t* header = 
        (const glr_serialized_forest_header_t*)data;
    
    if (header->tag != GLR_SERIAL_TAG_FOREST || 
        header->version != SERIAL_VERSION) {
        return -1;
    }
    
    glr_forest_t* forest = glr_forest_create();
    if (!forest) return -1;
    
    /* Allocate nodes array */
    if (header->node_count > 0) {
        forest->nodes = calloc(header->node_count, sizeof(glr_forest_node_t*));
        if (!forest->nodes) {
            glr_forest_destroy(forest);
            return -1;
        }
        forest->node_count = header->node_count;
    }
    
    const uint32_t* node_offsets = (const uint32_t*)(data + header->nodes_offset);
    
    /* Deserialize nodes at each position */
    for (size_t pos = 0; pos < header->node_count; pos++) {
        uint32_t offset = node_offsets[pos];
        if (offset + sizeof(uint32_t) > len) {
            glr_forest_destroy(forest);
            return -1;
        }
        
        const uint32_t* node_count_ptr = (const uint32_t*)(data + offset);
        uint32_t node_count_at_pos = *node_count_ptr;
        offset += sizeof(uint32_t);
        
        glr_forest_node_t* prev = NULL;
        for (uint32_t i = 0; i < node_count_at_pos; i++) {
            glr_forest_node_t* node;
            if (deserialize_node_recursive(data, len, offset, &node) < 0) {
                glr_forest_destroy(forest);
                return -1;
            }
            
            if (prev) {
                prev->next = node;
            } else {
                forest->nodes[pos] = node;
            }
            prev = node;
            
            /* Move offset forward */
            const glr_serialized_node_header_t* node_header = 
                (const glr_serialized_node_header_t*)(data + offset);
            offset += node_header->size;
        }
    }
    
    *out_forest = forest;
    return 0;
}

/* GSS node serialization: header followed by parent array.
 * Layout: glr_serialized_gss_node_t, then parent_count x 8-byte records
 * of (state_id, position) pairs, one per parent (one level deep). On
 * load we rebuild one-level parent nodes. Ownership: the deserialized
 * child holds borrowed pointers to freshly allocated parents; destroy
 * the child with glr_stack_node_destroy() and each parent obtained via
 * glr_stack_node_get_parent() with glr_stack_node_destroy(), or use
 * glr_stack_node_destroy_tree() for a one-shot deep destroy. */
int glr_serialize_stack_node(const glr_stack_node_t* node,
                             uint8_t** out_data,
                             size_t* out_len) {
    glr_serialized_gss_node_t header;
    size_t parent_count;
    size_t total;
    uint8_t *buf;
    uint8_t *cursor;

    if (!node || !out_data || !out_len) return -1;

    parent_count = glr_stack_node_get_parent_count (node);
    total = sizeof (header) + parent_count * 8u;

    buf = malloc (total);
    if (!buf) return -1;

    header.tag = GLR_SERIAL_TAG_GSS_NODE;
    header.size = (uint32_t) total;
    header.state_id = glr_stack_node_get_state (node);
    header.position = glr_stack_node_get_position (node);
    header.parent_count = (uint32_t) parent_count;
    header.parent_offset
        = parent_count > 0 ? (uint32_t) sizeof (header) : 0;

    memcpy (buf, &header, sizeof (header));
    cursor = buf + sizeof (header);
    for (size_t i = 0; i < parent_count; i++)
      {
        glr_stack_node_t *parent = glr_stack_node_get_parent (node, i);
        uint32_t state = parent ? glr_stack_node_get_state (parent) : 0;
        uint32_t pos = parent ? glr_stack_node_get_position (parent) : 0;
        memcpy (cursor, &state, sizeof (state));
        memcpy (cursor + 4, &pos, sizeof (pos));
        cursor += 8;
      }

    *out_data = buf;
    *out_len = total;

    return 0;
}

int glr_deserialize_stack_node(const uint8_t* data,
                               size_t len,
                               glr_stack_node_t** out_node) {
    const glr_serialized_gss_node_t *header;
    glr_stack_node_t *node;
    const uint8_t *cursor;

    if (!data || !out_node || len < sizeof(*header)) {
        return -1;
    }

    header = (const glr_serialized_gss_node_t *) data;
    if (header->tag != GLR_SERIAL_TAG_GSS_NODE) return -1;
    if (header->size > len || header->size < sizeof (*header)) return -1;

    node = glr_stack_node_create (header->state_id,
                                  (uint32_t) header->position);
    if (!node) return -1;

    if (header->parent_count > 0)
      {
        size_t need = (size_t) header->parent_offset
                      + (size_t) header->parent_count * 8u;
        if (need > len)
          {
            glr_stack_node_destroy (node);
            return -1;
          }
        cursor = data + header->parent_offset;
        for (uint32_t i = 0; i < header->parent_count; i++)
          {
            uint32_t state;
            uint32_t pos;
            glr_stack_node_t *parent;
            memcpy (&state, cursor, sizeof (state));
            memcpy (&pos, cursor + 4, sizeof (pos));
            cursor += 8;
            parent = glr_stack_node_create (state, pos);
            if (!parent)
              {
                glr_stack_node_destroy (node);
                return -1;
              }
            /* Parents are borrowed references. The caller destroys the
               child with glr_stack_node_destroy() and each parent via
               getters, or uses glr_stack_node_destroy_tree() for a
               one-shot deep destroy. */
            if (glr_stack_node_add_parent (node, parent) != 0)
              {
                glr_stack_node_destroy (parent);
                glr_stack_node_destroy_tree (node);
                return -1;
              }
          }
      }

    *out_node = node;
    return 0;
}

/* ============================================================================
 * XML event stream
 * ========================================================================= */

static const char *
xml_node_kind (glr_forest_node_type_t type)
{
    switch (type) {
    case GLR_NODE_TERMINAL:
        return "terminal";
    case GLR_NODE_NONTERMINAL:
        return "nonterminal";
    case GLR_NODE_CONSTRUCTOR:
        return "constructor";
    default:
        break;
    }
    return "unknown";
}

static const char *
xml_symbol_name (const glr_grammar_t *grammar, int symbol_id)
{
    const glr_symbol_t *symbol;

    if (grammar == NULL) {
        return NULL;
    }
    symbol = glr_grammar_get_symbol (grammar, symbol_id);
    return symbol != NULL ? symbol->name : NULL;
}

/* Depth-first walk of the forest. A shared node is emitted once, so the event
   stream matches the packed structure rather than expanding ambiguity. */
struct xml_walk_state {
    const glr_forest_t *forest;
    const glr_grammar_t *grammar;
    const char *input;
    size_t input_length;
    glr_xml_event_fn emit;
    void *user_data;
    glr_forest_node_t **seen;
    size_t seen_count;
    size_t seen_capacity;
    size_t emitted;
    bool failed;
};

static bool
xml_walk_seen (struct xml_walk_state *state, const glr_forest_node_t *node)
{
    for (size_t i = 0; i < state->seen_count; i++) {
        if (state->seen[i] == node) {
            return true;
        }
    }
    if (state->seen_count >= state->seen_capacity) {
        size_t new_capacity = state->seen_capacity == 0
                                  ? 64
                                  : state->seen_capacity * 2;
        glr_forest_node_t **grown = realloc (
            state->seen, new_capacity * sizeof (*grown));
        if (grown == NULL) {
            return true; /* treat as seen: stop rather than loop forever */
        }
        state->seen = grown;
        state->seen_capacity = new_capacity;
    }
    state->seen[state->seen_count++] = (glr_forest_node_t *) node;
    return false;
}

static void
xml_walk (struct xml_walk_state *state, const glr_forest_node_t *node,
          size_t depth)
{
    glr_xml_event_info_t info;

    if (state->failed || node == NULL) {
        return;
    }
    if (xml_walk_seen (state, node)) {
        return;
    }

    memset (&info, 0, sizeof (info));
    info.node = node;
    info.depth = depth;

    if (node->type == GLR_NODE_TERMINAL) {
        if (state->input != NULL
            && node->position <= state->input_length
            && node->end_position <= state->input_length
            && node->end_position >= node->position) {
            info.text = state->input + node->position;
            info.text_length = node->end_position - node->position;
        }
        state->emit (GLR_XML_EVENT_LEAF, &info, state->user_data);
        state->emitted++;
        return;
    }

    state->emit (GLR_XML_EVENT_NODE_START, &info, state->user_data);
    state->emitted++;
    for (size_t c = 0; c < node->child_count; c++) {
        xml_walk (state, node->children[c], depth + 1);
        if (state->failed) {
            return;
        }
    }
    state->emit (GLR_XML_EVENT_NODE_END, &info, state->user_data);
}

size_t
glr_forest_node_write_xml_events (const glr_forest_node_t *root,
                                  const glr_grammar_t *grammar,
                                  const char *input, size_t input_length,
                                  glr_xml_event_fn emit, void *user_data)
{
    struct xml_walk_state state;
    glr_xml_event_info_t info;

    if (emit == NULL)
    {
        return 0;
    }

    memset (&state, 0, sizeof (state));
    memset (&info, 0, sizeof (info));
    state.grammar = grammar;
    state.input = input;
    state.input_length = input_length;
    state.emit = emit;
    state.user_data = user_data;

    emit (GLR_XML_EVENT_DOCUMENT_START, &info, user_data);
    xml_walk (&state, root, 0);
    emit (GLR_XML_EVENT_DOCUMENT_END, &info, user_data);

    free (state.seen);
    return state.emitted;
}

size_t
glr_forest_write_xml_events (const glr_forest_t *forest,
                             const glr_grammar_t *grammar, const char *input,
                             size_t input_length, glr_xml_event_fn emit,
                             void *user_data)
{
    struct xml_walk_state state;
    glr_xml_event_info_t info;

    if (emit == NULL) {
        return 0;
    }

    memset (&state, 0, sizeof (state));
    memset (&info, 0, sizeof (info));
    state.forest = forest;
    state.grammar = grammar;
    state.input = input;
    state.input_length = input_length;
    state.emit = emit;
    state.user_data = user_data;

    emit (GLR_XML_EVENT_DOCUMENT_START, &info, user_data);
    if (forest != NULL && forest->root != NULL) {
        xml_walk (&state, forest->root, 0);
    }
    emit (GLR_XML_EVENT_DOCUMENT_END, &info, user_data);

    free (state.seen);
    return state.emitted;
}

/* Growable text buffer used by the XML writer. It also carries the forest and
   grammar, because the event callback signature has no room for them and the
   renderer needs both to name symbols and report the document's node count. */
struct xml_text {
    char *data;
    size_t length;
    size_t capacity;
    const glr_forest_t *seen_forest;
    const glr_grammar_t *grammar;
    bool failed;
};

static void
xml_text_append (struct xml_text *text, const char *bytes, size_t length)
{
    if (text->failed) {
        return;
    }
    if (text->length + length + 1 > text->capacity) {
        size_t new_capacity = text->capacity == 0 ? 256 : text->capacity * 2;
        char *grown;
        while (new_capacity < text->length + length + 1) {
            new_capacity *= 2;
        }
        grown = realloc (text->data, new_capacity);
        if (grown == NULL) {
            text->failed = true;
            return;
        }
        text->data = grown;
        text->capacity = new_capacity;
    }
    memcpy (text->data + text->length, bytes, length);
    text->length += length;
    text->data[text->length] = '\0';
}

static void
xml_text_puts (struct xml_text *text, const char *bytes)
{
    xml_text_append (text, bytes, strlen (bytes));
}

static void
xml_text_put_uint (struct xml_text *text, size_t value)
{
    char digits[32];
    int written = snprintf (digits, sizeof (digits), "%zu", value);
    if (written > 0) {
        xml_text_append (text, digits, (size_t) written);
    } else {
        text->failed = true;
    }
}

static void
xml_text_escape (struct xml_text *text, const char *bytes, size_t length,
                 bool in_attribute)
{
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char) bytes[i];
        switch (c) {
        case '&':
            xml_text_puts (text, "&amp;");
            break;
        case '<':
            xml_text_puts (text, "&lt;");
            break;
        case '>':
            xml_text_puts (text, "&gt;");
            break;
        case '"':
            if (in_attribute) {
                xml_text_puts (text, "&quot;");
            } else {
                xml_text_append (text, (const char *) &c, 1);
            }
            break;
        case '\'':
            if (in_attribute) {
                xml_text_puts (text, "&apos;");
            } else {
                xml_text_append (text, (const char *) &c, 1);
            }
            break;
        default:
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                /* XML 1.0 cannot represent other control characters; an
                   escape keeps the document well formed. */
                char entity[16];
                int written = snprintf (entity, sizeof (entity), "&#%u;", c);
                if (written > 0) {
                    xml_text_append (text, entity, (size_t) written);
                }
            } else {
                xml_text_append (text, (const char *) &c, 1);
            }
            break;
        }
    }
}

static void
xml_attribute_uint (struct xml_text *text, const char *name, size_t value)
{
    xml_text_puts (text, " ");
    xml_text_puts (text, name);
    xml_text_puts (text, "=\"");
    xml_text_put_uint (text, value);
    xml_text_puts (text, "\"");
}

static void
xml_attribute_name (struct xml_text *text, const char *name, const char *value)
{
    xml_text_puts (text, " ");
    xml_text_puts (text, name);
    xml_text_puts (text, "=\"");
    xml_text_escape (text, value, strlen (value), true);
    xml_text_puts (text, "\"");
}

static void
xml_render_event (glr_xml_event_t event, const glr_xml_event_info_t *info,
                  void *user_data)
{
    struct xml_text *text = user_data;
    const glr_forest_node_t *node = info->node;
    const char *name;

    if (event == GLR_XML_EVENT_DOCUMENT_END) {
        xml_text_puts (text, "</parse-forest>\n");
        return;
    }
    if (event == GLR_XML_EVENT_DOCUMENT_START) {
        xml_text_puts (text, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        xml_text_puts (text, "<parse-forest");
        xml_attribute_uint (text, "nodes",
                            glr_forest_total_nodes (text->seen_forest));
        xml_text_puts (text, ">\n");
        return;
    }
    if (text->failed || node == NULL) {
        return;
    }

    switch (event) {
    case GLR_XML_EVENT_LEAF:
        xml_text_puts (text, "<leaf");
        xml_attribute_name (text, "type", xml_node_kind (node->type));
        name = xml_symbol_name (text->grammar, node->symbol_id);
        if (name != NULL) {
            xml_attribute_name (text, "symbol", name);
        } else {
            char digits[32];
            int written = snprintf (digits, sizeof (digits), "%d",
                                    node->symbol_id);
            if (written > 0) {
                xml_attribute_name (text, "symbol", digits);
            }
        }
        xml_attribute_uint (text, "start", node->position);
        xml_attribute_uint (text, "end", node->end_position);
        xml_text_puts (text, ">");
        if (info->text != NULL) {
            xml_text_escape (text, info->text, info->text_length, false);
        }
        xml_text_puts (text, "</leaf>\n");
        return;
    case GLR_XML_EVENT_NODE_START:
        xml_text_puts (text, "<node");
        xml_attribute_name (text, "type", xml_node_kind (node->type));
        name = xml_symbol_name (text->grammar, node->symbol_id);
        if (node->type == GLR_NODE_CONSTRUCTOR) {
            char digits[32];
            int written = snprintf (digits, sizeof (digits), "%d",
                                    node->symbol_id);
            if (written > 0) {
                xml_attribute_name (text, "production", digits);
            }
        } else if (name != NULL) {
            xml_attribute_name (text, "symbol", name);
        } else {
            char digits[32];
            int written = snprintf (digits, sizeof (digits), "%d",
                                    node->symbol_id);
            if (written > 0) {
                xml_attribute_name (text, "symbol", digits);
            }
        }
        xml_attribute_uint (text, "start", node->position);
        xml_attribute_uint (text, "end", node->end_position);
        xml_text_puts (text, ">\n");
        return;
    case GLR_XML_EVENT_NODE_END:
        xml_text_puts (text, "</node>\n");
        return;
    case GLR_XML_EVENT_DOCUMENT_END:
    default:
        return;
    }
}

int
glr_forest_to_xml (const glr_forest_t *forest, const glr_grammar_t *grammar,
                   const char *input, size_t input_length, char **out,
                   size_t *out_length)
{
    struct xml_text text;

    if (out == NULL) {
        return -1;
    }
    *out = NULL;
    if (out_length != NULL) {
        *out_length = 0;
    }

    memset (&text, 0, sizeof (text));
    /* xml_render_event() needs the forest and grammar, which the event
       callback does not carry; stash them next to the buffer. */
    text.seen_forest = forest;
    text.grammar = grammar;

    (void) glr_forest_write_xml_events (forest, grammar, input, input_length,
                                        xml_render_event, &text);
    if (text.failed) {
        free (text.data);
        return -1;
    }
    if (text.data == NULL) {
        text.data = calloc (1, 1);
        if (text.data == NULL) {
            return -1;
        }
    }

    *out = text.data;
    if (out_length != NULL) {
        *out_length = text.length;
    }
    return 0;
}
