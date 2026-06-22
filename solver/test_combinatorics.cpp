// Scratch correctness check for Combinatorics.h — not part of the production build.
#include "Combinatorics.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <vector>

template <std::size_t N>
bool testPermRank() {
    std::array<int, N> base;
    std::iota(base.begin(), base.end(), 0);
    std::vector<std::array<int, N>> perms;
    std::array<int, N> p = base;
    do {
        perms.push_back(p);
    } while (std::next_permutation(p.begin(), p.end()));

    long long expectedCount = 1;
    for (std::size_t i = 1; i <= N; i++) expectedCount *= static_cast<long long>(i);
    if (static_cast<long long>(perms.size()) != expectedCount) {
        std::cerr << "N=" << N << ": expected " << expectedCount << " perms, got " << perms.size() << "\n";
        return false;
    }

    std::set<long long> seenRanks;
    for (const auto& perm : perms) {
        long long r = combinatorics::permRank<N>(perm);
        if (r < 0 || r >= expectedCount) {
            std::cerr << "N=" << N << ": rank " << r << " out of range for a permutation\n";
            return false;
        }
        if (!seenRanks.insert(r).second) {
            std::cerr << "N=" << N << ": duplicate rank " << r << "\n";
            return false;
        }
    }
    std::cout << "OK: permRank<" << N << "> is a bijection onto [0," << expectedCount << ")\n";
    return true;
}

bool testCombRank(int n, int k) {
    std::vector<int> all(n);
    std::iota(all.begin(), all.end(), 0);
    std::vector<int> mask(n, 0);
    std::fill(mask.end() - k, mask.end(), 1);

    long long expectedCount = combinatorics::choose(n, k);
    std::set<long long> seenRanks;
    long long actualCount = 0;

    do {
        std::vector<int> combo;
        for (int i = 0; i < n; i++) if (mask[i]) combo.push_back(i);
        long long r = combinatorics::combRank(combo);
        if (r < 0 || r >= expectedCount) {
            std::cerr << "n=" << n << " k=" << k << ": rank " << r << " out of range\n";
            return false;
        }
        if (!seenRanks.insert(r).second) {
            std::cerr << "n=" << n << " k=" << k << ": duplicate rank " << r << "\n";
            return false;
        }
        actualCount++;
    } while (std::next_permutation(mask.begin(), mask.end()));

    if (actualCount != expectedCount) {
        std::cerr << "n=" << n << " k=" << k << ": expected " << expectedCount << " combos, saw " << actualCount << "\n";
        return false;
    }
    std::cout << "OK: combRank for n=" << n << " k=" << k << " is a bijection onto [0," << expectedCount << ")\n";
    return true;
}

int main() {
    bool ok = true;
    ok &= testPermRank<3>();
    ok &= testPermRank<4>();
    ok &= testPermRank<6>();
    ok &= testPermRank<8>();
    ok &= testCombRank(5, 2);
    ok &= testCombRank(12, 6);
    ok &= testCombRank(8, 3);
    return ok ? 0 : 1;
}
