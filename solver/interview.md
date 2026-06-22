# Solver design notes

This file exists to explain *why* the solver is built the way it is — the
kind of thing worth being able to talk through in an interview, not just
"here's the code." It covers: why the original bidirectional BFS solver
couldn't handle real cubes, how IDA* + pattern databases fix that, how the
databases are indexed and built, and the bugs that came up implementing it
(some of which are the kind of thing that's easy to get subtly wrong and
hard to notice without rigorous testing).

## The problem: naive search doesn't scale

The first version of this solver did bidirectional BFS: search forward from
the scrambled cube and backward from the solved state simultaneously,
hashing every visited state by its 54-character sticker string, and stopping
when the two searches meet in the middle. It's a clean idea and it's *fast*
for lightly scrambled cubes (depth ≤ 8 or so), but it has no way to know
which branches are worth exploring — every one of the ~15 legal next moves
(18 total, minus the redundant "same face as the last move" turns) looks
equally promising. That branching factor means each BFS layer is roughly 15x
bigger than the last. A cube scrambled by hand typically needs **18-20
moves** to solve optimally (20 is the proven worst case, "God's number," in
the half-turn metric) — and a search that explodes 15x per layer, with nodes
stored as hashed strings in `unordered_map<string, vector<string>>`, runs out
of time and memory long before it gets that deep. That's exactly what
surfaced in production: real scans (not the depth ≤10 scrambles in the old
benchmark) reliably hit the backend's 30-second subprocess timeout.

The fix isn't a bigger timeout or a deeper search bound — it's giving the
search a sense of direction.

## The fix: IDA* with an admissible heuristic

IDA* (iterative-deepening A*) is depth-first search with a cutoff on
`f = g + h`, where `g` is moves taken so far and `h` is a lower-bound
estimate of moves still needed. Raise the cutoff by 1 and retry whenever a
pass fails to find a solution. If `h` is *admissible* (never overestimates
the true remaining distance), this is guaranteed to find an optimal
solution, and a good `h` prunes almost every branch that can't possibly
reach the goal within the current bound — turning "explores everything" into
"explores almost nothing."

The classic way to get a strong, fast-to-evaluate `h` for Rubik's Cube is a
**pattern database**: precompute, once, the exact minimum move-count to
solve every state of some *sub-problem* (e.g. "ignore the edges, just solve
the corners"), then use that exact sub-problem distance as a lower bound on
the full problem's distance — solving everything can't take fewer moves than
solving any one piece of it. Look it up by array index, not by search, at
solve time.

This solver builds three such tables (Korf's classic combination):

- **Corner database** — full state of all 8 corners (which corner sits in
  which slot, and its orientation), ignoring edges entirely.
  `8! × 3⁷ = 88,179,840` reachable states.
- **Edge database, group 0** — only 6 of the 12 edges (identities 0-5) are
  "tracked"; the table records the minimum moves to get those 6 into their
  correct slots and orientations, ignoring where the other 6 end up.
  `P(12,6) × 2⁶ = 42,577,920` states.
- **Edge database, group 1** — the same thing for the *other* 6 edges
  (identities 6-11).

`h(state) = max(cornerTable[...], edge0Table[...], edge1Table[...])` is the
heuristic IDA* uses.

This wasn't the first version. The first version tracked only one edge
group, on the theory that "explodes past depth ~10" -> "solves in well
under a second" was already the win that mattered, at roughly half the
implementation/memory/build-time cost of tracking both edge halves. That
held up at moderate depths (instant up to depth 15) but not at the depths a
real scrambled cube actually needs: depth 18 took **27.7 seconds** with one
edge table — comfortably enough to blow through the backend's solve
timeout that started this whole rewrite. The second edge table is what
actually closes that gap (see the benchmark table in the main README) — a
reminder that "feels fast on my test cases" and "fast at the depths that
matter in production" are different claims, and only one of them is
verified by testing depths the original bug report (cubes scrambled by
hand, 18-20 moves) actually contains.

## Optimal IDA* has no bounded worst case — and that's not a bug

Even with both edge tables, some depth-18 scrambles are still slow.
Profiling one (`bound`/`nodes`/`elapsed` printed per IDA* iteration) showed
node count growing roughly **14x per bound increment** — 605K nodes at
bound 12, 8.6M at bound 13. Extrapolating, an iteration at bound 15-16
would need on the order of a billion+ nodes — minutes, not seconds.

This is a real, well-documented property of Korf's optimal IDA*, not a
bug introduced here: the algorithm is guaranteed to find the *shortest*
solution if it finishes, but gives no guarantee on *how long that takes* —
some specific scrambles are simply harder for this heuristic than others.
It's exactly why Kociemba's two-phase algorithm exists as a *separate*
technique (group-coset decomposition into two smaller, well-behaved IDA*
searches) rather than a tuning fix on top of Korf's approach.

Two fallback ideas were tried and both failed in ways worth recording:

- **Greedy descent** (always take the move that most reduces `h`, no
  backtracking): stalls in plateaus. Over 80 moves on a stuck case, `h`
  barely moved (8 → 7-ish, with brief dips back up). The heuristic is a
  `max` of three independent lower bounds; locally minimizing that max
  doesn't reliably correspond to making progress on the whole cube, so
  greedy hill-climbing has nothing to follow once it hits a state where no
  single move improves any of the three components.
- **Weighted IDA\*** (inflate `h` by a constant factor to prune more
  aggressively): made things *worse*, not better, at weights of 2, 3, and
  20. Inflating `h` makes `f = g + weight*h` grow in coarser steps, so the
  bound-increment-by-minimum-excess scheme this IDA* uses needs *more*
  outer iterations to cover the same real search depth — and each
  iteration's cost didn't shrink enough to compensate. (Weighted A* does
  work in general; this particular bound-stepping interacted badly with
  large weights specifically.)

Both are real, individually defensible ideas that didn't survive contact
with measurement — which is itself the point: "this should work" isn't
evidence, re-running the same depth-18 cases after each change is. The
decision made here was to keep the single-phase optimal IDA* (no fallback)
and raise the backend's subprocess timeout generously (90s) instead of
chasing a bounded-worst-case fix that, done properly, is Kociemba's
algorithm under a different name. Most real (hand-scrambled) cubes solve
in well under a second; occasional unlucky ones take up to tens of
seconds; a small number of pathological cases could in principle still
exceed even a generous timeout. That residual risk is accepted here in
exchange for staying within one well-understood, explainable algorithm
rather than bolting on a partially-working patch.

## How a corner/edge sub-state becomes an array index

Pattern databases are only fast because of *how* they're indexed: a flat
array, looked up by direct integer index, no hashing.

**Corners.** A permutation of 8 distinct corners has a well-known canonical
rank in `[0, 8!)` via its **Lehmer code**: for each position, count how many
later elements are smaller, then combine those counts in mixed-radix
(factorial number system). Each of the 8 corners also has an orientation in
`{0,1,2}` — but only 7 of those 8 values are stored, since the 8th is always
determined by the others (corner-orientation sum is invariant mod 3 on any
reachable cube state). So: `index = permRank(perm) × 3⁷ + Σ orientation[i]·3ⁱ`
for `i` in `0..6`.

**Edges.** Only 6 of the 12 edges are tracked at a time, so the index needs
three pieces: *which* 6 of the 12 slots currently hold a tracked edge (a
**combinadic** rank in `[0, C(12,6))` — the same idea as the Lehmer code but
for choosing a subset instead of ordering a sequence), the relative order of
*which* tracked edge sits in each of those slots (a Lehmer-code rank again,
this time in `[0, 6!)`), and a 6-bit mask of their orientations. Multiply
those three ranges together and you get all `P(12,6) × 2⁶` states. Group 0
and group 1 share this exact indexing logic — the only difference is which
6 edge identities count as "tracked" (`edgeIdentity - group*6` in `[0,6)`),
so one function drives both tables.

Both rank functions are pure combinatorics with no cube-specific logic, so
they're tested in isolation (`test_combinatorics.cpp`) against brute-force
enumeration before ever touching real cube states — generate every
permutation of `{0,...,N-1}` via `std::next_permutation`, rank each one,
and check the ranks are a bijection onto `[0, N!)`. Same idea for combinadic
ranks against `std::next_permutation` over a 0/1 mask. This caught nothing
in the end (the implementation was right), but given what *did* go wrong
below, it was worth ruling out as a suspect early.

## Building the tables: BFS in coordinate space, not sticker space

The databases are built via plain BFS from the solved state — but critically,
*not* by replaying the existing 54-sticker `Cube::applyMove` and re-deriving
corner/edge coordinates after every move. That's correct but far too slow:
generating 88M + 42.5M + 42.5M states means ~2.4 billion move applications,
and each sticker-level apply-and-re-extract costs orders of magnitude more
than a direct coordinate-space update.

Instead, `Coord.cpp` extracts each of the 18 standard moves' effect
*directly in coordinate space* once (by applying that one move to a solved
cube and reading off the resulting corner/edge coordinates — call this the
move's "coordinate-space delta"), then composes that delta with the current
state on every subsequent application:

```
newPerm[slot]       = oldPerm[moveTable.perm[slot]]
newOrientation[slot] = (oldOrientation[moveTable.perm[slot]] + moveTable.oriDelta[slot]) % {3 or 2}
```

This is `O(8)` or `O(12)` per move application instead of `O(54)` plus a
re-extraction pass, which is what makes generating 130M+ states from scratch
in coordinate space tractable in minutes rather than hours. (It also means
the BFS for the corner table never touches edge data at all, and vice
versa — each table's BFS runs entirely over its own reduced state space.)

The BFS itself stores one nibble (4 bits) per state — a move-distance never
exceeds 11, so a full byte would waste half the table — bringing the three
tables to about 42 MB, 20 MB, and 20 MB on disk. It also skips applying two
consecutive moves on the same face: `U U'` is a no-op, `U U` equals `U2`,
`U U2` equals `U'`, so no optimal path ever needs two same-face turns in a
row. That's a free ~1/18 reduction in branching with no risk of missing a
shorter path, since BFS already finds the *shortest* route to every state
regardless of which redundant detours it declines to explore.

## Bugs that came up (and how they were caught)

**Corner orientation chirality.** The first version of corner-coordinate
extraction read each corner's 3 sticker colors in a fixed `(x, y, z)` axis
order and compared that triple's 3 cyclic rotations against a reference
table, on the theory that a corner can only be rotated, never reflected, by
a legal move. That's true *physically* — but reading the 3 facelets in a
fixed global axis order doesn't correspond to a consistent "clockwise as
seen from outside the cube" traversal at every corner position. Whether
`(x, y, z)` order is clockwise or counter-clockwise flips with the sign of
`x·y·z` — i.e., with the corner's octant. Two physically-equivalent corners
in different octants would read as mirror images of each other, which broke
exactly the "only rotates, never reflects" assumption the orientation logic
depended on.

This surfaced immediately and loudly: a regression test
(`test_coord.cpp`) applies random sequences of moves both via the existing,
already-trusted full-sticker simulation and via the new coordinate-space
composition, and asserts they agree after every single move. They diverged
on move 2 of the very first trial — duplicate corner identities appearing
in the extracted state, which is impossible on a real cube. The fix: read
each corner's facelets in `(x, y, z)` order when `x·y·z = 1`, and
`(x, z, y)` order (swap the last two) when `x·y·z = -1`, restoring a
consistent chirality. After that fix, 200 trials of 30 random moves each
matched exactly.

**Why this kind of bug is worth calling out specifically:** it wouldn't have
caused a crash or an obviously-wrong answer in casual testing — it would
have silently corrupted the pattern database with wrong distances for some
fraction of states, which could make the solver return suboptimal solutions,
or in the worst case fail to find any solution within the search bound, for
specific cube states that happened to hit the broken cases. The kind of bug
that passes a quick smoke test and fails in production on someone's actual
scrambled cube. The lesson generalizes beyond this project: when hand-deriving
a coordinate system from geometry, test the *invariants* the rest of the
algorithm depends on (here: "rotation only, never reflection") directly and
exhaustively, rather than testing a few example outputs and assuming they
generalize.

## A non-cube bug: MinGW DLL mismatch at -O2

Not every bug here was about cube geometry. After both pattern databases
built and verified correctly, `solver_cli` segfaulted on every single
invocation — even solving an already-solved cube, which returns before
ever touching the pattern-database heuristic. Bisection (cutting the
reproduction down file by file) eventually produced a two-file minimal
case with nothing cube-related at all: one file calling a function in
another file that does nothing but `std::ifstream` open + `std::vector::resize`
+ `.read()`. That crashed at `-O2` and worked at `-O0`, on this exact
machine, every time.

That signature — works unoptimized, crashes optimized, as soon as a call
crosses a translation-unit (and thus potential DLL) boundary — points at a
runtime mismatch rather than a logic bug: this Windows machine had more than
one MSYS2/MinGW toolchain on `PATH`, so the binary could end up dynamically
linked against a different libstdc++/libgcc DLL than it was compiled
against. `-static -static-libstdc++ -static-libgcc` statically links the
C++ runtime into the binary instead of resolving it at load time, which
made the crash disappear immediately. It's irrelevant on Linux (where this
actually deploys, via `backend/Dockerfile`'s single `gcc:13-bookworm` build
stage — no DLL-equivalent mismatch is possible there) and harmless to leave
on for every platform, so it's just always on now (see `CMakeLists.txt`'s
`if(MINGW)` block and the build commands in the main README).

Worth noting because of how it looked at first: a heuristic/search bug
report ("solver crashes") that turned out to have nothing to do with the
solver at all — the kind of thing where bisecting down to the smallest
possible reproduction (here, two files and four lines) is the fastest way
to tell "my algorithm is wrong" apart from "my toolchain is lying to me."

## Correctness checks, end to end

- `test_combinatorics.cpp` — Lehmer-code and combinadic rank functions are
  bijections onto their expected ranges, checked by brute-force enumeration.
- `test_coord.cpp` — coordinate-space move composition matches full
  54-sticker simulation across many random move sequences; solved cube
  extracts to the identity; every move composed with its named inverse
  returns to solved.
- `test_pdb.cpp` — all three tables fully cover their state space (no entry
  left unvisited), the solved state is distance 0, every single move from
  solved is distance 1, and the known published maximum distances for these
  exact pattern databases (corner: 11, each 6-edge subset: 10) come out
  correctly — a strong independent signal the indexing and BFS are both
  right, not just self-consistent.
- `Solver::solve` itself terminates on `state.isSolved()` (checked against
  the *full* 8-corner + 12-edge state), not on `h == 0` — `h` reaching 0 only
  means the corners and both edge groups individually look solved by their
  own table's bookkeeping; checking the real, full state directly is what
  actually guarantees correctness regardless of how the heuristic is scoped.
- Depth-by-depth timing (`depth_test.cpp`, ad hoc — not checked in) is what
  caught the one-edge-table gap in the first place: 0.003s at depth 12,
  0.29s at depth 15, 27.7s at depth 18. Without measuring at the depth the
  actual bug report cared about, the one-table version would have shipped
  looking like a complete fix.
