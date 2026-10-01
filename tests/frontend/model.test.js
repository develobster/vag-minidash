import test from "node:test";
import assert from "node:assert/strict";
import {
  makeWidget,
  freePosition,
  moveWidget,
  relayout,
  reading,
  draftIsCompatible,
  escape,
} from "../../minidash_live/web/model.js";
const signal = {
  signal: "engine.rpm",
  label: "RPM",
  unit: "rpm",
  min: 0,
  max: 8000,
};
test("tap movement snaps to free cells, rejects collisions and bounds", () => {
  const s = { columns: 2, widgets: [] };
  const a = makeWidget(signal, s);
  s.widgets.push(a);
  const b = makeWidget(signal, s);
  s.widgets.push(b);
  assert.equal(moveWidget(s, a, 1, 0), false);
  assert.equal(moveWidget(s, a, -1, 0), false);
  assert.equal(moveWidget(s, a, 0, 1), true);
  assert.deepEqual([a.x, a.y], [0, 1]);
});
test("all preset counts retain canonical signals and deterministic geometry", () => {
  for (const count of [1, 2, 3, 4, 6, 8]) {
    const s = { columns: 2, widgets: [] };
    relayout(s, count, [signal]);
    assert.equal(s.widgets.length, count);
    assert.equal(new Set(s.widgets.map((w) => `${w.x},${w.y}`)).size, count);
    assert.ok(s.widgets.every((w) => w.signal === "engine.rpm"));
  }
});
test("resized and duplicated widgets avoid occupied rectangles", () => {
  const s = {
    columns: 2,
    widgets: [{ id: "a", x: 0, y: 0, width: 2, height: 2 }],
  };
  assert.deepEqual(freePosition(s, 2, 1), { x: 0, y: 2 });
});
test("stale, candidate, disconnected and invalid samples never look live", () => {
  assert.equal(reading({ quality: "valid", value: 42 }), 42);
  for (const quality of ["stale", "candidate", "error", "unavailable"])
    assert.equal(reading({ quality, value: 42 }), null);
  assert.equal(reading({ quality: "valid", value: 42 }, false), null);
  assert.equal(reading({ quality: "valid", value: Infinity }), null);
});
test("draft restoration requires unchanged server configuration and revision", () => {
  const server = {
    configuration: { schema_version: 1, screens: [] },
    revision: 0,
  };
  const draft = {
    configuration: { schema_version: 1 },
    base: server.configuration,
    revision: 0,
  };
  assert.ok(draftIsCompatible(draft, server));
  assert.equal(draftIsCompatible(draft, { ...server, revision: 1 }), false);
  assert.equal(
    draftIsCompatible(draft, {
      ...server,
      configuration: { schema_version: 1, screens: ["changed"] },
    }),
    false,
  );
});
test("labels from backups cannot insert markup", () =>
  assert.equal(escape('<script>"&'), "&lt;script&gt;&quot;&amp;"));
