cmake_minimum_required(VERSION 3.10)
project(RubiksSolver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_library(cube_core STATIC
    Cube.cpp
    Move.cpp
    Solver.cpp
)
target_include_directories(cube_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(solver_cli main.cpp)
target_link_libraries(solver_cli PRIVATE cube_core)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE cube_core)
