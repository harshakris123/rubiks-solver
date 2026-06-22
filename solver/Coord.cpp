#include "Coord.h"
#include "Move.h"

namespace coord {
namespace {

struct V3 { int x, y, z; };

int stickerIndex(int face, int row, int col) { return face * 9 + row * 3 + col; }

// Row/col of the sticker on `face` at 3D position `p`. Mirrors the inverse
// mapping in Move.cpp's `locate`, restricted to the two axes orthogonal to
// the face's normal (valid for any p that actually touches that face).
void rowCol(int face, V3 p, int& row, int& col) {
    switch (face) {
        case TOP:    col = p.x + 1; row = p.z + 1; break;
        case BOTTOM: col = p.x + 1; row = 1 - p.z; break;
        case FRONT:  col = p.x + 1; row = 1 - p.y; break;
        case BACK:   col = 1 - p.x; row = 1 - p.y; break;
        case LEFT:   col = p.z + 1; row = 1 - p.y; break;
        case RIGHT:  col = 1 - p.z; row = 1 - p.y; break;
        default:     row = col = 0; break;
    }
}

int xFace(V3 p) { return p.x == 1 ? RIGHT : LEFT; }
int yFace(V3 p) { return p.y == 1 ? TOP : BOTTOM; }
int zFace(V3 p) { return p.z == 1 ? FRONT : BACK; }

int colorAt(const Cube& cube, int face, V3 p) {
    int row, col;
    rowCol(face, p, row, col);
    return cube.stickers[stickerIndex(face, row, col)];
}

// Fixed reference order for the 8 corner slots / 12 edge slots. The order is
// arbitrary but must stay consistent everywhere it's used (it implicitly
// defines what "corner identity 3" or "edge identity 7" means).
const std::array<V3, NUM_CORNERS>& cornerPositions() {
    static const std::array<V3, NUM_CORNERS> p = {
        V3{ 1, 1, 1}, V3{ 1, 1,-1}, V3{-1, 1,-1}, V3{-1, 1, 1},
        V3{ 1,-1, 1}, V3{ 1,-1,-1}, V3{-1,-1,-1}, V3{-1,-1, 1},
    };
    return p;
}

const std::array<V3, NUM_EDGES>& edgePositions() {
    static const std::array<V3, NUM_EDGES> p = {
        V3{ 1, 1, 0}, V3{ 0, 1, 1}, V3{-1, 1, 0}, V3{ 0, 1,-1},
        V3{ 1,-1, 0}, V3{ 0,-1, 1}, V3{-1,-1, 0}, V3{ 0,-1,-1},
        V3{ 1, 0, 1}, V3{ 1, 0,-1}, V3{-1, 0, 1}, V3{-1, 0,-1},
    };
    return p;
}

// The 3 sticker colors of the corner at position p, read in a consistently
// "clockwise as seen from outside the cube" order. A fixed (x,y,z) axis order
// is NOT consistently clockwise across all 8 octants — whether (x,y,z) order
// reads clockwise or counter-clockwise flips with the sign of x*y*z, since
// that sign is exactly the corner's chirality relative to the axes. Without
// this correction, two corners can be physically identical (related by a
// pure rotation) yet read as mirror images of each other, which breaks the
// "corners only cyclically rotate, never reflect" invariant orientation
// tracking depends on.
std::array<int, 3> cornerColors(const Cube& cube, V3 p) {
    int parity = p.x * p.y * p.z;
    if (parity == 1) {
        return { colorAt(cube, xFace(p), p), colorAt(cube, yFace(p), p), colorAt(cube, zFace(p), p) };
    }
    return { colorAt(cube, xFace(p), p), colorAt(cube, zFace(p), p), colorAt(cube, yFace(p), p) };
}

// The 2 sticker colors of the edge at position p, in fixed axis order, skipping
// whichever axis is zero for that edge.
std::array<int, 2> edgeColors(const Cube& cube, V3 p) {
    if (p.x == 0) return { colorAt(cube, yFace(p), p), colorAt(cube, zFace(p), p) };
    if (p.y == 0) return { colorAt(cube, xFace(p), p), colorAt(cube, zFace(p), p) };
    return { colorAt(cube, xFace(p), p), colorAt(cube, yFace(p), p) };
}

// Solved-state color triples/pairs, indexed by corner/edge identity (computed
// once; identity `i` is defined as "whatever sits at cornerPositions()[i] /
// edgePositions()[i] on a solved cube").
const std::array<std::array<int, 3>, NUM_CORNERS>& referenceCornerColors() {
    static const std::array<std::array<int, 3>, NUM_CORNERS> ref = [] {
        Cube solved;
        std::array<std::array<int, 3>, NUM_CORNERS> r{};
        const auto& pos = cornerPositions();
        for (int i = 0; i < NUM_CORNERS; i++) r[i] = cornerColors(solved, pos[i]);
        return r;
    }();
    return ref;
}

const std::array<std::array<int, 2>, NUM_EDGES>& referenceEdgeColors() {
    static const std::array<std::array<int, 2>, NUM_EDGES> ref = [] {
        Cube solved;
        std::array<std::array<int, 2>, NUM_EDGES> r{};
        const auto& pos = edgePositions();
        for (int i = 0; i < NUM_EDGES; i++) r[i] = edgeColors(solved, pos[i]);
        return r;
    }();
    return ref;
}

} // namespace

CubieState extract(const Cube& cube) {
    CubieState s{};

    const auto& cpos = cornerPositions();
    const auto& refC = referenceCornerColors();
    for (int slot = 0; slot < NUM_CORNERS; slot++) {
        auto obs = cornerColors(cube, cpos[slot]);
        for (int id = 0; id < NUM_CORNERS; id++) {
            bool found = false;
            for (int o = 0; o < 3 && !found; o++) {
                if (obs[0] == refC[id][o % 3] && obs[1] == refC[id][(o + 1) % 3] &&
                    obs[2] == refC[id][(o + 2) % 3]) {
                    s.cornerPerm[slot] = id;
                    s.cornerOri[slot] = o;
                    found = true;
                }
            }
            if (found) break;
        }
    }

    const auto& epos = edgePositions();
    const auto& refE = referenceEdgeColors();
    for (int slot = 0; slot < NUM_EDGES; slot++) {
        auto obs = edgeColors(cube, epos[slot]);
        for (int id = 0; id < NUM_EDGES; id++) {
            if (obs[0] == refE[id][0] && obs[1] == refE[id][1]) {
                s.edgePerm[slot] = id;
                s.edgeOri[slot] = 0;
                break;
            }
            if (obs[0] == refE[id][1] && obs[1] == refE[id][0]) {
                s.edgePerm[slot] = id;
                s.edgeOri[slot] = 1;
                break;
            }
        }
    }

    return s;
}

const MoveTables& moveTables() {
    static const MoveTables t = [] {
        MoveTables out{};
        const auto& moves = Move::getAllMoves();
        for (int m = 0; m < NUM_MOVES; m++) {
            Cube c;
            c.applyMove(moves[m]);
            CubieState s = extract(c);
            out.cornerPerm[m] = s.cornerPerm;
            out.cornerOriDelta[m] = s.cornerOri;
            out.edgePerm[m] = s.edgePerm;
            out.edgeOriDelta[m] = s.edgeOri;
        }
        return out;
    }();
    return t;
}

CubieState applyMove(const CubieState& state, int moveIndex) {
    const auto& t = moveTables();
    CubieState out{};
    for (int slot = 0; slot < NUM_CORNERS; slot++) {
        int src = t.cornerPerm[moveIndex][slot];
        out.cornerPerm[slot] = state.cornerPerm[src];
        out.cornerOri[slot] = (state.cornerOri[src] + t.cornerOriDelta[moveIndex][slot]) % 3;
    }
    for (int slot = 0; slot < NUM_EDGES; slot++) {
        int src = t.edgePerm[moveIndex][slot];
        out.edgePerm[slot] = state.edgePerm[src];
        out.edgeOri[slot] = (state.edgeOri[src] + t.edgeOriDelta[moveIndex][slot]) % 2;
    }
    return out;
}

} // namespace coord
