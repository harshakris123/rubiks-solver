#pragma once
#include "Cube.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

class Solver {
public:
    struct Result {
        std::vector<std::string> moves; // empty if already solved
        double solveTimeMs = 0.0;
        bool found = false;
        bool optimal = false; // true whenever found — kept for API stability
    };

    // Loads the corner/edge pattern databases (corner_pdb.dat, edge0_pdb.dat,
    // edge1_pdb.dat) from `dataDir`. These are generated once, at build
    // time, by pdb_gen — see PatternDB.h — and must be present for the
    // solver to run at all. Throws std::runtime_error if any file is
    // missing or short.
    explicit Solver(const std::string& dataDir);

    // IDA* search using max(corner, edge0, edge1) pattern-database distances
    // as an admissible heuristic — Korf's classic optimal solver. Always
    // finds the *shortest* solution if it finishes, but like any optimal
    // IDA* search has no bounded worst-case time: some scrambles need an
    // iteration whose node count dwarfs every iteration before it (see
    // solver/interview.md for measurements and why a "make it always fast"
    // fallback was deliberately not added). maxDepth bounds the search;
    // God's number is 20 in the half-turn metric. `timeBudgetMs` is
    // currently unused, kept so the backend's timeout-aware call site
    // doesn't need to change if a bounded fallback is added later.
    Result solve(const Cube& scrambled, int maxDepth = 21, int timeBudgetMs = 8000) const;

private:
    std::vector<uint8_t> cornerDB_;
    std::vector<uint8_t> edgeDB0_;
    std::vector<uint8_t> edgeDB1_;
};
