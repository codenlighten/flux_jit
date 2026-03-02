#include <algorithm>
#include <benchmark/benchmark.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <execution>
#include <immintrin.h>
#include <numeric>
#include <random>
#include <vector>

struct PlayerAoS {
    std::uint32_t id;
    float x;
    float y;
    float z;
    float health;
};

struct PlayersSoA {
    std::vector<std::uint32_t> ids;
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
    std::vector<float> health;
};

static std::vector<PlayerAoS> make_aos(std::size_t n) {
    std::vector<PlayerAoS> players(n);
    for (std::size_t i = 0; i < n; ++i) {
        players[i] = PlayerAoS{static_cast<std::uint32_t>(i), 1.0f, 2.0f, 3.0f, 100.0f};
    }
    return players;
}

static PlayersSoA make_soa(std::size_t n) {
    PlayersSoA p;
    p.ids.resize(n);
    p.x.resize(n, 1.0f);
    p.y.resize(n, 2.0f);
    p.z.resize(n, 3.0f);
    p.health.resize(n, 100.0f);
    std::iota(p.ids.begin(), p.ids.end(), 0u);
    return p;
}

static inline void update_health_scalar(float* health, std::size_t n, float damage) {
    for (std::size_t i = 0; i < n; ++i) {
        health[i] = std::max(0.0f, health[i] - damage);
    }
}

static inline void update_health_unseq(std::vector<float>& health, float damage) {
    std::for_each(std::execution::unseq, health.begin(), health.end(), [damage](float& h) {
        h = std::max(0.0f, h - damage);
    });
}

#ifdef __AVX2__
static inline void update_health_avx2(float* health, std::size_t n, float damage) {
    const __m256 v_damage = _mm256_set1_ps(damage);
    const __m256 v_zero = _mm256_setzero_ps();

    std::size_t i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 vh = _mm256_loadu_ps(health + i);
        vh = _mm256_sub_ps(vh, v_damage);
        vh = _mm256_max_ps(vh, v_zero);
        _mm256_storeu_ps(health + i, vh);
    }
    for (; i < n; ++i) {
        health[i] = std::max(0.0f, health[i] - damage);
    }
}

static inline void update_branchy_avx2(float* health, std::size_t n, float threshold, float damage, float regen) {
    const __m256 v_threshold = _mm256_set1_ps(threshold);
    const __m256 v_damage = _mm256_set1_ps(damage);
    const __m256 v_regen = _mm256_set1_ps(regen);

    std::size_t i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 vh = _mm256_loadu_ps(health + i);
        __m256 mask = _mm256_cmp_ps(vh, v_threshold, _CMP_GT_OQ);

        __m256 minus = _mm256_sub_ps(vh, v_damage);
        __m256 plus = _mm256_add_ps(vh, v_regen);
        __m256 out = _mm256_blendv_ps(plus, minus, mask);

        _mm256_storeu_ps(health + i, out);
    }

    for (; i < n; ++i) {
        health[i] = (health[i] > threshold) ? (health[i] - damage) : (health[i] + regen);
    }
}

static inline void update_triple_avx2(float* health, std::size_t n, float regen, float decay) {
    const __m256 v_max = _mm256_set1_ps(100.0f);
    const __m256 v_low = _mm256_set1_ps(20.0f);
    const __m256 v_regen = _mm256_set1_ps(regen);
    const __m256 v_decay = _mm256_set1_ps(decay);

    std::size_t i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 vh = _mm256_loadu_ps(health + i);
        __m256 res_cap = v_max;
        __m256 res_regen = _mm256_add_ps(vh, v_regen);
        __m256 res_decay = _mm256_sub_ps(vh, v_decay);

        __m256 mask_high = _mm256_cmp_ps(vh, v_max, _CMP_GT_OQ);
        __m256 mask_low = _mm256_cmp_ps(vh, v_low, _CMP_LT_OQ);

        __m256 out = _mm256_blendv_ps(res_decay, res_regen, mask_low);
        out = _mm256_blendv_ps(out, res_cap, mask_high);

        _mm256_storeu_ps(health + i, out);
    }
    for (; i < n; ++i) {
        if (health[i] > 100.0f) {
            health[i] = 100.0f;
        } else if (health[i] < 20.0f) {
            health[i] += regen;
        } else {
            health[i] -= decay;
        }
    }
}
#endif

static inline void update_branchy_scalar(float* health, std::size_t n, float threshold, float damage, float regen) {
    for (std::size_t i = 0; i < n; ++i) {
        health[i] = (health[i] > threshold) ? (health[i] - damage) : (health[i] + regen);
    }
}

static inline void update_triple_scalar(float* health, std::size_t n, float regen, float decay) {
    for (std::size_t i = 0; i < n; ++i) {
        if (health[i] > 100.0f) {
            health[i] = 100.0f;
        } else if (health[i] < 20.0f) {
            health[i] += regen;
        } else {
            health[i] -= decay;
        }
    }
}

static inline float heavy_work(float v, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        v = std::sin(v) + std::cos(v);
    }
    return v;
}

static void BM_AoS_Health(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_aos(n);

    for (auto _ : state) {
        for (auto& p : players) {
            p.health = std::max(0.0f, p.health - 1.0f);
        }
        benchmark::DoNotOptimize(players.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

static void BM_SoA_Scalar(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_soa(n);

    for (auto _ : state) {
        update_health_scalar(players.health.data(), n, 1.0f);
        benchmark::DoNotOptimize(players.health.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

static void BM_SoA_Unseq(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_soa(n);

    for (auto _ : state) {
        update_health_unseq(players.health, 1.0f);
        benchmark::DoNotOptimize(players.health.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

#ifdef __AVX2__
static void BM_SoA_AVX2(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_soa(n);

    for (auto _ : state) {
        update_health_avx2(players.health.data(), n, 1.0f);
        benchmark::DoNotOptimize(players.health.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
#endif

static void BM_Branchy_Scalar(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_soa(n);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 150.0f);
    for (auto& h : players.health) {
        h = dist(rng);
    }

    for (auto _ : state) {
        update_branchy_scalar(players.health.data(), n, 50.0f, 1.0f, 0.5f);
        benchmark::DoNotOptimize(players.health.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

#ifdef __AVX2__
static void BM_Branchy_AVX2(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_soa(n);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 150.0f);
    for (auto& h : players.health) {
        h = dist(rng);
    }

    for (auto _ : state) {
        update_branchy_avx2(players.health.data(), n, 50.0f, 1.0f, 0.5f);
        benchmark::DoNotOptimize(players.health.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
#endif

static void BM_Triple_Scalar(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_soa(n);

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(0.0f, 160.0f);
    for (auto& h : players.health) {
        h = dist(rng);
    }

    for (auto _ : state) {
        update_triple_scalar(players.health.data(), n, 0.8f, 1.2f);
        benchmark::DoNotOptimize(players.health.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

#ifdef __AVX2__
static void BM_Triple_AVX2(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    auto players = make_soa(n);

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(0.0f, 160.0f);
    for (auto& h : players.health) {
        h = dist(rng);
    }

    for (auto _ : state) {
        update_triple_avx2(players.health.data(), n, 0.8f, 1.2f);
        benchmark::DoNotOptimize(players.health.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}
#endif

static void BM_MaskedLike_ComputeBoth(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    const int work_factor = static_cast<int>(state.range(1));

    std::vector<float> data(n);
    std::iota(data.begin(), data.end(), 0.0f);
    std::vector<std::uint8_t> cond(n);
    for (std::size_t i = 0; i < n; ++i) {
        cond[i] = static_cast<std::uint8_t>(i & 1U);
    }

    for (auto _ : state) {
        for (std::size_t i = 0; i < n; ++i) {
            float expensive = heavy_work(data[i], work_factor);
            float cheap = data[i] * 0.5f;
            data[i] = cond[i] ? expensive : cheap;
        }
        benchmark::DoNotOptimize(data.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

static void BM_StreamCompaction_Partition(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    const int work_factor = static_cast<int>(state.range(1));

    std::vector<float> data(n);
    std::iota(data.begin(), data.end(), 0.0f);

    for (auto _ : state) {
        std::iota(data.begin(), data.end(), 0.0f);

        auto mid = std::partition(data.begin(), data.end(), [](float v) {
            return (static_cast<int>(v) & 1) != 0;
        });

        for (auto it = data.begin(); it != mid; ++it) {
            *it = heavy_work(*it, work_factor);
        }
        for (auto it = mid; it != data.end(); ++it) {
            *it *= 0.5f;
        }
        benchmark::DoNotOptimize(data.data());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_AoS_Health)->Arg(1 << 20);
BENCHMARK(BM_SoA_Scalar)->Arg(1 << 20);
BENCHMARK(BM_SoA_Unseq)->Arg(1 << 20);
#ifdef __AVX2__
BENCHMARK(BM_SoA_AVX2)->Arg(1 << 20);
#endif

BENCHMARK(BM_Branchy_Scalar)->Arg(1 << 20);
#ifdef __AVX2__
BENCHMARK(BM_Branchy_AVX2)->Arg(1 << 20);
#endif

BENCHMARK(BM_Triple_Scalar)->Arg(1 << 20);
#ifdef __AVX2__
BENCHMARK(BM_Triple_AVX2)->Arg(1 << 20);
#endif

BENCHMARK(BM_MaskedLike_ComputeBoth)->Args({1 << 18, 5})->Args({1 << 18, 20})->Args({1 << 18, 80});
BENCHMARK(BM_StreamCompaction_Partition)->Args({1 << 18, 5})->Args({1 << 18, 20})->Args({1 << 18, 80});

BENCHMARK_MAIN();
