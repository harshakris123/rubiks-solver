// Scratch correctness check for Coord.cpp — not part of the production build.
#include "Coord.h"
#include "Cube.h"
#include "Move.h"
#include <iostream>
#include <random>

static bool sameState(const coord::CubieState& a, const coord::CubieState& b) {
    return a.cornerPerm == b.cornerPerm && a.cornerOri == b.cornerOri &&
           a.edgePerm == b.edgePerm && a.edgeOri == b.edgeOri;
}

int main() {
    const auto& moves = Move::getAllMoves();
    std::mt19937 rng(12345);
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);

    int mismatches = 0;
    const int trials = 200;
    const int seqLen = 30;

    for (int trial = 0; trial < trials; trial++) {
        Cube cube;
        coord::CubieState state = coord::extract(cube);

        for (int step = 0; step < seqLen; step++) {
            int m = static_cast<int>(dist(rng));
            cube.applyMove(moves[m]);
            state = coord::applyMove(state, m);

            coord::CubieState groundTruth = coord::extract(cube);
            if (!sameState(state, groundTruth)) {
                std::cerr << "MISMATCH at trial " << trial << " step " << step
                          << " move " << moves[m].getName() << "\n";
                mismatches++;
            }
        }
    }

    if (mismatches == 0) {
        std::cout << "OK: " << trials << " trials x " << seqLen
                  << " random moves, coord::applyMove matches full sticker extraction.\n";
    } else {
        std::cout << mismatches << " mismatches found.\n";
        return 1;
    }

    // Sanity: solved cube extracts to identity.
    Cube solved;
    coord::CubieState s = coord::extract(solved);
    if (!s.isSolved()) {
        std::cout << "FAIL: solved cube did not extract to identity coord state.\n";
        return 1;
    }
    std::cout << "OK: solved cube extracts to identity.\n";

    // Sanity: applying a move then its inverse returns to identity.
    for (const auto& mv : moves) {
        Cube c;
        c.applyMove(mv);
        // find inverse move object
        std::string invName = Move::inverseName(mv.getName());
        const Move* inv = nullptr;
        for (const auto& m2 : moves) if (m2.getName() == invName) inv = &m2;
        c.applyMove(*inv);
        if (!c.isSolved()) {
            std::cout << "FAIL: " << mv.getName() << " then " << invName << " didn't return to solved.\n";
            return 1;
        }
    }
    std::cout << "OK: every move composed with its inverse returns to solved.\n";

    return 0;
}
