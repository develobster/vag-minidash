/* Actual Chromium DOM/input tests against the device HTML and native C validator.
 * The HTTP peer is a fixture server, never a real ESP32 or vehicle. */
const fs = require("node:fs"),
  path = require("node:path"),
  http = require("node:http");
const assert = require("node:assert/strict"),
  { spawnSync } = require("node:child_process");
const { chromium } = require("playwright");
const fixture = JSON.parse(
  fs.readFileSync(process.argv[2], "utf8").replace(/^\uFEFF/, ""),
);
const output = path.resolve(process.argv[3] || ".local/studio-browser");
const validator = path.resolve(
  process.argv[4] || "core/build-cyd-check/minidash-json-tests.exe",
);
fs.mkdirSync(output, { recursive: true });
fixture.device = {
  ...fixture.device,
  dimming: true,
  auto_brightness: true,
  simulation: true,
};
fixture.config.platform.brightness = 70;
fixture.config.platform.brightness_mode = 1;
let current = structuredClone(fixture.config),
  saved = structuredClone(current),
  failNext = 0,
  writes = [];
const html = fs.readFileSync(
  path.join(__dirname, "../firmware/main/webui.html"),
);
const server = http.createServer(async (req, res) => {
  if (req.url === "/") {
    res.setHeader("Content-Type", "text/html; charset=utf-8");
    res.end(html);
    return;
  }
  res.setHeader("Content-Type", "application/json");
  res.setHeader("Cache-Control", "no-store");
  if (req.method === "GET") {
    const value =
      req.url === "/api/session"
        ? { token: "fixture-session" }
        : req.url === "/api/config"
          ? current
          : fixture[req.url.slice(5)];
    res.end(JSON.stringify(value ?? {}));
    return;
  }
  let body = "";
  for await (const chunk of req) body += chunk;
  if (failNext) {
    res.statusCode = failNext;
    failNext = 0;
    res.end("conflict");
    return;
  }
  try {
    const candidate = JSON.parse(body);
    writes.push(req.url);
    assert.equal(req.headers["x-minidash-session"], "fixture-session");
    if (candidate.revision !== current.revision) {
      res.statusCode = 409;
      res.end("stale revision");
      return;
    }
    if (
      !["/api/cancel", "/api/reset-trip", "/api/reset-session"].includes(
        req.url,
      )
    ) {
      const result = spawnSync(validator, ["--validate"], {
        input: body,
        encoding: "utf8",
      });
      if (result.status !== 0) {
        res.statusCode = 400;
        res.end("native validation rejected");
        return;
      }
    }
    const revision = current.revision + 1;
    current = req.url === "/api/cancel" ? structuredClone(saved) : candidate;
    current.revision = revision;
    if (req.url === "/api/config") saved = structuredClone(current);
    res.end(JSON.stringify(current));
  } catch (error) {
    res.statusCode = 500;
    res.end(JSON.stringify({ error: error.message }));
  }
});
const read = (page) => page.evaluate(() => structuredClone(cfg));
async function importConfig(page, doc) {
  await page.locator("[data-panel=files]").click();
  await page.locator("#import").setInputFiles({
    name: "test.json",
    mimeType: "application/json",
    buffer: Buffer.from(JSON.stringify(doc)),
  });
}
(async () => {
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const browser = await chromium.launch({
    headless: true,
    ...(process.env.MINIDASH_CHROMIUM
      ? { executablePath: process.env.MINIDASH_CHROMIUM }
      : {}),
  });
  let checks = 0;
  const check = (value, message) => {
    assert.ok(value, message);
    checks++;
  };
  try {
    const page = await browser.newPage({
        viewport: { width: 1440, height: 1000 },
      }),
      errors = [];
    page.on("pageerror", (e) => errors.push(e.message));
    await page.goto("http://127.0.0.1:" + server.address().port);
    await page.waitForSelector("#content:not([hidden])");
    check(errors.length === 0, "boot without JS errors");
    check(
      (await page.locator("#themes option").count()) === 17,
      "all old and new themes",
    );
    await page.screenshot({
      path: path.join(output, "studio-desktop.png"),
      fullPage: true,
    });
    const doc = structuredClone(fixture.config);
    doc.common.pages[0].layout = 0;
    doc.common.pages[0].widgets = [
      {
        ...doc.common.pages[0].widgets[0],
        column: 0,
        row: 0,
        columns: 3,
        rows: 2,
      },
    ];
    await importConfig(page, doc);
    await page.locator("[data-panel=editor]").click();
    check(
      (await read(page)).common.pages[0].widgets.length === 1,
      "import into draft",
    );
    let box = await page.locator("#preview").boundingBox(),
      tile = await page.locator(".tile").first().boundingBox();
    await page.mouse.move(tile.x + 15, tile.y + 15);
    await page.mouse.down();
    await page.mouse.move(
      tile.x + 15 + (2 * (box.width + 3)) / 12,
      tile.y + 15 + (box.height + 3) / 8,
      { steps: 6 },
    );
    await page.mouse.up();
    let w = (await read(page)).common.pages[0].widgets[0];
    check(w.column === 2 && w.row === 1, "pointer drag snaps to cells");
    const handle = await page.locator(".resize").boundingBox();
    await page.mouse.move(handle.x + 18, handle.y + 18);
    await page.mouse.down();
    await page.mouse.move(
      handle.x + 18 + (box.width + 3) / 12,
      handle.y + 18 + (box.height + 3) / 8,
      { steps: 6 },
    );
    await page.mouse.up();
    w = (await read(page)).common.pages[0].widgets[0];
    check(w.columns === 4 && w.rows === 3, "resize persists cells");
    await page.locator("#undo").click();
    check(
      (await read(page)).common.pages[0].widgets[0].columns === 3,
      "undo geometry",
    );
    await page.locator("#redo").click();
    check(
      (await read(page)).common.pages[0].widgets[0].columns === 4,
      "redo geometry",
    );
    await page.locator(".tile").focus();
    await page.keyboard.press("ArrowRight");
    check(
      (await read(page)).common.pages[0].widgets[0].column === 3,
      "keyboard move",
    );
    await page.keyboard.press("Shift+ArrowRight");
    check(
      (await read(page)).common.pages[0].widgets[0].columns === 5,
      "keyboard resize",
    );
    await page.locator("#windowtools button").first().click();
    check(
      (await read(page)).common.pages[0].widgets.length === 2,
      "duplicate into free place",
    );
    await page.locator("#signalsearch").fill("definitely_no_signal");
    check(
      (await page.locator("#signalresult").textContent()) ===
        "Ничего не найдено. Измените запрос.",
      "empty search state",
    );
    await page.locator("#signalsearch").fill("");
    const before = await read(page);
    await page.getByText("Выравнивание", { exact: true }).click();
    await page.locator("#align button").nth(3).click();
    check(
      JSON.stringify((await read(page)).common.pages[0].widgets) ===
        JSON.stringify(before.common.pages[0].widgets),
      "collision rejected",
    );
    await page.locator("#windowtools button").last().click();
    check(
      (await read(page)).common.pages[0].widgets.length === 1,
      "delete widget",
    );
    await page.locator("[data-panel=appearance]").click();
    for (const id of [11, 14, 15, 16]) {
      await page.locator(`[data-theme="${id}"]`).click();
      check((await read(page)).common.theme === id, "theme " + id);
      await page.screenshot({
        path: path.join(output, `theme-${id}.png`),
        fullPage: true,
      });
      await page.locator("[data-panel=editor]").click();
      await page.screenshot({
        path: path.join(output, `editor-${id}.png`),
        fullPage: true,
      });
      await page.locator("[data-panel=appearance]").click();
    }
    await page.locator("#try").click();
    await page.waitForFunction(() => !busy);
    check(
      current.common.theme === 16 && saved.common.theme !== 16,
      "preview without saving",
    );
    await page.locator("#cancel").click();
    await page.waitForFunction(() => !busy);
    check(
      (await read(page)).common.theme === saved.common.theme,
      "cancel restores saved",
    );
    await page.locator('[data-theme="14"]').click();
    await page.locator("#save").click();
    await page.waitForFunction(() => !busy);
    check(saved.common.theme === 14, "save native valid theme");
    await page.reload();
    await page.waitForSelector("#content:not([hidden])");
    check((await read(page)).common.theme === 14, "reload persisted theme");
    await page.locator("[data-panel=device]").click();
    await page.locator("#general input[type=range]").focus();
    await page.keyboard.press("Home");
    await page.keyboard.press("ArrowRight");
    check(
      (await read(page)).platform.brightness === 11,
      "brightness slider keeps numeric wire value",
    );

    await page
      .locator("#general label")
      .filter({ hasText: "Режим яркости" })
      .locator("select")
      .selectOption("2");
    await page.locator("#save").click();
    await page.waitForFunction(() => !busy);
    check(saved.platform.brightness_mode === 2, "AUTO survives roundtrip");
    await page
      .locator("#general label")
      .filter({ hasText: "Поворот экрана" })
      .locator("select")
      .selectOption("1");
    check(
      (await page
        .locator("#preview")
        .evaluate((e) => e.style.gridTemplateColumns)) === "repeat(8, 1fr)",
      "portrait mapping",
    );
    failNext = 409;
    await page.locator("#save").click();
    await page.waitForFunction(() => !busy);
    check(
      (await page.locator("#message").textContent()).includes("Экспортируйте"),
      "actionable conflict state",
    );
    check((await read(page)).platform.rotation === 1, "conflict keeps draft");
    const invalid = structuredClone(fixture.config);
    invalid.common.pages[0].widgets[0].columns = 99;
    const prior = await read(page);
    await importConfig(page, invalid);
    check(
      JSON.stringify(await read(page)) === JSON.stringify(prior),
      "invalid import preserves draft",
    );
    await page.locator("[data-panel=editor]").click();
    await page.locator("#pageform input").fill("Имя");
    await page.locator("#pageform input").fill("Имя экрана");
    check(
      (await page.locator("#preview-title").textContent()) === "Имя экрана",
      "immediate local preview",
    );

    const badRange = await read(page);
    await page.locator("[data-panel=editor]").click();
    const rangeDetails = page.locator(
      "#widgets > .widget:not([hidden]) details",
    );
    await rangeDetails.locator("summary").click();
    await rangeDetails
      .getByLabel("Максимум, rpm", { exact: true })
      .fill("-100");
    check(
      await page.locator("#save").isDisabled(),
      "invalid range disables save",
    );
    await page.locator("#cancel").click();
    await page.waitForFunction(() => !busy);
    check(
      (await read(page)).common.theme === saved.common.theme,
      "cancel accepts invalid local draft using saved payload",
    );
    await page.locator("[data-panel=editor]").click();
    await page
      .locator("#add")
      .evaluate((e) => (e.closest("details").open = true));
    await page.locator("#add").click();
    await page.locator("#add").click();
    await page.locator("#add").click();
    check(
      (await read(page)).common.pages.length === 4 &&
        (await page.locator("#add").isDisabled()),
      "maximum pages",
    );
    await page.locator("#pageform select").selectOption("5");
    check(
      (await read(page)).common.pages[3].widgets.length === 8,
      "eight-window preset",
    );
    await page.locator("#save").click();
    await page.waitForFunction(() => !busy);
    check(saved.common.pages[3].widgets.length === 8, "four-page native save");
    const legacy = structuredClone(fixture.config);
    legacy.schema = 3;
    delete legacy.common.analytics;
    legacy.common.gauge_ranges.length = 14;
    await importConfig(page, legacy);
    check(
      (await read(page)).schema === 4 &&
        (await read(page)).common.gauge_ranges.length ===
          fixture.signals.length,
      "schema 3 import migration",
    );
    await page.locator("[data-panel=advanced]").click();
    await page
      .locator("#warnings")
      .evaluate((e) => (e.closest("details").open = true));
    await page
      .locator("#warnings .widget")
      .first()
      .getByLabel("Состояние", { exact: true })
      .selectOption("1");
    await page.locator("#save").click();
    await page.waitForFunction(() => !busy);
    check(saved.common.warnings[0].enabled, "warning setting save");
    check(
      await page.locator("#analytics input").first().isDisabled(),
      "CYD unsupported analytics disabled",
    );
    fixture.device.simulation = false;
    await page.reload();
    await page.waitForSelector("#content:not([hidden])");
    await page.locator("[data-panel=advanced]").click();
    await page
      .locator("#analytics")
      .evaluate((e) => (e.closest("details").open = true));
    await page
      .locator("#analytics .widget")
      .first()
      .getByLabel("Событие", { exact: true })
      .selectOption("1");
    await page.locator("#save").click();
    await page.waitForFunction(() => !busy);
    check(saved.common.analytics.rules[0].enabled, "analytics setting save");
    const future = structuredClone(fixture.config);
    future.schema = 99;
    const previous = await read(page);
    await importConfig(page, future);
    check(
      JSON.stringify(await read(page)) === JSON.stringify(previous),
      "future schema preserves draft",
    );
    await page.locator("[data-panel=editor]").click();
    check(errors.length === 0, "no uncaught runtime errors");

    await page.screenshot({
      path: path.join(output, "studio-portrait.png"),
      fullPage: true,
    });
    const mobile = await browser.newPage({
      viewport: { width: 390, height: 844 },
      isMobile: true,
      hasTouch: true,
    });
    await mobile.goto("http://127.0.0.1:" + server.address().port);
    await mobile.waitForSelector("#content:not([hidden])");
    check(
      await mobile.evaluate(() => document.documentElement.scrollWidth <= 390),
      "mobile no horizontal overflow",
    );
    await mobile.locator("[data-panel=appearance]").tap();
    await mobile.locator('[data-theme="15"]').tap();
    check((await read(mobile)).common.theme === 15, "touch theme control");
    check(
      await mobile.evaluate(() => document.documentElement.scrollWidth <= 390),
      "mobile themed no overflow",
    );
    await mobile.screenshot({
      path: path.join(output, "studio-mobile.png"),
      fullPage: true,
    });
    fs.writeFileSync(
      path.join(output, "results.json"),
      JSON.stringify(
        {
          status: "PASS",
          checks,
          scope: "Chromium + mocked HTTP + actual native schema validator",
          hardware: false,
          errors,
          writes,
        },
        null,
        2,
      ),
    );
    console.log("MINIDASH_STUDIO_BROWSER status=PASS checks=" + checks);
  } finally {
    await browser.close();
  }
})()
  .catch((error) => {
    console.error(error);
    process.exitCode = 1;
  })
  .finally(() => server.close());
