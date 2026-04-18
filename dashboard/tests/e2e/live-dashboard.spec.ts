import { expect, test } from "@playwright/test";

test("live dashboard updates from websocket stream", async ({ page }) => {
  await page.addInitScript(() => {
    class MockWebSocket {
      static instances = [];
      static OPEN = 1;
      static CONNECTING = 0;
      static CLOSING = 2;
      static CLOSED = 3;
      readyState = MockWebSocket.CONNECTING;
      url;
      listeners = new Map();

      constructor(url) {
        this.url = url;
        MockWebSocket.instances.push(this);
        queueMicrotask(() => {
          this.readyState = MockWebSocket.OPEN;
          this.dispatch("open", {});
        });
      }

      addEventListener(type, listener) {
        const list = this.listeners.get(type) ?? [];
        list.push(listener);
        this.listeners.set(type, list);
      }

      removeEventListener(type, listener) {
        const list = this.listeners.get(type) ?? [];
        this.listeners.set(
          type,
          list.filter((item) => item !== listener)
        );
      }

      close() {
        this.readyState = MockWebSocket.CLOSED;
        this.dispatch("close", {});
      }

      dispatch(type, event) {
        const list = this.listeners.get(type) ?? [];
        for (const listener of list) {
          listener({ ...event, type, target: this });
        }
      }

      __emitMessage(data) {
        this.dispatch("message", { data });
      }
    }

    const global = window as any;
    global.WebSocket = MockWebSocket;
    global.__mockSockets = MockWebSocket.instances;
  });

  await page.goto("http://localhost:3000");

  await expect(page.getByText(/Source:\s*LIVE/)).toBeVisible();

  const healthPanel = page.getByRole("region", { name: /Connection health/i });
  await expect(healthPanel).toBeVisible();
  await expect(healthPanel.getByText("WebSocket", { exact: true })).toBeVisible();
  await expect(healthPanel.getByText("Connected")).toBeVisible();
  await expect(healthPanel.getByText("Last event: Awaiting events")).toBeVisible();

  await page.waitForFunction(() => (window as any).__mockSockets?.length > 0);
  const replicaCard = page.locator("article", { hasText: "Replica Lag" });
  const replicaValue = replicaCard.locator("div").nth(1);
  await expect(replicaValue).toHaveText("8 ms");
  await page.evaluate(() => {
    const socket = (window as any).__mockSockets[0];
    socket.__emitMessage(
      JSON.stringify({
        type: "replica_lag",
        shardId: 1,
        lagMs: 42,
        ts: 123456
      })
    );
  });

  await expect(replicaValue).toHaveText("42 ms");
  await expect(healthPanel.getByText("Last event: 123456")).toBeVisible();
});
