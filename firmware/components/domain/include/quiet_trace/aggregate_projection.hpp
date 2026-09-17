#pragma once

#include "quiet_trace/acquisition_pipeline.hpp"
#include "quiet_trace/aggregate_contract.hpp"
#include "quiet_trace/record_ring.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quiet_trace {

// Frozen v1 stored-bit vocabulary for quality flags (metrics.md order).
// `reset_recovery` and `wall_time_unknown` are runtime/render-time flags and
// are intentionally not stored bits: wall-time state is derived from the
// active clock quality when a record is rendered.
inline constexpr std::uint16_t kQualityBitUncalibrated = 1U << 0U;
inline constexpr std::uint16_t kQualityBitCalibrationInvalid = 1U << 1U;
inline constexpr std::uint16_t kQualityBitClipped = 1U << 2U;
inline constexpr std::uint16_t kQualityBitAcquisitionOverrun = 1U << 3U;
inline constexpr std::uint16_t kQualityBitDroppedInterval = 1U << 4U;
inline constexpr std::uint16_t kQualityBitInsufficientSamples = 1U << 5U;
inline constexpr std::uint16_t kQualityBitClockAdjusted = 1U << 6U;
inline constexpr std::uint16_t kQualityBitWeightingUnvalidated = 1U << 7U;

inline constexpr std::string_view kWallTimeUnknownFlag = "wall_time_unknown";

[[nodiscard]] std::uint16_t
quality_bits_for(const MinuteAggregate &aggregate) noexcept;

// Stored-bit names only; use render_aggregate_record_json for the final
// combined, schema-bounded flag list.
[[nodiscard]] std::vector<std::string_view>
quality_flag_names(std::uint16_t bits);

// Project a completed minute into the storable record shape. Minutes without
// valid samples must not be stored (metrics.md: missing intervals are status
// events, never fabricated values). Minutes flagged `insufficient_samples`
// are also not storable in v1: the frozen aggregate/v1 histogram contract
// requires 480 completed 125 ms windows, and metrics.md defers partial
// retention to a future compatible schema revision.
[[nodiscard]] bool
is_storable_minute(const MinuteAggregate &aggregate) noexcept;

[[nodiscard]] StoredAggregate
stored_aggregate_from(const MinuteAggregate &aggregate, std::uint64_t sequence,
                      std::uint64_t monotonic_start_ms) noexcept;

[[nodiscard]] std::string iso8601_utc_from_epoch_ms(std::int64_t epoch_ms);

// Full aggregate/v1 renderer matching the app's parseAggregateRecord
// contract (closed allowlist, calibration object, clock/interval coupling).
// MVP note: stored records never carry calibration provenance, so the state
// is `uncalibrated` or `invalid` only; reference-adjusted provenance is a
// follow-up after the reference-meter comparison exists.
[[nodiscard]] std::string
render_aggregate_record_json(const StoredAggregate &record,
                             ClockQuality clock_quality,
                             std::optional<std::int64_t> epoch_offset_ms);

} // namespace quiet_trace
