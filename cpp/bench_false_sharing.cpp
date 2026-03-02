#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <thread>

namespace {

constexpr int kMaxThreads = 16;
constexpr std::uint64_t kInnerWork = 65536;

struct Counter {
    alignas(8) volatile std::uint64_t value = 0;
};

struct PaddedCounter {
    alignas(64) volatile std::uint64_t value = 0;
    std::array<std::byte, 64 - sizeof(std::uint64_t)> pad{};
};

static Counter g_false_shared[kMaxThreads]{};
static PaddedCounter g_padded[kMaxThreads]{};

}  // namespace

static void BM_FalseSharing(benchmark::State& state) {
    const int tid = state.thread_index();
    benchmark::DoNotOptimize(g_false_shared[tid].value);

    for (auto _ : state) {
        for (std::uint64_t i = 0; i < kInnerWork; ++i) {
            g_false_shared[tid].value++;
        }
        benchmark::ClobberMemory();
    }

    state.counters["updates"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(kInnerWork),
        benchmark::Counter::kIsRate);
}

static void BM_PaddedNoFalseSharing(benchmark::State& state) {
    const int tid = state.thread_index();
    benchmark::DoNotOptimize(g_padded[tid].value);

    for (auto _ : state) {
        for (std::uint64_t i = 0; i < kInnerWork; ++i) {
            g_padded[tid].value++;
        }
        benchmark::ClobberMemory();
    }

    state.counters["updates"] = benchmark::Counter(
        static_cast<double>(state.iterations()) * static_cast<double>(kInnerWork),
        benchmark::Counter::kIsRate);
}

BENCHMARK(BM_FalseSharing)
    ->ThreadRange(1, std::min(12u, std::thread::hardware_concurrency()))
    ->UseRealTime();

BENCHMARK(BM_PaddedNoFalseSharing)
    ->ThreadRange(1, std::min(12u, std::thread::hardware_concurrency()))
    ->UseRealTime();

BENCHMARK_MAIN();
