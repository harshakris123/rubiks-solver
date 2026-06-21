#pragma once
#include <array>
#include <string>
#include <vector>

// One of the 18 standard moves (U, U', U2, R, R', R2, F, F', F2,
// D, D', D2, L, L', L2, B, B', B2), represented as a permutation
// of the 54-sticker cube state: newState[i] = oldState[perm[i]].
class Move {
public:
    Move(std::string name, std::array<int, 54> permutation, std::string description, int faceIndex);

    const std::string& getName() const;
    const std::string& getDescription() const;
    const std::array<int, 54>& getPermutation() const;
    int getFaceIndex() const;

    // The 18 standard moves, built once, in the order:
    // U, U', U2, R, R', R2, F, F', F2, D, D', D2, L, L', L2, B, B', B2
    static const std::vector<Move>& getAllMoves();

    // Name of the move that undoes `name` (e.g. "U" <-> "U'", "U2" -> "U2").
    static std::string inverseName(const std::string& name);

private:
    std::string name_;
    std::array<int, 54> permutation_;
    std::string description_;
    int faceIndex_;
};
