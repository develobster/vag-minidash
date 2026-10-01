// One conversion boundary; stored canonical values, thresholds and recordings stay unchanged.
const conversions = {
  "°C": { family: "temperature", toBase: (v) => v, fromBase: (v) => v },
  degC: { family: "temperature", toBase: (v) => v, fromBase: (v) => v },
  "°F": {
    family: "temperature",
    toBase: (v) => (v - 32) / 1.8,
    fromBase: (v) => v * 1.8 + 32,
  },
  "km/h": { family: "speed", toBase: (v) => v, fromBase: (v) => v },
  mph: {
    family: "speed",
    toBase: (v) => v * 1.609344,
    fromBase: (v) => v / 1.609344,
  },
  hPa: { family: "pressure", toBase: (v) => v / 10, fromBase: (v) => v * 10 },
  kPa: { family: "pressure", toBase: (v) => v, fromBase: (v) => v },
  bar: { family: "pressure", toBase: (v) => v * 100, fromBase: (v) => v / 100 },
  psi: {
    family: "pressure",
    toBase: (v) => v * 6.894757293,
    fromBase: (v) => v / 6.894757293,
  },
};
export function unitFor(unit, general) {
  const family = conversions[unit]?.family;
  return family ? general[family] : unit;
}
export function convert(value, from, to) {
  if (
    typeof value !== "number" ||
    !conversions[from] ||
    !conversions[to] ||
    conversions[from].family !== conversions[to].family
  )
    return value;
  return conversions[to].fromBase(conversions[from].toBase(value));
}
export function preferred(value, unit, general) {
  const target = unitFor(unit, general);
  return { value: convert(value, unit, target), unit: target };
}
export function setUnitsPreset(general, preset) {
  general.units = preset;
  if (preset === "metric")
    Object.assign(general, {
      temperature: "°C",
      pressure: "bar",
      speed: "km/h",
    });
  if (preset === "imperial")
    Object.assign(general, {
      temperature: "°F",
      pressure: "psi",
      speed: "mph",
    });
}
export function dateTime(value, general) {
  return new Intl.DateTimeFormat(general.locale, {
    dateStyle: "short",
    timeStyle: "medium",
    timeZone: general.timezone,
    hour12: general.time_format === "12h",
  }).format(new Date(value));
}
