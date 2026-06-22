#pragma once
#include "Coord.h"
#include <cstdint>
#include <string>
#include <vector>

// Pattern databases for IDA* pruning: precomputed "minimum moves to solve
// this sub-state" tables, built once via breadth-first search in coordinate
// space and reused as an admissible heuristic for every solve.
//
// Three tables are built (Korf's classic combination for an optimal/near-
// optimal Rubik's Cube solver):
//  - Corner DB: full corner state (all 8 corners' permutation+orientation),
//    ignoring edges entirely. 8! * 3^7 = 88,179,840 reachable states (the
//    7, not 8, orientation digits are stored because the 8th is always
//    determined by the others on a reachable cube state).
//  - Edge DB (group 0): only 6 of the 12 edges are "tracked" (identities
//    0-5 in Coord's edge numbering); the table records the minimum moves to
//    get those 6 edges into their solved slots+orientations, ignoring where
//    the other 6 end up. P(12,6) * 2^6 = 42,577,920 states.
//  - Edge DB (group 1): same idea, tracking identities 6-11 instead — the
//    other half of the edges.
//
// All three are valid lower bounds on true solve distance (solving the
// whole cube requires at least solving each piece subset), so
// max(corner, edge0, edge1) is an admissible IDA* heuristic. An earlier
// version of this solver used only one edge table to halve the
// implementation/memory/build-time cost, on the theory that "explodes past
// depth ~10" -> "solves in well under a second" was already the win that
// mattered. That held at moderate scramble depths but not at the depths a
// real scrambled cube actually needs (~18-20 moves) — measured ~28s at
// depth 18 with one edge table, which would still blow through the
// backend's solve timeout. The second table is what actually closes that
// gap; see solver/interview.md for the measurements.
namespace pdb {

constexpr long long CORNER_DB_SIZE = 88179840LL; // 8! * 3^7
constexpr long long EDGE_DB_SIZE = 42577920LL;   // P(12,6) * 2^6

long long cornerIndex(const std::array<int, 8>& perm, const std::array<int, 8>& ori);
// `group` selects which 6 of the 12 edges are tracked: 0 for identities
// 0-5, 1 for identities 6-11.
long long edgeIndex(const coord::CubieState& state, int group);

// Builds a table (BFS from the solved state in coordinate space). Returned
// vector is packed 2 entries/byte (distances are always small, 0-11ish).
std::vector<uint8_t> buildCornerDB();
std::vector<uint8_t> buildEdgeDB(int group);

int lookup(const std::vector<uint8_t>& packed, long long index);

bool save(const std::vector<uint8_t>& packed, const std::string& path);
// `expectedEntries` is the logical entry count (CORNER_DB_SIZE / EDGE_DB_SIZE),
// used to size the buffer; returns false if the file is missing/short.
bool load(std::vector<uint8_t>& packed, const std::string& path, long long expectedEntries);

} // namespace pdb
