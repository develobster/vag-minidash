import { test, expect } from "@playwright/test";
test.beforeEach(async ({ request }) => {
  await expect
    .poll(
      async () =>
        (await (await request.get("/api/settings/status")).json()).operation
          ?.status !== "running",
    )
    .toBe(true);
  const status = await (await request.get("/api/settings/status")).json();
  const saved = await (await request.get("/api/configuration")).json();
  const reset = await request.post("/api/system/action", {
    data: {
      action: "factory_reset",
      confirmed: true,
      revision: saved.revision,
      device_id_confirmation: status.device.id,
    },
  });
  expect(reset.ok()).toBe(true);
});
const open = async (page, name) =>
  page.locator(`[data-setting-section="${name}"]`).click();
for (const size of [
  { width: 360, height: 850 },
  { width: 390, height: 850 },
  { width: 430, height: 850 },
  { width: 844, height: 390 },
  { width: 1280, height: 900 },
])
  test(`device settings search, draft, discard and Apply ${size.width}x${size.height}`, async ({
    page,
  }) => {
    await page.setViewportSize(size);
    await page.goto("/#device");
    await expect(page.locator("#connection")).toContainText("Connected");
    await open(page, "general");
    await expect(
      page.locator('[data-setting="general.device_name"]'),
    ).toBeEnabled();
    const original = await page
      .locator('[data-setting="general.device_name"]')
      .inputValue();
    await page
      .locator('[data-setting="general.device_name"]')
      .fill("Pending device");
    await page.locator("[data-setting-action=done]").click();
    await expect(page.locator("#savebar")).toBeVisible();
    await page.getByRole("button", { name: "Discard", exact: true }).click();
    await page.getByRole("button", { name: "Confirm", exact: true }).click();
    await open(page, "general");
    await expect(
      page.locator('[data-setting="general.device_name"]'),
    ).toHaveValue(original);
    await page
      .locator('[data-setting="general.device_name"]')
      .fill(`Device ${size.width}`);
    await page.locator("[data-setting-action=done]").click();
    await page.getByRole("button", { name: "Apply", exact: true }).click();
    await expect(page.locator("#toast")).toContainText("applied");
    await page.reload();
    await open(page, "general");
    await expect(
      page.locator('[data-setting="general.device_name"]'),
    ).toHaveValue(`Device ${size.width}`);
    await page.locator("[data-setting-action=done]").click();
    await page.locator("#settings-search").fill("brightness");
    await expect(page.locator("[data-setting-section]")).toHaveCount(1);
    await expect(page.locator("#settings-cards")).toContainText(
      "Display → Brightness",
    );
    await open(page, "display");
    await expect(
      page.locator('[data-setting="display.auto_brightness"]'),
    ).toBeDisabled();
    expect(
      await page.evaluate(
        () => document.documentElement.scrollWidth <= innerWidth,
      ),
    ).toBe(true);
  });
test("Wi-Fi password stays out of draft, backup, API and diagnostic bundle; reconnect impact is explicit", async ({
  page,
}) => {
  await page.goto("/#device");
  await expect(page.locator("#connection")).toContainText("Connected");
  await open(page, "wifi");
  await page
    .locator('[data-setting="wifi.ap_ssid"]')
    .fill("MiniDash-new-network");
  await page.locator("#ap_password").fill("pw-test-only-641");
  await page.locator("[data-reveal=ap_password]").check();
  await expect(page.locator("#ap_password")).toHaveAttribute("type", "text");
  expect(
    await page.evaluate(() => localStorage.getItem("minidash-draft")),
  ).not.toContain("pw-test-only-641");
  await page.locator("[data-setting-action=done]").click();
  await page.getByRole("button", { name: "Apply", exact: true }).click();
  await expect(page.locator("#confirm")).toContainText(
    "Wi-Fi reconnect required",
  );
  await expect(page.locator("#confirm")).toContainText("MiniDash-new-network");
  await page.getByRole("button", { name: "Confirm", exact: true }).click();
  await expect(page.locator("#settings-operation")).toContainText(
    "Simulation applied",
    { timeout: 10000 },
  );
  const response = await page.request.get("/api/configuration");
  const saved = await response.json();
  expect(saved.secret_status.ap_password).toBe(true);
  expect(JSON.stringify(saved)).not.toContain("pw-test-only-641");
  const bundle = await (
    await page.request.get("/api/diagnostics/bundle")
  ).text();
  expect(bundle).not.toContain("pw-test-only-641");
  await page.reload();
  await open(page, "wifi");
  await expect(page.locator("#ap_password")).toHaveValue("");
  await expect(page.locator("#ap_password")).toHaveAttribute(
    "placeholder",
    "Saved password · unchanged",
  );
});
test("rotation requires reboot confirmation, factory reset requires full Device ID", async ({
  page,
}) => {
  await page.goto("/#device");
  await expect(page.locator("#connection")).toContainText("Connected");
  await open(page, "display");
  await page.locator('[data-setting="display.rotation"]').selectOption("90");
  await page.locator("[data-setting-action=done]").click();
  await page.getByRole("button", { name: "Apply", exact: true }).click();
  await expect(page.locator("#confirm")).toContainText(
    "Device reboot required",
  );
  await page.getByRole("button", { name: "Confirm", exact: true }).click();
  await expect(page.locator("#settings-operation")).toContainText(
    "Simulation applied",
    { timeout: 10000 },
  );
  await open(page, "system");
  await page.locator("[data-setting-action=factory_reset]").click();
  await expect(page.locator("#factory-confirm")).toBeDisabled();
  await page.locator("#factory-id").fill("wrong-id");
  await expect(page.locator("#factory-confirm")).toBeDisabled();
  const status = await (await page.request.get("/api/settings/status")).json();
  await page.locator("#factory-id").fill(status.device.id);
  await page.locator("#factory-confirm").click();
  await expect(page.locator("#toast")).toContainText("System action accepted");
  const result = await (await page.request.get("/api/configuration")).json();
  expect(result.secret_status.ap_password).toBe(false);
  expect(result.configuration.device_settings.display.rotation).toBe(0);
});
test("advanced hidden by default, rates searchable only after explicit opt-in", async ({
  page,
}) => {
  await page.goto("/#device");
  await expect(page.locator("#connection")).toContainText("Connected");
  await expect(page.locator("[data-setting-section=advanced]")).toHaveCount(0);
  await expect(page.locator("[data-setting-section=acquisition]")).toHaveCount(
    0,
  );
  await page.locator("#show-advanced").check();
  await page.locator("#settings-search").fill("polling");
  await expect(page.locator("#settings-cards")).toContainText(
    "Data Acquisition → UDS polling rate",
  );
  await open(page, "acquisition");
  await expect(
    page.locator('[data-setting="acquisition.uds_rate_hz"]'),
  ).toHaveAttribute("max", "2");
});
test("brightness slider previews locally and remains a draft until Apply", async ({
  page,
}) => {
  await page.goto("/#device");
  await expect(page.locator("#connection")).toContainText("Connected");
  await open(page, "display");
  await page.locator('[data-setting="display.brightness"]').fill("40");
  await expect(page.locator("#brightness-preview .preview")).toHaveCSS(
    "filter",
    "brightness(0.4)",
  );
  const saved = await (await page.request.get("/api/configuration")).json();
  expect(saved.configuration.device_settings.display.brightness).toBe(75);
});
