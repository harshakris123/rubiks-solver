#pragma once
#include <array>
#include <string>

// Face indices (sticker index = face * 9 + row * 3 + col)
enum Face { TOP = 0, BOTTOM = 1, FRONT = 2, BACK = 3, LEFT = 4, RIGHT = 5 };

// Color ids used inside the 54-sticker state array
enum Color { WHITE = 0, YELLOW = 1, RED = 2, ORANGE = 3, BLUE = 4, GREEN = 5 };

class Move;

class Cube {
public:
    static constexpr int NUM_STICKERS = 54;

    std::array<int, NUM_STICKERS> stickers;

    // Builds a solved cube (face f is filled with color f).
    Cube();

    // Apply a raw 54-element permutation: newState[i] = oldState[perm[i]].
    void applyPermutation(const std::array<int, NUM_STICKERS>& perm);

    // Apply a named Move (see Move.h) to this cube's state.
    void applyMove(const Move& move);

    bool isSolved() const;

    // 54-character string (one digit 0-5 per sticker) used as a hash key.
    std::string serialize() const;

    void print() const;
};
