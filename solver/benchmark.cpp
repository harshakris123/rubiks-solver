#include "Cube.h"
#include "Move.h"
#include "PdbDir.h"
#include "Solver.h"
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

// Scrambles a solved cube with `n` uniformly random moves and returns it.
static Cube scrambleCube(int n, std::mt19937& rng) {
    Cube cube;
    const auto& moves = Move::getAllMoves();
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    for (int i = 0; i < n; i++) {
        cube.applyMove(moves[dist(rng)]);
    }
    return cube;
}

int main() {
    std::mt19937 rng(std::random_device{}());
    Solver solver(resolvePdbDir());

    std::cout << "Rubik's Cube Solver Benchmark (IDA* + corner/edge pattern databases)\n";
    std::cout << "======================================================================\n";
    std::cout << std::left
              << std::setw(18) << "Scramble Depth"
              << std::setw(20) << "Solve Time (ms)"
              << std::setw(20) << "Moves in Solution"
              << "\n";
    std::cout << std::string(58, '-') << "\n";

    // Beyond ~16, a single random scramble's solve time varies hugely (see
    // solver/interview.md: optimal IDA* has no bounded worst case), so this
    // loop stops at 18 to keep the benchmark itself fast and reproducible.
    for (int depth = 1; depth <= 18; depth++) {
        Cube scrambled = scrambleCube(depth, rng);
        Solver::Result result = solver.solve(scrambled);

        std::cout << std::left << std::setw(18) << depth;
        if (result.found) {
            std::cout << std::setw(20) << std::fixed << std::setprecision(3) << result.solveTimeMs
                      << std::setw(20) << result.moves.size() << "\n";
        } else {
            std::cout << std::setw(20) << "N/A" << std::setw(20) << "NOT FOUND" << "\n";
        }
        std::cout.flush();
    }

    std::cout << "\nDone.\n";
    return 0;
}
