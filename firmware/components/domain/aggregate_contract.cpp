#include "quiet_trace/aggregate_contract.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string>

namespace quiet_trace {
namespace {

constexpr std::array<std::string_view, 24> kForbiddenFields{
    "audio",        "authtoken",     "credential", "credentials",
    "encodedaudio", "encodedmedia",  "eventlabel", "fftbins",
    "pcm",          "rawsample",     "rawsamples", "recording",
    "samples",      "secret",        "speakerid",  "sourceid",
    "sourcelabel",  "spectra",       "spectrum",   "speech",
    "transcript",   "voiceactivity", "waveform",   "wifipassword",
};

std::string normalize_field_name(std::string_view field_name) {
  std::string normalized;
  normalized.reserve(field_name.size());
  for (const unsigned char character : field_name) {
    if (std::isalnum(character) != 0) {
      normalized.push_back(static_cast<char>(
          std::tolower(static_cast<unsigned char>(character))));
    }
  }
  return normalized;
}

bool is_utc_timestamp(std::string_view value) noexcept {
  if (value.size() < 20 || value.size() > 30 || value.back() != 'Z' ||
      value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
      value[13] != ':' || value[16] != ':') {
    return false;
  }
  if ((value.size() == 20 && value[19] != 'Z') ||
      (value.size() > 20 &&
       (value[19] != '.' || value.size() < 22 || value.size() > 30))) {
    return false;
  }

  const auto is_digit_at = [value](std::size_t index) {
    return std::isdigit(static_cast<unsigned char>(value[index])) != 0;
  };
  for (std::size_t index = 0; index < 19; ++index) {
    if (index == 4 || index == 7 || index == 10 || index == 13 || index == 16) {
      continue;
    }
    if (!is_digit_at(index)) {
      return false;
    }
  }
  for (std::size_t index = 20; index + 1 < value.size(); ++index) {
    if (!is_digit_at(index)) {
      return false;
    }
  }

  const auto parse_two = [value](std::size_t index) {
    return static_cast<unsigned>(value[index] - '0') * 10U +
           static_cast<unsigned>(value[index + 1] - '0');
  };
  unsigned year = 0;
  for (std::size_t index = 0; index < 4; ++index) {
    year = year * 10U + static_cast<unsigned>(value[index] - '0');
  }
  const unsigned month = parse_two(5);
  const unsigned day = parse_two(8);
  const unsigned hour = parse_two(11);
  const unsigned minute = parse_two(14);
  const unsigned second = parse_two(17);
  const bool leap_year =
      year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U);
  constexpr std::array<unsigned, 12> kDaysInMonth{31, 28, 31, 30, 31, 30,
                                                  31, 31, 30, 31, 30, 31};
  if (month < 1U || month > 12U) {
    return false;
  }
  unsigned max_day = kDaysInMonth[month - 1U];
  if (month == 2U && leap_year) {
    max_day = 29U;
  }
  return year >= 1U && day >= 1U && day <= max_day && hour <= 23U &&
         minute <= 59U && second <= 59U;
}

bool is_quality_flag(std::string_view value) noexcept {
  if (value.empty() || value.size() > 48 ||
      std::islower(static_cast<unsigned char>(value.front())) == 0) {
    return false;
  }
  return std::all_of(value.begin(), value.end(),
                     [](const unsigned char character) {
                       return std::islower(character) != 0 ||
                              std::isdigit(character) != 0 || character == '_';
                     });
}

std::optional<std::size_t>
unicode_scalar_count(std::string_view value) noexcept {
  std::size_t count = 0;
  for (std::size_t index = 0; index < value.size();) {
    const auto lead = static_cast<unsigned char>(value[index]);
    std::size_t width = 0;
    if (lead <= 0x7FU) {
      width = 1;
    } else if (lead >= 0xC2U && lead <= 0xDFU) {
      width = 2;
    } else if (lead >= 0xE0U && lead <= 0xEFU) {
      width = 3;
    } else if (lead >= 0xF0U && lead <= 0xF4U) {
      width = 4;
    } else {
      return std::nullopt;
    }
    if (index + width > value.size()) {
      return std::nullopt;
    }
    for (std::size_t offset = 1; offset < width; ++offset) {
      const auto continuation =
          static_cast<unsigned char>(value[index + offset]);
      if ((continuation & 0xC0U) != 0x80U) {
        return std::nullopt;
      }
    }
    if (width == 3) {
      const auto second = static_cast<unsigned char>(value[index + 1]);
      if ((lead == 0xE0U && second < 0xA0U) ||
          (lead == 0xEDU && second >= 0xA0U)) {
        return std::nullopt;
      }
    } else if (width == 4) {
      const auto second = static_cast<unsigned char>(value[index + 1]);
      if ((lead == 0xF0U && second < 0x90U) ||
          (lead == 0xF4U && second >= 0x90U)) {
        return std::nullopt;
      }
    }
    index += width;
    ++count;
  }
  return count;
}

bool is_bounded_text(std::string_view value, std::size_t minimum,
                     std::size_t maximum) noexcept {
  const auto count = unicode_scalar_count(value);
  return count.has_value() && *count >= minimum && *count <= maximum;
}

bool is_valid_clock_quality(ClockQuality quality) noexcept {
  switch (quality) {
  case ClockQuality::unknown:
  case ClockQuality::monotonic_only:
  case ClockQuality::host_set:
  case ClockQuality::synced:
    return true;
  }
  return false;
}

bool is_valid_calibration(const CalibrationMetadataV1 &calibration) noexcept {
  if (calibration.state == CalibrationState::reference_adjusted) {
    return calibration.has_offset && std::isfinite(calibration.offset_db) &&
           calibration.offset_db >= -80.0 && calibration.offset_db <= 80.0 &&
           is_utc_timestamp(calibration.calibrated_at) &&
           is_bounded_text(calibration.method, 1, 240) &&
           is_bounded_text(calibration.reference_instrument, 1, 160) &&
           is_bounded_text(calibration.reference_placement, 1, 240) &&
           is_bounded_text(calibration.reference_source, 1, 240) &&
           calibration.has_reference_duration &&
           calibration.reference_duration_s >= 1 &&
           calibration.reference_duration_s <= 86'400 &&
           is_bounded_text(calibration.firmware_version, 1, 64) &&
           is_bounded_text(calibration.hardware_revision, 1, 64);
  }

  return !calibration.has_offset && calibration.calibrated_at.empty() &&
         calibration.method.empty() &&
         calibration.reference_instrument.empty() &&
         calibration.reference_placement.empty() &&
         calibration.reference_source.empty() &&
         !calibration.has_reference_duration &&
         calibration.reference_duration_s == 0 &&
         calibration.firmware_version.empty() &&
         calibration.hardware_revision.empty() &&
         (calibration.state == CalibrationState::uncalibrated ||
          calibration.state == CalibrationState::invalid);
}

} // namespace

bool is_valid(const AggregateRecordV1 &record) noexcept {
  if (record.sequence > kMaxJsonSafeInteger ||
      record.monotonic_start_ms > kMaxJsonSafeInteger ||
      (record.has_interval_start && !is_utc_timestamp(record.interval_start)) ||
      (!record.has_interval_start && !record.interval_start.empty()) ||
      record.duration_ms != kAggregateDurationMs ||
      !std::isfinite(record.level_eq_dbfs) ||
      !std::isfinite(record.peak_125ms_dbfs) || record.level_eq_dbfs < -200.0 ||
      record.level_eq_dbfs > 0.0 || record.peak_125ms_dbfs < -200.0 ||
      record.peak_125ms_dbfs > 0.0 ||
      record.peak_125ms_dbfs < record.level_eq_dbfs ||
      !is_valid_calibration(record.calibration) ||
      !is_valid_clock_quality(record.clock_quality) ||
      record.quality_flag_count > record.quality_flags.size() ||
      (record.has_session_id && !is_bounded_text(record.session_id, 1, 64)) ||
      (!record.has_session_id && !record.session_id.empty())) {
    return false;
  }

  const bool requires_wall_time =
      record.clock_quality == ClockQuality::host_set ||
      record.clock_quality == ClockQuality::synced;
  if (record.has_interval_start != requires_wall_time) {
    return false;
  }

  if (std::any_of(record.histogram_counts.begin(),
                  record.histogram_counts.end(),
                  [](const std::int32_t count) { return count < 0; }) ||
      std::accumulate(record.histogram_counts.begin(),
                      record.histogram_counts.end(), std::int64_t{0}) !=
          static_cast<std::int64_t>(kExpectedHistogramWindows)) {
    return false;
  }

  for (std::size_t index = 0; index < record.quality_flag_count; ++index) {
    if (!is_quality_flag(record.quality_flags[index])) {
      return false;
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (record.quality_flags[index] == record.quality_flags[prior]) {
        return false;
      }
    }
  }

  return true;
}

bool is_forbidden_persistent_field(std::string_view field_name) noexcept {
  const auto normalized = normalize_field_name(field_name);
  return std::find(kForbiddenFields.begin(), kForbiddenFields.end(),
                   normalized) != kForbiddenFields.end();
}

} // namespace quiet_trace
