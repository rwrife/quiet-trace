#pragma once

#include "quiet_trace/acquisition_pipeline.hpp"
#include "quiet_trace/aggregate_contract.hpp"
#include "quiet_trace/control_plane.hpp"
#include "quiet_trace/record_ring.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quiet_trace::target {

// Hardware pin map for revision A (hardware/parts-selection.md pin audit and
// hardware/kicad/quiet-trace.kicad_sch MIC_* nets). This is a design-intent
// constant block, not verified-on-silicon behavior.
inline constexpr int kMicBclkGpio = 4;    // MIC_BCLK (module pin 4 / GPIO4)
inline constexpr int kMicWsGpio = 5;      // MIC_WS (module pin 5 / GPIO5)
inline constexpr int kMicSdGpio = 6;      // MIC_SD (module pin 6 / GPIO6)
inline constexpr int kStatusLedGpio = 48; // GPIO48 -> D1 through R6
inline constexpr int kSetupMarkGpio = 0;  // GPIO0 BOOT/SETUP_MARK (SW2)

// In-memory aggregate retention. The v1 slice keeps one boot session of
// minute records in bounded internal RAM; SPIFFS persistence and its
// power-loss behavior are deferred to physical bring-up (issue #6) because
// the on-flash format has no physical validation yet. 10,080 records is the
// 7-day capacity target from docs/metrics.md.
inline constexpr std::size_t kRingCapacityRecords = 10'080;

inline constexpr std::uint32_t kAcquisitionBlockSamples = 240U;

struct ClockSnapshot {
  ClockQuality quality;
  // Wall-clock epoch ms derived at snapshot time (null when no time source).
  std::optional<std::int64_t> epoch_offset_ms;
};

// Pure classification of clock quality (no hardware state) for host tests.
[[nodiscard]] ClockQuality
classify_clock_quality(bool time_synced, bool host_set_time,
                       bool sync_failed_since_host_set) noexcept;

[[nodiscard]] std::int64_t uptime_us_to_ms(std::int64_t uptime_us) noexcept;

// Parses the strict `YYYY-MM-DDTHH:MM:SSZ` form accepted by the USB
// `time set` command into epoch milliseconds; rejects impossible dates.
[[nodiscard]] std::optional<std::int64_t>
parse_iso8601_utc_ms(std::string_view timestamp) noexcept;

class SystemState {
public:
  SystemState();

  [[nodiscard]] AcquisitionPipeline &pipeline() noexcept;
  [[nodiscard]] AggregateRingStore &store() noexcept;
  [[nodiscard]] SetupAuthorizer &setup() noexcept;
  [[nodiscard]] UsbCommandRouter &router() noexcept;

  // Latches the button state with free-running microseconds at the edge.
  void set_button_held(bool held, std::int64_t now_us) noexcept;
  [[nodiscard]] bool button_held() const noexcept;
  [[nodiscard]] std::uint32_t
  button_hold_us(std::int64_t now_us) const noexcept;
  // True while the button is being held or was released within `window_us`.
  [[nodiscard]] bool
  physical_presence_recent(std::int64_t now_us,
                           std::int64_t window_us) const noexcept;

  void set_reboot_requested() noexcept;
  [[nodiscard]] bool reboot_requested() const noexcept;

  // Credentials held only in volatile RAM for this boot session (v1 slice);
  // `take_wifi_credentials` moves them out for the network task.
  void record_wifi_credentials(std::string ssid, std::string password);
  [[nodiscard]] bool take_wifi_credentials(std::string &ssid,
                                           std::string &password) noexcept;

  void set_host_time(std::int64_t epoch_ms, std::int64_t uptime_us) noexcept;
  void mark_time_synced(std::int64_t epoch_ms, std::int64_t uptime_us) noexcept;
  void mark_time_sync_failed() noexcept;
  [[nodiscard]] ClockSnapshot clock_snapshot(std::int64_t uptime_us) noexcept;

private:
  AcquisitionPipeline pipeline_;
  AggregateRingStore store_{kRingCapacityRecords};
  SetupAuthorizer setup_{};
  UsbCommandRouter router_{&setup_, &store_};

  bool button_held_ = false;
  std::int64_t button_edge_us_ = 0;

  bool reboot_requested_ = false;

  std::string pending_ssid_;
  std::string pending_password_;
  bool wifi_pending_ = false;

  bool time_synced_ = false;
  bool host_set_time_ = false;
  bool sync_failed_since_host_set_ = false;
  std::int64_t host_epoch_ms_ = 0;
  std::int64_t host_uptime_us_ = 0;
  std::int64_t synced_epoch_ms_ = 0;
  std::int64_t synced_uptime_us_ = 0;
};

// JSON/CSV export bodies for the USB `export` command and the HTTP export
// endpoint, rendered from the exported store contents. Aggregate-only.
[[nodiscard]] std::string render_usb_export_json(
    const std::vector<StoredAggregate> &records, ClockQuality clock_quality,
    std::optional<std::int64_t> epoch_offset_ms, std::int64_t uptime_us);
[[nodiscard]] std::string
render_usb_export_csv(const std::vector<StoredAggregate> &records,
                      ClockQuality clock_quality,
                      std::optional<std::int64_t> epoch_offset_ms);

// Console-line dispatch. Extends the shared UsbCommandRouter contract with
// `time set <token> <timestamp>` value application; everything else routes
// to the domain router. Kept free of IDF calls so host tests cover it.
[[nodiscard]] UsbResponse handle_console_line(SystemState &state,
                                              std::string_view line,
                                              std::int64_t now_us,
                                              bool physical_button_confirmed,
                                              std::uint64_t nonce);

// Single production entry point used by app_main(): initializes adapters and
// starts acquisition, aggregation, control, console, LED, and network tasks.
// Returns false when a required driver fails; the LED then runs the error
// pattern. Implemented in the ESP-IDF-only translation unit.
bool start_all();

SystemState &state();

} // namespace quiet_trace::target
