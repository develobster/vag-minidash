import test from "node:test";
import assert from "node:assert/strict";
import {
  convert,
  preferred,
  setUnitsPreset,
  dateTime,
} from "../../minidash_live/web/units.js";
test("temperature, speed and absolute/relative pressure use one conversion model", () => {
  assert.equal(convert(100, "°C", "°F"), 212);
  assert.ok(Math.abs(convert(100, "km/h", "mph") - 62.137119) < 0.0001);
  assert.equal(convert(1000, "hPa", "bar"), 1);
  assert.ok(Math.abs(convert(1, "bar", "psi") - 14.503774) < 0.0001);
  assert.equal(convert(14.2, "V", "V"), 14.2);
});
test("unit presets change preferences without modifying canonical input", () => {
  const general = {};
  setUnitsPreset(general, "imperial");
  const row = { value: 100, unit: "°C" };
  assert.deepEqual(preferred(row.value, row.unit, general), {
    value: 212,
    unit: "°F",
  });
  assert.equal(row.value, 100);
  setUnitsPreset(general, "metric");
  assert.equal(general.speed, "km/h");
});
test("time presentation respects timezone and format", () => {
  const text = dateTime("2026-10-01T00:00:00Z", {
    locale: "en",
    timezone: "Europe/Minsk",
    time_format: "24h",
  });
  assert.ok(text.includes("03:00:00"));
});
