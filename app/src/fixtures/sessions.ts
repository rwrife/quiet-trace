import {
  parseSessionsResponse,
  type SessionsResponseV1,
} from "../contracts/device-protocol";

const sessionsFixturePayload = {
  schema: "quiet-trace/sessions/v1",
  sessions: [
    {
      session_id: "session_morning_lab",
      label: "Morning lab run",
      records: 10,
      interval_start: "2026-09-07T12:00:00Z",
      interval_end: "2026-09-07T12:10:00Z",
      level_eq_avg_dbfs: -28.4,
      peak_dbfs: -17.2,
      calibration_state: "reference_adjusted",
      clock_quality: "host_set",
      quality_flags: ["calibration_pending"],
    },
    {
      session_id: "session_evening_office",
      label: "Evening office sample",
      records: 14,
      interval_start: "2026-09-07T18:00:00Z",
      interval_end: "2026-09-07T18:14:00Z",
      level_eq_avg_dbfs: -31.1,
      peak_dbfs: -21.5,
      calibration_state: "uncalibrated",
      clock_quality: "monotonic_only",
      quality_flags: ["wall_time_unknown", "uncalibrated"],
    },
  ],
} as const;

export const fixtureSessions: Readonly<SessionsResponseV1> =
  parseSessionsResponse(sessionsFixturePayload);
