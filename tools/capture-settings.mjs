import { chromium } from "@playwright/test";
import { mkdir } from "node:fs/promises";
import { resolve } from "node:path";

const directory = resolve(process.argv[2] || "work/settings-preview");
await mkdir(directory, { recursive: true });
const browser = await chromium.launch();
const errors = [];
for (const [width, height] of [[360, 850], [390, 850], [430, 850], [844, 390], [1280, 900]]) {
  const context = await browser.newContext({ viewport: { width, height }, hasTouch: true });
  const page = await context.newPage();
  page.on("pageerror", error => errors.push(error.message));
  await page.goto("http://127.0.0.1:8765/#device");
  await page.locator("#connection").filter({ hasText: "Connected" }).waitFor();
  await page.locator("#show-advanced").check();
  await page.screenshot({ path: resolve(directory, `device-${width}.png`), fullPage: true });
  for (const section of ["general", "display", "wifi", "vehicle", "acquisition", "logging", "power", "system", "advanced"]) {
    await page.locator(`[data-setting-section="${section}"]`).click();
    if (await page.evaluate(() => document.documentElement.scrollWidth > innerWidth))
      throw Error(`Horizontal overflow: ${section} ${width}`);
    await page.screenshot({ path: resolve(directory, `${section}-${width}.png`) });
    await page.locator("[data-setting-action=done]").click();
  }
  await context.close();
}
await browser.close();
if (errors.length) throw Error(errors.join("\n"));
console.log(`Verified nine settings sheets at five viewport sizes; screenshots: ${directory}`);
