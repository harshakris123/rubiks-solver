#include "Solver.h"
#include "Coord.h"
#include "Move.h"
#include "PatternDB.h"
#include <algorithm>
#include <chrono>

namespace {

int heuristic(const coord::CubieState& state, const std::vector<uint8_t>& cornerDB,
              const std::vector<uint8_t>& edgeDB0, const std::vector<uint8_t>& edgeDB1) {
    int cornerH = pdb::lookup(cornerDB, pdb::cornerIndex(state.cornerPerm, state.cornerOri));
    int edge0H = pdb::lookup(edgeDB0, pdb::edgeIndex(state, 0));
    int edge1H = pdb::lookup(edgeDB1, pdb::edgeIndex(state, 1));
    return std::max({cornerH, edge0H, edge1H});
}

// Depth-first search bounded by `bound` on f = g + h (standard IDA*). Returns
// true and leaves the solution in `pathMoves` if found within bound;
// otherwise records the smallest f that exceeded bound into `nextBound`, so
// the caller knows what threshold to retry with.
bool dfs(const coord::CubieState& state, int g, int bound, int lastFace, std::vector<int>& pathMoves,
         const std::vector<uint8_t>& cornerDB, const std::vector<uint8_t>& edgeDB0,
         const std::vector<uint8_t>& edgeDB1, const std::array<int, coord::NUM_MOVES>& moveFaces,
         int& nextBound) {
    if (state.isSolved()) return true;

    int h = heuristic(state, cornerDB, edgeDB0, edgeDB1);
    int f = g + h;
    if (f > bound) {
        if (nextBound == -1 || f < nextBound) nextBound = f;
        return false;
    }

    for (int m = 0; m < coord::NUM_MOVES; m++) {
        // Two consecutive same-face turns are never part of an optimal
        // solution (U U' cancels, U U = U2, U U2 = U', so any such pair
        // collapses to at most one move) — safe to skip, pure speedup.
        if (moveFaces[m] == lastFace) continue;
        coord::CubieState next = coord::applyMove(state, m);
        pathMoves.push_back(m);
        if (dfs(next, g + 1, bound, moveFaces[m], pathMoves, cornerDB, edgeDB0, edgeDB1, moveFaces,
                nextBound)) {
            return true;
        }
        pathMoves.pop_back();
    }
    return false;
}

} // namespace

Solver::Solver(const std::string& dataDir) {
    if (!pdb::load(cornerDB_, dataDir + "/corner_pdb.dat", pdb::CORNER_DB_SIZE)) {
        throw std::runtime_error(
            "Could not load corner pattern database from '" + dataDir +
            "/corner_pdb.dat'. Run pdb_gen first to generate it.");
    }
    if (!pdb::load(edgeDB0_, dataDir + "/edge0_pdb.dat", pdb::EDGE_DB_SIZE)) {
        throw std::runtime_error(
            "Could not load edge pattern database from '" + dataDir +
            "/edge0_pdb.dat'. Run pdb_gen first to generate it.");
    }
    if (!pdb::load(edgeDB1_, dataDir + "/edge1_pdb.dat", pdb::EDGE_DB_SIZE)) {
        throw std::runtime_error(
            "Could not load edge pattern database from '" + dataDir +
            "/edge1_pdb.dat'. Run pdb_gen first to generate it.");
    }
}

Solver::Result Solver::solve(const Cube& scrambled, int maxDepth, int timeBudgetMs) const {
    (void)timeBudgetMs; // unused: see Solver.h — kept simple, single-phase optimal IDA*.
    auto startTime = std::chrono::steady_clock::now();
    Result result;

    coord::CubieState start = coord::extract(scrambled);
    if (start.isSolved()) {
        result.found = true;
        result.optimal = true;
        result.solveTimeMs = 0.0;
        return result;
    }

    const auto& moves = Move::getAllMoves();
    std::array<int, coord::NUM_MOVES> moveFaces{};
    for (int m = 0; m < coord::NUM_MOVES; m++) moveFaces[m] = moves[m].getFaceIndex();

    int bound = heuristic(start, cornerDB_, edgeDB0_, edgeDB1_);
    std::vector<int> pathMoves;

    while (bound <= maxDepth) {
        int nextBound = -1;
        bool found = dfs(start, 0, bound, -1, pathMoves, cornerDB_, edgeDB0_, edgeDB1_, moveFaces, nextBound);
        if (found) {
            result.found = true;
            result.optimal = true;
            result.moves.reserve(pathMoves.size());
            for (int m : pathMoves) result.moves.push_back(moves[m].getName());
            break;
        }
        if (nextBound == -1) break;
        bound = nextBound;
    }

    auto endTime = std::chrono::steady_clock::now();
    result.solveTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}
