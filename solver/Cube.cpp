#include "Cube.h"
#include "Move.h"
#include <iostream>

Cube::Cube() {
    // Solved cube: face index doubles as its solid color id.
    for (int f = 0; f < 6; f++) {
        for (int i = 0; i < 9; i++) {
            stickers[f * 9 + i] = f;
        }
    }
}

void Cube::applyPermutation(const std::array<int, NUM_STICKERS>& perm) {
    std::array<int, NUM_STICKERS> newState;
    for (int i = 0; i < NUM_STICKERS; i++) {
        newState[i] = stickers[perm[i]];
    }
    stickers = newState;
}

void Cube::applyMove(const Move& move) {
    applyPermutation(move.getPermutation());
}

bool Cube::isSolved() const {
    for (int f = 0; f < 6; f++) {
        int color = stickers[f * 9];
        for (int i = 1; i < 9; i++) {
            if (stickers[f * 9 + i] != color) return false;
        }
    }
    return true;
}

std::string Cube::serialize() const {
    std::string s;
    s.reserve(NUM_STICKERS);
    for (int i = 0; i < NUM_STICKERS; i++) {
        s += static_cast<char>('0' + stickers[i]);
    }
    return s;
}

void Cube::print() const {
    static const char* colorNames[6] = {"W", "Y", "R", "O", "B", "G"};
    static const char* faceNames[6] = {"TOP", "BOTTOM", "FRONT", "BACK", "LEFT", "RIGHT"};
    for (int f = 0; f < 6; f++) {
        std::cout << faceNames[f] << ":\n";
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                std::cout << colorNames[stickers[f * 9 + r * 3 + c]] << " ";
            }
            std::cout << "\n";
        }
    }
}
