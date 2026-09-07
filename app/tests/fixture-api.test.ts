import { describe, expect, it } from "vitest";
import { FixtureDeviceApi } from "../src/api/device-api";

describe("FixtureDeviceApi", () => {
  it("supports deterministic paging", async () => {
    const api = new FixtureDeviceApi();
    const first = await api.getRecordsPage(null, 5);

    expect(first.records).toHaveLength(5);
    expect(first.paging.has_more).toBe(true);
    expect(first.paging.next_after).not.toBeNull();

    const second = await api.getRecordsPage(first.paging.next_after, 5);
    expect(second.records[0]?.sequence).toBeGreaterThan(
      first.records.at(-1)?.sequence ?? 0,
    );
  });

  it("enforces annotation bounds", async () => {
    const api = new FixtureDeviceApi();

    await expect(
      api.postAnnotation({
        note: "",
        sessionId: null,
        sequence: null,
      }),
    ).rejects.toThrow(/between 1 and 240/);

    const ack = await api.postAnnotation({
      note: "Needs follow-up calibration at desk edge.",
      sessionId: "session_morning_lab",
      sequence: 110,
    });

    expect(ack.note).toMatch(/follow-up/);
  });

  it("exports aggregate-only payloads without credential fields", async () => {
    const api = new FixtureDeviceApi();
    const jsonExport = await api.exportData("json");

    expect(jsonExport.body).toContain('"schema": "quiet-trace/export/v1"');
    expect(jsonExport.body).not.toMatch(
      /wifi_password|token|secret|raw_audio|pcm/i,
    );
  });

  it("validates and applies backups with tickets", async () => {
    const api = new FixtureDeviceApi();
    const backup = await api.exportData("json");

    const validation = await api.validateBackup(backup.body);
    expect(validation.ok).toBe(true);
    expect(validation.ticket).toBeTruthy();

    const applied = await api.applyBackup(validation.ticket ?? "");
    expect(applied.ok).toBe(true);
  });

  it("requires physical confirmation for factory erase", async () => {
    const api = new FixtureDeviceApi();

    const noPhysical = await api.factoryErase({
      confirmationText: "ERASE",
      physicalConfirmed: false,
    });
    expect(noPhysical.ok).toBe(false);

    const erased = await api.factoryErase({
      confirmationText: "ERASE",
      physicalConfirmed: true,
    });
    expect(erased.ok).toBe(true);

    const page = await api.getRecordsPage(null, 50);
    expect(page.records).toHaveLength(0);
  });
});
