use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};
use std::ops::{Deref, DerefMut};

// ============================================================================
// Traditional OOP Approach: Trait-based Entity
// ============================================================================

trait Entity {
    fn update(&mut self);
}

struct MovingEntity {
    x: f32,
    y: f32,
    dx: f32,
    dy: f32,
    health: f32,
}

impl Entity for MovingEntity {
    fn update(&mut self) {
        self.x += self.dx;
        self.y += self.dy;
        self.health = (self.health - 0.1).max(0.0);
    }
}

struct StaticEntity {
    x: f32,
    y: f32,
    health: f32,
}

impl Entity for StaticEntity {
    fn update(&mut self) {
        self.health = (self.health - 0.05).max(0.0);
    }
}

// ============================================================================
// ECS Approach: Component Storage
// ============================================================================

#[derive(Clone, Copy)]
struct Position {
    x: f32,
    y: f32,
}

#[derive(Clone, Copy)]
struct Velocity {
    dx: f32,
    dy: f32,
}

#[derive(Clone, Copy)]
struct Health {
    value: f32,
}

struct ComponentManager {
    positions: Vec<Position>,
    velocities: Vec<Velocity>,
    healths: Vec<Health>,
    static_positions: Vec<Position>,
    static_healths: Vec<Health>,
}

impl ComponentManager {
    fn new() -> Self {
        Self {
            positions: Vec::new(),
            velocities: Vec::new(),
            healths: Vec::new(),
            static_positions: Vec::new(),
            static_healths: Vec::new(),
        }
    }

    fn add_moving_entity(&mut self, p: Position, v: Velocity, h: Health) {
        self.positions.push(p);
        self.velocities.push(v);
        self.healths.push(h);
    }

    fn add_static_entity(&mut self, p: Position, h: Health) {
        self.static_positions.push(p);
        self.static_healths.push(h);
    }
}

fn movement_system(cm: &mut ComponentManager) {
    for i in 0..cm.positions.len() {
        cm.positions[i].x += cm.velocities[i].dx;
        cm.positions[i].y += cm.velocities[i].dy;
    }
}

fn health_decay_system(healths: &mut [Health]) {
    for h in healths {
        h.value = (h.value - 0.1).max(0.0);
    }
}

fn static_health_decay_system(healths: &mut [Health]) {
    for h in healths {
        h.value = (h.value - 0.05).max(0.0);
    }
}

// ============================================================================
// Benchmarks
// ============================================================================

fn bench_oop_vs_ecs(c: &mut Criterion) {
    let mut group = c.benchmark_group("ecs_vs_oop");

    for &n in &[1 << 16, 1 << 18, 1 << 20] {
        group.bench_with_input(BenchmarkId::new("OOP_virtual", n), &n, |b, &n| {
            let mut entities: Vec<Box<dyn Entity>> = Vec::with_capacity(n);
            
            for i in 0..n {
                if i % 10 < 7 {
                    entities.push(Box::new(MovingEntity {
                        x: (i as f32) * 0.1,
                        y: (i as f32) * 0.2,
                        dx: 0.5,
                        dy: -0.3,
                        health: 100.0,
                    }));
                } else {
                    entities.push(Box::new(StaticEntity {
                        x: (i as f32) * 0.1,
                        y: (i as f32) * 0.2,
                        health: 100.0,
                    }));
                }
            }

            b.iter(|| {
                for e in &mut entities {
                    e.update();
                }
                black_box(&entities);
            });
        });

        group.bench_with_input(BenchmarkId::new("ECS_components", n), &n, |b, &n| {
            let mut cm = ComponentManager::new();
            let moving_count = (n * 7) / 10;
            let static_count = n - moving_count;

            for i in 0..moving_count {
                cm.add_moving_entity(
                    Position { x: (i as f32) * 0.1, y: (i as f32) * 0.2 },
                    Velocity { dx: 0.5, dy: -0.3 },
                    Health { value: 100.0 },
                );
            }

            for i in 0..static_count {
                cm.add_static_entity(
                    Position { x: (i as f32) * 0.1, y: (i as f32) * 0.2 },
                    Health { value: 100.0 },
                );
            }

            b.iter(|| {
                movement_system(&mut cm);
                health_decay_system(&mut cm.healths);
                static_health_decay_system(&mut cm.static_healths);
                black_box(&cm.positions);
                black_box(&cm.healths);
            });
        });
    }

    group.finish();
}

fn bench_pointer_chasing(c: &mut Criterion) {
    let mut group = c.benchmark_group("memory_access_pattern");

    for &n in &[1 << 16, 1 << 18] {
        group.bench_with_input(BenchmarkId::new("OOP_fragmented", n), &n, |b, &n| {
            let mut entities: Vec<Box<MovingEntity>> = Vec::with_capacity(n);
            
            for i in 0..n {
                entities.push(Box::new(MovingEntity {
                    x: (i as f32) * 0.1,
                    y: (i as f32) * 0.2,
                    dx: 0.5,
                    dy: -0.3,
                    health: 100.0,
                }));
            }

            b.iter(|| {
                for e in &mut entities {
                    e.update();
                }
                black_box(&entities);
            });
        });

        group.bench_with_input(BenchmarkId::new("ECS_linear", n), &n, |b, &n| {
            let mut cm = ComponentManager::new();

            for i in 0..n {
                cm.add_moving_entity(
                    Position { x: (i as f32) * 0.1, y: (i as f32) * 0.2 },
                    Velocity { dx: 0.5, dy: -0.3 },
                    Health { value: 100.0 },
                );
            }

            b.iter(|| {
                movement_system(&mut cm);
                health_decay_system(&mut cm.healths);
                black_box(&cm.positions);
            });
        });
    }

    group.finish();
}

criterion_group!(benches, bench_oop_vs_ecs, bench_pointer_chasing);
criterion_main!(benches);
