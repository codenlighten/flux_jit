#ifndef FLUXJIT_H
#define FLUXJIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct flux_ctx flux_ctx;
typedef struct flux_kernel flux_kernel;

flux_ctx* flux_create(void);
void flux_destroy(flux_ctx* ctx);

flux_kernel* flux_compile_json(flux_ctx* ctx, const char* ir_json, char** error_msg);
void flux_kernel_destroy(flux_kernel* k);

int flux_run_f32(
    flux_kernel* k,
    int64_t n,
    const float* in0,
    const float* in1,
    float* out0,
    float* out1,
    char** error_msg
);

void flux_free_string(char* s);

#ifdef __cplusplus
}
#endif

#endif
