#include "quiet_trace/aggregate_projection.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>

namespace quiet_trace {
namespace {

constexpr std::int32_t mdbfs(const double dbfs_value) noexcept {
  if (!std::isfinite(dbfs_value)) {
    return -200'000;
  }
  const double scaled = dbfs_value * 1000.0;
  const double clamped = std::clamp(scaled, -200'000.0, 0.0);
  return static_cast<std::int32_t>(std::lround(clamped));
}

// Howard Hinnant's civil_from_days algorithm: seconds since Unix epoch to
// broken-down UTC components without pulling in a libc locale/time zone.
struct CivilTime {
  int year;
  unsigned month;
  unsigned day;
  unsigned hour;
  unsigned minute;
  unsigned second;
};

CivilTime civil_from_epoch_seconds(const std::int64_t epoch_seconds) {
  std::int64_t days = epoch_seconds / 86'400;
  std::int64_t rem = epoch_seconds % 86'400;
  if (rem < 0) {
    rem += 86'400;
    --days;
  }

  // Howard Hinnant's civil_from_days (days are counted from 0000-03-01).
  days += 719'468;
  const std::int64_t era = (days >= 0 ? days : days - 146'096) / 146'097;
  const auto doe = static_cast<std::uint64_t>(days - era * 146'097);
  const auto yoe = static_cast<std::uint64_t>(
      (doe - doe / 1'460 + doe / 36'524 - doe / 146'096) / 365);
  const auto year_of_era = static_cast<std::int64_t>(yoe);
  const auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const auto mp = (5 * doy + 2) / 153;
  const auto day = static_cast<unsigned>(doy - (153 * mp + 2) / 5 + 1);
  const auto month = static_cast<unsigned>(mp + (mp < 10 ? 3 : -9));
  const auto year =
      static_cast<int>(year_of_era + era * 400 + (month <= 2 ? 1 : 0));

  return CivilTime{
      .year = year,
      .month = month,
      .day = day,
      .hour = static_cast<unsigned>(rem / 3'600),
      .minute = static_cast<unsigned>((rem % 3'600) / 60),
      .second = static_cast<unsigned>(rem % 60),
  };
}

std::string clock_quality_token(const ClockQuality quality) {
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

bool has_wall_time(const ClockQuality quality) noexcept {
  return quality == ClockQuality::host_set || quality == ClockQuality::synced;
}

} // namespace

std::uint16_t quality_bits_for(const MinuteAggregate &aggregate) noexcept {
  std::uint16_t bits = 0;
  if (aggregate.calibration_state == CalibrationState::uncalibrated) {
    bits |= kQualityBitUncalibrated;
  }
  if (aggregate.calibration_state == CalibrationState::invalid) {
    bits |= kQualityBitCalibrationInvalid;
  }
  if (aggregate.clipped) {
    bits |= kQualityBitClipped;
  }
  if (aggregate.acquisition_overrun) {
    bits |= kQualityBitAcquisitionOverrun;
  }
  if (aggregate.dropped_interval) {
    bits |= kQualityBitDroppedInterval;
  }
  if (aggregate.insufficient_samples) {
    bits |= kQualityBitInsufficientSamples;
  }
  if (aggregate.clock_adjusted) {
    bits |= kQualityBitClockAdjusted;
  }
  if (aggregate.weighting_unvalidated) {
    bits |= kQualityBitWeightingUnvalidated;
  }
  return bits;
}

std::vector<std::string_view> quality_flag_names(const std::uint16_t bits) {
  std::vector<std::string_view> names;
  if ((bits & kQualityBitUncalibrated) != 0U) {
    names.emplace_back("uncalibrated");
  }
  if ((bits & kQualityBitCalibrationInvalid) != 0U) {
    names.emplace_back("calibration_invalid");
  }
  if ((bits & kQualityBitClipped) != 0U) {
    names.emplace_back("clipped");
  }
  if ((bits & kQualityBitAcquisitionOverrun) != 0U) {
    names.emplace_back("acquisition_overrun");
  }
  if ((bits & kQualityBitDroppedInterval) != 0U) {
    names.emplace_back("dropped_interval");
  }
  if ((bits & kQualityBitInsufficientSamples) != 0U) {
    names.emplace_back("insufficient_samples");
  }
  if ((bits & kQualityBitClockAdjusted) != 0U) {
    names.emplace_back("clock_adjusted");
  }
  if ((bits & kQualityBitWeightingUnvalidated) != 0U) {
    names.emplace_back("weighting_unvalidated");
  }
  return names;
}

bool is_storable_minute(const MinuteAggregate &aggregate) noexcept {
  return aggregate.has_valid_samples && !aggregate.insufficient_samples &&
         std::accumulate(aggregate.histogram_counts.begin(),
                         aggregate.histogram_counts.end(), 0LL) ==
             static_cast<std::int64_t>(kExpectedHistogramWindows);
}

StoredAggregate
stored_aggregate_from(const MinuteAggregate &aggregate,
                      const std::uint64_t sequence,
                      const std::uint64_t monotonic_start_ms) noexcept {
  StoredAggregate record{
      .version = kStoredAggregateVersion,
      .sequence = sequence,
      .monotonic_start_ms = monotonic_start_ms,
      .level_eq_mdbfs = mdbfs(aggregate.level_eq_adjusted_dbfs),
      .peak_125ms_mdbfs = mdbfs(aggregate.peak_125ms_adjusted_dbfs),
      .histogram_counts = {},
      .quality_flags_bits = quality_bits_for(aggregate),
      .checksum = 0,
  };

  // Guarantee the semantic peak>=level invariant even after clamping/rounding.
  record.peak_125ms_mdbfs =
      std::max(record.peak_125ms_mdbfs, record.level_eq_mdbfs);

  for (std::size_t index = 0; index < record.histogram_counts.size(); ++index) {
    const auto count = aggregate.histogram_counts[index];
    record.histogram_counts[index] = static_cast<std::uint16_t>(
        count < 0 ? 0 : std::min<std::int32_t>(count, 65'535));
  }
  return record;
}

std::string iso8601_utc_from_epoch_ms(const std::int64_t epoch_ms) {
  auto seconds = epoch_ms / 1'000;
  if (epoch_ms < 0 && epoch_ms % 1'000 != 0) {
    --seconds;
  }
  const auto civil = civil_from_epoch_seconds(seconds);

  std::ostringstream stream;
  stream << std::setfill('0') << std::setw(4) << civil.year << '-'
         << std::setw(2) << civil.month << '-' << std::setw(2) << civil.day
         << 'T' << std::setw(2) << civil.hour << ':' << std::setw(2)
         << civil.minute << ':' << std::setw(2) << civil.second << 'Z';
  return stream.str();
}

std::string render_aggregate_record_json(
    const StoredAggregate &record, const ClockQuality clock_quality,
    const std::optional<std::int64_t> epoch_offset_ms) {
  const auto flags = quality_flag_names(record.quality_flags_bits);
  std::string calibration_state = "uncalibrated";
  if ((record.quality_flags_bits & kQualityBitCalibrationInvalid) != 0U) {
    calibration_state = "invalid";
  }

  const bool wall_time = has_wall_time(clock_quality);
  std::string interval_start = "null";
  if (wall_time && epoch_offset_ms.has_value()) {
    // Floor toward negative infinity so pre-1970 offsets stay correct.
    auto epoch_ms =
        *epoch_offset_ms + static_cast<std::int64_t>(record.monotonic_start_ms);
    auto epoch_seconds = epoch_ms / 1000LL;
    if (epoch_ms < 0 && epoch_ms % 1000LL != 0) {
      --epoch_seconds;
    }
    if (epoch_seconds <=
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) *
            4'000LL) {
      interval_start =
          "\"" + iso8601_utc_from_epoch_ms(epoch_seconds * 1000LL) + "\"";
    }
  }

  std::ostringstream stream;
  stream << '{' << "\"schema\":\"quiet-trace/aggregate/v1\","
         << "\"sequence\":" << record.sequence << ','
         << "\"interval_start\":" << interval_start << ','
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

  stream << "],\"calibration\":{\"state\":\"" << calibration_state
         << "\",\"offset_db\":null,\"calibrated_at\":null,\"method\":null,"
            "\"reference_instrument\":null,\"reference_placement\":null,"
            "\"reference_source\":null,\"reference_duration_s\":null,"
            "\"firmware_version\":null,\"hardware_revision\":null},"
         << "\"clock_quality\":\"" << clock_quality_token(clock_quality)
         << "\",\"quality_flags\":[";

  bool first = true;
  for (const auto flag : flags) {
    if (!first) {
      stream << ',';
    }
    stream << '"' << flag << '"';
    first = false;
  }
  if (!wall_time) {
    if (!first) {
      stream << ',';
    }
    stream << '"' << kWallTimeUnknownFlag << '"';
  }

  stream << "],\"session_id\":null}";
  return stream.str();
}

} // namespace quiet_trace
