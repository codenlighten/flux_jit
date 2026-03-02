cmake_minimum_required(VERSION 3.16)
project(cpu_data_oriented_bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(benchmark REQUIRED)

add_executable(cpu_benchmarks bench_aos_soa_simd_compaction.cpp)
target_link_libraries(cpu_benchmarks PRIVATE benchmark::benchmark)

add_executable(ecs_benchmarks bench_ecs_vs_oop.cpp)
target_link_libraries(ecs_benchmarks PRIVATE benchmark::benchmark)

add_executable(false_sharing_benchmarks bench_false_sharing.cpp)
target_link_libraries(false_sharing_benchmarks PRIVATE benchmark::benchmark)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(cpu_benchmarks PRIVATE -O3 -march=native -ffast-math)
  target_compile_options(ecs_benchmarks PRIVATE -O3 -march=native -ffast-math)
  target_compile_options(false_sharing_benchmarks PRIVATE -O3 -march=native -ffast-math)
elseif (MSVC)
  target_compile_options(cpu_benchmarks PRIVATE /O2)
  target_compile_options(ecs_benchmarks PRIVATE /O2)
  target_compile_options(false_sharing_benchmarks PRIVATE /O2)
endif()
