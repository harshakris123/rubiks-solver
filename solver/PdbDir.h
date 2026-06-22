#pragma once
#include <cstdlib>
#include <string>

// Where corner_pdb.dat / edge_pdb.dat live. Set via the Dockerfile in
// production (baked in at image-build time); defaults to "data" next to
// the binary's working directory for local development.
inline std::string resolvePdbDir() {
    const char* env = std::getenv("PDB_DIR");
    return env ? std::string(env) : std::string("data");
}
