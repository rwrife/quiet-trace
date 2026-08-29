#include "quiet_trace/aggregate_contract.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool expect(bool condition, std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    return false;
  }
  return true;
}

} // namespace

int main() {
  using quiet_trace::AggregateRecordV1;
  using quiet_trace::CalibrationMetadataV1;
  using quiet_trace::CalibrationState;
  using quiet_trace::ClockQuality;

  AggregateRecordV1 fixture{
      .sequence = 42,
      .interval_start = {},
      .has_interval_start = false,
      .monotonic_start_ms = 2'520'000,
      .duration_ms = 60'000,
      .level_eq_dbfs = -38.5,
      .peak_125ms_dbfs = -21.25,
      .histogram_counts = {0, 0, 8, 64, 192, 152, 56, 8, 0, 0},
      .calibration =
          CalibrationMetadataV1{
              .state = CalibrationState::uncalibrated,
              .offset_db = 0.0,
              .has_offset = false,
              .calibrated_at = {},
              .method = {},
              .reference_instrument = {},
              .reference_placement = {},
              .reference_source = {},
              .reference_duration_s = 0,
              .has_reference_duration = false,
              .firmware_version = {},
              .hardware_revision = {},
          },
      .clock_quality = ClockQuality::monotonic_only,
      .quality_flags = {"uncalibrated", "wall_time_unknown"},
      .quality_flag_count = 2,
      .session_id = "fixture-office",
      .has_session_id = true,
  };

  bool passed = expect(quiet_trace::is_valid(fixture), "fixture is valid");

  auto invalid_histogram = fixture;
  invalid_histogram.histogram_counts = {0, 0, 1, 8, 24, 19, 7, 1, 0, 0};
  passed &= expect(!quiet_trace::is_valid(invalid_histogram),
                   "histogram must represent 480 complete windows");

  auto invalid_duration = fixture;
  invalid_duration.duration_ms = 1'000;
  passed &= expect(!quiet_trace::is_valid(invalid_duration),
                   "non-minute persistence interval is rejected");

  auto invalid_peak = fixture;
  invalid_peak.peak_125ms_dbfs = -40.0;
  passed &= expect(!quiet_trace::is_valid(invalid_peak),
                   "peak below equivalent level is rejected");

  auto valid_calibration = fixture;
  valid_calibration.calibration = CalibrationMetadataV1{
      .state = CalibrationState::reference_adjusted,
      .offset_db = 42.0,
      .has_offset = true,
      .calibrated_at = "2026-08-28T12:00:00Z",
      .method = "side-by-side comparison",
      .reference_instrument = "traceable reference meter asset QT-REF-01",
      .reference_placement = "capsules adjacent at desk position",
      .reference_source = "steady broadband calibration signal",
      .reference_duration_s = 300,
      .has_reference_duration = true,
      .firmware_version = "0.1.0",
      .hardware_revision = "unbuilt-fixture",
  };
  passed &= expect(quiet_trace::is_valid(valid_calibration),
                   "complete reference provenance is accepted");

  auto incomplete_calibration = fixture;
  incomplete_calibration.calibration.state =
      CalibrationState::reference_adjusted;
  incomplete_calibration.calibration.offset_db = 42.0;
  incomplete_calibration.calibration.has_offset = true;
  passed &= expect(!quiet_trace::is_valid(incomplete_calibration),
                   "reference adjustment requires date and method");

  auto hidden_duration = fixture;
  hidden_duration.calibration.reference_duration_s = 300;
  passed &= expect(!quiet_trace::is_valid(hidden_duration),
                   "uncalibrated record cannot hide reference duration");

  auto valid_timestamp = fixture;
  valid_timestamp.interval_start = "2024-02-29T23:59:59.123Z";
  valid_timestamp.has_interval_start = true;
  valid_timestamp.clock_quality = ClockQuality::host_set;
  passed &= expect(quiet_trace::is_valid(valid_timestamp),
                   "valid UTC leap-day timestamp is accepted");

  constexpr std::array<std::string_view, 8> kInvalidTimestamps{
      "2026-02-29T12:00:00Z",      "2016-12-31T23:59:60Z",
      "2026-08-28T12:00:00.Z",     "2026-08-28 12:00:00Z",
      "2026-08-28t12:00:00Z",      "2026-08-28T12:00:00.1234567890Z",
      "2026-08-28T12:00:00+00:00", "0000-01-01T00:00:00Z",
  };
  for (const auto timestamp : kInvalidTimestamps) {
    auto invalid_timestamp = fixture;
    invalid_timestamp.interval_start = timestamp;
    invalid_timestamp.has_interval_start = true;
    invalid_timestamp.clock_quality = ClockQuality::host_set;
    passed &= expect(!quiet_trace::is_valid(invalid_timestamp),
                     "non-canonical UTC timestamp is rejected");
  }

  auto unknown_with_wall_time = valid_timestamp;
  unknown_with_wall_time.clock_quality = ClockQuality::unknown;
  passed &= expect(!quiet_trace::is_valid(unknown_with_wall_time),
                   "unknown clock quality rejects wall time");

  auto monotonic_with_wall_time = valid_timestamp;
  monotonic_with_wall_time.clock_quality = ClockQuality::monotonic_only;
  passed &= expect(!quiet_trace::is_valid(monotonic_with_wall_time),
                   "monotonic-only clock rejects wall time");

  auto host_set_without_wall_time = fixture;
  host_set_without_wall_time.clock_quality = ClockQuality::host_set;
  passed &= expect(!quiet_trace::is_valid(host_set_without_wall_time),
                   "host-set clock requires wall time");

  auto synced_without_wall_time = fixture;
  synced_without_wall_time.clock_quality = ClockQuality::synced;
  passed &= expect(!quiet_trace::is_valid(synced_without_wall_time),
                   "synced clock requires wall time");

  for (const auto timestamp : kInvalidTimestamps) {
    auto invalid_calibration_timestamp = valid_calibration;
    invalid_calibration_timestamp.calibration.calibrated_at = timestamp;
    passed &= expect(!quiet_trace::is_valid(invalid_calibration_timestamp),
                     "non-canonical calibration timestamp is rejected");
  }

  std::string unicode_session;
  for (std::size_t index = 0; index < 64; ++index) {
    unicode_session += "\xF0\x9F\x98\x80";
  }
  auto valid_unicode_session = fixture;
  valid_unicode_session.session_id = unicode_session;
  passed &= expect(quiet_trace::is_valid(valid_unicode_session),
                   "64 Unicode scalar session id is accepted");
  unicode_session += "\xF0\x9F\x98\x80";
  auto invalid_unicode_session = fixture;
  invalid_unicode_session.session_id = unicode_session;
  passed &= expect(!quiet_trace::is_valid(invalid_unicode_session),
                   "65 Unicode scalar session id is rejected");

  const std::string malformed_utf8{"\xF0\x28\x8C\x28", 4};
  auto invalid_utf8_session = fixture;
  invalid_utf8_session.session_id = malformed_utf8;
  passed &= expect(!quiet_trace::is_valid(invalid_utf8_session),
                   "malformed UTF-8 session id is rejected");

  std::string unicode_method;
  for (std::size_t index = 0; index < 240; ++index) {
    unicode_method += "\xF0\x9F\x98\x80";
  }
  auto valid_unicode_calibration = valid_calibration;
  valid_unicode_calibration.calibration.method = unicode_method;
  passed &= expect(quiet_trace::is_valid(valid_unicode_calibration),
                   "240 Unicode scalar calibration method is accepted");

  auto unsafe_sequence = fixture;
  unsafe_sequence.sequence = quiet_trace::kMaxJsonSafeInteger + 1;
  passed &= expect(!quiet_trace::is_valid(unsafe_sequence),
                   "sequence above JSON safe integer is rejected");

  auto unsafe_monotonic = fixture;
  unsafe_monotonic.monotonic_start_ms = quiet_trace::kMaxJsonSafeInteger + 1;
  passed &= expect(!quiet_trace::is_valid(unsafe_monotonic),
                   "monotonic time above JSON safe integer is rejected");

  auto invalid_session = fixture;
  invalid_session.session_id =
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  passed &= expect(!quiet_trace::is_valid(invalid_session),
                   "session id over 64 characters is rejected");

  constexpr std::array forbidden{
      std::string_view{"samples"},       std::string_view{"raw_samples"},
      std::string_view{"pcm"},           std::string_view{"encoded_audio"},
      std::string_view{"encoded_media"}, std::string_view{"recording"},
      std::string_view{"waveform"},      std::string_view{"spectrum"},
      std::string_view{"fft_bins"},      std::string_view{"transcript"},
      std::string_view{"speech"},        std::string_view{"voice_activity"},
      std::string_view{"speaker_id"},    std::string_view{"source_label"},
      std::string_view{"event_label"},   std::string_view{"wifi_password"},
      std::string_view{"auth_token"},
  };
  for (const auto field : forbidden) {
    passed &= expect(quiet_trace::is_forbidden_persistent_field(field), field);
  }
  passed &= expect(!quiet_trace::is_forbidden_persistent_field("quality_flags"),
                   "quality flags are allowed");
  passed &= expect(!quiet_trace::is_forbidden_persistent_field("session_id"),
                   "session id is allowed");

  return passed ? 0 : 1;
}
