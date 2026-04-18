import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { createClusterSocket } from "@/lib/ws-client";

const SOCKET_URL = "ws://localhost:1234/ws";
const HEARTBEAT_TIMEOUT_MS = 20000;

type Listener = (event: unknown) => void;

class MockWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSING = 2;
  static CLOSED = 3;
  static instances: MockWebSocket[] = [];

  static reset() {
    MockWebSocket.instances = [];
  }

  readyState = MockWebSocket.CONNECTING;
  closeCalls = 0;
  private listeners: Record<string, Listener[]> = {};

  constructor(public url: string) {
    MockWebSocket.instances.push(this);
  }

  addEventListener(type: string, listener: Listener) {
    if (!this.listeners[type]) {
      this.listeners[type] = [];
    }
    this.listeners[type].push(listener);
  }

  close() {
    if (this.readyState === MockWebSocket.CLOSED) {
      return;
    }
    this.closeCalls += 1;
    this.readyState = MockWebSocket.CLOSED;
    this.dispatch("close", new Event("close"));
  }

  error() {
    this.dispatch("error", new Event("error"));
  }

  open() {
    this.readyState = MockWebSocket.OPEN;
    this.dispatch("open", new Event("open"));
  }

  message(data: string) {
    this.dispatch("message", { data });
  }

  private dispatch(type: string, event: unknown) {
    const listeners = this.listeners[type] ?? [];
    listeners.forEach((listener) => listener(event));
  }
}

const globalWithWebSocket = globalThis as typeof globalThis & {
  WebSocket?: typeof WebSocket;
};

let originalWebSocket: typeof WebSocket | undefined;

beforeEach(() => {
  originalWebSocket = globalWithWebSocket.WebSocket;
  MockWebSocket.reset();
  vi.useFakeTimers();
  globalWithWebSocket.WebSocket = MockWebSocket as unknown as typeof WebSocket;
});

afterEach(() => {
  vi.clearAllTimers();
  vi.useRealTimers();
  if (originalWebSocket) {
    globalWithWebSocket.WebSocket = originalWebSocket;
  } else {
    delete globalWithWebSocket.WebSocket;
  }
});

describe("createClusterSocket", () => {
  it("schedules reconnects with backoff and resets on open", () => {
    const clusterSocket = createClusterSocket(SOCKET_URL, () => undefined);

    const firstSocket = MockWebSocket.instances[0];
    expect(firstSocket).toBeDefined();

    firstSocket.close();

    vi.advanceTimersByTime(999);
    expect(MockWebSocket.instances).toHaveLength(1);

    vi.advanceTimersByTime(1);
    const secondSocket = MockWebSocket.instances[1];
    expect(secondSocket).toBeDefined();

    secondSocket.close();

    vi.advanceTimersByTime(1999);
    expect(MockWebSocket.instances).toHaveLength(2);

    vi.advanceTimersByTime(1);
    const thirdSocket = MockWebSocket.instances[2];
    expect(thirdSocket).toBeDefined();

    thirdSocket.open();
    thirdSocket.close();

    vi.advanceTimersByTime(999);
    expect(MockWebSocket.instances).toHaveLength(3);

    vi.advanceTimersByTime(1);
    expect(MockWebSocket.instances).toHaveLength(4);

    clusterSocket.close();
  });

  it("closes stale sockets after the heartbeat timeout", () => {
    const clusterSocket = createClusterSocket(SOCKET_URL, () => undefined);
    const socket = MockWebSocket.instances[0];

    socket.open();

    vi.advanceTimersByTime(HEARTBEAT_TIMEOUT_MS);
    expect(socket.closeCalls).toBe(1);

    clusterSocket.close();
  });

  it("ignores stale socket errors after reconnect", () => {
    const clusterSocket = createClusterSocket(SOCKET_URL, () => undefined);

    const firstSocket = MockWebSocket.instances[0];
    firstSocket.close();

    vi.advanceTimersByTime(1000);
    const secondSocket = MockWebSocket.instances[1];
    secondSocket.open();

    firstSocket.error();

    expect(secondSocket.closeCalls).toBe(0);

    clusterSocket.close();
  });

  it("prevents reconnect attempts after close()", () => {
    const clusterSocket = createClusterSocket(SOCKET_URL, () => undefined);
    const socket = MockWebSocket.instances[0];

    socket.close();
    clusterSocket.close();

    vi.advanceTimersByTime(5000);
    expect(MockWebSocket.instances).toHaveLength(1);
  });
});
