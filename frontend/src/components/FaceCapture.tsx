import React, { useRef, useState } from "react";
import { Face, FaceImages, FACE_LABELS, FACE_ORDER } from "../cubeTypes";

interface FaceCaptureProps {
  onComplete: (images: FaceImages) => void;
}

function readFileAsBase64(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result as string);
    reader.onerror = () => reject(reader.error);
    reader.readAsDataURL(file);
  });
}

export default function FaceCapture({ onComplete }: FaceCaptureProps) {
  const [images, setImages] = useState<Partial<FaceImages>>({});
  const inputRefs = useRef<Partial<Record<Face, HTMLInputElement | null>>>({});

  const capturedCount = FACE_ORDER.filter((face) => images[face]).length;
  const allCaptured = capturedCount === FACE_ORDER.length;

  async function handleFileChange(face: Face, e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    const base64 = await readFileAsBase64(file);
    setImages((prev) => ({ ...prev, [face]: base64 }));
  }

  return (
    <div>
      <h2>Capture Cube Faces</h2>
      <p>
        Progress: {capturedCount}/6 faces captured
      </p>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 12 }}>
        {FACE_ORDER.map((face) => (
          <div
            key={face}
            style={{
              border: "1px solid #ddd",
              borderRadius: 8,
              padding: 10,
              textAlign: "center",
            }}
          >
            <div style={{ fontWeight: 600, marginBottom: 6 }}>{FACE_LABELS[face]}</div>
            {images[face] ? (
              <img
                src={images[face]}
                alt={`${FACE_LABELS[face]} face preview`}
                style={{ width: "100%", aspectRatio: "1 / 1", objectFit: "cover", borderRadius: 6 }}
              />
            ) : (
              <div
                style={{
                  width: "100%",
                  aspectRatio: "1 / 1",
                  background: "#f0f0f0",
                  borderRadius: 6,
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center",
                  color: "#999",
                  fontSize: "0.85rem",
                }}
              >
                No photo yet
              </div>
            )}
            <input
              ref={(el) => {
                inputRefs.current[face] = el;
              }}
              type="file"
              accept="image/*"
              capture="environment"
              style={{ display: "none" }}
              onChange={(e) => handleFileChange(face, e)}
            />
            <button
              className="secondary"
              style={{ width: "100%", marginTop: 8 }}
              onClick={() => inputRefs.current[face]?.click()}
            >
              {images[face] ? "Retake" : "Capture"}
            </button>
          </div>
        ))}
      </div>

      <div className="app-actions">
        <button
          disabled={!allCaptured}
          onClick={() => onComplete(images as FaceImages)}
        >
          Continue ({capturedCount}/6)
        </button>
      </div>
    </div>
  );
}
