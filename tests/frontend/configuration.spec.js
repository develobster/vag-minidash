import { test, expect } from "@playwright/test";
test("validated backup import, persistence and rejected unknown version", async ({
  page,
}) => {
  await page.goto("/#device");
  await expect(page.locator("#connection")).toContainText("Connected");
  const saved = await (await page.request.get("/api/configuration")).json();
  const imported = structuredClone(saved.configuration);
  imported.screens[0].name = "Backup restored";
  await page.locator("#config-file").setInputFiles({
    name: "backup.json",
    mimeType: "application/json",
    buffer: Buffer.from(JSON.stringify(imported)),
  });
  await page.getByRole("button", { name: "Confirm", exact: true }).click();
  await page.getByRole("button", { name: "Apply", exact: true }).click();
  await expect(page.locator("#toast")).toContainText("saved");
  const result = await (await page.request.get("/api/configuration")).json();
  expect(result.configuration.screens[0].name).toBe("Backup restored");
  imported.schema_version = 99;
  await page.locator("#config-file").setInputFiles({
    name: "future.json",
    mimeType: "application/json",
    buffer: Buffer.from(JSON.stringify(imported)),
  });
  await expect(page.locator("#toast")).toContainText(
    "Unsupported configuration version",
  );
  expect(
    (await (await page.request.get("/api/configuration")).json()).configuration
      .screens[0].name,
  ).toBe("Backup restored");
});
test("service worker offline shell launches without cached live telemetry", async ({
  page,
  context,
}) => {
  await page.goto("/");
  await expect(page.locator("#connection")).toContainText("Connected");
  await page.evaluate(async () => {
    await navigator.serviceWorker.ready;
  });
  await page.reload();
  await expect(page.locator("#connection")).toContainText("Connected");
  await context.setOffline(true);
  await page.reload();
  await expect(page.locator("h1")).toContainText("MiniDash unavailable");
  await expect(page.locator("#connection")).toContainText("Reconnecting");
  expect(
    await page.evaluate(async () => {
      const cache = await caches.open("minidash-shell-v2");
      return (await cache.keys()).some((r) =>
        new URL(r.url).pathname.startsWith("/api/"),
      );
    }),
  ).toBe(false);
  await context.setOffline(false);
  await expect(page.locator("#connection")).toContainText("Connected", {
    timeout: 15000,
  });
});
test("gauge, thresholds, resize, duplicate and delete use touch controls", async ({
  page,
}) => {
  await page.goto("/#screens");
  await expect(page.locator("#connection")).toContainText("Connected");
  await page.getByRole("button", { name: "2", exact: true }).click();
  await page.getByRole("button", { name: "Confirm", exact: true }).click();
  await page.locator("[data-widget]").first().tap();
  await page.locator("#widget-form [name=type]").selectOption("gauge");
  await page.locator("#widget-form [name=width]").fill("2");
  await page.getByRole("button", { name: "Apply widget", exact: true }).click();
  await expect(page.locator("[data-gauge]")).toHaveCount(1);
  await page.locator("[data-widget]").first().tap();
  await page
    .locator("#sheet")
    .getByRole("button", { name: "Duplicate", exact: true })
    .click();
  await expect(page.locator("[data-widget]")).toHaveCount(3);
  await page.getByRole("button", { name: "Delete", exact: true }).click();
  await page.getByRole("button", { name: "Confirm", exact: true }).click();
  await expect(page.locator("[data-widget]")).toHaveCount(2);
});
