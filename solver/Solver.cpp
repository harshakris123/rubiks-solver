#include "Solver.h"
#include "Move.h"
#include <chrono>
#include <unordered_map>
#include <vector>

namespace {

struct Node {
    Cube cube;
    int lastFace; // face index of the last move applied, -1 if none
};

using VisitedMap = std::unordered_map<std::string, std::vector<std::string>>;

// Expands one full BFS layer for one side. Returns the new frontier and
// stops early (returning an empty vector signal via foundMeeting) if a
// state is discovered that the opposite side has already visited.
bool expandLayer(std::vector<Node>& frontier,
                  VisitedMap& visitedSame,
                  const VisitedMap& visitedOther,
                  std::vector<Node>& nextFrontier,
                  std::string& meetingKey) {
    const auto& moves = Move::getAllMoves();

    for (const auto& node : frontier) {
        const std::string curKey = node.cube.serialize();
        const std::vector<std::string>& curPath = visitedSame[curKey];

        for (const auto& mv : moves) {
            if (mv.getFaceIndex() == node.lastFace) continue; // skip redundant same-face turns

            Cube next = node.cube;
            next.applyMove(mv);
            std::string key = next.serialize();

            if (visitedSame.find(key) != visitedSame.end()) continue;

            std::vector<std::string> newPath = curPath;
            newPath.push_back(mv.getName());
            visitedSame[key] = newPath;
            nextFrontier.push_back({ next, mv.getFaceIndex() });

            if (visitedOther.find(key) != visitedOther.end()) {
                meetingKey = key;
                return true;
            }
        }
    }
    return false;
}

} // namespace

Solver::Result Solver::solve(const Cube& scrambled, int maxDepthPerSide) {
    auto startTime = std::chrono::steady_clock::now();
    Result result;

    if (scrambled.isSolved()) {
        result.found = true;
        result.solveTimeMs = 0.0;
        return result;
    }

    Cube solved;
    VisitedMap visitedF, visitedB;
    visitedF[scrambled.serialize()] = {};
    visitedB[solved.serialize()] = {};

    std::vector<Node> frontierF = { { scrambled, -1 } };
    std::vector<Node> frontierB = { { solved, -1 } };

    std::string meetingKey;

    for (int depth = 0; depth < maxDepthPerSide * 2; depth++) {
        bool expandForward = frontierF.size() <= frontierB.size();
        std::vector<Node> nextFrontier;

        bool met = expandForward
            ? expandLayer(frontierF, visitedF, visitedB, nextFrontier, meetingKey)
            : expandLayer(frontierB, visitedB, visitedF, nextFrontier, meetingKey);

        if (met) {
            const std::vector<std::string>& pathF = visitedF[meetingKey];
            const std::vector<std::string>& pathB = visitedB[meetingKey];

            std::vector<std::string> solution = pathF;
            for (auto it = pathB.rbegin(); it != pathB.rend(); ++it) {
                solution.push_back(Move::inverseName(*it));
            }

            result.moves = solution;
            result.found = true;
            auto endTime = std::chrono::steady_clock::now();
            result.solveTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            return result;
        }

        if (expandForward) {
            frontierF = std::move(nextFrontier);
            if (frontierF.empty()) break;
        } else {
            frontierB = std::move(nextFrontier);
            if (frontierB.empty()) break;
        }
    }

    result.found = false;
    auto endTime = std::chrono::steady_clock::now();
    result.solveTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}
