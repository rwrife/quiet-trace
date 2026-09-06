#pragma once

#include "quiet_trace/acquisition_pipeline.hpp"
#include "quiet_trace/aggregate_contract.hpp"
#include "quiet_trace/record_ring.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace quiet_trace {

enum class ButtonAction {
  none,
  mark_annotation,
  open_setup,
};

[[nodiscard]] ButtonAction
classify_button_press(std::uint32_t duration_ms) noexcept;

enum class LedState {
  idle,
  active,
  setup_window,
  quality_warning,
  recovery,
  error,
};

struct LedPattern {
  std::uint16_t on_ms;
  std::uint16_t off_ms;
  std::uint8_t pulses;
  bool repeating;
  std::string_view accessibility_label;
};

[[nodiscard]] LedPattern led_pattern_for(LedState state) noexcept;

class SetupAuthorizer {
public:
  explicit SetupAuthorizer(std::uint64_t ttl_ms = 300'000) noexcept;

  [[nodiscard]] std::string open(bool physical_button_confirmed,
                                 std::uint64_t now_ms,
                                 std::uint64_t nonce) noexcept;
  [[nodiscard]] bool authorize(std::string_view token,
                               std::uint64_t now_ms) const noexcept;
  [[nodiscard]] bool active(std::uint64_t now_ms) const noexcept;
  void close() noexcept;

private:
  std::uint64_t ttl_ms_;
  std::string token_;
  std::uint64_t expires_at_ms_;
};

struct ApiRequestBoundary {
  bool authenticated;
  std::span<const std::string_view> field_names;
};

enum class ApiRejectReason {
  none,
  unauthenticated,
  forbidden_field,
};

[[nodiscard]] ApiRejectReason
validate_api_boundary(const ApiRequestBoundary &request) noexcept;

[[nodiscard]] std::string
render_status_json(const AggregationCounters &counters,
                   std::size_t stored_records, std::uint64_t generation,
                   bool setup_active, CalibrationState calibration_state,
                   ClockQuality clock_quality, bool fixture_mode);
[[nodiscard]] std::string render_aggregate_json(const StoredAggregate &record);
[[nodiscard]] std::string render_sse_event(std::string_view event,
                                           std::string_view payload_json);

struct UsbResponse {
  bool ok;
  std::string code;
  std::string message;
};

class UsbCommandRouter {
public:
  explicit UsbCommandRouter(SetupAuthorizer *setup,
                            AggregateRingStore *store) noexcept;
  [[nodiscard]] UsbResponse handle(std::string_view line, std::uint64_t now_ms,
                                   bool physical_button_confirmed,
                                   std::uint64_t nonce) noexcept;

private:
  SetupAuthorizer *setup_;
  AggregateRingStore *store_;
};

} // namespace quiet_trace
