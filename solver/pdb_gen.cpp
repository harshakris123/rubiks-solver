// Standalone build-time tool: generates corner_pdb.dat, edge0_pdb.dat, and
// edge1_pdb.dat (see PatternDB.h) into the given output directory. Run once
// per build — the resulting files are what Solver loads at runtime. Takes
// roughly 8-9 minutes since it's a from-scratch BFS over ~88M and two ~42M
// state spaces; this is meant to run during the Docker image build, not per
// request.
#include "PatternDB.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

bool ensureSaved(const std::vector<uint8_t>& packed, const std::string& path) {
    if (!pdb::save(packed, path)) {
        std::cerr << "Failed to write " << path << "\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string outDir = argc > 1 ? argv[1] : "data";
    std::filesystem::create_directories(outDir);

    std::cout << "Building corner pattern database (8! x 3^7 = "
              << pdb::CORNER_DB_SIZE << " states)...\n";
    auto t0 = std::chrono::steady_clock::now();
    auto cornerDB = pdb::buildCornerDB();
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "  done in " << std::chrono::duration<double>(t1 - t0).count() << "s\n";
    if (!ensureSaved(cornerDB, outDir + "/corner_pdb.dat")) return 1;

    for (int group = 0; group < 2; group++) {
        std::cout << "Building edge pattern database group " << group
                  << " (P(12,6) x 2^6 = " << pdb::EDGE_DB_SIZE << " states)...\n";
        auto t2 = std::chrono::steady_clock::now();
        auto edgeDB = pdb::buildEdgeDB(group);
        auto t3 = std::chrono::steady_clock::now();
        std::cout << "  done in " << std::chrono::duration<double>(t3 - t2).count() << "s\n";
        if (!ensureSaved(edgeDB, outDir + "/edge" + std::to_string(group) + "_pdb.dat")) return 1;
    }

    std::cout << "Pattern databases written to " << outDir << "/\n";
    return 0;
}
