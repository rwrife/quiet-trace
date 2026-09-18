#include "quiet_trace/aggregate_projection.hpp"
#include "quiet_trace/target.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string_view>
#include <vector>

namespace {

bool expect(const bool condition, const std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    return false;
  }
  return true;
}

quiet_trace::AcquisitionPipelineConfig test_config() {
  return quiet_trace::AcquisitionPipelineConfig{
      .sample_rate_hz = 800,
      .full_scale = 8'388'608,
      .framing_bits = 24,
      .dc_block_alpha = 0.995,
      .weighting_high_pass_alpha = 0.85,
      .enable_dc_block = false,
      .enable_weighting_approx = false,
  };
}

std::vector<std::int32_t> make_sine_minute(const double amplitude_ratio,
                                           const double frequency_hz) {
  const auto config = test_config();
  const std::size_t sample_count =
      static_cast<std::size_t>(config.sample_rate_hz) * 60U;
  std::vector<std::int32_t> values(sample_count);
  for (std::size_t index = 0; index < sample_count; ++index) {
    const double t =
        static_cast<double>(index) / static_cast<double>(config.sample_rate_hz);
    values[index] = static_cast<std::int32_t>(std::llround(
        amplitude_ratio * std::sin(2.0 * std::numbers::pi * frequency_hz * t) *
        static_cast<double>(config.full_scale)));
  }
  return values;
}

// App-side parseAggregateRecord constraints that must hold for the emitted
// JSON text (checked as string invariants without a JSON parser).
bool json_invariants_hold(const std::string &json) {
  if (json.find("\"schema\":\"quiet-trace/aggregate/v1\"") ==
      std::string::npos) {
    return false;
  }
  // Forbidden field names must never appear as keys.
  for (const auto forbidden :
       {std::string_view("\"samples\""), std::string_view("\"pcm\""),
        std::string_view("\"waveform\""), std::string_view("\"spectrum\""),
        std::string_view("\"wifi_password\""), std::string_view("\"secret\""),
        std::string_view("\"speech\""), std::string_view("\"session\"")}) {
    if (json.find(forbidden) != std::string::npos) {
      return false;
    }
  }
  return true;
}

} // namespace

int main() {
  bool passed = true;

  // --- Minute projection ----------------------------------------------------
  quiet_trace::AcquisitionPipeline pipeline(test_config());
  auto minute_samples = make_sine_minute(0.5, 8.0);
  pipeline.ingest_block(minute_samples, false, false, false);
  const auto minute = pipeline.consume_minute();

  passed &= expect(quiet_trace::is_storable_minute(minute),
                   "complete sine minute is storable");

  const auto bits = quiet_trace::quality_bits_for(minute);
  passed &= expect((bits & quiet_trace::kQualityBitUncalibrated) != 0U,
                   "default pipeline records uncalibrated bit");
  passed &= expect((bits & quiet_trace::kQualityBitWeightingUnvalidated) != 0U,
                   "default pipeline records weighting_unvalidated bit");
  passed &= expect((bits & quiet_trace::kQualityBitClipped) == 0U,
                   "mid-scale sine does not set clipped");

  const auto names = quiet_trace::quality_flag_names(bits);
  passed &= expect(names.size() <= quiet_trace::kMaxQualityFlags,
                   "stored flag names fit the schema flag budget");
  passed &= expect(!names.empty(), "uncalibrated flag name present");

  const auto stored =
      quiet_trace::stored_aggregate_from(minute, 42, 12'000'000ULL);
  passed &= expect(stored.version == quiet_trace::kStoredAggregateVersion,
                   "projection uses current stored version");
  passed &= expect(stored.sequence == 42, "projection carries sequence");
  std::int64_t histogram_total = 0;
  for (const auto count : stored.histogram_counts) {
    histogram_total += count;
  }
  passed &=
      expect(histogram_total == 480, "projected histogram totals 480 windows");
  passed &= expect(stored.peak_125ms_mdbfs >= stored.level_eq_mdbfs,
                   "projected peak >= level after rounding/clamping");
  passed &=
      expect(stored.level_eq_mdbfs >= -200'000 && stored.level_eq_mdbfs <= 0,
             "projected level within -200..0 dBFS bounds");

  // --- Clock helpers
  // ----------------------------------------------------------
  passed &=
      expect(quiet_trace::target::classify_clock_quality(false, false, false) ==
                 quiet_trace::ClockQuality::monotonic_only,
             "no time source yields monotonic_only");
  passed &=
      expect(quiet_trace::target::classify_clock_quality(false, true, false) ==
                 quiet_trace::ClockQuality::host_set,
             "host-set time without sync failure yields host_set");
  passed &=
      expect(quiet_trace::target::classify_clock_quality(false, true, true) ==
                 quiet_trace::ClockQuality::monotonic_only,
             "failed sync demotes host_set clock");
  passed &= expect(quiet_trace::target::classify_clock_quality(
                       true, false, false) == quiet_trace::ClockQuality::synced,
                   "synced wins over other sources");

  // 2026-09-06T12:00:00Z == 1'788'696'000 seconds since epoch (verified
  // against `date -u`).
  const auto parsed =
      quiet_trace::target::parse_iso8601_utc_ms("2026-09-06T12:00:00Z");
  passed &= expect(parsed.has_value() && *parsed == 1'788'696'000'000LL,
                   "valid ISO-8601 parses to expected epoch ms");
  passed &=
      expect(!quiet_trace::target::parse_iso8601_utc_ms("2026-09-06 12:00:00")
                  .has_value(),
             "non-ISO timestamp rejected");
  passed &=
      expect(!quiet_trace::target::parse_iso8601_utc_ms("2026-02-30T00:00:00Z")
                  .has_value(),
             "impossible calendar date rejected");
  passed &=
      expect(!quiet_trace::target::parse_iso8601_utc_ms("1969-12-31T23:59:59Z")
                  .has_value(),
             "pre-epoch timestamp rejected");
  passed &= expect(quiet_trace::iso8601_utc_from_epoch_ms(
                       1'788'696'000'000LL) == "2026-09-06T12:00:00Z",
                   "epoch ms renders back to the same ISO-8601 string");
  passed &= expect(quiet_trace::iso8601_utc_from_epoch_ms(0) ==
                       "1970-01-01T00:00:00Z",
                   "epoch zero renders to Unix epoch");
  passed &= expect(quiet_trace::iso8601_utc_from_epoch_ms(
                       1'751'327'999'000LL) == "2025-06-30T23:59:59Z",
                   "leap-year boundary date renders correctly");

  // --- JSON rendering
  // ---------------------------------------------------------
  {
    const auto json = quiet_trace::render_aggregate_record_json(
        stored, quiet_trace::ClockQuality::monotonic_only, std::nullopt);
    passed &= expect(json_invariants_hold(json),
                     "monotonic aggregate JSON satisfies app contract keys");
    passed &= expect(json.find("\"interval_start\":null") != std::string::npos,
                     "monotonic quality carries null interval_start");
    passed &= expect(json.find("\"wall_time_unknown\"") != std::string::npos,
                     "monotonic quality adds wall_time_unknown flag");
    passed &= expect(json.find("\"clock_quality\":\"monotonic_only\"") !=
                         std::string::npos,
                     "clock quality serialized");
    passed &= expect(json.find("\"session_id\":null") != std::string::npos,
                     "session id present and null");
    passed &= expect(json.find("\"uncalibrated\"") != std::string::npos,
                     "uncalibrated flag serialized");
  }
  {
    const std::optional<std::int64_t> offset =
        1'788'696'000'000LL - 12'000'000; // aligned with monotonic start
    const auto json = quiet_trace::render_aggregate_record_json(
        stored, quiet_trace::ClockQuality::synced, offset);
    passed &= expect(json_invariants_hold(json),
                     "synced aggregate JSON satisfies app contract keys");
    passed &= expect(json.find("\"interval_start\":\"2026-09-06T12:00:00Z\"") !=
                         std::string::npos,
                     "synced quality derives wall-clock interval_start");
    passed &= expect(json.find("\"wall_time_unknown\"") == std::string::npos,
                     "synced quality has no wall_time_unknown flag");
  }
  {
    const auto invalid = stored;
    auto with_invalid = stored;
    with_invalid.quality_flags_bits |=
        quiet_trace::kQualityBitCalibrationInvalid;
    with_invalid.quality_flags_bits &= ~quiet_trace::kQualityBitUncalibrated;
    const auto json = quiet_trace::render_aggregate_record_json(
        with_invalid, quiet_trace::ClockQuality::monotonic_only, std::nullopt);
    passed &= expect(json.find("\"calibration\":{\"state\":\"invalid\"") !=
                         std::string::npos,
                     "invalid calibration bit projects to invalid state");
    passed &= expect(json_invariants_hold(json),
                     "invalid-calibration JSON satisfies app contract keys");
    static_cast<void>(invalid);
  }

  // --- Export envelopes
  // -------------------------------------------------------
  {
    std::vector<quiet_trace::StoredAggregate> records;
    records.push_back(stored);
    records.push_back(
        quiet_trace::stored_aggregate_from(minute, 43, 12'060'000ULL));

    const auto json = quiet_trace::target::render_usb_export_json(
        records, quiet_trace::ClockQuality::monotonic_only, std::nullopt,
        12'120'000LL);
    passed &= expect(json.find("\"schema\":\"quiet-trace/export/v1\"") !=
                         std::string::npos,
                     "export JSON carries frozen export schema");
    passed &= expect(json.find("\"exported_at\":null") != std::string::npos,
                     "monotonic export carries null exported_at");
    passed &= expect(json.find("\"source\":\"device\"") != std::string::npos,
                     "export marks device source");
    passed &= expect(json.find("\"sessions\":[]") != std::string::npos,
                     "export carries empty sessions list");
    passed &= expect(json.find("\"annotations\":[]") != std::string::npos,
                     "export carries empty annotations list");

    // exported_at epoch ms = offset + uptime_us/1000.
    const std::optional<std::int64_t> now_offset = 1'788'696'000'000LL - 12'120;
    const auto synced_json = quiet_trace::target::render_usb_export_json(
        records, quiet_trace::ClockQuality::synced, now_offset, 12'120'000LL);
    passed &=
        expect(synced_json.find("\"exported_at\":\"2026-09-06T12:00:00Z\"") !=
                   std::string::npos,
               "synced export stamps exported_at");

    // interval_start epoch ms = offset + monotonic_start_ms (12'000'000).
    const std::optional<std::int64_t> record_offset =
        1'788'696'000'000LL - 12'000'000;
    const auto csv = quiet_trace::target::render_usb_export_csv(
        records, quiet_trace::ClockQuality::synced, record_offset);
    passed &=
        expect(csv.rfind("sequence,interval_start,interval_end,level_eq_dbfs,",
                         0) == 0,
               "CSV header matches the dashboard export columns");
    passed &= expect(csv.find("\"42\"") != std::string::npos,
                     "CSV rows quote sequence values");
    passed &= expect(csv.find("2026-09-06T12:00:00Z") != std::string::npos,
                     "CSV includes wall-clock interval start");
  }

  // --- Console dispatch (SystemState seam)
  // ------------------------------------
  {
    quiet_trace::target::SystemState state;
    const auto unknown =
        quiet_trace::target::handle_console_line(state, "wat", 1000, false, 7);
    passed &= expect(!unknown.ok && unknown.code == "bad_command",
                     "unknown console command rejected");

    const auto denied = quiet_trace::target::handle_console_line(
        state, "setup begin", 1000, false, 11);
    passed &=
        expect(!denied.ok, "setup begin rejected without physical presence");

    const auto opened = quiet_trace::target::handle_console_line(
        state, "setup begin", 1000, true, 12);
    passed &= expect(opened.ok && !opened.message.empty(),
                     "setup begin yields token with physical presence");
    const auto token = opened.message;

    const auto bad_time = quiet_trace::target::handle_console_line(
        state, "time set " + token + " not-a-time", 1200, false, 0);
    passed &= expect(!bad_time.ok, "malformed time value rejected");

    const auto time_ok = quiet_trace::target::handle_console_line(
        state, "time set " + token + " 2026-09-06T12:00:00Z", 1200, false, 0);
    passed &= expect(time_ok.ok, "valid time value accepted");
    const auto snapshot = state.clock_snapshot(1200);
    passed &= expect(snapshot.quality == quiet_trace::ClockQuality::host_set &&
                         snapshot.epoch_offset_ms.has_value(),
                     "time set promotes clock to host_set with offset");
    passed &= expect(*snapshot.epoch_offset_ms ==
                         1'788'696'000'000LL - 1, // 1200 us -> 1 ms
                     "clock offset anchored to host time at set instant");

    const auto wifi = quiet_trace::target::handle_console_line(
        state, "wifi set " + token + " office secretpass", 1300, false, 0);
    passed &= expect(wifi.ok, "wifi set accepted during setup window");
    std::string ssid;
    std::string password;
    passed &= expect(state.take_wifi_credentials(ssid, password) &&
                         ssid == "office" && password == "secretpass",
                     "wifi credentials staged into volatile holder");
    passed &= expect(!state.take_wifi_credentials(ssid, password),
                     "credential holder single-consumption");

    const auto unauth_wifi = quiet_trace::target::handle_console_line(
        state, "wifi set deadbeef office secretpass", 1400, false, 0);
    passed &=
        expect(!unauth_wifi.ok, "wifi set rejected with invalid setup token");

    const auto reboot = quiet_trace::target::handle_console_line(
        state, "reboot recovery " + token, 1500, false, 0);
    passed &= expect(reboot.ok && state.reboot_requested(),
                     "reboot recovery authorized and latched");
  }

  return passed ? 0 : 1;
}
