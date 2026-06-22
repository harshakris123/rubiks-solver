#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Small, generic combinatorial ranking helpers used to turn cubie-coordinate
// sub-states into dense array indices for the pattern databases.
namespace combinatorics {

// Rank of a permutation of {0,...,N-1} within S_N, via its Lehmer code, in a
// fixed (but arbitrary) bijective order: rank 0 is {0,1,...,N-1}, rank N!-1
// is {N-1,...,1,0}.
template <std::size_t N>
long long permRank(const std::array<int, N>& perm) {
    long long rank = 0;
    long long fact = 1;
    for (int i = static_cast<int>(N) - 1; i >= 0; i--) {
        int smaller = 0;
        for (int j = i + 1; j < static_cast<int>(N); j++) {
            if (perm[j] < perm[i]) smaller++;
        }
        rank += static_cast<long long>(smaller) * fact;
        fact *= (static_cast<int>(N) - i);
    }
    return rank;
}

inline long long choose(int n, int k) {
    if (k < 0 || k > n) return 0;
    long long r = 1;
    for (int i = 0; i < k; i++) {
        r = r * (n - i) / (i + 1);
    }
    return r;
}

// Combinatorial-number-system rank of a strictly increasing sequence
// c[0] < c[1] < ... < c[k-1], each in [0,n), among all C(n,k) such sequences.
// Pointer+count overload avoids a heap allocation in hot paths (this runs
// once per IDA* node evaluated, so it matters); the vector overload is kept
// for the brute-force regression test.
inline long long combRank(const int* c, int count) {
    long long rank = 0;
    for (int i = 0; i < count; i++) {
        rank += choose(c[i], i + 1);
    }
    return rank;
}

inline long long combRank(const std::vector<int>& c) {
    return combRank(c.data(), static_cast<int>(c.size()));
}

} // namespace combinatorics
