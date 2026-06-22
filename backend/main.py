"""
FastAPI backend for the Rubik's Cube Solver.

POST /solve accepts 6 base64-encoded face photos, reads sticker colors via
Gemini Vision (vision.py), validates the resulting cube state (validator.py),
and solves it by shelling out to the compiled C++ solver binary.
"""
import json
import os
import subprocess
from pathlib import Path
from typing import Dict, List, Optional

from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

import vision
from validator import FACE_ORDER, validate_cube_state

load_dotenv()

app = FastAPI(title="Rubik's Cube Solver API")

_default_origins = [
    "http://localhost:3000",
    "http://127.0.0.1:3000",
    "http://localhost:5173",
    "http://127.0.0.1:5173",
]
_extra_origins = [o.strip() for o in os.environ.get("ALLOWED_ORIGINS", "").split(",") if o.strip()]

app.add_middleware(
    CORSMiddleware,
    allow_origins=_default_origins + _extra_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

SOLVER_BINARY_PATH = os.environ.get(
    "SOLVER_BINARY_PATH",
    str(Path(__file__).resolve().parent.parent / "solver" / "build" / "solver_cli"),
)


class FaceImages(BaseModel):
    top: str
    bottom: str
    front: str
    back: str
    left: str
    right: str


class SolveRequest(FaceImages):
    # Optional pre-corrected color grids (one 9-color list per face, as
    # returned by /detect and possibly edited by the user in ColorGrid).
    # When provided, vision detection is skipped entirely for /solve.
    colors: Optional[Dict[str, List[str]]] = None


def _resolve_solver_binary() -> str:
    candidate = Path(SOLVER_BINARY_PATH)
    if candidate.exists():
        return str(candidate)
    exe_candidate = candidate.with_suffix(".exe")
    if exe_candidate.exists():
        return str(exe_candidate)
    return str(candidate)


@app.get("/health")
def health():
    return {"status": "ok"}


@app.post("/detect")
def detect(request: FaceImages):
    """Runs vision detection only (no validation, no solving) so the
    frontend can show detected colors for review/correction before /solve."""
    images = {
        "top": request.top,
        "bottom": request.bottom,
        "front": request.front,
        "back": request.back,
        "left": request.left,
        "right": request.right,
    }

    try:
        colors = vision.get_all_face_colors(images)
    except (ValueError, RuntimeError) as exc:
        raise HTTPException(status_code=422, detail=f"Vision detection failed: {exc}")

    return {"colors": colors}


@app.post("/solve")
def solve(request: SolveRequest):
    if request.colors is not None:
        faces: Dict[str, List[str]] = request.colors
    else:
        images = {
            "top": request.top,
            "bottom": request.bottom,
            "front": request.front,
            "back": request.back,
            "left": request.left,
            "right": request.right,
        }
        try:
            faces = vision.get_all_face_colors(images)
        except (ValueError, RuntimeError) as exc:
            raise HTTPException(status_code=422, detail=f"Vision detection failed: {exc}")

    try:
        validate_cube_state(faces)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))

    # Encode colors as solver-compatible IDs: a sticker's color id is the
    # index of the face whose center has that color (see validator.py docs).
    centers = [faces[name][4] for name in FACE_ORDER]
    color_to_id = {color: idx for idx, color in enumerate(centers)}
    sticker_ids = [color_to_id[faces[name][i]] for name in FACE_ORDER for i in range(9)]

    solver_path = _resolve_solver_binary()
    try:
        proc = subprocess.run(
            [solver_path],
            input=" ".join(str(x) for x in sticker_ids),
            capture_output=True,
            text=True,
            timeout=90,
        )
    except FileNotFoundError:
        raise HTTPException(
            status_code=500,
            detail=(
                f"Solver binary not found at '{solver_path}'. Build it first: "
                "see solver/CMakeLists.txt, or set SOLVER_BINARY_PATH."
            ),
        )
    except subprocess.TimeoutExpired:
        raise HTTPException(status_code=504, detail="Solver timed out")

    stdout = proc.stdout.strip()
    if not stdout:
        raise HTTPException(
            status_code=500,
            detail=f"Solver process produced no output (exit code {proc.returncode}): {proc.stderr.strip()}",
        )

    try:
        result = json.loads(stdout)
    except json.JSONDecodeError:
        raise HTTPException(status_code=500, detail=f"Solver returned invalid output: {stdout!r}")

    if "error" in result:
        raise HTTPException(status_code=500, detail=result["error"])

    return result
