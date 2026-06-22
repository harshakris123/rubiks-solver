# Rubik's Cube Solver

A full-stack Rubik's Cube solver: photograph all 6 faces of a real cube, get a move-by-move solution computed by a custom C++ IDA* solver backed by precomputed corner/edge pattern databases.

## Architecture

```
+---------------------+   photos (base64)   +---------------------------+
|   React + TS SPA    | -------------------> |     FastAPI backend      |
|   (frontend/)        |                       |     (backend/)            |
|                       | <-------------------  |                           |
|  Capture -> Review   |  colors / solution    |  vision.py    --> Gemini |
|  -> Solve -> Steps   |                       |  validator.py             |
+---------------------+                       |  main.py (FastAPI app)   |
                                                +-------------+-------------+
                                                              |
                                                  subprocess: 54 sticker ids
                                                  over stdin, JSON over stdout
                                                              |
                                                +-------------v-------------+
                                                |      C++ solver_cli        |
                                                |      (solver/)             |
                                                |  Cube / Move / Coord       |
                                                |  PatternDB / Solver (IDA*) |
                                                +----------------------------+
```

**Pipeline:** the user photographs the 6 cube faces in the browser → the backend calls Gemini Vision to read each face's 9 sticker colors → `validator.py` checks color counts, distinct centers, and permutation-parity solvability → the cube state is encoded as 54 integers and piped into the compiled C++ `solver_cli` binary → the solver extracts cubie-level coordinates (`Coord.cpp`) and runs IDA*, pruned by two precomputed pattern databases (`PatternDB.cpp`) → the move sequence and plain-English descriptions are returned to the frontend, which walks the user through the solution step by step.

See [solver/interview.md](solver/interview.md) for the full design rationale behind the solver (why bidirectional BFS doesn't scale, how the pattern databases are built and indexed, and the bugs that came up implementing it).

## Tech stack

- **Solver core:** C++17, CMake (or g++ directly), IDA* over cubie-level coordinates pruned by two precomputed pattern databases (see [solver/interview.md](solver/interview.md))
- **Backend:** Python, FastAPI, Uvicorn, `google-generativeai` (Gemini `gemini-2.5-flash` for vision, batched into one request per scan), `python-dotenv`
- **Frontend:** React 18, TypeScript, Vite
- **Glue:** the backend shells out to the compiled solver binary via `subprocess`, passing the cube state over stdin and reading a JSON result from stdout

## Project structure

```
rubiks-solver/
├── solver/        # C++ IDA* solver (pattern databases) + benchmark + pdb_gen
├── backend/       # FastAPI app: vision, validation, solver orchestration
├── frontend/      # React + TypeScript SPA
└── .env           # GEMINI_API_KEY (see below)
```

## How to run locally

### 1. Build the C++ solver

CMake needs a generator with an actual build tool (e.g. Ninja or `mingw32-make`). If you don't have one installed, building directly with `g++` works just as well and is what was used to verify this project:

```bash
cd solver
g++ -std=c++17 -O2 -static -o build/solver_cli main.cpp Cube.cpp Move.cpp Coord.cpp PatternDB.cpp Solver.cpp
g++ -std=c++17 -O2 -static -o build/benchmark benchmark.cpp Cube.cpp Move.cpp Coord.cpp PatternDB.cpp Solver.cpp
g++ -std=c++17 -O2 -static -o build/pdb_gen pdb_gen.cpp Cube.cpp Move.cpp Coord.cpp PatternDB.cpp
```

`-static` matters on MinGW/Windows: with more than one MSYS2/MinGW
toolchain on PATH, dynamically linking against a mismatched
libstdc++/libgcc DLL causes real, reproducible segfaults at `-O2`
specifically (a basic `ifstream` + `vector::resize` + `.read()` call across
two translation units was enough to crash — see
[solver/interview.md](solver/interview.md)). `-static` sidesteps the DLL
mismatch entirely; it's a no-op concern on Linux/macOS but harmless there
too.

If you have Ninja or `mingw32-make` available:

```bash
cd solver
cmake -S . -B build -G Ninja      # or -G "MinGW Makefiles"
cmake --build build
```

Either way you should end up with `solver/build/solver_cli`, `solver/build/benchmark`, and `solver/build/pdb_gen` (each with a `.exe` suffix on Windows).

### 2. Generate the pattern databases (one-time, ~8-9 minutes)

The solver needs `corner_pdb.dat`, `edge0_pdb.dat`, and `edge1_pdb.dat` to run at all — they're generated once via breadth-first search and then just loaded from disk on every solve. Not committed to git (they're ~83 MB combined); regenerate them after a clean checkout or whenever `Coord.cpp`/`PatternDB.cpp` change:

```bash
cd build
./pdb_gen data       # writes data/corner_pdb.dat, data/edge0_pdb.dat, data/edge1_pdb.dat
```

Then point the solver at that directory (the default is `./data` relative to wherever you run `solver_cli` from):

```bash
export PDB_DIR=$(pwd)/data   # Windows: set PDB_DIR=%cd%\data
```

Sanity-check the solver:

```bash
./build/solver_cli --test "R U F' L2 D'"
./build/benchmark
```

### 2. Set up the backend

```bash
cd backend
python -m venv .venv
.venv\Scripts\activate        # Windows
# source .venv/bin/activate   # macOS/Linux
pip install -r requirements.txt
```

The backend looks for the solver binary at `../solver/build/solver_cli` relative to `backend/` by default. Override with the `SOLVER_BINARY_PATH` environment variable if yours lives elsewhere. The solver itself also needs `PDB_DIR` set to wherever you ran `pdb_gen` (see step 1) — set it before starting `uvicorn`, since the backend just inherits the parent process's environment when it shells out to `solver_cli`.

Make sure `.env` (see [Environment variables](#environment-variables)) is present at the project root, then start the API:

```bash
uvicorn main:app --reload --port 8000
```

### 3. Set up the frontend

```bash
cd frontend
npm install
npm run dev
```

Open the printed local URL (default `http://localhost:5173`). The frontend talks to the backend at `http://localhost:8000` by default — override with a `VITE_API_BASE_URL` env var if needed.

### API endpoints

- `POST /detect` — accepts the 6 face photos, runs Gemini Vision only, and returns the detected 3×3 color grid per face. Used to populate the "Review Colors" screen so the user can correct any misdetected stickers before solving.
- `POST /solve` — accepts the 6 face photos and, optionally, a corrected `colors` object (as returned/edited from `/detect`). If `colors` is provided, vision detection is skipped and the corrected colors are validated and solved directly. Returns:
  ```json
  {
    "moves": ["R", "U'", "F2"],
    "descriptions": ["Rotate the right face clockwise", "..."],
    "solve_time_ms": 42,
    "total_moves": 3,
    "optimal": true
  }
  ```

## Benchmark results

Produced by `solver/build/benchmark` (IDA* + 3 pattern databases, one random scramble per depth):

| Scramble Depth | Solve Time (ms) | Moves in Solution |
|---|---|---|
| 1  | 0.031     | 1  |
| 2  | 0.024     | 2  |
| 3  | 0.000     | 0  |
| 4  | 0.008     | 2  |
| 5  | 0.052     | 4  |
| 6  | 0.042     | 5  |
| 7  | 0.129     | 7  |
| 8  | 0.059     | 5  |
| 9  | 0.080     | 7  |
| 10 | 0.179     | 8  |
| 11 | 0.170     | 7  |
| 12 | 8.677     | 10 |
| 13 | 9.575     | 11 |
| 14 | 25008.510 | 14 |
| 15 | 56.855    | 12 |
| 16 | 1867.746  | 13 |
| 17 | 24701.860 | 14 |

This solver finds the **shortest possible** solution (optimal IDA*, Korf's classic approach — see [solver/interview.md](solver/interview.md)), which is a fundamentally different tradeoff than the old bidirectional BFS: solve time no longer grows smoothly with depth. Up to depth ~11 it's consistently sub-millisecond; beyond that, some specific scrambles are simply much harder for this heuristic than others regardless of depth (note depth 14 and 17 above taking ~25 seconds while 15 and 16 stayed fast) — this is a documented property of optimal pattern-database IDA*, not a bug, and is why the backend's solve timeout is set generously (90s) rather than tightly. Real hand-scrambled cubes (depth ~18-20) usually solve in well under a second but can occasionally take tens of seconds.

## Environment variables

Create a `.env` file at the project root (must be UTF-8 encoded):

```
GEMINI_API_KEY=your_key_here
```

Optional overrides:

```
SOLVER_BINARY_PATH=/absolute/path/to/solver_cli   # backend: where to find the compiled solver
PDB_DIR=/absolute/path/to/solver/build/data       # solver: where corner_pdb.dat / edge0_pdb.dat / edge1_pdb.dat live
VITE_API_BASE_URL=http://localhost:8000           # frontend: backend base URL
ALLOWED_ORIGINS=https://your-frontend.example.com # backend: extra CORS origins (comma-separated)
```

## Deploying (so you can use it on your phone)

The backend shells out to a compiled C++ binary, so it needs a host that runs
Docker rather than a pure static/serverless host. `backend/Dockerfile` builds
`solver_cli` and `pdb_gen` in a build stage, runs `pdb_gen` to generate the
pattern databases (adds ~8-9 minutes to the image build — see
[solver/interview.md](solver/interview.md) for why), and copies both the
binary and the generated `data/` directory into the final image alongside
FastAPI; `render.yaml` deploys that backend plus a static frontend site on
[Render](https://render.com) in one go.

1. Push this repo to GitHub.
2. In the Render dashboard, **New > Blueprint**, point it at the repo. Render
   reads `render.yaml` and creates two services:
   - `rubiks-solver-backend` — Docker web service running the API.
   - `rubiks-solver-frontend` — static site serving the built React app.
3. Once both exist, set the env vars (Render dashboard > each service > Environment):
   - Backend: `GEMINI_API_KEY` = your key, `ALLOWED_ORIGINS` = the frontend's
     `https://rubiks-solver-frontend.onrender.com` URL Render assigned it.
   - Frontend: `VITE_API_BASE_URL` = the backend's `https://rubiks-solver-backend.onrender.com`
     URL Render assigned it, then trigger a manual redeploy (Vite bakes env
     vars in at build time, so this must happen *after* you know the backend URL).
4. Open the frontend's `https://...onrender.com` URL on your phone's browser.
   Camera capture requires HTTPS, which Render provides by default — no extra
   config needed. Optionally "Add to Home Screen" from the browser menu for an
   app-like icon.

Render's free tier spins down idle services, so the first request after a
while will be slow (cold start) while the container restarts.

The backend allows up to 90s for the solver subprocess (see [Benchmark
results](#benchmark-results) for why some scrambles are slow). If Render's
own reverse proxy enforces a shorter request timeout on your plan, a slow
solve could still get cut off there even though the backend itself would
have waited — check Render's current request-timeout docs for your plan if
you see solves failing around the same mark every time.
