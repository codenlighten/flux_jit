#ifndef FLUX_IR_V1_H
#define FLUX_IR_V1_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Flux IR C-ABI: Version 1
//
// Goal:
//   Provide a stable, language-neutral wire contract for submitting IR payloads
//   to FluxJit across C/C++/Rust/Go/Python FFI boundaries.
//
// Notes:
//   - This header defines the public ABI shape only.
//   - Payload encoding may be JSON initially, with FlatBuffers/Protobuf later.
//   - All pointers are non-owning unless explicitly documented otherwise.
// -----------------------------------------------------------------------------

#define FLUX_IR_V1_ABI_MAJOR 1u
#define FLUX_IR_V1_ABI_MINOR 0u
#define FLUX_IR_V1_ABI_PATCH 0u

#define FLUX_IR_MAKE_ABI_VERSION(major, minor, patch) \
    ((((uint32_t)(major) & 0x3FFu) << 22) | (((uint32_t)(minor) & 0x3FFu) << 12) | ((uint32_t)(patch) & 0xFFFu))

#define FLUX_IR_V1_ABI_VERSION \
    FLUX_IR_MAKE_ABI_VERSION(FLUX_IR_V1_ABI_MAJOR, FLUX_IR_V1_ABI_MINOR, FLUX_IR_V1_ABI_PATCH)

typedef enum flux_ir_status_v1 {
    FLUX_IR_STATUS_OK = 0,
    FLUX_IR_STATUS_INVALID_ARGUMENT = 1,
    FLUX_IR_STATUS_UNSUPPORTED_VERSION = 2,
    FLUX_IR_STATUS_UNSUPPORTED_ENCODING = 3,
    FLUX_IR_STATUS_VALIDATION_ERROR = 4,
    FLUX_IR_STATUS_INTERNAL_ERROR = 255
} flux_ir_status_v1;

typedef enum flux_ir_encoding_v1 {
    FLUX_IR_ENCODING_JSON_UTF8 = 0,
    FLUX_IR_ENCODING_FLATBUFFERS = 1,
    FLUX_IR_ENCODING_PROTOBUF = 2
} flux_ir_encoding_v1;

typedef enum flux_ir_scalar_type_v1 {
    FLUX_IR_SCALAR_F32 = 0,
    FLUX_IR_SCALAR_F64 = 1,
    FLUX_IR_SCALAR_I32 = 2,
    FLUX_IR_SCALAR_I64 = 3,
    FLUX_IR_SCALAR_U32 = 4,
    FLUX_IR_SCALAR_U64 = 5
} flux_ir_scalar_type_v1;

typedef enum flux_ir_layout_v1 {
    FLUX_IR_LAYOUT_CONTIGUOUS = 0,
    FLUX_IR_LAYOUT_SOA = 1,
    FLUX_IR_LAYOUT_AOS = 2,
    FLUX_IR_LAYOUT_STRIDED = 3
} flux_ir_layout_v1;

typedef struct flux_ir_string_view_v1 {
    const char* data;
    uint64_t size_bytes;
} flux_ir_string_view_v1;

typedef struct flux_ir_span_u8_v1 {
    const uint8_t* data;
    uint64_t size_bytes;
} flux_ir_span_u8_v1;

typedef struct flux_ir_shape_v1 {
    const int64_t* dims;
    uint32_t rank;
} flux_ir_shape_v1;

typedef struct flux_ir_tensor_desc_v1 {
    uint32_t tensor_id;
    flux_ir_string_view_v1 name;
    flux_ir_scalar_type_v1 scalar_type;
    flux_ir_layout_v1 layout;
    flux_ir_shape_v1 shape;
    const int64_t* strides; // Optional; required when layout == FLUX_IR_LAYOUT_STRIDED
} flux_ir_tensor_desc_v1;

typedef struct flux_ir_envelope_v1 {
    uint32_t abi_version;               // Must be FLUX_IR_V1_ABI_VERSION
    flux_ir_encoding_v1 encoding;       // Payload format
    flux_ir_string_view_v1 module_name; // Logical module identifier
    flux_ir_string_view_v1 entrypoint;  // Entry kernel name/symbol
    flux_ir_span_u8_v1 payload;         // Serialized IR payload
} flux_ir_envelope_v1;

typedef struct flux_ir_compile_options_v1 {
    uint32_t flags;
    uint32_t reserved;
    flux_ir_string_view_v1 target_triple; // Optional (e.g. x86_64-linux-gnu)
    flux_ir_string_view_v1 cpu_features;  // Optional (e.g. avx2,fma)
} flux_ir_compile_options_v1;

// Compile option flags
#define FLUX_IR_COMPILE_FLAG_ENABLE_AUTOTUNE   (1u << 0)
#define FLUX_IR_COMPILE_FLAG_ALLOW_FALLBACK    (1u << 1)
#define FLUX_IR_COMPILE_FLAG_RECORD_PROFILE    (1u << 2)
#define FLUX_IR_COMPILE_FLAG_DETERMINISTIC     (1u << 3)

// Runtime strategy hints (optional, may be ignored by engine policy)
#define FLUX_IR_STRATEGY_HINT_NONE             0u
#define FLUX_IR_STRATEGY_HINT_MASKED           1u
#define FLUX_IR_STRATEGY_HINT_COMPACTED        2u

typedef struct flux_ir_runtime_hints_v1 {
    uint32_t strategy_hint;
    uint32_t reserved;
    float alive_ratio_hint; // Optional in [0,1], set <0 for unknown
    uint32_t work_factor_hint;
} flux_ir_runtime_hints_v1;

#ifdef __cplusplus
}
#endif

#endif // FLUX_IR_V1_H
