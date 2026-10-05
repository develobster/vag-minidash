import { test, expect } from "@playwright/test";
for (const width of [360, 390, 430, 1280])
  test(`navigation and touch editor at ${width}px`, async ({ page }) => {
    await page.setViewportSize({ width, height: 850 });
    await page.goto("/");
    await expect(page.locator("#connection")).toContainText("Connected");
    for (const name of ["Screens", "Signals", "Logs", "Device", "Dashboard"]) {
      await page.getByRole("button", { name, exact: true }).click();
      await expect(page.locator("h1")).toBeVisible();
      expect(
        await page.evaluate(
          () => document.documentElement.scrollWidth <= innerWidth,
        ),
      ).toBe(true);
    }
    await page.getByRole("button", { name: "Screens", exact: true }).click();
    await page.locator("[data-widget]").first().tap();
    await expect(
      page.getByRole("dialog", { name: "Widget settings" }),
    ).toBeVisible();
    await page
      .locator("#widget-form input[name=label]")
      .fill(`Mobile ${width}`);
    await page.getByRole("button", { name: "Apply widget" }).click();
    await page.getByRole("button", { name: "Save", exact: true }).click();
    await expect(page.locator("#toast")).toContainText("saved");
    await page.reload();
    await page.getByRole("button", { name: "Screens", exact: true }).click();
    await expect(page.locator(".preview .label").first()).toHaveText(
      `Mobile ${width}`,
    );
  });
test("eight widgets, add signal, theme and recording work end to end", async ({
  page,
}) => {
  await page.setViewportSize({ width: 390, height: 850 });
  await page.goto("/#screens");
  await expect(page.locator("#connection")).toContainText("Connected");
  await page.getByRole("button", { name: "8", exact: true }).click();
  await page.getByRole("button", { name: "Confirm", exact: true }).click();
  await expect(page.locator("[data-widget]")).toHaveCount(8);
  await page.getByRole("button", { name: "Edit theme" }).click();
  await page.locator("#theme-form input[name=name]").fill("My night");
  await page.getByRole("button", { name: "Apply theme" }).click();
  await page.getByRole("button", { name: "Save", exact: true }).click();
  await page.getByRole("button", { name: "Signals", exact: true }).click();
  await page.locator("[data-add-signal]").first().click();
  await page.getByRole("button", { name: "Add widget", exact: true }).click();
  await expect(page.locator("[data-widget]")).toHaveCount(9);
  await page.getByRole("button", { name: "Logs", exact: true }).click();
  await page
    .getByRole("button", { name: "Start recording", exact: true })
    .click();
  await expect(page.locator("[data-log-status]")).toContainText("samples");
  await expect
    .poll(async () => await page.locator("[data-log-status]").textContent())
    .not.toContain("0 samples");
  await page
    .getByRole("button", { name: "Stop recording", exact: true })
    .click();
  await expect(page.locator("[data-log-graph]").first()).toBeVisible();
  await page.locator("[data-log-graph]").first().click();
  await expect(page.locator(".spark")).toBeVisible();
});
test("disconnect suppresses live values, reconnect retains draft", async ({
  page,
}) => {
  await page.goto("/#screens");
  await expect(page.locator("#connection")).toContainText("Connected");
  await page.locator("[name=screen-name]").fill("Kept draft");
  await page.context().setOffline(true);
  await expect(page.locator("#connection")).toContainText("Reconnecting", {
    timeout: 10000,
  });
  await expect(page.locator("[data-value]").first()).toHaveText("—");
  await page.context().setOffline(false);
  await expect(page.locator("#connection")).toContainText("Connected", {
    timeout: 15000,
  });
  await expect(page.locator("[name=screen-name]")).toHaveValue("Kept draft");
  await page.reload();
  await expect(page.locator("[name=screen-name]")).toHaveValue("Kept draft");
});
