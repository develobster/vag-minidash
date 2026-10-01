import { test, expect } from "@playwright/test";
test("host without firmware capabilities disables Wi-Fi, brightness, scan and restart controls", async ({
  page,
}) => {
  await page.goto("http://127.0.0.1:8767/#device");
  await expect(page.locator("#connection")).toContainText("Connected");
  await page.locator("[data-setting-section=wifi]").click();
  await expect(page.locator('[data-setting="wifi.ap_enabled"]')).toBeDisabled();
  await expect(page.locator("#ap_password")).toBeDisabled();
  await expect(page.locator("[data-setting-action=scan]")).toBeDisabled();
  await expect(page.locator("#sheet")).toContainText(
    "Not supported by current firmware",
  );
  await page.locator("[data-setting-action=done]").click();
  await page.locator("[data-setting-section=display]").click();
  await expect(
    page.locator('[data-setting="display.brightness"]'),
  ).toBeDisabled();
  await page.locator("[data-setting-action=done]").click();
  await page.locator("[data-setting-section=system]").click();
  await expect(
    page.locator("[data-setting-action=restart_device]"),
  ).toBeDisabled();
  await expect(
    page.locator("[data-setting-action=restart_wifi]"),
  ).toBeDisabled();
});
