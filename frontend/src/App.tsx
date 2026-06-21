import { useState } from "react";
import FaceCapture from "./components/FaceCapture";
import ColorGrid from "./components/ColorGrid";
import CubePreview from "./components/CubePreview";
import SolveSteps from "./components/SolveSteps";
import {
  ColorName,
  Face,
  FaceColors,
  FaceImages,
  FACE_ORDER,
  SolveResult,
} from "./cubeTypes";

type Stage = "capture" | "detecting" | "review" | "solving" | "steps";

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL ?? "http://localhost:8000";

export default function App() {
  const [stage, setStage] = useState<Stage>("capture");
  const [images, setImages] = useState<FaceImages | null>(null);
  const [faceColors, setFaceColors] = useState<FaceColors | null>(null);
  const [result, setResult] = useState<SolveResult | null>(null);
  const [error, setError] = useState<string | null>(null);

  async function handleCaptureComplete(capturedImages: FaceImages) {
    setImages(capturedImages);
    setError(null);
    setStage("detecting");

    try {
      const response = await fetch(`${API_BASE_URL}/detect`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(capturedImages),
      });
      const data = await response.json();
      if (!response.ok) {
        throw new Error(data.detail ?? "Color detection failed");
      }
      setFaceColors(data.colors as FaceColors);
      setStage("review");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Color detection failed");
      setStage("capture");
    }
  }

  function handleColorChange(face: Face, index: number, newColor: ColorName) {
    if (!faceColors) return;
    const updated = { ...faceColors, [face]: [...faceColors[face]] };
    updated[face][index] = newColor;
    setFaceColors(updated);
  }

  async function handleSolve() {
    if (!images || !faceColors) return;
    setError(null);
    setStage("solving");

    try {
      const response = await fetch(`${API_BASE_URL}/solve`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ...images, colors: faceColors }),
      });
      const data = await response.json();
      if (!response.ok) {
        throw new Error(data.detail ?? "Solve request failed");
      }
      setResult(data as SolveResult);
      setStage("steps");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Solve request failed");
      setStage("review");
    }
  }

  function handleStartOver() {
    setStage("capture");
    setImages(null);
    setFaceColors(null);
    setResult(null);
    setError(null);
  }

  return (
    <div>
      <h1>Rubik's Cube Solver</h1>

      {error && <div className="app-error">{error}</div>}

      {stage === "capture" && <FaceCapture onComplete={handleCaptureComplete} />}

      {stage === "detecting" && <div className="app-loading">Detecting sticker colors…</div>}

      {stage === "review" && faceColors && (
        <div>
          <h2>Review Colors</h2>
          <p>Tap any square to correct a misdetected color, then solve.</p>

          <div style={{ display: "flex", justifyContent: "center", marginBottom: 20 }}>
            <CubePreview faces={faceColors} />
          </div>

          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 16 }}>
            {FACE_ORDER.map((face) => (
              <ColorGrid
                key={face}
                face={face}
                colors={faceColors[face]}
                onChange={(index, newColor) => handleColorChange(face, index, newColor)}
              />
            ))}
          </div>

          <div className="app-actions">
            <button className="secondary" onClick={handleStartOver}>
              Start Over
            </button>
            <button onClick={handleSolve}>Solve</button>
          </div>
        </div>
      )}

      {stage === "solving" && <div className="app-loading">Solving cube…</div>}

      {stage === "steps" && result && (
        <div>
          <h2>Solution</h2>
          <p style={{ color: "#666" }}>
            Solved in {result.total_moves} moves ({result.solve_time_ms.toFixed(2)} ms)
          </p>
          <SolveSteps moves={result.moves} descriptions={result.descriptions} />
          <div className="app-actions">
            <button className="secondary" onClick={handleStartOver}>
              Solve Another Cube
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
