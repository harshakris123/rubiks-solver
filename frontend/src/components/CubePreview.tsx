import { ColorName, COLOR_HEX, Face, FaceColors } from "../cubeTypes";

interface CubePreviewProps {
  faces: FaceColors;
}

function MiniFace({ colors }: { colors: ColorName[] }) {
  return (
    <div
      style={{
        display: "grid",
        gridTemplateColumns: "repeat(3, 1fr)",
        gap: 1,
        width: 36,
        height: 36,
        border: "1px solid #333",
      }}
    >
      {colors.map((color, i) => (
        <div key={i} style={{ background: COLOR_HEX[color] }} />
      ))}
    </div>
  );
}

// Renders the 6 faces as a flattened cross-shaped net:
//        [top]
// [left] [front] [right] [back]
//        [bottom]
export default function CubePreview({ faces }: CubePreviewProps) {
  const cell = (face: Face) => <MiniFace colors={faces[face]} />;

  return (
    <div style={{ display: "inline-block" }}>
      <div style={{ display: "flex", justifyContent: "center" }}>{cell("top")}</div>
      <div style={{ display: "flex" }}>
        {cell("left")}
        {cell("front")}
        {cell("right")}
        {cell("back")}
      </div>
      <div style={{ display: "flex", justifyContent: "center" }}>{cell("bottom")}</div>
    </div>
  );
}
