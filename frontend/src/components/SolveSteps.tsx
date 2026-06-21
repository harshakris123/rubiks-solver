import { useState } from "react";

interface SolveStepsProps {
  moves: string[];
  descriptions: string[];
}

export default function SolveSteps({ moves, descriptions }: SolveStepsProps) {
  const [currentStep, setCurrentStep] = useState(0);
  const total = moves.length;

  if (total === 0) {
    return <p>The cube is already solved — no moves needed!</p>;
  }

  const progressPercent = ((currentStep + 1) / total) * 100;

  return (
    <div style={{ textAlign: "center" }}>
      <p>
        Step {currentStep + 1} of {total}
      </p>

      <div
        style={{
          fontSize: "4rem",
          fontWeight: 700,
          margin: "20px 0",
        }}
      >
        {moves[currentStep]}
      </div>

      <p style={{ fontSize: "1.1rem", color: "#444" }}>{descriptions[currentStep]}</p>

      <div
        style={{
          background: "#eee",
          borderRadius: 6,
          height: 10,
          margin: "20px 0",
          overflow: "hidden",
        }}
      >
        <div
          style={{
            background: "#1a1a1a",
            height: "100%",
            width: `${progressPercent}%`,
            transition: "width 0.2s ease",
          }}
        />
      </div>

      <div className="app-actions" style={{ justifyContent: "center" }}>
        <button
          className="secondary"
          disabled={currentStep === 0}
          onClick={() => setCurrentStep((s) => Math.max(0, s - 1))}
        >
          Previous
        </button>
        <button
          disabled={currentStep === total - 1}
          onClick={() => setCurrentStep((s) => Math.min(total - 1, s + 1))}
        >
          Next
        </button>
      </div>
    </div>
  );
}
