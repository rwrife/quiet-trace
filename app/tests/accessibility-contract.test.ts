import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { describe, expect, it } from "vitest";

const root = dirname(fileURLToPath(new URL("../src/main.ts", import.meta.url)));
const stylePath = join(root, "style.css");
const indexPath = join(root, "../index.html");

describe("dashboard accessibility contract", () => {
  it("keeps touch targets, focus rings, reduced motion, and narrow viewport support", () => {
    const css = readFileSync(stylePath, "utf8");

    expect(css).toContain(".target-control");
    expect(css).toMatch(/min-height:\s*44px/);
    expect(css).toMatch(/min-width:\s*44px/);
    expect(css).toContain(":focus-visible");
    expect(css).toContain("prefers-reduced-motion: reduce");
    expect(css).toContain("@media (max-width: 320px)");
  });

  it("retains skip-link and aria-live region", () => {
    const html = readFileSync(indexPath, "utf8");

    expect(html).toContain('class="skip-link"');
    expect(html).toContain('id="app"');
    expect(html).toContain('aria-live="polite"');
    expect(html).toContain(
      'meta name="viewport" content="width=device-width, initial-scale=1.0"',
    );
  });
});
