#include "Move.h"
#include "Cube.h"
#include <stdexcept>

Move::Move(std::string name, std::array<int, 54> permutation, std::string description, int faceIndex)
    : name_(std::move(name)), permutation_(permutation), description_(std::move(description)), faceIndex_(faceIndex) {}

const std::string& Move::getName() const { return name_; }
const std::string& Move::getDescription() const { return description_; }
const std::array<int, 54>& Move::getPermutation() const { return permutation_; }
int Move::getFaceIndex() const { return faceIndex_; }

std::string Move::inverseName(const std::string& name) {
    if (name.empty()) throw std::invalid_argument("empty move name");
    if (name.size() == 2 && name[1] == '\'') return name.substr(0, 1);
    if (name.size() == 2 && name[1] == '2') return name;
    return name + "'";
}

// ---------------------------------------------------------------------------
// Geometric permutation generator.
//
// Every sticker is identified by the 3D position of the cubie it sits on
// (x,y,z each in {-1,0,1}) plus the outward-facing normal of the sticker
// (one of +-x, +-y, +-z). A face turn is a 90-degree rotation of all
// stickers whose cubie lies in that face's layer; both the sticker's
// position and its facing normal are rotated by the same matrix. The new
// (position, normal) pair is then mapped back to a (face, row, col)
// triple to find the new sticker index. This avoids hand-typing 54-entry
// permutation tables and is easy to verify for correctness.
// ---------------------------------------------------------------------------
namespace {

struct Vec3 { int x, y, z; };

// Position of sticker (row r, col c) on face f, viewed from outside the cube.
Vec3 facePos(int f, int r, int c) {
    switch (f) {
        case TOP:    return { c - 1, 1, r - 1 };
        case BOTTOM: return { c - 1, -1, 1 - r };
        case FRONT:  return { c - 1, 1 - r, 1 };
        case BACK:   return { 1 - c, 1 - r, -1 };
        case LEFT:   return { -1, 1 - r, c - 1 };
        case RIGHT:  return { 1, 1 - r, 1 - c };
        default:     return { 0, 0, 0 };
    }
}

Vec3 faceNormal(int f) {
    switch (f) {
        case TOP:    return { 0, 1, 0 };
        case BOTTOM: return { 0, -1, 0 };
        case FRONT:  return { 0, 0, 1 };
        case BACK:   return { 0, 0, -1 };
        case LEFT:   return { -1, 0, 0 };
        case RIGHT:  return { 1, 0, 0 };
        default:     return { 0, 0, 0 };
    }
}

// Inverse of facePos/faceNormal: given a position and normal, find (face,row,col).
void locate(Vec3 pos, Vec3 normal, int& outFace, int& outRow, int& outCol) {
    if (normal.y == 1) outFace = TOP;
    else if (normal.y == -1) outFace = BOTTOM;
    else if (normal.z == 1) outFace = FRONT;
    else if (normal.z == -1) outFace = BACK;
    else if (normal.x == -1) outFace = LEFT;
    else outFace = RIGHT;

    switch (outFace) {
        case TOP:    outCol = pos.x + 1; outRow = pos.z + 1; break;
        case BOTTOM: outCol = pos.x + 1; outRow = 1 - pos.z; break;
        case FRONT:  outCol = pos.x + 1; outRow = 1 - pos.y; break;
        case BACK:   outCol = 1 - pos.x; outRow = 1 - pos.y; break;
        case LEFT:   outCol = pos.z + 1; outRow = 1 - pos.y; break;
        case RIGHT:  outCol = 1 - pos.z; outRow = 1 - pos.y; break;
    }
}

// One 90-degree clockwise rotation (as seen by someone looking straight at
// `face` from outside the cube) of vector v.
Vec3 rotateClockwise(int face, Vec3 v) {
    switch (face) {
        case RIGHT:  return { v.x, v.z, -v.y };
        case LEFT:   return { v.x, -v.z, v.y };
        case TOP:    return { -v.z, v.y, v.x };
        case BOTTOM: return { v.z, v.y, -v.x };
        case FRONT:  return { v.y, -v.x, v.z };
        case BACK:   return { -v.y, v.x, v.z };
        default:     return v;
    }
}

bool inLayer(int face, Vec3 pos) {
    switch (face) {
        case RIGHT:  return pos.x == 1;
        case LEFT:   return pos.x == -1;
        case TOP:    return pos.y == 1;
        case BOTTOM: return pos.y == -1;
        case FRONT:  return pos.z == 1;
        case BACK:   return pos.z == -1;
        default:     return false;
    }
}

int stickerIndex(int face, int row, int col) { return face * 9 + row * 3 + col; }

// turns = number of 90-degree clockwise turns to apply (1 = CW, 2 = 180, 3 = CCW).
std::array<int, 54> buildPermutation(int face, int turns) {
    std::array<int, 54> perm;
    for (int i = 0; i < 54; i++) perm[i] = i;

    for (int f = 0; f < 6; f++) {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                Vec3 pos = facePos(f, r, c);
                if (!inLayer(face, pos)) continue;

                Vec3 normal = faceNormal(f);
                for (int t = 0; t < turns; t++) {
                    pos = rotateClockwise(face, pos);
                    normal = rotateClockwise(face, normal);
                }

                int newFace, newRow, newCol;
                locate(pos, normal, newFace, newRow, newCol);

                int oldIdx = stickerIndex(f, r, c);
                int newIdx = stickerIndex(newFace, newRow, newCol);
                perm[newIdx] = oldIdx;
            }
        }
    }
    return perm;
}

const char* faceLetter(int face) {
    switch (face) {
        case TOP: return "U";
        case BOTTOM: return "D";
        case FRONT: return "F";
        case BACK: return "B";
        case LEFT: return "L";
        case RIGHT: return "R";
        default: return "?";
    }
}

const char* faceWord(int face) {
    switch (face) {
        case TOP: return "top";
        case BOTTOM: return "bottom";
        case FRONT: return "front";
        case BACK: return "back";
        case LEFT: return "left";
        case RIGHT: return "right";
        default: return "?";
    }
}

std::vector<Move> buildAllMoves() {
    std::vector<Move> moves;
    moves.reserve(18);
    const int faces[6] = { TOP, RIGHT, FRONT, BOTTOM, LEFT, BACK };

    for (int face : faces) {
        std::string letter = faceLetter(face);
        std::string word = faceWord(face);

        // Clockwise (single turn)
        moves.emplace_back(
            letter,
            buildPermutation(face, 1),
            "Rotate the " + word + " face clockwise",
            face);

        // Counter-clockwise (3 clockwise turns = 1 CCW turn)
        moves.emplace_back(
            letter + "'",
            buildPermutation(face, 3),
            "Rotate the " + word + " face counterclockwise",
            face);

        // Double turn
        moves.emplace_back(
            letter + "2",
            buildPermutation(face, 2),
            "Rotate the " + word + " face 180 degrees",
            face);
    }
    return moves;
}

} // namespace

const std::vector<Move>& Move::getAllMoves() {
    static const std::vector<Move> moves = buildAllMoves();
    return moves;
}
