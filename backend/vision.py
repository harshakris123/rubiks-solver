"""
Uses Google Gemini Vision (gemini-2.5-flash) to read the 9 sticker colors
off a single photographed Rubik's Cube face.
"""
import base64
import json
import os
import re
from typing import List

import google.generativeai as genai

VALID_COLORS = {"white", "yellow", "red", "orange", "blue", "green"}

PROMPT = (
    "Look at this Rubik's Cube face. Return ONLY a JSON array of exactly 9 "
    "color names representing the 3x3 grid of stickers, read left to right, "
    "top to bottom. Use only these color names: white, yellow, red, orange, "
    "blue, green. Return nothing else, no explanation, no markdown."
)

_model = None


def _get_model():
    global _model
    if _model is None:
        api_key = os.environ.get("GEMINI_API_KEY")
        if not api_key:
            raise RuntimeError("GEMINI_API_KEY environment variable is not set")
        genai.configure(api_key=api_key)
        _model = genai.GenerativeModel("gemini-2.5-flash")
    return _model


def _decode_image(base64_image: str) -> bytes:
    # Strip a data URL prefix like "data:image/jpeg;base64,..." if present.
    if "," in base64_image and base64_image.strip().startswith("data:"):
        base64_image = base64_image.split(",", 1)[1]
    try:
        return base64.b64decode(base64_image, validate=False)
    except Exception as exc:
        raise ValueError(f"Could not decode base64 image data: {exc}") from exc


def _extract_json_array(text: str) -> List[str]:
    text = text.strip()
    # Defensively strip markdown code fences if the model adds them anyway.
    text = re.sub(r"^```(?:json)?", "", text).strip()
    text = re.sub(r"```$", "", text).strip()

    match = re.search(r"\[.*\]", text, re.DOTALL)
    if not match:
        raise ValueError(f"Model response did not contain a JSON array: {text!r}")

    try:
        parsed = json.loads(match.group(0))
    except json.JSONDecodeError as exc:
        raise ValueError(f"Model response was not valid JSON: {text!r}") from exc

    if not isinstance(parsed, list):
        raise ValueError(f"Expected a JSON array, got: {parsed!r}")

    return parsed


def get_face_colors(base64_image: str) -> List[str]:
    """Returns a list of 9 color name strings for one cube face, read
    left-to-right, top-to-bottom. Raises ValueError on unexpected output."""
    image_bytes = _decode_image(base64_image)
    model = _get_model()

    try:
        response = model.generate_content(
            [PROMPT, {"mime_type": "image/jpeg", "data": image_bytes}]
        )
    except Exception as exc:
        raise RuntimeError(f"Gemini Vision API request failed: {exc}") from exc

    if not response.text:
        raise ValueError("Gemini Vision API returned an empty response")

    colors = _extract_json_array(response.text)

    if len(colors) != 9:
        raise ValueError(f"Expected 9 colors, got {len(colors)}: {colors}")

    normalized = []
    for c in colors:
        if not isinstance(c, str):
            raise ValueError(f"Expected color name strings, got: {c!r}")
        name = c.strip().lower()
        if name not in VALID_COLORS:
            raise ValueError(f"Unexpected color name '{c}' (expected one of {sorted(VALID_COLORS)})")
        normalized.append(name)

    return normalized
