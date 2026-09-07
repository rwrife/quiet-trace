import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import type { AnySchema } from "ajv";
import Ajv2020 from "ajv/dist/2020";
import addFormats from "ajv-formats";
import {
  parseDeviceStatus,
  parseRecordsPage,
} from "../src/contracts/device-protocol";

function loadJson(path: string): unknown {
  return JSON.parse(
    readFileSync(new URL(path, import.meta.url), "utf8"),
  ) as unknown;
}

function makeAjv(): Ajv2020 {
  const ajv = new Ajv2020({ allErrors: true, strict: false });
  addFormats(ajv);
  const aggregateSchema = loadJson(
    "../../docs/schemas/aggregate-record-v1.schema.json",
  ) as AnySchema;
  ajv.addSchema(aggregateSchema);
  return ajv;
}

describe("status + records protocol schemas", () => {
  it("validates status fixture against schema and parser", () => {
    const ajv = makeAjv();
    const schema = loadJson(
      "../../docs/schemas/status-v1.schema.json",
    ) as AnySchema;
    const fixture = loadJson("../../docs/schemas/fixtures/status-v1.json");

    const validator = ajv.compile(schema);
    const valid = validator(fixture);
    expect(valid).toBe(true);

    const parsed = parseDeviceStatus(fixture);
    expect(parsed.schema).toBe("quiet-trace/status/v1");
  });

  it("validates records page fixture against schema and parser", () => {
    const ajv = makeAjv();
    const schema = loadJson(
      "../../docs/schemas/records-page-v1.schema.json",
    ) as AnySchema;
    const fixture = loadJson(
      "../../docs/schemas/fixtures/records-page-v1.json",
    );

    const validator = ajv.compile(schema);
    const valid = validator(fixture);
    expect(valid).toBe(true);

    const parsed = parseRecordsPage(fixture);
    expect(parsed.records).toHaveLength(2);
  });
});
