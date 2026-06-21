# Rubik's Cube Solver

A full-stack Rubik's Cube solver: photograph all 6 faces of a real cube, get an optimal-ish move-by-move solution computed by a custom C++ bidirectional BFS solver.

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
                                                |  Cube / Move / Solver      |
                                                |  Bidirectional BFS         |
                                                +----------------------------+
```

**Pipeline:** the user photographs the 6 cube faces in the browser → the backend calls Gemini Vision to read each face's 9 sticker colors → `validator.py` checks color counts, distinct centers, and permutation-parity solvability → the cube state is encoded as 54 integers and piped into the compiled C++ `solver_cli` binary → the solver runs a bidirectional BFS (search simultaneously from the scramble and from the solved state, meeting in the middle) → the move sequence and plain-English descriptions are returned to the frontend, which walks the user through the solution step by step.

## Tech stack

- **Solver core:** C++17, CMake (or g++ directly), bidirectional BFS over a 54-sticker cube representation
- **Backend:** Python, FastAPI, Uvicorn, `google-generativeai` (Gemini `gemini-1.5-flash` for vision), `python-dotenv`
- **Frontend:** React 18, TypeScript, Vite
- **Glue:** the backend shells out to the compiled solver binary via `subprocess`, passing the cube state over stdin and reading a JSON result from stdout

## Project structure

```
rubiks-solver/
├── solver/        # C++ bidirectional BFS solver + benchmark
├── backend/       # FastAPI app: vision, validation, solver orchestration
├── frontend/      # React + TypeScript SPA
└── .env           # GEMINI_API_KEY (see below)
```

## How to run locally

### 1. Build the C++ solver

CMake needs a generator with an actual build tool (e.g. Ninja or `mingw32-make`). If you don't have one installed, building directly with `g++` works just as well and is what was used to verify this project:

```bash
cd solver
g++ -std=c++17 -O2 -o build/solver_cli main.cpp Cube.cpp Move.cpp Solver.cpp
g++ -std=c++17 -O2 -o build/benchmark benchmark.cpp Cube.cpp Move.cpp Solver.cpp
```

If you have Ninja or `mingw32-make` available:

```bash
cd solver
cmake -S . -B build -G Ninja      # or -G "MinGW Makefiles"
cmake --build build
```

Either way you should end up with `solver/build/solver_cli` (or `solver_cli.exe` on Windows) and `solver/build/benchmark`. Sanity-check the solver:

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

The backend looks for the solver binary at `../solver/build/solver_cli` relative to `backend/` by default. Override with the `SOLVER_BINARY_PATH` environment variable if yours lives elsewhere.

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
    "total_moves": 3
  }
  ```

## Benchmark results

Produced by `solver/build/benchmark` (Bidirectional BFS, one random scramble per depth):

| Scramble Depth | Solve Time (ms) | Moves in Solution |
|---|---|---|
| 1  | 0.038   | 1 |
| 2  | 0.020   | 2 |
| 3  | 0.150   | 3 |
| 4  | 0.160   | 3 |
| 5  | 0.199   | 3 |
| 6  | 0.360   | 4 |
| 7  | 5.374   | 6 |
| 8  | 26.597  | 7 |
| 9  | 729.944 | 9 |
| 10 | 5.046   | 6 |

Moves in solution is often lower than scramble depth because random scrambles frequently contain redundant/cancelling turns; the solver finds the shortest sequence it can via bidirectional search, not necessarily the literal reverse of the scramble. Solve time grows quickly with optimal-solution depth since branching factor is 18 per move — this is a demonstration-scale solver, not a God's-number-20 production solver (those use pattern databases / IDA*).

## Environment variables

Create a `.env` file at the project root (must be UTF-8 encoded):

```
GEMINI_API_KEY=your_key_here
```

Optional overrides:

```
SOLVER_BINARY_PATH=/absolute/path/to/solver_cli   # backend: where to find the compiled solver
VITE_API_BASE_URL=http://localhost:8000           # frontend: backend base URL
ALLOWED_ORIGINS=https://your-frontend.example.com # backend: extra CORS origins (comma-separated)
```

## Deploying (so you can use it on your phone)

The backend shells out to a compiled C++ binary, so it needs a host that runs
Docker rather than a pure static/serverless host. `backend/Dockerfile` builds
`solver_cli` in a build stage and runs FastAPI in the final image; `render.yaml`
deploys that backend plus a static frontend site on [Render](https://render.com)
in one go.

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
