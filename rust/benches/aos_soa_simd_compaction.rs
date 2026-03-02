use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion};

#[derive(Clone)]
struct PlayerAoS {
    id: u32,
    x: f32,
    y: f32,
    z: f32,
    health: f32,
}

struct PlayersSoA {
    ids: Vec<u32>,
    x: Vec<f32>,
    y: Vec<f32>,
    z: Vec<f32>,
    health: Vec<f32>,
}

fn make_aos(n: usize) -> Vec<PlayerAoS> {
    (0..n)
        .map(|i| PlayerAoS {
            id: i as u32,
            x: 1.0,
            y: 2.0,
            z: 3.0,
            health: 100.0,
        })
        .collect()
}

fn make_soa(n: usize) -> PlayersSoA {
    PlayersSoA {
        ids: (0..n as u32).collect(),
        x: vec![1.0; n],
        y: vec![2.0; n],
        z: vec![3.0; n],
        health: vec![100.0; n],
    }
}

fn update_health_scalar(health: &mut [f32], damage: f32) {
    for h in health {
        *h = (*h - damage).max(0.0);
    }
}

fn update_health_unseq_like(health: &mut [f32], damage: f32) {
    // Rust stable has no direct std::execution::unseq equivalent;
    // this is the baseline contiguous loop that LLVM can auto-vectorize.
    health.iter_mut().for_each(|h| {
        *h = (*h - damage).max(0.0);
    });
}

fn update_branchy_scalar(health: &mut [f32], threshold: f32, damage: f32, regen: f32) {
    for h in health {
        *h = if *h > threshold { *h - damage } else { *h + regen };
    }
}

fn update_triple_scalar(health: &mut [f32], regen: f32, decay: f32) {
    for h in health {
        if *h > 100.0 {
            *h = 100.0;
        } else if *h < 20.0 {
            *h += regen;
        } else {
            *h -= decay;
        }
    }
}

fn heavy_work(mut v: f32, iterations: usize) -> f32 {
    for _ in 0..iterations {
        v = v.sin() + v.cos();
    }
    v
}

fn bench_aos_vs_soa(c: &mut Criterion) {
    let mut group = c.benchmark_group("aos_soa_health_update");
    for &n in &[1 << 18, 1 << 20] {
        group.bench_with_input(BenchmarkId::new("AoS", n), &n, |b, &n| {
            let mut players = make_aos(n);
            b.iter(|| {
                for p in &mut players {
                    p.health = (p.health - 1.0).max(0.0);
                }
                black_box(&players);
            });
        });

        group.bench_with_input(BenchmarkId::new("SoA_scalar", n), &n, |b, &n| {
            let mut p = make_soa(n);
            b.iter(|| {
                update_health_scalar(&mut p.health, 1.0);
                black_box(&p.health);
            });
        });

        group.bench_with_input(BenchmarkId::new("SoA_unseq_like", n), &n, |b, &n| {
            let mut p = make_soa(n);
            b.iter(|| {
                update_health_unseq_like(&mut p.health, 1.0);
                black_box(&p.health);
            });
        });
    }
    group.finish();
}

fn bench_branchy(c: &mut Criterion) {
    let mut group = c.benchmark_group("branchy_scalar");
    let n = 1 << 20;

    group.bench_function("if_else", |b| {
        let mut p = make_soa(n);
        for (i, h) in p.health.iter_mut().enumerate() {
            *h = (i % 151) as f32;
        }
        b.iter(|| {
            update_branchy_scalar(&mut p.health, 50.0, 1.0, 0.5);
            black_box(&p.health);
        });
    });

    group.bench_function("if_else_if_else", |b| {
        let mut p = make_soa(n);
        for (i, h) in p.health.iter_mut().enumerate() {
            *h = (i % 161) as f32;
        }
        b.iter(|| {
            update_triple_scalar(&mut p.health, 0.8, 1.2);
            black_box(&p.health);
        });
    });

    group.finish();
}

fn bench_compaction_crossover(c: &mut Criterion) {
    let mut group = c.benchmark_group("compaction_crossover");
    let n = 1 << 18;

    for &work in &[5_usize, 20, 80] {
        group.bench_with_input(BenchmarkId::new("masked_like_compute_both", work), &work, |b, &work| {
            let mut data: Vec<f32> = (0..n).map(|i| i as f32).collect();
            let cond: Vec<bool> = (0..n).map(|i| i % 2 == 1).collect();
            b.iter(|| {
                for i in 0..n {
                    let expensive = heavy_work(data[i], work);
                    let cheap = data[i] * 0.5;
                    data[i] = if cond[i] { expensive } else { cheap };
                }
                black_box(&data);
            });
        });

        group.bench_with_input(BenchmarkId::new("partition_then_process", work), &work, |b, &work| {
            let mut data: Vec<f32> = (0..n).map(|i| i as f32).collect();
            b.iter(|| {
                data.iter_mut().enumerate().for_each(|(i, x)| *x = i as f32);
                let mid = data.partition_point(|v| (*v as i32) % 2 != 0);

                // `partition_point` requires sorted-by-predicate input; since this isn't sorted,
                // emulate partitioning with a stable, explicit pass.
                let mut odd = Vec::with_capacity(n / 2 + 1);
                let mut even = Vec::with_capacity(n / 2 + 1);
                for &v in &data {
                    if (v as i32) % 2 != 0 {
                        odd.push(v);
                    } else {
                        even.push(v);
                    }
                }

                for v in &mut odd {
                    *v = heavy_work(*v, work);
                }
                for v in &mut even {
                    *v *= 0.5;
                }

                data.clear();
                data.extend_from_slice(&odd);
                data.extend_from_slice(&even);

                black_box(mid);
                black_box(&data);
            });
        });
    }

    group.finish();
}

criterion_group!(
    benches,
    bench_aos_vs_soa,
    bench_branchy,
    bench_compaction_crossover
);
criterion_main!(benches);
