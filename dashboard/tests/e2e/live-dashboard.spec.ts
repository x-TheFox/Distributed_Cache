import { expect, test } from "@playwright/test";

test("live dashboard updates from websocket stream", async ({ page }) => {
  await page.goto("http://localhost:3000");
  await expect(
    page.getByRole("heading", { name: "Cluster Topology" })
  ).toBeVisible();
  await expect(page.getByRole("heading", { name: "Replica Lag" })).toBeVisible();
  await expect(
    page.getByRole("heading", { name: "Failover Simulation" })
  ).toBeVisible();
});
