#include "quiet_trace/control_plane.hpp"

#include "quiet_trace/acquisition_pipeline.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace {

bool expect(const bool condition, const std::string_view description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    return false;
  }
  return true;
}

quiet_trace::StoredAggregate make_record(const std::uint64_t sequence) {
  return quiet_trace::StoredAggregate{
      .version = quiet_trace::kStoredAggregateVersion,
      .sequence = sequence,
      .monotonic_start_ms = sequence * 60'000ULL,
      .level_eq_mdbfs = -30'000,
      .peak_125ms_mdbfs = -20'000,
      .histogram_counts = {0, 0, 8, 64, 192, 152, 56, 8, 0, 0},
      .quality_flags_bits = 3,
      .checksum = 0,
  };
}

} // namespace

int main() {
  bool passed = true;

  passed &= expect(quiet_trace::classify_button_press(20) ==
                       quiet_trace::ButtonAction::none,
                   "short press ignored");
  passed &= expect(quiet_trace::classify_button_press(100) ==
                       quiet_trace::ButtonAction::mark_annotation,
                   "short hold marks annotation");
  passed &= expect(quiet_trace::classify_button_press(3'200) ==
                       quiet_trace::ButtonAction::open_setup,
                   "long hold opens setup");

  const auto setup_pattern =
      quiet_trace::led_pattern_for(quiet_trace::LedState::setup_window);
  passed &= expect(setup_pattern.pulses >= 2,
                   "setup LED pattern uses pulse count not color-only signal");
  passed &= expect(!setup_pattern.accessibility_label.empty(),
                   "LED pattern includes accessibility label");

  quiet_trace::SetupAuthorizer setup(1'000);
  passed &= expect(setup.open(false, 100, 0x11).empty(),
                   "setup requires physical button confirmation");

  const auto token = setup.open(true, 100, 0x1234);
  passed &= expect(!token.empty(), "setup token generated");
  passed &= expect(setup.authorize(token, 500), "token works before TTL");
  passed &= expect(!setup.authorize(token, 1200), "token expires at TTL");

  std::array<std::string_view, 2> safe_fields{"records", "quality_flags"};
  std::array<std::string_view, 2> forbidden_fields{"records", "samples"};
  passed &=
      expect(quiet_trace::validate_api_boundary(quiet_trace::ApiRequestBoundary{
                 .authenticated = false, .field_names = safe_fields}) ==
                 quiet_trace::ApiRejectReason::unauthenticated,
             "API rejects unauthenticated request");
  passed &=
      expect(quiet_trace::validate_api_boundary(quiet_trace::ApiRequestBoundary{
                 .authenticated = true, .field_names = forbidden_fields}) ==
                 quiet_trace::ApiRejectReason::forbidden_field,
             "API rejects forbidden field names");

  quiet_trace::AggregationCounters counters{
      .clipping_samples = 3,
      .framing_error_samples = 1,
      .acquisition_overruns = 2,
      .dropped_intervals = 4,
      .clock_uncertain_intervals = 5,
      .valid_samples = 900,
      .dropped_samples = 30,
  };
  const auto status_json = quiet_trace::render_status_json(
      counters, 12, 7, true, quiet_trace::CalibrationState::uncalibrated,
      quiet_trace::ClockQuality::monotonic_only, false);
  passed &= expect(status_json.find("\"schema\":\"quiet-trace/status/v1\"") !=
                       std::string::npos,
                   "status payload has expected schema tag");
  passed &= expect(status_json.find("\"samples\"") == std::string::npos,
                   "status payload does not include raw sample arrays");
  passed &= expect(status_json.find("wifi_password") == std::string::npos,
                   "status payload excludes credential fields");

  const auto aggregate_json =
      quiet_trace::render_aggregate_json(make_record(9));
  passed &= expect(aggregate_json.find("histogram_counts") != std::string::npos,
                   "aggregate payload includes histogram");
  passed &= expect(aggregate_json.find("waveform") == std::string::npos,
                   "aggregate payload excludes waveform fields");

  const auto sse_event =
      quiet_trace::render_sse_event("aggregate", aggregate_json);
  passed &= expect(sse_event.rfind("event: aggregate\n", 0) == 0,
                   "SSE payload starts with event header");
  passed &= expect(sse_event.find("\ndata: ") != std::string::npos,
                   "SSE payload contains data line");

  quiet_trace::AggregateRingStore store(4);
  store.append(make_record(1));

  quiet_trace::SetupAuthorizer router_setup(1'000);
  quiet_trace::UsbCommandRouter router(&router_setup, &store);
  const auto unauthorized_export = router.handle("export json", 500, false, 0);
  passed &= expect(!unauthorized_export.ok,
                   "USB export blocked until setup session opens");

  const auto setup_begin_denied =
      router.handle("setup begin", 500, false, 0xBEEF);
  passed &= expect(!setup_begin_denied.ok,
                   "USB setup begin denied without button confirmation");

  const auto setup_begin = router.handle("setup begin", 500, true, 0xBEEF);
  passed &= expect(setup_begin.ok, "USB setup begin succeeds");

  const auto wifi_saved = router.handle(
      "wifi set " + setup_begin.message + " office secretpass", 600, false, 0);
  passed &= expect(wifi_saved.ok, "wifi set accepted with valid setup token");
  passed &= expect(wifi_saved.message.find("secretpass") == std::string::npos,
                   "wifi command response redacts secret value");

  store.append(make_record(2));
  const auto erase_before_export =
      router.handle("erase records " + setup_begin.message, 700, false, 0);
  passed &=
      expect(!erase_before_export.ok, "erase blocked until explicit export");

  const auto export_json = router.handle("export json", 710, false, 0);
  passed &=
      expect(export_json.ok, "export command succeeds after setup active");

  const auto erase_after_export =
      router.handle("erase records " + setup_begin.message, 720, false, 0);
  passed &= expect(erase_after_export.ok,
                   "erase succeeds after export-before-erase gate");

  const auto bad_time =
      router.handle("time set " + setup_begin.message + " 2026-09-06 12:00:00",
                    730, false, 0);
  passed &= expect(!bad_time.ok, "invalid timestamp rejected");

  const auto valid_time =
      router.handle("time set " + setup_begin.message + " 2026-09-06T12:00:00Z",
                    740, false, 0);
  passed &= expect(valid_time.ok, "valid timestamp accepted");

  const auto factory_without_button =
      router.handle("erase factory " + setup_begin.message, 750, false, 0);
  passed &= expect(!factory_without_button.ok,
                   "factory erase requires physical hold");

  const auto factory_with_button =
      router.handle("erase factory " + setup_begin.message, 750, true, 0);
  passed &= expect(factory_with_button.ok,
                   "factory erase accepted with token and physical hold");

  const auto recovery =
      router.handle("reboot recovery " + setup_begin.message, 800, false, 0);
  passed &= expect(recovery.ok, "recovery reboot command accepted");

  return passed ? 0 : 1;
}
