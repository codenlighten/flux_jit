#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <random>
#include <cmath>

// ============================================================================
// Traditional OOP Approach: Inheritance-based Entity
// ============================================================================

class Entity {
public:
    virtual void update() = 0;
    virtual ~Entity() = default;
};

class MovingEntity : public Entity {
public:
    float x, y;
    float dx, dy;
    float health;

    MovingEntity(float x_, float y_, float dx_, float dy_, float health_)
        : x(x_), y(y_), dx(dx_), dy(dy_), health(health_) {}

    void update() override {
        x += dx;
        y += dy;
        health = std::max(0.0f, health - 0.1f);
    }
};

class StaticEntity : public Entity {
public:
    float x, y;
    float health;

    StaticEntity(float x_, float y_, float health_)
        : x(x_), y(y_), health(health_) {}

    void update() override {
        health = std::max(0.0f, health - 0.05f);
    }
};

// ============================================================================
// ECS Approach: Component Storage
// ============================================================================

struct Position { float x, y; };
struct Velocity { float dx, dy; };
struct Health { float value; };

class ComponentManager {
public:
    std::vector<Position> positions;
    std::vector<Velocity> velocities;
    std::vector<Health> healths;

    void add_moving_entity(Position p, Velocity v, Health h) {
        positions.push_back(p);
        velocities.push_back(v);
        healths.push_back(h);
    }

    void add_static_entity(Position p, Health h) {
        static_positions.push_back(p);
        static_healths.push_back(h);
    }

    // Static entities (no velocity)
    std::vector<Position> static_positions;
    std::vector<Health> static_healths;
};

void MovementSystem(ComponentManager& cm) {
    for (size_t i = 0; i < cm.positions.size(); ++i) {
        cm.positions[i].x += cm.velocities[i].dx;
        cm.positions[i].y += cm.velocities[i].dy;
    }
}

void HealthDecaySystem(ComponentManager& cm) {
    for (auto& h : cm.healths) {
        h.value = std::max(0.0f, h.value - 0.1f);
    }
}

void StaticHealthDecaySystem(ComponentManager& cm) {
    for (auto& h : cm.static_healths) {
        h.value = std::max(0.0f, h.value - 0.05f);
    }
}

// ============================================================================
// Benchmarks
// ============================================================================

static void BM_OOP_VirtualUpdate(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    std::vector<std::unique_ptr<Entity>> entities;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(-1000.0f, 1000.0f);
    std::uniform_real_distribution<float> vel(-10.0f, 10.0f);
    std::uniform_real_distribution<float> hp(50.0f, 100.0f);

    // 70% moving entities, 30% static
    for (size_t i = 0; i < n; ++i) {
        if (i % 10 < 7) {
            entities.push_back(std::make_unique<MovingEntity>(
                pos(rng), pos(rng), vel(rng), vel(rng), hp(rng)
            ));
        } else {
            entities.push_back(std::make_unique<StaticEntity>(
                pos(rng), pos(rng), hp(rng)
            ));
        }
    }

    for (auto _ : state) {
        for (auto& e : entities) {
            e->update();
        }
        benchmark::DoNotOptimize(entities.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

static void BM_ECS_ComponentIteration(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    ComponentManager cm;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos(-1000.0f, 1000.0f);
    std::uniform_real_distribution<float> vel(-10.0f, 10.0f);
    std::uniform_real_distribution<float> hp(50.0f, 100.0f);

    size_t moving_count = (n * 7) / 10;
    size_t static_count = n - moving_count;

    for (size_t i = 0; i < moving_count; ++i) {
        cm.add_moving_entity(
            {pos(rng), pos(rng)},
            {vel(rng), vel(rng)},
            {hp(rng)}
        );
    }

    for (size_t i = 0; i < static_count; ++i) {
        cm.add_static_entity(
            {pos(rng), pos(rng)},
            {hp(rng)}
        );
    }

    for (auto _ : state) {
        MovementSystem(cm);
        HealthDecaySystem(cm);
        StaticHealthDecaySystem(cm);
        benchmark::DoNotOptimize(cm.positions.data());
        benchmark::DoNotOptimize(cm.healths.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

static void BM_OOP_PointerChasing(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    
    // Intentionally fragment memory by allocating in random order
    std::vector<std::unique_ptr<MovingEntity>> entities;
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    std::vector<size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);

    for (size_t i = 0; i < n; ++i) {
        entities.push_back(std::make_unique<MovingEntity>(
            dist(rng), dist(rng), dist(rng), dist(rng), 100.0f
        ));
    }

    // Access in shuffled order to maximize cache misses
    for (auto _ : state) {
        for (auto idx : indices) {
            entities[idx]->update();
        }
        benchmark::DoNotOptimize(entities.data());
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

static void BM_ECS_LinearScan(benchmark::State& state) {
    const size_t n = static_cast<size_t>(state.range(0));
    ComponentManager cm;

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    for (size_t i = 0; i < n; ++i) {
        cm.add_moving_entity(
            {dist(rng), dist(rng)},
            {dist(rng), dist(rng)},
            {100.0f}
        );
    }

    for (auto _ : state) {
        MovementSystem(cm);
        HealthDecaySystem(cm);
        benchmark::DoNotOptimize(cm.positions.data());
    }

    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n));
}

BENCHMARK(BM_OOP_VirtualUpdate)->Arg(1 << 16)->Arg(1 << 18)->Arg(1 << 20);
BENCHMARK(BM_ECS_ComponentIteration)->Arg(1 << 16)->Arg(1 << 18)->Arg(1 << 20);
BENCHMARK(BM_OOP_PointerChasing)->Arg(1 << 16)->Arg(1 << 18)->Arg(1 << 20);
BENCHMARK(BM_ECS_LinearScan)->Arg(1 << 16)->Arg(1 << 18)->Arg(1 << 20);

BENCHMARK_MAIN();
