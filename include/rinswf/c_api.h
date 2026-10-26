// SPDX-License-Identifier: MIT
#ifndef RINSWF_C_API_H
#define RINSWF_C_API_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(RINSWF_SHARED)
# if defined(RINSWF_BUILDING)
#  define RINSWF_API __declspec(dllexport)
# else
#  define RINSWF_API __declspec(dllimport)
# endif
#elif defined(__GNUC__)
# define RINSWF_API __attribute__((visibility("default")))
#else
# define RINSWF_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct rinswf_document rinswf_document;
typedef enum rinswf_status {
    RINSWF_OK=0, RINSWF_INVALID_ARGUMENT=1, RINSWF_TRUNCATED=2,
    RINSWF_INVALID_SIGNATURE=3, RINSWF_UNSUPPORTED_COMPRESSION=4,
    RINSWF_INVALID_LENGTH=5, RINSWF_SIZE_LIMIT=6,
    RINSWF_MALFORMED_TAG=7, RINSWF_DECOMPRESSION_FAILED=8,
    RINSWF_OUT_OF_MEMORY=9, RINSWF_INTERNAL_ERROR=10
} rinswf_status;
typedef struct rinswf_limits {
    size_t max_uncompressed_bytes;
    size_t max_tags;
    size_t max_tag_bytes;
} rinswf_limits;
typedef struct rinswf_header {
    uint8_t version;
    uint32_t declared_size;
    int32_t xmin, xmax, ymin, ymax;
    uint16_t frame_rate_fixed8, frame_count;
} rinswf_header;
typedef struct rinswf_tag_view {
    uint16_t code;
    size_t offset;
    const uint8_t* data;
    size_t size;
} rinswf_tag_view;
/* Callers may pass NULL for limits to use library defaults.
 * On failure *out_document is always NULL. Returned tag payload pointers
 * remain valid only while the document lives. Never free them separately. */
RINSWF_API rinswf_status rinswf_parse(const uint8_t* bytes, size_t size,
    const rinswf_limits* limits, rinswf_document** out_document);
RINSWF_API void rinswf_document_destroy(rinswf_document* document);
RINSWF_API rinswf_status rinswf_document_header(const rinswf_document* document,
    rinswf_header* out_header);
RINSWF_API size_t rinswf_document_tag_count(const rinswf_document* document);
RINSWF_API rinswf_status rinswf_document_tag(const rinswf_document* document,
    size_t index, rinswf_tag_view* out_tag);
RINSWF_API const char* rinswf_status_string(rinswf_status status);
#ifdef __cplusplus
}
#endif
#endif
