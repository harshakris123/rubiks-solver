export type Face = "top" | "bottom" | "front" | "back" | "left" | "right";

export const FACE_ORDER: Face[] = ["top", "bottom", "front", "back", "left", "right"];

export const FACE_LABELS: Record<Face, string> = {
  top: "Top",
  bottom: "Bottom",
  front: "Front",
  back: "Back",
  left: "Left",
  right: "Right",
};

export type ColorName = "white" | "yellow" | "red" | "orange" | "blue" | "green";

export const COLOR_NAMES: ColorName[] = ["white", "yellow", "red", "orange", "blue", "green"];

export const COLOR_HEX: Record<ColorName, string> = {
  white: "#FFFFFF",
  yellow: "#FFD500",
  red: "#C41E3A",
  orange: "#FF5800",
  blue: "#003680",
  green: "#009B48",
};

export type FaceColors = Record<Face, ColorName[]>;
export type FaceImages = Record<Face, string>;

export interface SolveResult {
  moves: string[];
  descriptions: string[];
  solve_time_ms: number;
  total_moves: number;
}
