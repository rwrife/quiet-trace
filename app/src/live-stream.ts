export interface EventSourceLike {
  onopen: ((event: Event) => void) | null;
  onerror: ((event: Event) => void) | null;
  addEventListener(
    type: string,
    listener: (event: MessageEvent<string>) => void,
  ): void;
  close(): void;
}

export interface LiveStreamEvent {
  readonly type: string;
  readonly payload: unknown;
}

export interface LiveStreamHandlers {
  onOpen?: () => void;
  onError?: (error: Error) => void;
  onEvent?: (event: LiveStreamEvent) => void;
}

export interface LiveStreamOptions extends LiveStreamHandlers {
  readonly url: string;
  readonly createEventSource: (url: string) => EventSourceLike;
  readonly parseEvent: (eventType: string, data: string) => unknown;
  readonly reconnectInitialMs?: number;
  readonly reconnectMaxMs?: number;
  readonly jitterMs?: number;
}

const KNOWN_EVENTS = [
  "aggregate",
  "status",
  "storage",
  "clock",
  "annotation",
] as const;

export function startReconnectingLiveStream(
  options: LiveStreamOptions,
): () => void {
  const reconnectInitialMs = options.reconnectInitialMs ?? 1_000;
  const reconnectMaxMs = options.reconnectMaxMs ?? 30_000;
  const jitterMs = options.jitterMs ?? 200;

  let stopped = false;
  let retries = 0;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  let source: EventSourceLike | null = null;

  const clearTimer = () => {
    if (reconnectTimer !== null) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
  };

  const scheduleReconnect = () => {
    if (stopped) {
      return;
    }

    clearTimer();
    const delay = Math.min(
      reconnectInitialMs * 2 ** Math.min(retries, 8) +
        Math.floor(Math.random() * jitterMs),
      reconnectMaxMs,
    );
    retries += 1;

    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      connect();
    }, delay);
  };

  const connect = () => {
    if (stopped) {
      return;
    }

    source = options.createEventSource(options.url);

    source.onopen = () => {
      retries = 0;
      options.onOpen?.();
    };

    source.onerror = () => {
      source?.close();
      options.onError?.(new Error("SSE connection lost"));
      scheduleReconnect();
    };

    for (const eventName of KNOWN_EVENTS) {
      source.addEventListener(eventName, (event) => {
        try {
          const payload = options.parseEvent(eventName, event.data);
          options.onEvent?.({
            type: eventName,
            payload,
          });
        } catch (error) {
          const message =
            error instanceof Error ? error.message : String(error);
          options.onError?.(
            new Error(`Rejected ${eventName} event: ${message}`),
          );
        }
      });
    }
  };

  connect();

  return () => {
    stopped = true;
    clearTimer();
    source?.close();
  };
}
