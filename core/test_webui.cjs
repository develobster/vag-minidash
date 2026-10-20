/* Browser-independent geometry/import guard tests against the shipped HTML.
 * Real DOM, API, keyboard, pointer and mobile flows: test_webui_browser.cjs. */
const fs = require("node:fs"),
  vm = require("node:vm"),
  assert = require("node:assert/strict");
const path = require("node:path");
const fixture = JSON.parse(
  fs.readFileSync(process.argv[2], "utf8").replace(/^\uFEFF/, ""),
);
const html = fs.readFileSync(
  path.join(__dirname, "../firmware/main/webui.html"),
  "utf8",
);
const source = html.match(/<script>([\s\S]*)<\/script>/)[1];
new vm.Script(source);
const start = source.indexOf("const geom"),
  end = source.indexOf("function checkpoint", start);
assert.ok(start >= 0 && end > start, "editor model extraction markers");
const context = {
  ...structuredClone(fixture),
  cfg: structuredClone(fixture.config),
  TextEncoder,
  portrait: false,
};
context.port = () => context.portrait;
vm.createContext(context);
new vm.Script(source.slice(start, end)).runInContext(context);
const run = (code) => vm.runInContext(code, context);
assert.equal(run("problems().length"), 0, "current native fixture accepted");
assert.equal(fixture.themes.length, 17);
assert.deepEqual(
  fixture.themes.slice(11).map((t) => t.name),
  [
    "Pixel Arcade",
    "Pixel Dungeon",
    "Pixel Handheld",
    "Desktop 98",
    "Neon Arcade",
    "Techno 2000",
  ],
);
assert.equal(fixture.themes[14].style.font_family, 1);
assert.equal(fixture.themes[14].style.frame_kind, 8);
assert.ok(
  fixture.themes[15].style.glow_width > 0 &&
    fixture.themes[15].style.scanline_opacity > 0,
);
assert.ok(
  fixture.themes[16].style.radius > 0 &&
    fixture.themes[16].style.frame_kind === 9,
);
context.p = {
  layout: 0,
  widgets: [{ column: 0, row: 0, columns: 3, rows: 2 }],
};
assert.equal(run("fits(p,0,{column:9,row:6,columns:3,rows:2})"), true);
for (const w of [
  { column: 10, row: 0, columns: 3, rows: 2 },
  { column: 0, row: 7, columns: 3, rows: 2 },
  { column: 0, row: 0, columns: 2, rows: 2 },
  { column: 0.5, row: 0, columns: 3, rows: 2 },
]) {
  context.w = w;
  assert.equal(run("fits(p,0,w)"), false);
}
assert.equal(run("fits(p,-1,p.widgets[0])"), false, "overlap rejected");
assert.equal(
  JSON.stringify(run("free(p,p.widgets[0])")),
  JSON.stringify({ column: 3, row: 0, columns: 3, rows: 2 }),
);
run("p.widgets=[{column:0,row:0,columns:12,rows:8}]");
assert.equal(run("free(p,{column:0,row:0,columns:3,rows:2})"), null);
context.portrait = true;
context.w = { column: 3, row: 2, columns: 6, rows: 4 };
assert.equal(
  JSON.stringify(run("view(view(w))")),
  JSON.stringify(context.w),
  "portrait transform is reversible",
);
for (const mutate of [
  "cfg.common.pages[0].widgets[0].precision=4",
  "cfg.common.pages[0].widgets[0].minimum=cfg.common.pages[0].widgets[0].maximum",
  "cfg.common.pages[0].title='Я'.repeat(16)",
  "cfg.common.pages[0].widgets[0].label='Я'.repeat(16)",
  "cfg.common.pages[0].widgets[0].signal='unknown'",
  "cfg.common.theme=99",
  "cfg.common.pages=[]",
  "cfg.common.warnings[0].signal=999",
]) {
  context.cfg = structuredClone(fixture.config);
  run(mutate);
  assert.ok(run("problems().length") > 0, mutate);
}
for (let layout = 0; layout < fixture.layouts.length; layout++) {
  context.cfg = structuredClone(fixture.config);
  const preset = fixture.layouts[layout];
  context.cfg.common.pages[0].layout = preset.id;
  context.cfg.common.pages[0].widgets = preset.slots.map((slot, i) => ({
    ...structuredClone(fixture.config.common.pages[0].widgets[i % 4]),
    ...slot,
  }));
  assert.equal(run("problems().length"), 0, "native layout " + preset.id);
}
console.log("MINIDASH_WEBUI_LOGIC_TEST geometry/presets/guards/themes=PASS");
