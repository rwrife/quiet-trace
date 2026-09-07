import {
  parseDeviceStatus,
  type DeviceStatusV1,
} from "../contracts/device-protocol";

const statusFixturePayload = {
  schema: "quiet-trace/status/v1",
  records: 24,
  generation: 3,
  setup_active: false,
  calibration_state: "reference_adjusted",
  clock_quality: "host_set",
  fixture_mode: true,
  counters: {
    clipping_samples: 12,
    framing_error_samples: 2,
    acquisition_overruns: 1,
    dropped_intervals: 3,
    clock_uncertain_intervals: 4,
    valid_samples: 48_000,
    dropped_samples: 160,
  },
} as const;

export const fixtureStatus: Readonly<DeviceStatusV1> =
  parseDeviceStatus(statusFixturePayload);
