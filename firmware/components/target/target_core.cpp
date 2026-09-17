#include "quiet_trace/target.hpp"

#include "quiet_trace/aggregate_projection.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace quiet_trace::target {
namespace {

bool all_digits(const std::string_view text, const std::size_t begin,
                const std::size_t end) noexcept {
  for (std::size_t index = begin; index < end; ++index) {
    if (std::isdigit(static_cast<unsigned char>(text[index])) == 0) {
      return false;
    }
  }
  return true;
}

constexpr int days_in_month(const int year, const int month) noexcept {
  constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0)) {
    return 29;
  }
  return kDays[month - 1];
}

std::int64_t days_from_civil(const int year, const int month,
                             const int day) noexcept {
  // Inverse of Hinnant's civil_from_days.
  const int adjusted_year = year - (month <= 2 ? 1 : 0);
  const int era =
      (adjusted_year >= 0 ? adjusted_year : adjusted_year - 399) / 400;
  const int year_of_era = adjusted_year - era * 400;
  const int day_of_year =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int day_of_era =
      year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return static_cast<std::int64_t>(day_of_era) + era * 146'097 - 719'468;
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

void append_csv_row(std::ostringstream &stream, const StoredAggregate &record,
                    const ClockQuality clock_quality,
                    const std::optional<std::int64_t> epoch_offset_ms) {
  // Columns match the dashboard export (app/src/api/device-api.ts toCsv).
  std::string interval_start;
  std::string interval_end;
  const bool wall_time = clock_quality == ClockQuality::host_set ||
                         clock_quality == ClockQuality::synced;
  if (wall_time && epoch_offset_ms.has_value()) {
    const auto epoch_ms =
        *epoch_offset_ms + static_cast<std::int64_t>(record.monotonic_start_ms);
    interval_start = iso8601_utc_from_epoch_ms(epoch_ms);
    interval_end = iso8601_utc_from_epoch_ms(epoch_ms + 60'000LL);
  }

  std::string flags;
  for (const auto flag : quality_flag_names(record.quality_flags_bits)) {
    if (!flags.empty()) {
      flags.push_back('|');
    }
    flags.append(flag);
  }
  if (!wall_time) {
    if (!flags.empty()) {
      flags.push_back('|');
    }
    flags.append(kWallTimeUnknownFlag);
  }

  const std::string_view calibration_state =
      (record.quality_flags_bits & kQualityBitCalibrationInvalid) != 0U
          ? "invalid"
          : "uncalibrated";

  std::ostringstream level;
  level << std::fixed << std::setprecision(3)
        << static_cast<double>(record.level_eq_mdbfs) / 1000.0;
  std::ostringstream peak;
  peak << std::fixed << std::setprecision(3)
       << static_cast<double>(record.peak_125ms_mdbfs) / 1000.0;

  const auto quote = [](const std::string_view value) {
    std::string quoted = "\"";
    for (const char character : value) {
      if (character == '"') {
        quoted.push_back('"');
      }
      quoted.push_back(character);
    }
    quoted.push_back('"');
    return quoted;
  };

  stream << quote(std::to_string(record.sequence)) << ','
         << quote(interval_start) << ',' << quote(interval_end) << ','
         << quote(level.str()) << ',' << quote(peak.str()) << ','
         << quote(flags) << ',' << quote(calibration_state) << ',' << quote("")
         << ',' << quote(clock_quality_token(clock_quality)) << '\n';
}

} // namespace

ClockQuality
classify_clock_quality(const bool time_synced, const bool host_set_time,
                       const bool sync_failed_since_host_set) noexcept {
  if (time_synced) {
    return ClockQuality::synced;
  }
  if (host_set_time && !sync_failed_since_host_set) {
    // A host-set clock may later be corrected by SNTP; treat it as trusted
    // wall time until a sync failure proves otherwise.
    return ClockQuality::host_set;
  }
  return ClockQuality::monotonic_only;
}

std::int64_t uptime_us_to_ms(const std::int64_t uptime_us) noexcept {
  return uptime_us / 1000LL;
}

std::optional<std::int64_t>
parse_iso8601_utc_ms(const std::string_view timestamp) noexcept {
  if (timestamp.size() != 20U || timestamp[4] != '-' || timestamp[7] != '-' ||
      timestamp[10] != 'T' || timestamp[13] != ':' || timestamp[16] != ':' ||
      timestamp.back() != 'Z') {
    return std::nullopt;
  }
  if (!all_digits(timestamp, 0, 4) || !all_digits(timestamp, 5, 7) ||
      !all_digits(timestamp, 8, 10) || !all_digits(timestamp, 11, 13) ||
      !all_digits(timestamp, 14, 16) || !all_digits(timestamp, 17, 19)) {
    return std::nullopt;
  }

  const auto to_int = [timestamp](const std::size_t begin,
                                  const std::size_t end) {
    int value = 0;
    for (std::size_t index = begin; index < end; ++index) {
      value = value * 10 + (timestamp[index] - '0');
    }
    return value;
  };

  const int year = to_int(0, 4);
  const int month = to_int(5, 7);
  const int day = to_int(8, 10);
  const int hour = to_int(11, 13);
  const int minute = to_int(14, 16);
  const int second = to_int(17, 19);

  if (year < 1970 || month < 1 || month > 12 || day < 1 ||
      day > days_in_month(year, month) || hour > 23 || minute > 59 ||
      second > 60) {
    return std::nullopt;
  }

  const auto days = days_from_civil(year, month, day);
  const std::int64_t seconds =
      days * 86'400LL + hour * 3'600LL + minute * 60LL + second;
  return seconds * 1'000LL;
}

SystemState::SystemState() = default;

AcquisitionPipeline &SystemState::pipeline() noexcept { return pipeline_; }

AggregateRingStore &SystemState::store() noexcept { return store_; }

SetupAuthorizer &SystemState::setup() noexcept { return setup_; }

UsbCommandRouter &SystemState::router() noexcept { return router_; }

void SystemState::set_button_held(const bool held,
                                  const std::int64_t now_us) noexcept {
  if (held != button_held_) {
    button_held_ = held;
    button_edge_us_ = now_us;
  }
}

bool SystemState::button_held() const noexcept { return button_held_; }

std::uint32_t
SystemState::button_hold_us(const std::int64_t now_us) const noexcept {
  if (!button_held_) {
    return 0U;
  }
  const auto elapsed = now_us - button_edge_us_;
  return elapsed > 0 ? static_cast<std::uint32_t>(elapsed / 1000ULL) : 0U;
}

bool SystemState::physical_presence_recent(
    const std::int64_t now_us, const std::int64_t window_us) const noexcept {
  if (button_held_) {
    return true;
  }
  const auto idle_us = now_us - button_edge_us_;
  return idle_us >= 0 && idle_us <= window_us;
}

void SystemState::set_reboot_requested() noexcept { reboot_requested_ = true; }

bool SystemState::reboot_requested() const noexcept {
  return reboot_requested_;
}

void SystemState::record_wifi_credentials(std::string ssid,
                                          std::string password) {
  pending_ssid_ = std::move(ssid);
  pending_password_ = std::move(password);
  wifi_pending_ = true;
}

bool SystemState::take_wifi_credentials(std::string &ssid,
                                        std::string &password) noexcept {
  if (!wifi_pending_) {
    return false;
  }
  wifi_pending_ = false;
  ssid = std::move(pending_ssid_);
  password = std::move(pending_password_);
  pending_ssid_.clear();
  pending_password_.clear();
  return true;
}

void SystemState::set_host_time(const std::int64_t epoch_ms,
                                const std::int64_t uptime_us) noexcept {
  host_epoch_ms_ = epoch_ms;
  host_uptime_us_ = uptime_us;
  host_set_time_ = true;
  sync_failed_since_host_set_ = false;
}

void SystemState::mark_time_synced(const std::int64_t epoch_ms,
                                   const std::int64_t uptime_us) noexcept {
  synced_epoch_ms_ = epoch_ms;
  synced_uptime_us_ = uptime_us;
  time_synced_ = true;
  sync_failed_since_host_set_ = false;
}

void SystemState::mark_time_sync_failed() noexcept {
  if (host_set_time_ && !time_synced_) {
    sync_failed_since_host_set_ = true;
  }
}

ClockSnapshot
SystemState::clock_snapshot(const std::int64_t uptime_us) noexcept {
  ClockSnapshot snapshot{};
  snapshot.quality = classify_clock_quality(time_synced_, host_set_time_,
                                            sync_failed_since_host_set_);
  switch (snapshot.quality) {
  case ClockQuality::synced:
    snapshot.epoch_offset_ms = synced_epoch_ms_ - synced_uptime_us_ / 1000LL;
    break;
  case ClockQuality::host_set:
    snapshot.epoch_offset_ms = host_epoch_ms_ - host_uptime_us_ / 1000LL;
    break;
  case ClockQuality::unknown:
  case ClockQuality::monotonic_only:
    snapshot.epoch_offset_ms = std::nullopt;
    break;
  }
  static_cast<void>(uptime_us);
  return snapshot;
}

std::string
render_usb_export_json(const std::vector<StoredAggregate> &records,
                       const ClockQuality clock_quality,
                       const std::optional<std::int64_t> epoch_offset_ms,
                       const std::int64_t uptime_us) {
  std::string exported_at = "null";
  const bool wall_time = clock_quality == ClockQuality::host_set ||
                         clock_quality == ClockQuality::synced;
  if (wall_time && epoch_offset_ms.has_value()) {
    exported_at =
        "\"" +
        iso8601_utc_from_epoch_ms(*epoch_offset_ms + uptime_us / 1000LL) + "\"";
  }
  std::ostringstream stream;
  stream << "{\"schema\":\"quiet-trace/export/v1\",\"exported_at\":"
         << exported_at << ",\"source\":\"device\",\"records\":[";
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0U) {
      stream << ',';
    }
    stream << render_aggregate_record_json(records[index], clock_quality,
                                           epoch_offset_ms);
  }
  // v1 has no stored sessions/annotations; the keys exist for the frozen
  // export/v1 backup envelope consumed by the dashboard restore flow.
  stream << "],\"sessions\":[],\"annotations\":[]}";
  return stream.str();
}

std::string
render_usb_export_csv(const std::vector<StoredAggregate> &records,
                      const ClockQuality clock_quality,
                      const std::optional<std::int64_t> epoch_offset_ms) {
  std::ostringstream stream;
  stream << "sequence,interval_start,interval_end,level_eq_dbfs,"
            "peak_125ms_dbfs,quality_flags,calibration_state,"
            "calibration_offset_db,clock_quality\n";
  for (const auto &record : records) {
    append_csv_row(stream, record, clock_quality, epoch_offset_ms);
  }
  return stream.str();
}

UsbResponse handle_console_line(SystemState &state, const std::string_view line,
                                const std::int64_t now_us,
                                const bool physical_button_confirmed,
                                const std::uint64_t nonce) {
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

  const auto now_ms = uptime_us_to_ms(now_us);

  // `wifi set <token> <ssid> <password...>`: validate against the setup
  // window and hand the credential to the volatile in-RAM holder instead of
  // only acknowledging it.
  if (tokens.size() >= 5U && tokens[0] == "wifi" && tokens[1] == "set") {
    if (!state.setup().authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    if (tokens[3].empty()) {
      return {.ok = false, .code = "bad_command", .message = "ssid required"};
    }
    std::string password;
    for (std::size_t position = 4; position < tokens.size(); ++position) {
      if (position > 4) {
        password.push_back(' ');
      }
      password.append(tokens[position]);
    }
    state.record_wifi_credentials(std::string(tokens[3]), std::move(password));
    return {.ok = true, .code = "ok", .message = "wifi_saved"};
  }

  // `time set <token> <timestamp>`: the shared router validates the token
  // and format; the value is applied here to the volatile clock state.
  if (tokens.size() == 4U && tokens[0] == "time" && tokens[1] == "set") {
    if (!state.setup().authorize(tokens[2], now_ms)) {
      return {.ok = false, .code = "unauthorized", .message = "invalid token"};
    }
    const auto epoch_ms = parse_iso8601_utc_ms(tokens[3]);
    if (!epoch_ms.has_value()) {
      return {.ok = false,
              .code = "bad_command",
              .message = "time must be YYYY-MM-DDTHH:MM:SSZ"};
    }
    state.set_host_time(*epoch_ms, now_us);
    return {.ok = true, .code = "ok", .message = "time_set"};
  }

  // `reboot recovery <token>`: the shared router authorizes; the reboot
  // latch lives in system state so the control task can drain the LED first.
  if (tokens.size() == 3U && tokens[0] == "reboot" && tokens[1] == "recovery") {
    auto response =
        state.router().handle(line, now_ms, physical_button_confirmed, nonce);
    if (response.ok) {
      state.set_reboot_requested();
    }
    return response;
  }

  return state.router().handle(line, now_ms, physical_button_confirmed, nonce);
}

} // namespace quiet_trace::target
