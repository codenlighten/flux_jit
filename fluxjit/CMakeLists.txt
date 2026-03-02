cmake_minimum_required(VERSION 3.16)
project(fluxjit CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fluxjit SHARED src/fluxjit.cpp)
target_include_directories(fluxjit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(fluxjit PRIVATE -O3 -march=native -fPIC)
elseif (MSVC)
  target_compile_options(fluxjit PRIVATE /O2)
endif()
