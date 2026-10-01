export const TYPES = ["numeric", "gauge", "bar", "compact", "graph"];
export const escape = (v) =>
  String(v ?? "").replace(
    /[&<>"']/g,
    (c) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[
        c
      ],
  );
export const uid = () =>
  globalThis.crypto?.randomUUID?.() ||
  `${Date.now()}-${Math.random().toString(16).slice(2)}`;
export function freePosition(screen, width = 1, height = 1, ignore = null) {
  for (let y = 0; y + height <= 24; y++)
    for (let x = 0; x + width <= screen.columns; x++) {
      if (
        !screen.widgets.some(
          (w) =>
            w.id !== ignore &&
            x < w.x + w.width &&
            x + width > w.x &&
            y < w.y + w.height &&
            y + height > w.y,
        )
      )
        return { x, y };
    }
  throw Error("No free grid space. Remove a widget first.");
}
export function makeWidget(signal, screen) {
  const low = signal.min ?? 0,
    high = signal.max ?? 100;
  return {
    id: uid(),
    signal: signal.signal,
    label: signal.label || signal.signal,
    units: signal.unit || "",
    type: "numeric",
    width: 1,
    height: 1,
    ...freePosition(screen),
    min: low,
    max: high,
    warning: low + (high - low) * 0.8,
    critical: low + (high - low) * 0.9,
    precision: 0,
  };
}
export function relayout(screen, count, signals) {
  screen.columns = count === 1 ? 1 : count === 3 ? 3 : 2;
  screen.widgets = Array.from({ length: count }, (_, i) => {
    const old = screen.widgets[i];
    return {
      ...(old ||
        makeWidget(signals[i % signals.length], {
          columns: screen.columns,
          widgets: [],
        })),
      x: i % screen.columns,
      y: Math.floor(i / screen.columns),
      width: 1,
      height: 1,
    };
  });
}
export function moveWidget(screen, w, dx, dy) {
  const x = w.x + dx,
    y = w.y + dy;
  if (x < 0 || y < 0 || x + w.width > screen.columns || y + w.height > 24)
    return false;
  if (
    screen.widgets.some(
      (other) =>
        other.id !== w.id &&
        x < other.x + other.width &&
        x + w.width > other.x &&
        y < other.y + other.height &&
        y + w.height > other.y,
    )
  )
    return false;
  w.x = x;
  w.y = y;
  return true;
}
export function reading(row, connected = true) {
  return connected && row?.quality === "valid" && Number.isFinite(row.value)
    ? row.value
    : null;
}
export function draftIsCompatible(draft, server) {
  return (
    draft?.configuration?.schema_version ===
      server.configuration.schema_version &&
    draft.revision === server.revision &&
    JSON.stringify(draft.base) === JSON.stringify(server.configuration)
  );
}
export function displayReading(row, connected = true) {
  if (!connected || row?.quality !== "valid") return null;
  if (typeof row.value === "string") return row.value;
  return reading(row, connected);
}
