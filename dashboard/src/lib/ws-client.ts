import { parseClusterEvent, type ClusterEvent } from "@/contracts/cluster-events";

type ClusterSocket = {
  close: () => void;
};

const hasWebSocket = (): boolean =>
  typeof window !== "undefined" && typeof WebSocket !== "undefined";

export function createClusterSocket(
  url: string,
  onEvent: (event: ClusterEvent) => void
): ClusterSocket {
  if (!hasWebSocket()) {
    return { close: () => undefined };
  }

  let socket: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  let heartbeatTimer: ReturnType<typeof setTimeout> | null = null;
  let closed = false;
  let backoffMs = 1000;

  const maxBackoffMs = 30000;
  const heartbeatTimeoutMs = 20000;

  const clearTimers = () => {
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    if (heartbeatTimer) {
      clearTimeout(heartbeatTimer);
      heartbeatTimer = null;
    }
  };

  const scheduleHeartbeat = () => {
    if (heartbeatTimer) {
      clearTimeout(heartbeatTimer);
    }

    heartbeatTimer = setTimeout(() => {
      if (socket && socket.readyState === WebSocket.OPEN) {
        socket.close();
      }
    }, heartbeatTimeoutMs);
  };

  const handleMessage = (event: MessageEvent) => {
    scheduleHeartbeat();

    if (typeof event.data !== "string") {
      return;
    }

    try {
      const parsed = parseClusterEvent(JSON.parse(event.data));
      onEvent(parsed);
    } catch {
      // Ignore malformed events.
    }
  };

  const connect = () => {
    if (closed) {
      return;
    }

    socket = new WebSocket(url);

    socket.addEventListener("open", () => {
      backoffMs = 1000;
      scheduleHeartbeat();
    });
    socket.addEventListener("message", handleMessage);
    socket.addEventListener("close", () => {
      clearTimers();
      scheduleReconnect();
    });
    socket.addEventListener("error", () => {
      socket?.close();
    });
  };

  const scheduleReconnect = () => {
    if (closed) {
      return;
    }

    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
    }

    reconnectTimer = setTimeout(() => {
      connect();
    }, backoffMs);

    backoffMs = Math.min(maxBackoffMs, backoffMs * 2);
  };

  connect();

  return {
    close: () => {
      closed = true;
      clearTimers();
      socket?.close();
    }
  };
}
