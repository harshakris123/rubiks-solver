#include "PatternDB.h"
#include "Combinatorics.h"
#include "Move.h"
#include <fstream>
#include <vector>

namespace pdb {
namespace {

constexpr int NUM_TRACKED_EDGES = 6;

// `group` 0 tracks edge identities 0-5, `group` 1 tracks 6-11. Returns the
// piece's index *within its group* (0-5) if it belongs to `group`, else -1.
// Composition during BFS only ever needs this relative index, never the
// absolute identity, so the same code drives both tables.
int relativeTrackedId(int edgeIdentity, int group) {
    int lo = group * NUM_TRACKED_EDGES;
    if (edgeIdentity >= lo && edgeIdentity < lo + NUM_TRACKED_EDGES) return edgeIdentity - lo;
    return -1;
}

// occupant[slot] = tracked piece's relative index (0-5) at that slot, or -1
// if the slot holds one of the other (untracked) 6 edges. ori[slot] only
// meaningful where occupant[slot] >= 0.
long long edgeIndexFromReduced(const std::array<int, 12>& occupant, const std::array<int, 12>& ori) {
    std::array<int, NUM_TRACKED_EDGES> slots{};
    std::array<int, NUM_TRACKED_EDGES> among{};
    int count = 0;
    for (int s = 0; s < 12; s++) {
        if (occupant[s] >= 0) {
            slots[count] = s;
            among[count] = occupant[s];
            count++;
        }
    }

    long long locationRank = combinatorics::combRank(slots.data(), count);
    long long amongRank = combinatorics::permRank<NUM_TRACKED_EDGES>(among);

    int oriBits = 0;
    for (int k = 0; k < count; k++) {
        if (ori[slots[k]]) oriBits |= (1 << k);
    }

    return (locationRank * 720 + amongRank) * 64 + oriBits;
}

void setNibble(std::vector<uint8_t>& packed, long long index, int value) {
    uint8_t& byte = packed[index / 2];
    if (index % 2 == 0) {
        byte = static_cast<uint8_t>((byte & 0xF0) | (value & 0x0F));
    } else {
        byte = static_cast<uint8_t>((byte & 0x0F) | ((value & 0x0F) << 4));
    }
}

int getNibble(const std::vector<uint8_t>& packed, long long index) {
    uint8_t byte = packed[index / 2];
    return (index % 2 == 0) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
}

} // namespace

long long cornerIndex(const std::array<int, 8>& perm, const std::array<int, 8>& ori) {
    long long oriIndex = 0;
    long long pow3 = 1;
    for (int i = 0; i < 7; i++) {
        oriIndex += static_cast<long long>(ori[i]) * pow3;
        pow3 *= 3;
    }
    return combinatorics::permRank<8>(perm) * 2187 + oriIndex;
}

long long edgeIndex(const coord::CubieState& state, int group) {
    std::array<int, 12> occupant{};
    std::array<int, 12> ori{};
    for (int s = 0; s < 12; s++) {
        occupant[s] = relativeTrackedId(state.edgePerm[s], group);
        ori[s] = state.edgeOri[s];
    }
    return edgeIndexFromReduced(occupant, ori);
}

int lookup(const std::vector<uint8_t>& packed, long long index) {
    return getNibble(packed, index);
}

std::array<int, coord::NUM_MOVES> moveFaces() {
    std::array<int, coord::NUM_MOVES> faces{};
    const auto& moves = Move::getAllMoves();
    for (int m = 0; m < coord::NUM_MOVES; m++) faces[m] = moves[m].getFaceIndex();
    return faces;
}

std::vector<uint8_t> buildCornerDB() {
    std::vector<uint8_t> packed((CORNER_DB_SIZE + 1) / 2, 0xFF);

    std::array<int, 8> identPerm = {0, 1, 2, 3, 4, 5, 6, 7};
    std::array<int, 8> identOri = {0, 0, 0, 0, 0, 0, 0, 0};
    setNibble(packed, cornerIndex(identPerm, identOri), 0);

    const auto& mt = coord::moveTables();
    const auto faces = moveFaces();

    // Frontier entries carry the last move's face so consecutive same-face
    // turns can be skipped — any optimal path never needs two in a row
    // (U U' is a no-op, U U = U2, U U2 = U', so a pair always collapses to
    // at most one move), so this is a pure speedup, not a completeness risk.
    std::vector<std::array<int, 8>> frontierPerm = {identPerm};
    std::vector<std::array<int, 8>> frontierOri = {identOri};
    std::vector<int> frontierLastFace = {-1};

    long long visited = 1;
    int depth = 0;
    while (!frontierPerm.empty() && visited < CORNER_DB_SIZE) {
        std::vector<std::array<int, 8>> nextPerm, nextOri;
        std::vector<int> nextLastFace;
        for (std::size_t k = 0; k < frontierPerm.size(); k++) {
            const auto& perm = frontierPerm[k];
            const auto& ori = frontierOri[k];
            int lastFace = frontierLastFace[k];
            for (int m = 0; m < coord::NUM_MOVES; m++) {
                if (faces[m] == lastFace) continue;
                std::array<int, 8> np{}, no{};
                for (int slot = 0; slot < 8; slot++) {
                    int src = mt.cornerPerm[m][slot];
                    np[slot] = perm[src];
                    no[slot] = (ori[src] + mt.cornerOriDelta[m][slot]) % 3;
                }
                long long idx = cornerIndex(np, no);
                if (getNibble(packed, idx) == 0xF) {
                    setNibble(packed, idx, depth + 1);
                    nextPerm.push_back(np);
                    nextOri.push_back(no);
                    nextLastFace.push_back(faces[m]);
                    visited++;
                }
            }
        }
        frontierPerm = std::move(nextPerm);
        frontierOri = std::move(nextOri);
        frontierLastFace = std::move(nextLastFace);
        depth++;
    }
    return packed;
}

std::vector<uint8_t> buildEdgeDB(int group) {
    std::vector<uint8_t> packed((EDGE_DB_SIZE + 1) / 2, 0xFF);

    std::array<int, 12> identOccupant{};
    std::array<int, 12> identOri{};
    for (int s = 0; s < 12; s++) identOccupant[s] = relativeTrackedId(s, group);
    setNibble(packed, edgeIndexFromReduced(identOccupant, identOri), 0);

    const auto& mt = coord::moveTables();
    const auto faces = moveFaces();
    std::vector<std::array<int, 12>> frontierOcc = {identOccupant};
    std::vector<std::array<int, 12>> frontierOri = {identOri};
    std::vector<int> frontierLastFace = {-1};

    long long visited = 1;
    int depth = 0;
    while (!frontierOcc.empty() && visited < EDGE_DB_SIZE) {
        std::vector<std::array<int, 12>> nextOcc, nextOri;
        std::vector<int> nextLastFace;
        for (std::size_t k = 0; k < frontierOcc.size(); k++) {
            const auto& occ = frontierOcc[k];
            const auto& ori = frontierOri[k];
            int lastFace = frontierLastFace[k];
            for (int m = 0; m < coord::NUM_MOVES; m++) {
                if (faces[m] == lastFace) continue;
                std::array<int, 12> nocc{}, nori{};
                for (int slot = 0; slot < 12; slot++) {
                    int src = mt.edgePerm[m][slot];
                    nocc[slot] = occ[src];
                    nori[slot] = (occ[src] >= 0) ? (ori[src] + mt.edgeOriDelta[m][slot]) % 2 : 0;
                }
                long long idx = edgeIndexFromReduced(nocc, nori);
                if (getNibble(packed, idx) == 0xF) {
                    setNibble(packed, idx, depth + 1);
                    nextOcc.push_back(nocc);
                    nextOri.push_back(nori);
                    nextLastFace.push_back(faces[m]);
                    visited++;
                }
            }
        }
        frontierOcc = std::move(nextOcc);
        frontierOri = std::move(nextOri);
        frontierLastFace = std::move(nextLastFace);
        depth++;
    }
    return packed;
}

bool save(const std::vector<uint8_t>& packed, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(packed.data()), static_cast<std::streamsize>(packed.size()));
    return out.good();
}

bool load(std::vector<uint8_t>& packed, const std::string& path, long long expectedEntries) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    long long expectedBytes = (expectedEntries + 1) / 2;
    packed.resize(static_cast<std::size_t>(expectedBytes));
    in.read(reinterpret_cast<char*>(packed.data()), expectedBytes);
    return in.gcount() == expectedBytes;
}

} // namespace pdb
