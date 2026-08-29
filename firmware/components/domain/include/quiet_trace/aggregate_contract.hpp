#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace quiet_trace {

inline constexpr std::uint32_t kAggregateDurationMs = 60'000;
inline constexpr std::size_t kHistogramBinCount = 10;
inline constexpr std::size_t kExpectedHistogramWindows = 480;
inline constexpr std::uint64_t kMaxJsonSafeInteger = 9'007'199'254'740'991ULL;
inline constexpr std::size_t kMaxQualityFlags = 8;

enum class CalibrationState {
  uncalibrated,
  reference_adjusted,
  invalid,
};

enum class ClockQuality {
  unknown,
  monotonic_only,
  host_set,
  synced,
};

struct CalibrationMetadataV1 {
  CalibrationState state;
  double offset_db;
  bool has_offset;
  std::string_view calibrated_at;
  std::string_view method;
  std::string_view reference_instrument;
  std::string_view reference_placement;
  std::string_view reference_source;
  std::uint32_t reference_duration_s;
  bool has_reference_duration;
  std::string_view firmware_version;
  std::string_view hardware_revision;
};

struct AggregateRecordV1 {
  std::uint64_t sequence;
  std::string_view interval_start;
  bool has_interval_start;
  std::uint64_t monotonic_start_ms;
  std::uint32_t duration_ms;
  double level_eq_dbfs;
  double peak_125ms_dbfs;
  std::array<std::int32_t, kHistogramBinCount> histogram_counts;
  CalibrationMetadataV1 calibration;
  ClockQuality clock_quality;
  std::array<std::string_view, kMaxQualityFlags> quality_flags;
  std::size_t quality_flag_count;
  std::string_view session_id;
  bool has_session_id;
};

[[nodiscard]] bool is_valid(const AggregateRecordV1 &record) noexcept;
[[nodiscard]] bool
is_forbidden_persistent_field(std::string_view field_name) noexcept;

} // namespace quiet_trace
