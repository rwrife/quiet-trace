#include "quiet_trace/control_plane.hpp"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace quiet_trace {
namespace {

std::string hex_token(const std::uint64_t seed) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << seed;
  return stream.str();
}

std::vector<std::string_view> split_tokens(const std::string_view line) {
  std::vector<std::string_view> tokens;
  std::size_t index = 0;
  while (index < line.size()) {
    while (index < line.size() && line[index] == ' ') {
      ++index;
    }
    if (index >= line.size()) {
      break;
    }
    std::size_t next = index;
    while (next < line.size() && line[next] != ' ') {
      ++next;
    }
    tokens.emplace_back(line.substr(index, next - index));
    index = next;
  }
  return tokens;
}

std::string clock_quality_string(const ClockQuality quality) {
  switch (quality) {
  case ClockQuality::unknown:
    return "unknown";
  case ClockQuality::monotonic_only:
    return "monotonic_only";
  case ClockQuality::host_set:
    return "host_set";
  case ClockQuality::synced:
    return "synced";
  }
  return "unknown";
}

std::string calibration_state_string(const CalibrationState state) {
  switch (state) {
  case CalibrationState::uncalibrated:
    return "uncalibrated";
  case CalibrationState::reference_adjusted:
    return "reference_adjusted";
  case CalibrationState::invalid:
    return "invalid";
  }
  return "invalid";
}

bool is_iso8601_utc(const std::string_view timestamp) {
  if (timestamp.size() < 20U || timestamp.size() > 30U ||
      timestamp.back() != 'Z') {
    return false;
  }
  if (timestamp[4] != '-' || timestamp[7] != '-' || timestamp[10] != 'T' ||
      timestamp[13] != ':' || timestamp[16] != ':') {
    return false;
  }

  for (std::size_t index = 0; index < timestamp.size() - 1U; ++index) {
    if (index == 4U || index == 7U || index == 10U || index == 13U ||
        index == 16U || index == 19U) {
      continue;
    }
    if (timestamp[index] < '0' || timestamp[index] > '9') {
      return false;
    }
  }
  return true;
}

} // namespace

ButtonAction classify_button_press(const std::uint32_t duration_ms) noexcept {
  if (duration_ms >= 3'000U) {
    return ButtonAction::open_setup;
  }
  if (duration_ms >= 50U) {
    return ButtonAction::mark_annotation;
  }
  return ButtonAction::none;
}

LedPattern led_pattern_for(const LedState state) noexcept {
  switch (state) {
  case LedState::idle:
    return LedPattern{.on_ms = 30,
                      .off_ms = 1970,
                      .pulses = 1,
                      .repeating = true,
                      .accessibility_label = "idle heartbeat"};
  case LedState::active:
    return LedPattern{.on_ms = 120,
                      .off_ms = 880,
                      .pulses = 1,
                      .repeating = true,
                      .accessibility_label = "aggregate logging active"};
  case LedState::setup_window:
    return LedPattern{.on_ms = 120,
                      .off_ms = 120,
                      .pulses = 3,
                      .repeating = true,
                      .accessibility_label = "setup window open"};
  case LedState::quality_warning:
    return LedPattern{.on_ms = 240,
                      .off_ms = 240,
                      .pulses = 2,
                      .repeating = true,
                      .accessibility_label = "quality warning"};
  case LedState::recovery:
    return LedPattern{.on_ms = 100,
                      .off_ms = 100,
                      .pulses = 5,
                      .repeating = true,
                      .accessibility_label = "recovery mode"};
  case LedState::error:
    return LedPattern{.on_ms = 500,
                      .off_ms = 500,
                      .pulses = 1,
                      .repeating = true,
                      .accessibility_label = "error"};
  }
  return LedPattern{.on_ms = 500,
                    .off_ms = 500,
                    .pulses = 1,
                    .repeating = true,
                    .accessibility_label = "error"};
}

SetupAuthorizer::SetupAuthorizer(const std::uint64_t ttl_ms) noexcept
    : ttl_ms_(ttl_ms), token_(), expires_at_ms_(0) {}

std::string SetupAuthorizer::open(const bool physical_button_confirmed,
                                  const std::uint64_t now_ms,
                                  const std::uint64_t nonce) noexcept {
  if (!physical_button_confirmed) {
    return {};
  }
  token_ = hex_token(nonce ^ now_ms);
  expires_at_ms_ = now_ms + ttl_ms_;
  return token_;
}

bool SetupAuthorizer::authorize(const std::string_view token,
                                const std::uint64_t now_ms) const noexcept {
  return active(now_ms) && token == token_;
}

bool SetupAuthorizer::active(const std::uint64_t now_ms) const noexcept {
  return !token_.empty() && now_ms <= expires_at_ms_;
}

void SetupAuthorizer::close() noexcept {
  token_.clear();
  expires_at_ms_ = 0;
}

ApiRejectReason
validate_api_boundary(const ApiRequestBoundary &request) noexcept {
  if (!request.authenticated) {
    return ApiRejectReason::unauthenticated;
  }
  return std::any_of(request.field_names.begin(), request.field_names.end(),
                     [](const std::string_view field) {
                       return is_forbidden_persistent_field(field);
                     })
             ? ApiRejectReason::forbidden_field
             : ApiRejectReason::none;
}

std::string render_status_json(const AggregationCounters &counters,
                               const std::size_t stored_records,
                               const std::uint64_t generation,
                               const bool setup_active,
                               const CalibrationState calibration_state,
                               const ClockQuality clock_quality,
                               const bool fixture_mode) {
  std::ostringstream stream;
  stream << '{' << "\"schema\":\"quiet-trace/status/v1\","
         << "\"records\":" << stored_records << ','
         << "\"generation\":" << generation << ','
         << "\"setup_active\":" << std::boolalpha << setup_active << ','
         << "\"calibration_state\":\""
         << calibration_state_string(calibration_state) << "\","
         << "\"clock_quality\":\"" << clock_quality_string(clock_quality)
         << "\"," << "\"fixture_mode\":" << fixture_mode << ','
         << "\"counters\":{"
         << "\"clipping_samples\":" << counters.clipping_samples << ','
         << "\"framing_error_samples\":" << counters.framing_error_samples
         << ',' << "\"acquisition_overruns\":" << counters.acquisition_overruns
         << ',' << "\"dropped_intervals\":" << counters.dropped_intervals << ','
         << "\"clock_uncertain_intervals\":"
         << counters.clock_uncertain_intervals << ','
         << "\"valid_samples\":" << counters.valid_samples << ','
         << "\"dropped_samples\":" << counters.dropped_samples << "}}";
  return stream.str();
}

std::string render_aggregate_json(const StoredAggregate &record) {
  std::ostringstream stream;
  stream << '{' << "\"schema\":\"quiet-trace/aggregate/v1\","
         << "\"sequence\":" << record.sequence << ','
         << "\"monotonic_start_ms\":" << record.monotonic_start_ms << ','
         << "\"duration_ms\":60000," << "\"level_eq_dbfs\":" << std::fixed
         << std::setprecision(3)
         << static_cast<double>(record.level_eq_mdbfs) / 1000.0 << ','
         << "\"peak_125ms_dbfs\":"
         << static_cast<double>(record.peak_125ms_mdbfs) / 1000.0 << ','
         << "\"histogram_counts\":[";

  for (std::size_t index = 0; index < record.histogram_counts.size(); ++index) {
    if (index != 0U) {
      stream << ',';
    }
    stream << record.histogram_counts[index];
  }

  stream << "],\"quality_flags_bits\":" << record.quality_flags_bits << '}';
  return stream.str();
}

std::string render_sse_event(const std::string_view event,
                             const std::string_view payload_json) {
  std::ostringstream stream;
  stream << "event: " << event << "\n";
  stream << "data: " << payload_json << "\n\n";
  return stream.str();
}

UsbCommandRouter::UsbCommandRouter(SetupAuthorizer *setup,
                                   AggregateRingStore *store) noexcept
    : setup_(setup), store_(store) {}

UsbResponse UsbCommandRouter::handle(const std::string_view line,
                                     const std::uint64_t now_ms,
                                     const bool physical_button_confirmed,
                                     const std::uint64_t nonce) noexcept {
  const auto tokens = split_tokens(line);
  if (tokens.empty()) {
    return {.ok = false, .code = "bad_command", .message = "empty command"};
  }

  if (tokens[0] == "status") {
    return {.ok = true,
            .code = "ok",
            .message =
                setup_->active(now_ms) ? "setup_active" : "setup_inactive"};
  }

  if (tokens.size() == 2U && tokens[0] == "setup" && tokens[1] == "begin") {
    const auto token = setup_->open(physical_button_confirmed, now_ms, nonce);
    if (token.empty()) {
      return {.ok = false,
              .code = "physical_presence_required",
              .message = "press setup/mark button before setup"};
    }
    return {.ok = true, .code = "ok", .message = token};
  }

  if (tokens.size() == 3U && tokens[0] == "time" && tokens[1] == "set") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    return {.ok = true, .code = "ok", .message = "time_set_pending_value"};
  }

  if (tokens.size() >= 5U && tokens[0] == "wifi" && tokens[1] == "set") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    if (tokens[3].empty()) {
      return {.ok = false, .code = "bad_command", .message = "ssid required"};
    }
    return {.ok = true, .code = "ok", .message = "wifi_saved"};
  }

  if (tokens.size() == 3U && tokens[0] == "time" && tokens[1] == "validate") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    return {.ok = true, .code = "ok", .message = "time_validated"};
  }

  if (tokens.size() == 2U && tokens[0] == "export") {
    if (!setup_->active(now_ms)) {
      return {.ok = false,
              .code = "unauthorized",
              .message = "open setup session first"};
    }
    if (tokens[1] != "json" && tokens[1] != "csv") {
      return {.ok = false, .code = "bad_command", .message = "export json|csv"};
    }
    static_cast<void>(store_->export_records());
    return {.ok = true, .code = "ok", .message = "export_ready"};
  }

  if (tokens.size() == 3U && tokens[0] == "erase" && tokens[1] == "records") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    if (!store_->erase_records(true)) {
      return {.ok = false,
              .code = "export_required",
              .message = "export before erase"};
    }
    return {.ok = true, .code = "ok", .message = "records_erased"};
  }

  if (tokens.size() == 3U && tokens[0] == "erase" && tokens[1] == "factory") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    if (!physical_button_confirmed) {
      return {.ok = false,
              .code = "physical_presence_required",
              .message = "hold setup/mark button during factory reset"};
    }
    store_->factory_erase();
    return {.ok = true, .code = "ok", .message = "factory_reset"};
  }

  if (tokens.size() == 3U && tokens[0] == "recover" && tokens[1] == "store") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    const auto removed = store_->recover();
    return {.ok = true,
            .code = "ok",
            .message = "recovered_removed=" + std::to_string(removed)};
  }

  if (tokens.size() == 3U && tokens[0] == "reboot" && tokens[1] == "recovery") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    return {.ok = true, .code = "ok", .message = "reboot_recovery"};
  }

  if (tokens.size() >= 4U && tokens[0] == "time" && tokens[1] == "set") {
    if (!setup_->authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    if (!is_iso8601_utc(tokens[3])) {
      return {.ok = false,
              .code = "bad_command",
              .message = "time must be ISO-8601 UTC"};
    }
    return {.ok = true, .code = "ok", .message = "time_set"};
  }

  return {.ok = false, .code = "bad_command", .message = "unknown command"};
}

} // namespace quiet_trace
