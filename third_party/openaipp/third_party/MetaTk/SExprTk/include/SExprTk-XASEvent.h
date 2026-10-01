/*
 * SExprTk-XASEvent.h
 *
 * XAS (eXchangeable Abstract Syntax) event interface and wire protocol.
 *
 * This is NOT a library: it is a pure interface/protocol header. It
 * declares, in ANSI C (C89/C90), the symbols, data structures and
 * function prototypes needed for two processes to communicate XAS
 * events between each other (e.g. over UDP datagrams). It contains no
 * implementation: encoders, decoders and dispatchers are provided by
 * the host library (see SExprTk.hpp) or by user code.
 *
 * The protocol
 * ------------
 * XAS is a streaming event model over an s-expression tree, in the
 * spirit of SAX for XML. A parser walks a document and emits a flat,
 * totally ordered sequence of events:
 *
 *   DocumentBegin                 once, at the start of a document
 *   ( ListBegin Atom* ListEnd )*  balanced list nesting, atoms in order
 *   Comment                       zero or more, interleaved anywhere
 *   Error                         zero or more, on malformed input
 *   DocumentEnd                   once, at the end of a document
 *
 * Every event carries a monotonically increasing sequence number, a
 * kind tag, source location information and an optional payload.
 * Events are self-delimiting and may be serialized into fixed-header
 * datagrams suitable for unreliable transports such as UDP.
 *
 * Wire format (datagram frame, all integers big-endian):
 *
 *   offset  size  field
 *   ------  ----  --------------------------------------------
 *   0       2     magic        ASCII "XA" (0x58 0x41)
 *   2       1     version      protocol version (currently 1)
 *   3       1     flags        bit 0: end-of-stream, bits 1-7: reserved
 *   4       1     event kind   one of SEXPRTK_XAS_EVENT_*
 *   5       1     reserved     must be zero on send, ignored on receive
 *   6       2     source id    sender-assigned endpoint identifier
 *   8       8     sequence     monotonically increasing event counter
 *   16      2     line         1-based source line (0 = unknown)
 *   18      2     column       1-based source column (0 = unknown)
 *   20      4     payload len  byte length of the payload that follows
 *   24      n     payload      kind-specific, UTF-8, not NUL-terminated
 *
 * Header size is therefore 24 bytes. A frame fits comfortably inside
 * a single UDP datagram for any reasonable payload.
 *
 * Payload conventions:
 *   Atom        the literal text of the atom as it appeared in source
 *   Comment     the comment text (without the leading ';')
 *   Error       a human-readable diagnostic message
 *   others      empty (length 0)
 */

#ifndef SEXPRTK_XASEVENT_H
#define SEXPRTK_XASEVENT_H

#include <stddef.h> /* size_t            */
#include <stdint.h> /* uint8_t, uint16_t, uint32_t, uint64_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Protocol constants                                                  */
/* ------------------------------------------------------------------ */

#define SEXPRTK_XAS_MAGIC0          ((unsigned char)0x58) /* 'X' */
#define SEXPRTK_XAS_MAGIC1          ((unsigned char)0x41) /* 'A' */
#define SEXPRTK_XAS_VERSION         ((unsigned char)1)
#define SEXPRTK_XAS_HEADER_SIZE     ((size_t)24)
#define SEXPRTK_XAS_MAX_PAYLOAD     ((size_t)65507) /* max UDP payload */
#define SEXPRTK_XAS_MAX_DATAGRAM    (SEXPRTK_XAS_HEADER_SIZE + SEXPRTK_XAS_MAX_PAYLOAD)

/* flags */
#define SEXPRTK_XAS_FLAG_EOS        ((unsigned char)0x01) /* end of stream */

/* ------------------------------------------------------------------ */
/* Event kinds                                                         */
/* ------------------------------------------------------------------ */

/*
 * The XAS event vocabulary. Values are fixed by the protocol and must
 * never be renumbered; new kinds may only be appended.
 */
typedef enum sexprtk_xas_event_kind {
    SEXPRTK_XAS_EVENT_DOCUMENT_BEGIN = 1, /* start of a document stream  */
    SEXPRTK_XAS_EVENT_DOCUMENT_END   = 2, /* end of a document stream    */
    SEXPRTK_XAS_EVENT_LIST_BEGIN     = 3, /* '('                         */
    SEXPRTK_XAS_EVENT_LIST_END       = 4, /* ')'                         */
    SEXPRTK_XAS_EVENT_ATOM           = 5, /* atom; payload = source text */
    SEXPRTK_XAS_EVENT_COMMENT        = 6, /* ';...' comment              */
    SEXPRTK_XAS_EVENT_QUOTE          = 7, /* quote prefix: ' ` , ,@      */
    SEXPRTK_XAS_EVENT_ERROR          = 8  /* malformed input             */
} sexprtk_xas_event_kind;

/* ------------------------------------------------------------------ */
/* Status codes                                                        */
/* ------------------------------------------------------------------ */

typedef enum sexprtk_xas_status {
    SEXPRTK_XAS_OK              = 0,  /* success                        */
    SEXPRTK_XAS_ERR_BAD_MAGIC   = -1, /* frame magic bytes mismatch     */
    SEXPRTK_XAS_ERR_BAD_VERSION = -2, /* unsupported protocol version   */
    SEXPRTK_XAS_ERR_TRUNCATED   = -3, /* buffer shorter than header/payload */
    SEXPRTK_XAS_ERR_BAD_KIND    = -4, /* unknown event kind             */
    SEXPRTK_XAS_ERR_TOO_LARGE   = -5, /* payload exceeds maximum size   */
    SEXPRTK_XAS_ERR_INVALID     = -6  /* invalid argument (NULL, etc.)  */
} sexprtk_xas_status;

/* ------------------------------------------------------------------ */
/* Core data structures                                                */
/* ------------------------------------------------------------------ */

/*
 * An XAS event. This is the canonical in-memory representation that
 * producers fill in and consumers inspect. The payload is borrowed:
 * `payload` points at `payload_length` bytes owned by the caller and
 * is never freed by protocol code.
 */
typedef struct sexprtk_xas_event {
    uint64_t         sequence;       /* monotonic event counter          */
    uint16_t         source_id;      /* sender-assigned endpoint id      */
    uint16_t         line;           /* 1-based source line, 0 = unknown */
    uint16_t         column;         /* 1-based source col, 0 = unknown  */
    uint8_t          kind;           /* sexprtk_xas_event_kind           */
    uint8_t          flags;          /* SEXPRTK_XAS_FLAG_* bitmask       */
    const char      *payload;        /* borrowed payload bytes (may be NULL) */
    uint32_t         payload_length; /* payload size in bytes            */
} sexprtk_xas_event;

/*
 * A raw datagram frame: a byte buffer plus its valid length. Used to
 * hand encoded frames to a transport and to receive frames from one.
 * Memory management belongs to the caller.
 */
typedef struct sexprtk_xas_frame {
    unsigned char *bytes;   /* frame storage (header + payload)      */
    size_t         length;  /* valid bytes in `bytes`                */
    size_t         capacity;/* allocated size of `bytes`             */
} sexprtk_xas_frame;

/*
 * Sink interface: the receiving half of the protocol. A producer
 * (parser, network reader) calls `handle` once per decoded event.
 * `userdata` is the opaque context supplied by the implementor.
 * Returning a negative value aborts the stream with an error.
 */
typedef int (*sexprtk_xas_event_sink_fn)(const sexprtk_xas_event *event,
                                         void *userdata);

typedef struct sexprtk_xas_event_sink {
    sexprtk_xas_event_sink_fn handle;
    void                     *userdata;
} sexprtk_xas_event_sink;

/*
 * Source interface: the producing half of the protocol. `next`
 * fills `event` and returns SEXPRTK_XAS_OK, or a status code when the
 * stream is exhausted (SEXPRTK_XAS_ERR_TRUNCATED) or fails.
 */
typedef int (*sexprtk_xas_event_source_fn)(sexprtk_xas_event *event,
                                           void *userdata);

typedef struct sexprtk_xas_event_source {
    sexprtk_xas_event_source_fn next;
    void                       *userdata;
} sexprtk_xas_event_source;

/* ------------------------------------------------------------------ */
/* Interface: protocol helpers (declarations only)                     */
/*                                                                     */
/* Host code links these against its own implementation; SExprTk.hpp  */
/* provides a reference implementation for C++ consumers.             */
/* ------------------------------------------------------------------ */

/* Human-readable name of an event kind ("atom", "list-begin", ...).
 * Returns "unknown" for out-of-range kinds. */
const char *sexprtk_xas_event_kind_name(int kind);

/* Parse the name produced by sexprtk_xas_event_kind_name back into a
 * kind value. Returns -1 if the name is not recognized. */
int sexprtk_xas_event_kind_from_name(const char *name);

/* Non-zero if `kind` is a valid protocol event kind. */
int sexprtk_xas_event_kind_valid(int kind);

/* Encode `event` into a datagram frame.
 *
 * On entry `frame->bytes`/`frame->capacity` describe caller-owned
 * storage of at least SEXPRTK_XAS_HEADER_SIZE + event->payload_length
 * bytes. On success `frame->length` is set to the encoded size and
 * SEXPRTK_XAS_OK is returned. Otherwise a sexprtk_xas_status error
 * code is returned and the frame contents are undefined.
 */
int sexprtk_xas_frame_encode(const sexprtk_xas_event *event,
                             sexprtk_xas_frame *frame);

/* Decode a datagram frame into an event.
 *
 * `frame->bytes`/`frame->length` must describe a complete frame as
 * produced by sexprtk_xas_frame_encode. On success `event` is filled
 * in; its `payload` pointer borrows from the frame storage, so the
 * frame must outlive the event. Returns SEXPRTK_XAS_OK or a
 * sexprtk_xas_status error code.
 */
int sexprtk_xas_frame_decode(const sexprtk_xas_frame *frame,
                             sexprtk_xas_event *event);

/* Validate a frame without materializing an event: checks magic,
 * version, kind and payload-length consistency. Returns
 * SEXPRTK_XAS_OK or a sexprtk_xas_status error code. */
int sexprtk_xas_frame_validate(const sexprtk_xas_frame *frame);

/* Convenience: number of payload bytes declared by a (validated)
 * frame header. Returns 0 for malformed frames. */
uint32_t sexprtk_xas_frame_payload_length(const sexprtk_xas_frame *frame);

/* Pump events: repeatedly pull from `source` and push into `sink`
 * until the source is exhausted (SEXPRTK_XAS_ERR_TRUNCATED from
 * `next`) or either side reports an error. Returns SEXPRTK_XAS_OK on
 * a clean end of stream, otherwise the failing status code. */
int sexprtk_xas_pump(sexprtk_xas_event_source *source,
                     sexprtk_xas_event_sink *sink);

/* Initialize an event to safe defaults (sequence 0, ATOM kind, empty
 * payload). */
void sexprtk_xas_event_init(sexprtk_xas_event *event);

/* Initialize a frame to empty (NULL storage, zero length/capacity). */
void sexprtk_xas_frame_init(sexprtk_xas_frame *frame);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SEXPRTK_XASEVENT_H */
