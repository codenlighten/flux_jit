#include "fluxjit.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct flux_ctx {};

enum class kernel_kind {
    passthrough,
    integrate,
    damage_masked,
    damage_compacted
};

struct flux_kernel {
    kernel_kind kind = kernel_kind::passthrough;
    float dt = 1.0F;
    float damage = 1.0F;
    int work_factor = 0;
    std::string raw_ir;
};

static char* dup_cstr(const char* s) {
    if (!s) {
        return nullptr;
    }
    const size_t n = std::strlen(s);
    auto* out = static_cast<char*>(std::malloc(n + 1));
    if (!out) {
        return nullptr;
    }
    std::memcpy(out, s, n + 1);
    return out;
}

static void set_error(char** error_msg, const char* msg) {
    if (!error_msg) {
        return;
    }
    *error_msg = dup_cstr(msg);
}

static float parse_float_key(const char* json, const char* key_name, float fallback) {
    if (!json) {
        return fallback;
    }

    std::string key_pat = "\"";
    key_pat += key_name;
    key_pat += "\"";

    const char* key = std::strstr(json, key_pat.c_str());
    if (!key) {
        return fallback;
    }

    const char* colon = std::strchr(key, ':');
    if (!colon) {
        return fallback;
    }

    char* end_ptr = nullptr;
    const float parsed = std::strtof(colon + 1, &end_ptr);
    if (end_ptr == colon + 1) {
        return fallback;
    }
    return parsed;
}

static int parse_int_key(const char* json, const char* key_name, int fallback) {
    if (!json) {
        return fallback;
    }

    std::string key_pat = "\"";
    key_pat += key_name;
    key_pat += "\"";

    const char* key = std::strstr(json, key_pat.c_str());
    if (!key) {
        return fallback;
    }

    const char* colon = std::strchr(key, ':');
    if (!colon) {
        return fallback;
    }

    char* end_ptr = nullptr;
    const long parsed = std::strtol(colon + 1, &end_ptr, 10);
    if (end_ptr == colon + 1) {
        return fallback;
    }
    if (parsed < 0) {
        return 0;
    }
    return static_cast<int>(parsed);
}

static float heavy_payload(float v, int work_factor) {
    float x = v;
    for (int i = 0; i < work_factor; ++i) {
        x = std::sin(x) + std::cos(x * 0.5F) + (x * 0.0001F);
    }
    return x;
}

static bool json_has_strategy_compacted(const std::string& raw_ir) {
    return raw_ir.find("\"strategy\": \"compacted\"") != std::string::npos ||
           raw_ir.find("\"strategy\":\"compacted\"") != std::string::npos ||
           raw_ir.find("\"strategy\": \"compact\"") != std::string::npos ||
           raw_ir.find("\"strategy\":\"compact\"") != std::string::npos;
}

extern "C" flux_ctx* flux_create(void) {
    return new flux_ctx();
}

extern "C" void flux_destroy(flux_ctx* ctx) {
    delete ctx;
}

extern "C" flux_kernel* flux_compile_json(flux_ctx* ctx, const char* ir_json, char** error_msg) {
    if (error_msg) {
        *error_msg = nullptr;
    }

    if (!ctx) {
        set_error(error_msg, "flux_compile_json: ctx is null");
        return nullptr;
    }
    if (!ir_json) {
        set_error(error_msg, "flux_compile_json: ir_json is null");
        return nullptr;
    }

    auto* k = new flux_kernel();
    k->raw_ir = ir_json;

    if (k->raw_ir.find("Damage") != std::string::npos ||
        k->raw_ir.find("damage") != std::string::npos) {
        k->kind = json_has_strategy_compacted(k->raw_ir)
            ? kernel_kind::damage_compacted
            : kernel_kind::damage_masked;
    } else if (k->raw_ir.find("Integrate") != std::string::npos ||
               k->raw_ir.find("integrate") != std::string::npos) {
        k->kind = kernel_kind::integrate;
    } else {
        k->kind = kernel_kind::passthrough;
    }

    k->dt = parse_float_key(ir_json, "dt", 1.0F);
    k->damage = parse_float_key(ir_json, "damage", 1.0F);
    k->work_factor = parse_int_key(ir_json, "work_factor", 0);
    return k;
}

extern "C" void flux_kernel_destroy(flux_kernel* k) {
    delete k;
}

extern "C" int flux_run_f32(
    flux_kernel* k,
    int64_t n,
    const float* in0,
    const float* in1,
    float* out0,
    float* out1,
    char** error_msg
) {
    if (error_msg) {
        *error_msg = nullptr;
    }

    if (!k) {
        set_error(error_msg, "flux_run_f32: kernel is null");
        return -1;
    }
    if (n < 0) {
        set_error(error_msg, "flux_run_f32: n must be >= 0");
        return -2;
    }
    if (!in0 || !out0) {
        set_error(error_msg, "flux_run_f32: in0 and out0 are required");
        return -3;
    }

    if (k->kind == kernel_kind::integrate) {
        if (!in1) {
            set_error(error_msg, "flux_run_f32: integrate kernel requires in1");
            return -4;
        }
        for (int64_t i = 0; i < n; ++i) {
            out0[i] = in0[i] + in1[i] * k->dt;
            if (out1) {
                out1[i] = in1[i];
            }
        }
        return 0;
    }

    if (k->kind == kernel_kind::damage_masked) {
        // "Masked" path: compute candidate then select.
        for (int64_t i = 0; i < n; ++i) {
            const float v = in0[i];
            const bool alive = v > 0.0F;
            const float payload = heavy_payload(v, k->work_factor);
            const float decayed = payload - k->damage;
            out0[i] = alive ? decayed : v;
            if (out1) {
                out1[i] = alive ? 1.0F : 0.0F;
            }
        }
        return 0;
    }

    if (k->kind == kernel_kind::damage_compacted) {
        // "Compacted" path: classify alive lanes, then process dense index set.
        std::vector<int64_t> alive_idx;
        alive_idx.reserve(static_cast<size_t>(n));

        for (int64_t i = 0; i < n; ++i) {
            out0[i] = in0[i];
            if (out1) {
                out1[i] = 0.0F;
            }
            if (in0[i] > 0.0F) {
                alive_idx.push_back(i);
            }
        }

        for (const int64_t idx : alive_idx) {
            const float payload = heavy_payload(in0[idx], k->work_factor);
            out0[idx] = payload - k->damage;
            if (out1) {
                out1[idx] = 1.0F;
            }
        }
        return 0;
    }

    for (int64_t i = 0; i < n; ++i) {
        out0[i] = in0[i];
        if (out1) {
            out1[i] = in1 ? in1[i] : 0.0F;
        }
    }
    return 0;
}

extern "C" void flux_free_string(char* s) {
    std::free(s);
}
