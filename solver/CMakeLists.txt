cmake_minimum_required(VERSION 3.10)
project(RubiksSolver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# MinGW systems can end up with more than one libstdc++/libgcc DLL on PATH
# (e.g. multiple MSYS2 installs), which causes real, reproducible segfaults
# at -O2 when std::ifstream/std::vector calls cross a DLL boundary with a
# mismatched runtime. Statically linking the runtime sidesteps that
# entirely. Harmless on other platforms, so just always do it.
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()

add_library(cube_core STATIC
    Cube.cpp
    Move.cpp
    Coord.cpp
    PatternDB.cpp
    Solver.cpp
)
target_include_directories(cube_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(solver_cli main.cpp)
target_link_libraries(solver_cli PRIVATE cube_core)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE cube_core)

# Build-time tool: generates corner_pdb.dat / edge_pdb.dat that Solver loads
# at runtime. Not needed by solver_cli/benchmark directly, just run once.
add_executable(pdb_gen pdb_gen.cpp)
target_link_libraries(pdb_gen PRIVATE cube_core)

# Regression tests for the coordinate layer / pattern-database indexing math
# (see Coord.cpp, PatternDB.cpp) — these caught real bugs during development
# (a chirality bug in corner orientation, an off-by-one in the Lehmer-code
# rank) and are cheap enough to keep running. test_pdb takes a few minutes
# since it builds both full pattern databases from scratch.
add_executable(test_combinatorics test_combinatorics.cpp)

add_executable(test_coord test_coord.cpp)
target_link_libraries(test_coord PRIVATE cube_core)

add_executable(test_pdb test_pdb.cpp)
target_link_libraries(test_pdb PRIVATE cube_core)
