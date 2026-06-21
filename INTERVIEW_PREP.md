# Interview Prep — Rubik's Cube Solver

This document explains the entire project in plain language so you can talk about it confidently in an interview — what was built, why it was built that way, and what a good interviewer might push back on.

---

## 1. Master Architecture

### The three layers

Think of the project as three independent programs that talk to each other:

- **Frontend (React + TypeScript + Vite)** — runs in the browser. Captures photos, shows colors for review, displays the solution.
- **Backend (Python + FastAPI)** — the "orchestrator." It doesn't do any heavy computation itself — it calls Gemini Vision, validates the result, and hands the real work to the C++ program.
- **Solver (C++ binary)** — a standalone command-line program that takes a cube state in and prints a solution out. It knows nothing about HTTP, images, or React. It only knows about cubes and moves.

This separation matters: each layer can be tested, replaced, or scaled independently. The C++ solver doesn't care if it's called from Python, from a test script, or from the command line by hand — and indeed it was built and verified standalone before any backend code touched it.

### Step-by-step data flow

1. **Capture** — `FaceCapture.tsx` lets the user take/upload 6 photos (Top, Bottom, Front, Back, Left, Right). Each photo is read into a base64 string in the browser using `FileReader`, kept in React state. Nothing leaves the browser yet.
2. **Detect** — the frontend sends all 6 base64 images in one `POST /detect` call. The backend forwards each image to Gemini Vision (`gemini-1.5-flash`) with a strict prompt, and gets back a 3×3 grid of color names per face.
3. **Review** — the frontend shows `ColorGrid` (a 3×3 grid of colored squares per face) and `CubePreview` (all 6 faces laid out as a flattened net). If Gemini misread a sticker, the user taps it to cycle to the correct color.
4. **Solve** — the frontend calls `POST /solve`, sending the original images **and** the (possibly corrected) color grids. The backend sees the corrected colors are present and **skips vision entirely** — no point calling the AI again.
5. **Validate** — `validator.py` checks the state is physically possible: every color appears exactly 9 times, all 6 centers are different colors, and the corner/edge permutation parity matches (a real law of how a physical cube can be scrambled — explained in Section 5).
6. **Encode** — the 6×9 color grid is flattened into 54 integers (0-5, one per sticker) in a fixed face order: Top, Bottom, Front, Back, Left, Right.
7. **Solve (subprocess)** — the backend launches the compiled C++ binary (`solver_cli`) as a child process, writes the 54 integers to its **stdin**, and reads a line of JSON back from its **stdout**.
8. **Compute** — inside the C++ process: the 54 integers become a `Cube`, and `Solver::solve()` runs a bidirectional BFS to find a move sequence. The program prints `{"moves": [...], "descriptions": [...], "solve_time_ms": ..., "total_moves": ...}` and exits.
9. **Respond** — the backend reads that JSON line, parses it, and returns it as the HTTP response.
10. **Walk through** — `SolveSteps.tsx` shows the moves one at a time with Next/Previous buttons.

### Two boundaries worth naming in an interview

- **HTTP boundary**: browser ↔ FastAPI, using JSON over the network.
- **Process boundary**: FastAPI ↔ C++ binary, using plain text over a pipe (stdin/stdout), not a network call at all.

Why a *process* boundary instead of binding the C++ code directly into Python (e.g. with `pybind11` or `ctypes`)? It's simpler and safer to reason about — if the solver crashes or hangs, it's an isolated OS process that can be killed/timed-out without taking down the API server. The cost is a small amount of process-startup latency on every request, which is a deliberate simplicity-vs-performance tradeoff (see Section 8, Improvement #5).

---

## 2. Database Schema

### What this project actually uses today: no database

There is **no database** in this project. State management today is:

- **Frontend**: everything lives in React `useState` inside `App.tsx` — images, detected colors, the solution. Close the browser tab and it's gone.
- **Backend**: completely stateless. Every `/solve` call is independent; nothing is written to disk or remembered between requests.
- **Solver**: the bidirectional BFS builds two hash maps (`visitedF`, `visitedB`) purely in memory for the duration of one solve, then the process exits and they're discarded.

This is a deliberate, honest design for a single-session demo tool — but it's exactly the kind of thing an interviewer will ask "how would you add persistence?" about. Here's the answer.

### A schema for "what if we added persistence"

If we wanted login, scan history, and analytics, here's a clean relational schema (e.g. PostgreSQL via SQLAlchemy):

**`users`**
| column | type | notes |
|---|---|---|
| id | UUID / serial PK | |
| email | text, unique | |
| password_hash | text | never store plaintext passwords |
| created_at | timestamp | |

**`scan_sessions`** — one row per "attempt to scan and solve a cube"
| column | type | notes |
|---|---|---|
| id | UUID / serial PK | |
| user_id | FK → users.id, **nullable** | nullable so anonymous use still works |
| status | enum: `capturing`, `reviewing`, `solved`, `failed` | tracks where the user is in the flow |
| created_at | timestamp | |

**`face_scans`** — one row per photographed face (6 per session)
| column | type | notes |
|---|---|---|
| id | UUID / serial PK | |
| session_id | FK → scan_sessions.id | |
| face_name | enum: top/bottom/front/back/left/right | |
| image_url | text | store the image in object storage (S3-like), not the DB |
| detected_colors | JSON array of 9 strings | what Gemini said |
| corrected_colors | JSON array of 9 strings, nullable | what the user fixed, if anything |

**`solves`** — one row per solution computed
| column | type | notes |
|---|---|---|
| id | UUID / serial PK | |
| session_id | FK → scan_sessions.id | |
| total_moves | int | |
| solve_time_ms | float | |
| moves_json | JSON array of strings | simplest option |
| created_at | timestamp | |

**`move_steps`** (optional, normalized alternative to `moves_json`)
| column | type | notes |
|---|---|---|
| id | PK | |
| solve_id | FK → solves.id | |
| step_index | int | 0, 1, 2, ... |
| move_name | varchar(3) | e.g. "R", "U'", "F2" |
| description | text | e.g. "Rotate the right face clockwise" |

### Why two options for storing moves?

- **JSON column** (`moves_json`): simplest to write and read — you always read the whole list together anyway when showing `SolveSteps`. Good default.
- **Normalized `move_steps` table**: needed only if you want to *query across solves*, e.g. "what's the most common first move?" or "how many solves contain F2?" That requires the moves broken into actual rows you can `GROUP BY`. A relational DB question like "when would you normalize vs. use JSON?" is exactly this tradeoff — favor JSON for simple read/write-together data, normalize when you need to query *into* the structure.

### Indexes you'd actually need

- `scan_sessions(user_id)` — "show me my history"
- `face_scans(session_id)` and `solves(session_id)` — both are pure lookups by parent, so a foreign key index is enough; no need for anything fancier here.

---

## 3. AI Layer Logic

### Which model, and why

`vision.py` uses **Gemini `gemini-1.5-flash`**, not the larger `pro` model. The task — "read 9 colored squares off a photo" — doesn't need deep reasoning, so the faster/cheaper "flash" tier is the right tool. This is a good interview point: matching model size to task complexity instead of defaulting to the biggest model.

### The exact prompt

```
Look at this Rubik's Cube face. Return ONLY a JSON array of exactly 9
color names representing the 3x3 grid of stickers, read left to right,
top to bottom. Use only these color names: white, yellow, red, orange,
blue, green. Return nothing else, no explanation, no markdown.
```

Sent alongside the raw image bytes (`mime_type: "image/jpeg"`) directly in the `generate_content()` call — the image is small enough to send inline rather than uploading it separately through Gemini's File API.

### Why such a rigid prompt

LLMs are not deterministic parsers — they will sometimes add commentary, wrap the answer in a markdown code fence, or use a color name you didn't ask for, even when told not to. The prompt is written defensively (ONLY, exactly 9, no explanation, no markdown) to *reduce* that risk, but the code never *trusts* the prompt to be obeyed perfectly — which is the real lesson here:

> **Never trust an LLM's output format just because you asked nicely for it. Always validate.**

### The full error-handling chain

Every one of these is a real `raise` in `vision.py`, in the order they'd actually trigger:

1. **Bad base64 input** — if the string isn't valid base64 (or has a `data:image/...;base64,` prefix), decoding fails → wrapped as a `ValueError` with the underlying reason.
2. **API/network failure** — anything Gemini's SDK throws (timeout, auth error, quota) is caught and re-raised as a `RuntimeError` with context, instead of leaking a raw SDK exception to the caller.
3. **Empty response** — if `response.text` is empty, that's a `ValueError` rather than silently treating it as "no colors."
4. **Markdown fences anyway** — even though the prompt forbids it, the code strips a leading ` ```json ` and trailing ` ``` ` defensively before parsing.
5. **Extra text around the array** — a regex (`\[.*\]`) pulls out just the `[...]` substring in case the model adds a stray sentence before/after it.
6. **Invalid JSON** — if what's left still doesn't parse, a `ValueError` includes the raw text so a developer can see exactly what the model returned.
7. **Wrong shape** — must be a JSON *array*, not an object or a string.
8. **Wrong length** — must be exactly 9 entries.
9. **Wrong values** — every entry is lowercased/stripped and checked against the 6 known color names; the first bad one is named explicitly in the error (e.g. `Unexpected color name 'beige'`).

### Why this matters at the API layer

In `main.py`, vision errors for **one face** are caught and turned into an HTTP `422` that names *which face* failed — e.g. `Vision detection failed for face 'top': Expected 9 colors, got 8`. This means a bad photo of one face doesn't corrupt the whole cube silently; it fails loud, with enough detail for the user to know exactly which photo to retake.

---

## 4. Cognitive Load

### What "cognitive load" means here

Cognitive load is how much a person has to hold in their working memory at once. A wall of 17 move names (`R U F' L2 D' B U2 ...`) dumped on screen forces the user to constantly scan back and forth to find "where am I right now?" — that's high cognitive load, and it's exactly how people lose track and mess up a physical cube mid-solve.

### How `SolveSteps.tsx` reduces it

- **One move at a time.** Only the current move is on screen — nothing to scan for.
- **Step counter** ("Step 3 of 17") gives a sense of position without requiring the user to count.
- **The move itself is huge and bold** (`R`, `U'`, `F2`) — glanceable in under a second, important because the user's hands are busy holding a physical cube, not reading paragraphs.
- **The plain-English description is secondary**, shown smaller below the move — it's there for beginners who don't know cube notation yet, but doesn't compete for attention with the move itself.
- **A progress bar** gives a constant ambient sense of "how much is left" without needing to do the (step / total) math in your head.
- **Previous/Next buttons** let the user re-check a step or back up after a mistake without losing their place in the rest of the sequence — they can always get back to exactly where they were.

### The analogy worth using in an interview

This is the same idea as turn-by-turn GPS navigation: it never shows you the entire 40-step route at once. It shows *one* upcoming turn, large, and tells you the next one only when you get there. Breaking a long sequence into single-item focus is called **chunking**, and it's a deliberate UX decision here, not an accident of running out of screen space.

---

## 5. Adaptive Scheduler — Bidirectional BFS

> "Adaptive" here refers to the search picking, at every step, *which side* (forward or backward) to expand next, based on which side currently has less work to do — explained below.

### Why not just run plain BFS?

A Rubik's Cube has about **4.3 × 10¹⁹** possible states, and from any state you can reach 18 new states in one move (the 18 standard moves: U, U', U2, R, R', R2, ...). Optimally solving *any* scrambled cube can require up to 20 moves (this is called **God's Number**).

If you ran a single BFS forward from the scramble, in the worst case you might need to explore depth 20 — and the number of states at depth 20 is roughly 18²⁰, an incomprehensibly large number. That's not just slow, it's physically impossible to fit in any computer's memory.

### The bidirectional trick

Instead of searching depth `d` from one side, **search depth `d/2` from both sides** — forward from the scrambled cube, and backward from the solved cube — and stop the moment they meet at a common state.

Why does this help so much? Because the cost is exponential in the *depth searched*, not in the total distance:
- One-directional search to depth 10: roughly `18^10 ≈ 3.5 × 10^12` states.
- Bidirectional search, ~5 moves each side: roughly `2 × 18^5 ≈ 3.8 × 10^6` states.

That's the difference between "will never finish" and "finishes in milliseconds." This is the single most important idea behind why this solver works at all for the demo's scramble depths.

### How it's actually implemented (`Solver.cpp`)

- **Two hash maps**: `visitedF` (states reachable forward from the scramble, each mapped to the move sequence that reached it) and `visitedB` (states reachable backward from the solved cube, same idea).
- **The "adaptive" part**: at each round, the algorithm checks `frontierF.size()` vs `frontierB.size()` and expands whichever frontier is currently *smaller*. This keeps both sides roughly balanced and avoids wasting time growing an already-huge frontier — it adapts which direction to grow based on the current state of the search, rather than blindly alternating.
- **Pruning redundant moves**: while expanding, the search skips any move on the *same face* as the move that was just applied (e.g. it won't try `U` right after `U`, since two same-face moves are never optimal — they should always have been combined into a single `U2` or cancelled into nothing). This cuts the branching factor from 18 down to 15 after the first move.
- **Meeting detection**: every time a new state is generated on one side, its serialized key (a 54-character string, one digit per sticker) is checked against the *other* side's visited map. The instant there's a match, the search stops — that state is the "meeting point."

### Move reconstruction — the part people get wrong

Say the meeting point was reached:
- **Forward**: from the scrambled cube, by playing moves `pathF`.
- **Backward**: from the *solved* cube, by playing moves `pathB`.

`pathB` tells you how to go **from solved → meeting point**. To actually solve the cube, you need to go the *other way*: **meeting point → solved**. That means:

1. **Reverse the order** of `pathB` (last move first).
2. **Invert each move** (undo it): `U` becomes `U'`, `U'` becomes `U`, and `U2` stays `U2` (turning twice and turning twice the other way both bring it back).

```
final_solution = pathF + [inverse(m) for m in reversed(pathB)]
```

**Concrete example from this project's own testing**: scrambling a solved cube with `R U F' L2 D'` and feeding it back into the solver returned `D L2 F U' R'` — which is *exactly* the scramble reversed and every move inverted. That's not a coincidence; it's the textbook-correct answer, and seeing the solver independently rediscover it (rather than it being hard-coded) is the real proof the search logic works.

### Complexity, in interview-answerable terms

- **Time**: roughly `O(b^(d/2))` per side instead of `O(b^d)` for one-directional search, where `b ≈ 15-18` is the branching factor and `d` is the optimal solution length. This is the central selling point.
- **Space**: also `O(b^(d/2))`, because you must keep both visited maps in memory — bidirectional search trades memory for time savings, it doesn't reduce total work done, it reduces the *peak depth* searched.
- **Hash map lookups**: each visited-state check is `O(1)` average case, using the 54-character serialized cube state as the key.
- **Caveat worth admitting**: this simple "stop at first meeting" version is not *guaranteed* mathematically optimal in every edge case (a known subtlety of naive bidirectional BFS) — true optimality requires finishing the current depth level before accepting a meeting point. For this project's bounded scramble depths it's a non-issue, but it's an honest limitation to mention if asked.

---

## 6. Technical Grilling — 15 Tough Questions

**1. Why bidirectional BFS instead of plain BFS or DFS?**
DFS doesn't even guarantee the shortest path — it would happily go 200 moves deep before considering 3-move solutions. Plain BFS guarantees shortest path but at exponential cost in the *full* search depth. Bidirectional BFS still guarantees a short(est) path in practice, but pays the exponential cost only for *half* the depth, which is the difference between feasible and infeasible at cube-scale branching factors.

**2. What's the time/space complexity, and how does it scale with scramble depth?**
`O(b^(d/2))` time and space, where `b` is the branching factor (~15-18) and `d` is the optimal solution depth. It scales *brutally* — going from depth 9 to depth 10 in this project's own benchmark jumped the solve time from under a second to over 700ms in one run, because each extra unit of depth multiplies the work by roughly `b`.

**3. Why represent the cube as a flat 54-element array instead of objects per cubie (corner/edge/center objects)?**
A flat array of sticker colors is simpler, cache-friendly, and trivially serializable to a string (just concatenate the digits) for hashing. A move is then just a fixed permutation applied to that array — no need to model 3D cubie geometry at solve time at all. The geometry is only needed *once*, offline, to generate the permutation tables.

**4. Walk me through generating the move permutations without typing 1,000+ numbers by hand.**
Every sticker is treated as a 3D point (its cubie's `(x,y,z)` position, each in `{-1,0,1}`) plus a facing direction (the outward normal). A 90° face turn is just a rotation matrix applied to both the position and the normal of every sticker in that layer. After rotating, the new `(position, normal)` pair is mapped back to a `(face, row, col)` to find where that sticker landed. This is computed once at startup for all 18 moves and is far less error-prone than hand-deriving cycles.

**5. How do you guarantee the meeting-in-the-middle reconstruction is correct?**
By construction: the forward path is read straight from `visitedF`. The backward path, read from `visitedB`, describes "solved → meeting point," so to go the opposite direction you reverse the move order *and* invert each move. This was directly verified by scrambling with a known move sequence and confirming the solver returns exactly that sequence reversed-and-inverted.

**6. What stops bidirectional BFS from exploding in memory for harder scrambles?**
Nothing fully stops it — it's inherent to the algorithm. The implementation expands the smaller frontier each round (keeping both sides balanced) and prunes same-face-repeat moves, but for very deep scrambles (e.g. depth 15-20) this approach would still become impractical. A production solver would switch to IDA* with pattern databases instead (see Section 8).

**7. How do you know a cube state is solvable before trying to solve it?**
Real Rubik's Cubes obey a permutation-parity law: you can't validly transform a solved cube into *any* arrangement of stickers — only half of all naive sticker arrangements are physically reachable by twisting. The validator identifies each corner/edge cubie by its *set* of colors (which uniquely identifies it among the 8 corners / 12 edges), builds the permutation that maps "solved position → current position," and checks that the corner permutation's parity matches the edge permutation's parity. If they don't match, no sequence of legal twists could have produced that state.

**8. Why is corner orientation tricky, and what bug came up?**
A corner's 3 stickers have a fixed physical "handedness" (clockwise order), but if you always read a corner's stickers in a fixed axis order (X-face, then Y-face, then Z-face), that reading flips between clockwise and counter-clockwise depending on which of the 8 octants the corner sits in — it's a geometric chirality flip, not a code bug in the rotation math. During testing, a *known-solvable* scrambled cube was incorrectly rejected as "unsolvable" because the orientation/twist check didn't account for this. The fix was to stop relying on ordered-tuple rotation matching for *identity*, and instead identify each corner by its unordered color *set* (sets don't care about reading order, so the chirality problem disappears) — keeping only the permutation-parity check, which is simpler and doesn't have this fragility.

**9. Why subprocess instead of binding the C++ solver into Python directly?**
Simplicity and isolation. A subprocess crash or infinite loop is just a child process that can be killed or timed out (`subprocess.run(..., timeout=30)`) without affecting the API server. The cost is process-startup latency on every request — acceptable for a demo, but a real production system would prefer a persistent binding (`pybind11`) or a long-running solver service to avoid paying that startup cost repeatedly.

**10. What happens if Gemini returns malformed output?**
It goes through nine layers of defensive handling (decode errors → API errors → empty response → markdown-fence stripping → regex array extraction → JSON parse errors → type check → length check → per-value validation), and any failure becomes a `ValueError`/`RuntimeError` that the API turns into an HTTP 422 naming exactly which face failed and why — never a silent wrong answer.

**11. How would you scale this to 1,000 concurrent solve requests?**
Today, each request spawns a new OS process for the solver — fine at low volume, but process-spawn overhead would dominate at scale. I'd move to a pool of long-running solver workers (or a `pybind11` binding) so the binary is loaded once and reused, add a queue (e.g. Redis-backed) so requests don't all hit the solver simultaneously, and cache `/detect` results by image hash so identical photos don't re-pay for Gemini calls.

**12. Why does the solver sometimes return fewer moves than the scramble depth?**
Because a "10-move scramble" picked uniformly at random from 18 moves often contains redundant or cancelling turns (e.g. doing `R` then later effectively undoing it), so the *true* optimal solution length is frequently shorter than the number of scramble moves applied. The solver always finds the shortest path it can locate, not literally "undo the scramble move-by-move" — though by coincidence (or rather, by correctness) those often end up being the same thing for short scrambles.

**13. How does this differ from production solvers like Kociemba's two-phase algorithm?**
Production solvers use **IDA*** (iterative-deepening A*) guided by precomputed **pattern databases** — lookup tables that give a strong lower-bound estimate of "moves remaining" for partial cube states, letting the search ignore huge swaths of obviously-bad branches. That's how real solvers handle the full worst case (20 moves) in milliseconds. This project's bidirectional BFS has no heuristic at all — it's a brute-force (if halved) search, which is why it's fine for shallow scrambles but wouldn't scale to arbitrary 20-move scrambles.

**14. How do you keep the frontend and backend agreeing on cube state encoding?**
By a single fixed convention used everywhere: face order is always Top, Bottom, Front, Back, Left, Right, and a sticker's color *id* is defined as "the index of the face whose center has that color" — computed identically in `validator.py` (for the parity check) and in `main.py` (when building the 54 integers for the solver). If this convention ever drifted between layers, the solver would search forever for a "solved" state it can mathematically never reach, because its hardcoded solved-cube color scheme wouldn't match the input's color scheme.

**15. What's the hash-collision risk in the visited-state maps, and how is it avoided?**
The key is the literal 54-character string of the cube state (`'0'`-`'5'` per sticker), not a hashed/compressed digest — so there's no possibility of two *different* cube states colliding to the same key; the "hash" used by `unordered_map` is just for bucket placement, and even if two different states hashed to the same bucket, the map still compares full string equality before treating them as the same entry. The only way two entries are considered equal is if they really are the same cube state.

---

## 7. Challenges Faced

- **Getting corner orientation math right.** The geometric rotation formulas for the 6 faces were derived from scratch (treating every sticker as a 3D point + facing direction), and while the move-permutation generator worked correctly on the first real test (recovering an exact scramble inverse), a *separate* attempt to add an orientation/twist solvability check revealed a subtle chirality bug: reading a corner's 3 colors in a fixed axis order isn't consistent across all 8 octants. The fix was to simplify — identify cubies by color *set* (order-independent) rather than relying on a fragile ordered-tuple comparison, keeping the permutation-parity check correct without needing the harder orientation law at all.
- **No build tool available for CMake.** The target machine had `cmake` and `g++` but neither `mingw32-make` nor `ninja` installed, so CMake couldn't actually drive a build. Worked around it by compiling directly with `g++` (verifying it produces an identical, working binary) while still shipping a correct `CMakeLists.txt` for anyone with a proper generator installed.
- **A spec mismatch between backend and frontend.** The backend was specified with a single `/solve` endpoint (images in, solution out), but the frontend's "Review Colors" step needs to *see* detected colors before committing to solve. Resolved by adding a small, additive `/detect` endpoint and an optional `colors` override on `/solve` — neither of which breaks the originally-specified contract, they just extend it.
- **A silent `.env` encoding bug.** The project's `.env` file had been saved as UTF-16 (likely from a PowerShell redirect), which crashed `python-dotenv` with a cryptic `UnicodeDecodeError` the moment the backend started. Found by reading the raw bytes and spotting the UTF-16 byte-order-mark, then re-saving as UTF-8 while preserving the actual API key value exactly.
- **Keeping one color-encoding convention consistent across three layers.** The C++ solver's solved-cube state, the Python validator's center-based color-to-id mapping, and the backend's sticker-array construction all have to agree on "what does color id `3` mean on the Left face" — any drift here wouldn't crash anything, it would just make the solver search forever for an unreachable goal state, which is a much scarier class of bug than a crash.

---

## 8. Improvements — Ranked by Impact

1. **Replace bidirectional BFS with IDA* + pattern databases (Kociemba-style two-phase algorithm).** This is the single highest-impact change. The current solver only works comfortably for shallow scrambles; a heuristic-guided search would solve *any* scramble (up to God's Number, 20 moves) in milliseconds, removing the core limitation of the whole project.
2. **Add a real database** (schema in Section 2) for scan/solve history and optional accounts — turns this from a single-session demo into a product people can come back to.
3. **Migrate off `google-generativeai`** to the newer `google-genai` SDK — the library currently used is officially deprecated and will stop receiving fixes.
4. **Add automated tests** — C++ unit tests (e.g. Catch2/GoogleTest) for `Cube`/`Move`/`Solver`, `pytest` for the backend, and Vitest/React Testing Library for the frontend. Right now correctness was verified through manual/ad hoc testing during development; there's no regression safety net for future changes.
5. **Bind the C++ solver directly into the Python process** (e.g. `pybind11`) instead of spawning a subprocess per request — removes process-startup latency and allows a warm, reusable solver instance, which matters a lot under real traffic.
6. **Add authentication and rate limiting** on the API — today anyone can call `/solve` or `/detect` without limits, which is a direct risk to a paid, quota-limited Gemini API key.
7. **Add a real 3D animated cube preview** (e.g. Three.js / react-three-fiber) that visually rotates with each move, instead of the current static flattened-net preview — a much stronger learning aid for following along on the physical cube.
8. **Containerize with Docker Compose** — one service for the solver build, one for the backend, one for the frontend — removes exactly the kind of environment friction hit during this project (missing CMake generator, Python/Node version drift).
9. **Cache vision results by image hash** — avoid paying for a repeat Gemini call if the same face photo is resubmitted (e.g. after a transient network failure on the frontend).
10. **Stream solve progress via WebSocket/Server-Sent Events** instead of a single blocking HTTP request — useful once deeper scrambles are supported (the benchmark already showed depth-9 scrambles taking ~700ms in one run; a heuristic solver handling depth-20 could take longer on certain inputs), so the user sees "still solving…" instead of staring at a frozen button.
