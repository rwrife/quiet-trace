import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import {
  startReconnectingLiveStream,
  type EventSourceLike,
  type LiveStreamEvent,
} from "../src/live-stream";

class FakeEventSource implements EventSourceLike {
  onopen: ((event: Event) => void) | null = null;

  onerror: ((event: Event) => void) | null = null;

  closed = false;

  private listeners = new Map<
    string,
    Array<(event: MessageEvent<string>) => void>
  >();

  addEventListener(
    type: string,
    listener: (event: MessageEvent<string>) => void,
  ): void {
    const existing = this.listeners.get(type) ?? [];
    existing.push(listener);
    this.listeners.set(type, existing);
  }

  emit(type: string, data: string): void {
    const listeners = this.listeners.get(type) ?? [];
    const messageEvent = { data } as MessageEvent<string>;
    for (const listener of listeners) {
      listener(messageEvent);
    }
  }

  close(): void {
    this.closed = true;
  }
}

describe("startReconnectingLiveStream", () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("reconnects after stream errors", () => {
    const instances: FakeEventSource[] = [];
    const events: LiveStreamEvent[] = [];

    const stop = startReconnectingLiveStream({
      url: "/api/v1/live",
      createEventSource: () => {
        const source = new FakeEventSource();
        instances.push(source);
        return source;
      },
      parseEvent: (_eventType, data) => JSON.parse(data),
      onEvent: (event) => events.push(event),
    });

    expect(instances).toHaveLength(1);
    instances[0]?.onerror?.(new Event("error"));
    vi.advanceTimersByTime(1_500);

    expect(instances).toHaveLength(2);
    expect(instances[0]?.closed).toBe(true);

    instances[1]?.emit("status", '{"ok":true}');
    expect(events[0]).toEqual({ type: "status", payload: { ok: true } });

    stop();
    expect(instances[1]?.closed).toBe(true);
  });
});
