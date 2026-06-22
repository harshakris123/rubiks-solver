// Scratch correctness/perf check for PatternDB.cpp — not part of the production build.
#include "PatternDB.h"
#include "Coord.h"
#include <chrono>
#include <iostream>

int main() {
    auto t0 = std::chrono::steady_clock::now();
    auto cornerDB = pdb::buildCornerDB();
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "Corner DB built in "
              << std::chrono::duration<double>(t1 - t0).count() << "s, "
              << cornerDB.size() << " bytes\n";

    // Solved state must be distance 0.
    coord::CubieState solved{};
    for (int i = 0; i < 8; i++) { solved.cornerPerm[i] = i; solved.cornerOri[i] = 0; }
    for (int i = 0; i < 12; i++) { solved.edgePerm[i] = i; solved.edgeOri[i] = 0; }
    long long solvedIdx = pdb::cornerIndex(solved.cornerPerm, solved.cornerOri);
    std::cout << "corner dist at solved = " << pdb::lookup(cornerDB, solvedIdx) << " (expect 0)\n";

    // Every reachable state must have been visited (no 0xF nibble left, since
    // the BFS loop only stops once `visited == CORNER_DB_SIZE`).
    int maxDist = 0;
    long long unvisited = 0;
    for (long long i = 0; i < pdb::CORNER_DB_SIZE; i++) {
        int d = pdb::lookup(cornerDB, i);
        if (d == 0xF) unvisited++;
        else if (d > maxDist) maxDist = d;
    }
    std::cout << "unvisited entries: " << unvisited << " (expect 0)\n";
    std::cout << "max corner-only distance: " << maxDist << "\n";

    // One known fact: applying a single move from solved must give distance 1.
    const auto& mt = coord::moveTables();
    for (int m = 0; m < coord::NUM_MOVES; m++) {
        long long idx = pdb::cornerIndex(mt.cornerPerm[m], mt.cornerOriDelta[m]);
        int d = pdb::lookup(cornerDB, idx);
        if (d != 1) {
            std::cout << "FAIL: move " << m << " single application has corner-dist " << d << ", expected 1\n";
        }
    }
    std::cout << "Checked: every single move from solved has corner-distance 1.\n";

    for (int group = 0; group < 2; group++) {
        auto t2 = std::chrono::steady_clock::now();
        auto edgeDB = pdb::buildEdgeDB(group);
        auto t3 = std::chrono::steady_clock::now();
        std::cout << "\nEdge DB group " << group << " built in "
                  << std::chrono::duration<double>(t3 - t2).count() << "s, "
                  << edgeDB.size() << " bytes\n";

        long long edgeSolvedIdx = pdb::edgeIndex(solved, group);
        std::cout << "edge dist at solved = " << pdb::lookup(edgeDB, edgeSolvedIdx) << " (expect 0)\n";

        long long edgeUnvisited = 0;
        int edgeMaxDist = 0;
        for (long long i = 0; i < pdb::EDGE_DB_SIZE; i++) {
            int d = pdb::lookup(edgeDB, i);
            if (d == 0xF) edgeUnvisited++;
            else if (d > edgeMaxDist) edgeMaxDist = d;
        }
        std::cout << "unvisited entries: " << edgeUnvisited << " (expect 0)\n";
        std::cout << "max edge group " << group << " distance: " << edgeMaxDist << "\n";

        for (int m = 0; m < coord::NUM_MOVES; m++) {
            coord::CubieState afterMove{};
            for (int i = 0; i < 8; i++) { afterMove.cornerPerm[i] = i; afterMove.cornerOri[i] = 0; }
            afterMove.edgePerm = mt.edgePerm[m];
            afterMove.edgeOri = mt.edgeOriDelta[m];
            long long idx = pdb::edgeIndex(afterMove, group);
            int d = pdb::lookup(edgeDB, idx);
            if (d != 1) {
                std::cout << "FAIL: move " << m << " group " << group << " single application has edge-dist "
                          << d << ", expected 1\n";
            }
        }
        std::cout << "Checked: every single move from solved has group " << group << " edge-distance 1.\n";
    }

    return 0;
}
