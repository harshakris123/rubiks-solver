#include "Cube.h"
#include "Move.h"
#include "Solver.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string jsonStringArray(const std::vector<std::string>& items) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < items.size(); i++) {
        if (i > 0) out << ", ";
        out << "\"" << items[i] << "\"";
    }
    out << "]";
    return out.str();
}

const std::map<std::string, const Move*>& moveLookup() {
    static const std::map<std::string, const Move*> lookup = [] {
        std::map<std::string, const Move*> m;
        for (const auto& mv : Move::getAllMoves()) m[mv.getName()] = &mv;
        return m;
    }();
    return lookup;
}

// Reads a self-test: verify every move permutation is a bijection on 0..53,
// and that applying a single-turn move 4 times returns to the solved state.
bool selfTestPermutations() {
    bool ok = true;
    for (const auto& mv : Move::getAllMoves()) {
        std::array<bool, 54> seen{};
        for (int v : mv.getPermutation()) {
            if (v < 0 || v >= 54 || seen[v]) {
                std::cerr << "Move " << mv.getName() << " has an invalid permutation!\n";
                ok = false;
                break;
            }
            seen[v] = true;
        }
    }

    // U applied 4 times must return to solved.
    Cube c;
    const Move* u = moveLookup().at("U");
    for (int i = 0; i < 4; i++) c.applyMove(*u);
    if (!c.isSolved()) {
        std::cerr << "U applied 4 times did not return to solved state!\n";
        ok = false;
    }

    // R then R' must return to solved.
    Cube c2;
    c2.applyMove(*moveLookup().at("R"));
    c2.applyMove(*moveLookup().at("R'"));
    if (!c2.isSolved()) {
        std::cerr << "R then R' did not return to solved state!\n";
        ok = false;
    }

    return ok;
}

void runTestMode(const std::vector<std::string>& scrambleMoves) {
    if (!selfTestPermutations()) {
        std::cerr << "Self-test FAILED.\n";
        std::exit(1);
    }
    std::cout << "Self-test passed: all 18 move permutations are valid bijections.\n\n";

    Cube cube;
    std::cout << "Scrambling with: ";
    for (const auto& name : scrambleMoves) std::cout << name << " ";
    std::cout << "\n\n";

    for (const auto& name : scrambleMoves) {
        cube.applyMove(*moveLookup().at(name));
    }

    std::cout << "Scrambled state:\n";
    cube.print();

    Solver solver;
    Solver::Result result = solver.solve(cube);

    if (!result.found) {
        std::cout << "\nNo solution found within depth limit.\n";
        return;
    }

    std::cout << "\nSolved in " << result.moves.size() << " moves ("
              << result.solveTimeMs << " ms):\n";
    for (const auto& m : result.moves) std::cout << m << " ";
    std::cout << "\n";
}

void runStdinMode() {
    std::array<int, 54> values{};
    for (int i = 0; i < 54; i++) {
        if (!(std::cin >> values[i])) {
            std::cout << "{\"error\": \"expected 54 integers (0-5) on stdin, got fewer\"}\n";
            std::exit(1);
        }
        if (values[i] < 0 || values[i] > 5) {
            std::cout << "{\"error\": \"sticker value out of range 0-5 at index " << i << "\"}\n";
            std::exit(1);
        }
    }

    Cube cube;
    cube.stickers = values;

    Solver solver;
    Solver::Result result = solver.solve(cube);

    if (!result.found) {
        std::cout << "{\"error\": \"no solution found within search depth limit\"}\n";
        std::exit(1);
    }

    std::vector<std::string> descriptions;
    descriptions.reserve(result.moves.size());
    for (const auto& name : result.moves) {
        descriptions.push_back(moveLookup().at(name)->getDescription());
    }

    std::cout << "{"
              << "\"moves\": " << jsonStringArray(result.moves) << ", "
              << "\"descriptions\": " << jsonStringArray(descriptions) << ", "
              << "\"solve_time_ms\": " << result.solveTimeMs << ", "
              << "\"total_moves\": " << result.moves.size()
              << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--test") {
        std::vector<std::string> scramble;
        if (argc > 2) {
            std::istringstream iss(argv[2]);
            std::string tok;
            while (iss >> tok) scramble.push_back(tok);
        } else {
            scramble = { "R", "U", "F'", "L2", "D'" };
        }
        runTestMode(scramble);
        return 0;
    }

    runStdinMode();
    return 0;
}
