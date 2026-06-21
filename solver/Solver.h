#pragma once
#include "Cube.h"
#include <string>
#include <vector>

class Solver {
public:
    struct Result {
        std::vector<std::string> moves; // empty if already solved or unsolved within depth bound
        double solveTimeMs = 0.0;
        bool found = false;
    };

    // Bidirectional BFS: searches forward from `scrambled` and backward from
    // the solved state simultaneously, meeting in the middle.
    // maxDepthPerSide bounds the search to keep memory/time in check.
    Result solve(const Cube& scrambled, int maxDepthPerSide = 7);
};
