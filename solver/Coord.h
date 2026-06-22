#pragma once
#include "Cube.h"
#include <array>

// Cubie-level coordinates (corner/edge permutation + orientation), used by
// the IDA* solver and its pattern databases. The 54-sticker representation
// in Cube is great for applying moves and for the frontend/backend wire
// format, but pattern-database search needs the compact "8 corners + 12
// edges, each with an identity and an orientation" view that classic
// Rubik's Cube theory operates on.
namespace coord {

constexpr int NUM_CORNERS = 8;
constexpr int NUM_EDGES = 12;
constexpr int NUM_MOVES = 18;

struct CubieState {
    // perm[slot] = identity (0..7 / 0..11) of the cubie currently sitting
    // in reference slot `slot`. ori[slot] = that cubie's orientation
    // (0..2 for corners, 0..1 for edges) relative to its solved orientation.
    std::array<int, NUM_CORNERS> cornerPerm;
    std::array<int, NUM_CORNERS> cornerOri;
    std::array<int, NUM_EDGES> edgePerm;
    std::array<int, NUM_EDGES> edgeOri;

    bool isSolved() const {
        for (int i = 0; i < NUM_CORNERS; i++) {
            if (cornerPerm[i] != i || cornerOri[i] != 0) return false;
        }
        for (int i = 0; i < NUM_EDGES; i++) {
            if (edgePerm[i] != i || edgeOri[i] != 0) return false;
        }
        return true;
    }
};

// Reads cubie-level coordinates off a 54-sticker cube state.
CubieState extract(const Cube& cube);

// moveTables().cornerPerm[m] / cornerOriDelta[m] / edgePerm[m] / edgeOriDelta[m]
// describe the intrinsic effect of move index m (matching the order of
// Move::getAllMoves()) on cubie coordinates: it's exactly the CubieState you'd
// extract from a solved cube after applying move m alone.
struct MoveTables {
    std::array<std::array<int, NUM_CORNERS>, NUM_MOVES> cornerPerm;
    std::array<std::array<int, NUM_CORNERS>, NUM_MOVES> cornerOriDelta;
    std::array<std::array<int, NUM_EDGES>, NUM_MOVES> edgePerm;
    std::array<std::array<int, NUM_EDGES>, NUM_MOVES> edgeOriDelta;
};

const MoveTables& moveTables();

// Applies move index m to `state` entirely in coordinate space (no sticker
// array involved), used for the millions of expansions pattern-database
// construction and IDA* search both need to do quickly.
CubieState applyMove(const CubieState& state, int moveIndex);

} // namespace coord
