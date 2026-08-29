import "./style.css";

import { resolveDataMode } from "./data-mode";
import { aggregateFixture } from "./fixtures/aggregate";

function addTextElement(
  parent: HTMLElement,
  tag: "h1" | "h2" | "p" | "strong",
  text: string,
  className?: string,
): HTMLElement {
  const element = document.createElement(tag);
  element.textContent = text;
  if (className !== undefined) {
    element.className = className;
  }
  parent.append(element);
  return element;
}

const root = document.querySelector<HTMLElement>("#app");
if (root === null) {
  throw new Error("Quiet Trace app root is missing");
}

const mode = resolveDataMode(new URL(window.location.href));
addTextElement(root, "p", "LOCAL · AGGREGATE ONLY", "eyebrow");
addTextElement(root, "h1", "Quiet Trace");
addTextElement(
  root,
  "p",
  "Ambient loudness summaries stay on the device. No audio is stored or transmitted.",
  "lede",
);

const panel = document.createElement("section");
panel.setAttribute("aria-labelledby", "connection-heading");
const heading = addTextElement(
  panel,
  "h2",
  mode === "fixture" ? "Synthetic fixture" : "Device connection",
);
heading.id = "connection-heading";

if (mode === "fixture") {
  addTextElement(
    panel,
    "strong",
    "Fixture data — not microphone, bench, calibration, or field evidence.",
    "notice",
  );
  addTextElement(
    panel,
    "p",
    `Relative one-minute level: ${aggregateFixture.level_eq_dbfs.toFixed(1)} dBFS`,
  );
  addTextElement(panel, "p", "Calibration state: uncalibrated");
} else {
  addTextElement(
    panel,
    "p",
    "Disconnected. No production sensor data is simulated. Add ?fixture=1 only for deterministic UI development.",
  );
}

root.append(panel);
