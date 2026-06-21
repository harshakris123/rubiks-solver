import { ColorName, COLOR_HEX, COLOR_NAMES, Face, FACE_LABELS } from "../cubeTypes";

interface ColorGridProps {
  face: Face;
  colors: ColorName[]; // 9 entries, left-to-right, top-to-bottom
  onChange: (index: number, newColor: ColorName) => void;
}

export default function ColorGrid({ face, colors, onChange }: ColorGridProps) {
  function cycleColor(index: number) {
    const current = colors[index];
    const currentIdx = COLOR_NAMES.indexOf(current);
    const next = COLOR_NAMES[(currentIdx + 1) % COLOR_NAMES.length];
    onChange(index, next);
  }

  return (
    <div style={{ textAlign: "center" }}>
      <div style={{ fontWeight: 600, marginBottom: 6 }}>{FACE_LABELS[face]}</div>
      <div
        style={{
          display: "grid",
          gridTemplateColumns: "repeat(3, 1fr)",
          gap: 4,
          width: 96,
          margin: "0 auto",
        }}
      >
        {colors.map((color, i) => (
          <div
            key={i}
            role="button"
            title="Click to correct this sticker's color"
            onClick={() => cycleColor(i)}
            style={{
              width: 28,
              height: 28,
              background: COLOR_HEX[color],
              border: "1px solid #999",
              borderRadius: 4,
              cursor: "pointer",
            }}
          />
        ))}
      </div>
    </div>
  );
}
