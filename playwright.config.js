import { defineConfig } from "@playwright/test";
export default defineConfig({
  testDir: "tests/frontend",
  testMatch: "*.spec.js",
  workers: 1,
  use: {
    baseURL: "http://127.0.0.1:8766",
    browserName: "chromium",
    hasTouch: true,
  },
  webServer: {
    command:
      "python -m minidash_live.configurator --port 8766 --data-dir work/ui-test-data",
    url: "http://127.0.0.1:8766/api/configuration",
    reuseExistingServer: false,
  },
  reporter: "list",
});
