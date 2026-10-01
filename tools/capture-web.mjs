import { chromium } from "@playwright/test";
import { mkdir } from "node:fs/promises";
import { resolve } from "node:path";
const directory = resolve(process.argv[2] || "work/screenshots");
await mkdir(directory, { recursive: true });
const browser = await chromium.launch();
const context = await browser.newContext({ hasTouch: true });
const page = await context.newPage();
const errors = [];
page.on("pageerror", (error) => errors.push(error.message));
for (const width of [360, 390, 430, 1280]) {
  await page.setViewportSize({ width, height: 850 });
  for (const route of ["dashboard", "screens", "signals", "logs", "device"]) {
    await page.goto(`http://127.0.0.1:8765/#${route}`);
    await page
      .locator("#connection")
      .filter({ hasText: "Connected" })
      .waitFor();
    if (
      await page.evaluate(
        () => document.documentElement.scrollWidth > innerWidth,
      )
    )
      throw Error(`Overflow: ${route} ${width}`);
    await page.screenshot({
      path: resolve(directory, `${route}-${width}.png`),
      fullPage: true,
    });
  }
}
await page.setViewportSize({ width: 844, height: 390 });
await page.goto("http://127.0.0.1:8765/#screens");
await page.locator("[data-widget]").first().tap();
await page.screenshot({
  path: resolve(directory, "editor-landscape.png"),
  fullPage: true,
});
await browser.close();
if (errors.length) throw Error(errors.join("\n"));
console.log(
  `Verified 20 page/viewport combinations and landscape editor; screenshots: ${directory}`,
);
