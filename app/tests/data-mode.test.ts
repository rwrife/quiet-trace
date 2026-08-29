import { describe, expect, it } from "vitest";

import { resolveDataMode } from "../src/data-mode";

describe("deterministic fixture mode", () => {
  it("defaults to disconnected production behavior", () => {
    expect(resolveDataMode(new URL("https://quiet-trace.local/"))).toBe(
      "device",
    );
  });

  it("requires an explicit fixture query parameter", () => {
    expect(
      resolveDataMode(new URL("https://quiet-trace.local/?fixture=1")),
    ).toBe("fixture");
  });
});
