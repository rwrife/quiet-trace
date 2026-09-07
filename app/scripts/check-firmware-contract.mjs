import { readFileSync } from "node:fs";

const firmwareControlPlanePath = new URL(
  "../../firmware/components/domain/control_plane.cpp",
  import.meta.url,
);
const statusSchemaPath = new URL(
  "../../docs/schemas/status-v1.schema.json",
  import.meta.url,
);

const controlPlane = readFileSync(firmwareControlPlanePath, "utf8");
const statusSchema = JSON.parse(readFileSync(statusSchemaPath, "utf8"));

const expectedStatusTokens = [
  "quiet-trace/status/v1",
  "records",
  "generation",
  "setup_active",
  "calibration_state",
  "clock_quality",
  "fixture_mode",
  "counters",
  "clipping_samples",
  "framing_error_samples",
  "acquisition_overruns",
  "dropped_intervals",
  "clock_uncertain_intervals",
  "valid_samples",
  "dropped_samples",
];

for (const token of expectedStatusTokens) {
  if (!controlPlane.includes(token)) {
    console.error(`Firmware status serializer missing token: ${token}`);
    process.exit(1);
  }
}

const required = statusSchema.required ?? [];
const requiredFields = [
  "schema",
  "records",
  "generation",
  "setup_active",
  "calibration_state",
  "clock_quality",
  "fixture_mode",
  "counters",
];

for (const field of requiredFields) {
  if (!required.includes(field)) {
    console.error(`Status schema missing required field: ${field}`);
    process.exit(1);
  }
}

console.log("Firmware control-plane contract check passed.");
