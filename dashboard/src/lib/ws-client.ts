import { parseClusterEvent, type ClusterEvent } from "@/contracts/cluster-events";

type ClusterSocket = {
  close: () => void;
};

type SocketStatus = "connected" | "connecting" | "reconnecting" | "disconnected";

type ClusterSocketOptions = {
  onStatusChange?: (status: SocketStatus) => void;
};

const hasWebSocket = (): boolean =>
  typeof window !== "undefined" && typeof WebSocket !== "undefined";

export function createClusterSocket(
  url: string,
  onEvent: (event: ClusterEvent) => void,
  options: ClusterSocketOptions = {}
): ClusterSocket {
  const { onStatusChange } = options;
  const updateStatus = (status: SocketStatus) => {
    onStatusChange?.(status);
  };

  if (!hasWebSocket()) {
    updateStatus("disconnected");
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

  const scheduleHeartbeat = (ws: WebSocket) => {
    if (heartbeatTimer) {
      clearTimeout(heartbeatTimer);
    }

    heartbeatTimer = setTimeout(() => {
      if (socket !== ws) {
        return;
      }
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    }, heartbeatTimeoutMs);
  };

  const handleMessage = (ws: WebSocket) => (event: MessageEvent) => {
    if (socket !== ws) {
      return;
    }

    scheduleHeartbeat(ws);

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

    updateStatus("connecting");

    const ws = new WebSocket(url);
    socket = ws;

    ws.addEventListener("open", () => {
      if (socket !== ws) {
        return;
      }
      backoffMs = 1000;
      updateStatus("connected");
      scheduleHeartbeat(ws);
    });
    ws.addEventListener("message", handleMessage(ws));
    ws.addEventListener("close", () => {
      if (socket !== ws) {
        return;
      }
      clearTimers();
      if (closed) {
        updateStatus("disconnected");
        return;
      }
      updateStatus("reconnecting");
      scheduleReconnect();
    });
    ws.addEventListener("error", () => {
      if (socket !== ws) {
        return;
      }
      ws.close();
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
      updateStatus("disconnected");
    }
  };
}
