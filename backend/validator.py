"""
Validates a scanned Rubik's Cube state before it is handed to the solver.

Expected input shape: a dict with keys "top", "bottom", "front", "back",
"left", "right", each mapping to a list of 9 color name strings (one of
"white", "yellow", "red", "orange", "blue", "green"), read left-to-right,
top-to-bottom for that face.

Face/sticker geometry mirrors the C++ solver's convention exactly so the
solvability check operates on the same physical model:
  - Face order: TOP=0, BOTTOM=1, FRONT=2, BACK=3, LEFT=4, RIGHT=5
  - Sticker position on a face, given row r and column c (0-2), maps to a
    3D cubie coordinate (x, y, z) each in {-1, 0, 1}:
        TOP:    (c-1,  1, r-1)
        BOTTOM: (c-1, -1, 1-r)
        FRONT:  (c-1, 1-r, 1)
        BACK:   (1-c, 1-r, -1)
        LEFT:   (-1, 1-r, c-1)
        RIGHT:  (1, 1-r, 1-c)
"""
from itertools import product
from typing import Dict, List, Tuple

VALID_COLORS = {"white", "yellow", "red", "orange", "blue", "green"}

FACE_ORDER = ["top", "bottom", "front", "back", "left", "right"]
TOP, BOTTOM, FRONT, BACK, LEFT, RIGHT = range(6)


def _row_col(face: int, x: int, y: int, z: int) -> Tuple[int, int]:
    """Inverse of the face geometry above: given a face and a 3D position
    on that face's plane, return (row, col)."""
    if face == TOP:
        return z + 1, x + 1
    if face == BOTTOM:
        return 1 - z, x + 1
    if face == FRONT:
        return 1 - y, x + 1
    if face == BACK:
        return 1 - y, 1 - x
    if face == LEFT:
        return 1 - y, z + 1
    if face == RIGHT:
        return 1 - y, 1 - z
    raise ValueError(f"invalid face index {face}")


def validate_cube_state(faces: Dict[str, List[str]]) -> None:
    """Raises ValueError with a clear message if the cube state is invalid."""

    for name in FACE_ORDER:
        if name not in faces:
            raise ValueError(f"Missing face '{name}' in cube state")
        if len(faces[name]) != 9:
            raise ValueError(f"Face '{name}' must have exactly 9 stickers, got {len(faces[name])}")
        for color in faces[name]:
            if color not in VALID_COLORS:
                raise ValueError(f"Face '{name}' has an invalid color '{color}'")

    # 1. Each color must appear exactly 9 times across the whole cube.
    counts = {c: 0 for c in VALID_COLORS}
    for name in FACE_ORDER:
        for color in faces[name]:
            counts[color] += 1
    bad = {c: n for c, n in counts.items() if n != 9}
    if bad:
        details = ", ".join(f"{c}={n}" for c, n in bad.items())
        raise ValueError(f"Each color must appear exactly 9 times. Found: {details}")

    # 2. Centers must all be different colors (they define the 6 faces).
    centers = [faces[name][4] for name in FACE_ORDER]
    if len(set(centers)) != 6:
        raise ValueError(
            "Center stickers must all be different colors, found duplicates: "
            + ", ".join(centers)
        )

    _check_solvable(faces, centers)


def _color_id_lookup(faces: Dict[str, List[str]], centers: List[str]):
    color_to_face_id = {color: idx for idx, color in enumerate(centers)}

    def color_at(face_idx: int, row: int, col: int) -> int:
        name = FACE_ORDER[face_idx]
        color_name = faces[name][row * 3 + col]
        return color_to_face_id[color_name]

    return color_at


def _check_solvable(faces: Dict[str, List[str]], centers: List[str]) -> None:
    color_at = _color_id_lookup(faces, centers)

    # ---- Corners ----
    corner_positions = list(product([1, -1], repeat=3))  # (sx, sy, sz)

    def corner_faces(sx, sy, sz):
        return (RIGHT if sx == 1 else LEFT, TOP if sy == 1 else BOTTOM, FRONT if sz == 1 else BACK)

    reference_corner_colors = []
    for sx, sy, sz in corner_positions:
        fx, fy, fz = corner_faces(sx, sy, sz)
        reference_corner_colors.append((fx, fy, fz))  # a face's own color id == its face index

    observed_corner_colors = []
    for sx, sy, sz in corner_positions:
        fx, fy, fz = corner_faces(sx, sy, sz)
        cx = color_at(fx, *_row_col(fx, sx, sy, sz))
        cy = color_at(fy, *_row_col(fy, sx, sy, sz))
        cz = color_at(fz, *_row_col(fz, sx, sy, sz))
        observed_corner_colors.append((cx, cy, cz))

    # Identify each corner cubie by its unordered color set (a cubie's set of
    # 3 colors is unique among the 8 corners). This sidesteps the geometric
    # subtlety that the "clockwise" reading order of a corner's 3 stickers
    # flips between octants of opposite chirality - orientation isn't needed
    # for a permutation-parity solvability check, only identity.
    reference_corner_sets = [frozenset(t) for t in reference_corner_colors]
    corner_perm = [None] * 8
    for i, observed in enumerate(observed_corner_colors):
        observed_set = frozenset(observed)
        match_idx = None
        for j, ref_set in enumerate(reference_corner_sets):
            if ref_set == observed_set:
                match_idx = j
                break
        if match_idx is None:
            raise ValueError("Corner colors do not match any valid cubie; scan looks inconsistent")
        corner_perm[i] = match_idx

    if len(set(corner_perm)) != 8:
        raise ValueError("Corner pieces are not a valid permutation; scan looks inconsistent")

    # ---- Edges ----
    edge_positions = []
    for coords in product([1, -1, 0], repeat=3):
        if coords.count(0) == 1:
            edge_positions.append(coords)

    def edge_faces(sx, sy, sz):
        result = []
        if sx != 0:
            result.append(RIGHT if sx == 1 else LEFT)
        if sy != 0:
            result.append(TOP if sy == 1 else BOTTOM)
        if sz != 0:
            result.append(FRONT if sz == 1 else BACK)
        return tuple(result)

    reference_edge_colors = []
    for sx, sy, sz in edge_positions:
        f1, f2 = edge_faces(sx, sy, sz)
        reference_edge_colors.append((f1, f2))

    observed_edge_colors = []
    for sx, sy, sz in edge_positions:
        f1, f2 = edge_faces(sx, sy, sz)
        c1 = color_at(f1, *_row_col(f1, sx, sy, sz))
        c2 = color_at(f2, *_row_col(f2, sx, sy, sz))
        observed_edge_colors.append((c1, c2))

    reference_edge_sets = [frozenset(t) for t in reference_edge_colors]
    edge_perm = [None] * 12
    for i, observed in enumerate(observed_edge_colors):
        observed_set = frozenset(observed)
        match_idx = None
        for j, ref_set in enumerate(reference_edge_sets):
            if ref_set == observed_set:
                match_idx = j
                break
        if match_idx is None:
            raise ValueError("Edge colors do not match any valid cubie; scan looks inconsistent")
        edge_perm[i] = match_idx

    if len(set(edge_perm)) != 12:
        raise ValueError("Edge pieces are not a valid permutation; scan looks inconsistent")

    # ---- Permutation parity law ----
    # A cube is solvable only if the corner permutation and edge permutation
    # have matching parity (both even or both odd) - swapping any two pieces
    # without rotating others is impossible on a real cube.
    def permutation_parity(perm: List[int]) -> int:
        visited = [False] * len(perm)
        parity = 0
        for i in range(len(perm)):
            if visited[i]:
                continue
            cycle_len = 0
            j = i
            while not visited[j]:
                visited[j] = True
                j = perm[j]
                cycle_len += 1
            parity += cycle_len - 1
        return parity % 2

    if permutation_parity(corner_perm) != permutation_parity(edge_perm):
        raise ValueError("Cube state is unsolvable: corner and edge permutation parity mismatch")
