// ESP-IDF target adapters and task wiring for revision A (ESP32-S3-WROOM-1).
//
// PRIVACY CONTRACT for this file: raw microphone samples exist only inside
// the bounded `s_sample_block` buffer below and inside I2S DMA descriptors.
// AcquisitionPipeline::ingest_block zeroizes each block after aggregation.
// HTTP, USB console, and storage paths carry aggregate DTOs only.
//
// Evidence class: static/compiled target integration only. No on-hardware
// behavior is claimed until physical bring-up (issue #6) captures bench
// evidence. In particular, I2S framing alignment, button timing, LED drive,
// and HTTP security properties are compiled, not measured.

#include "quiet_trace/target.hpp"

#include "quiet_trace/aggregate_projection.hpp"
#include "quiet_trace/control_plane.hpp"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace quiet_trace::target {
namespace {

constexpr char kTag[] = "quiet_trace.target";

SystemState *g_state = nullptr;
std::atomic<std::uint64_t> g_sequence{0};
std::int64_t g_boot_uptime_us = 0;
httpd_handle_t g_http_server = nullptr;

SystemState &app_state() { return *g_state; }

std::int64_t now_us() { return esp_timer_get_time(); }

// ---------------------------------------------------------------------------
// Audio source: I2S RX, Philips standard mode. The ICS-43434 emits a 24-bit
// sample inside a 32-bit slot; samples are shifted into the signed 24-bit
// domain the pipeline expects. Framing is verified at physical bring-up.
// ---------------------------------------------------------------------------

constexpr std::uint32_t kSampleRateHz = 48'000U;
constexpr std::uint32_t kDmaDescCount = 8U;
constexpr std::uint32_t kDmaFramesPerDesc = 120U;

i2s_chan_handle_t g_i2s_rx_handle = nullptr;

bool init_i2s() {
  i2s_chan_config_t chan_config =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_config.dma_desc_num = kDmaDescCount;
  chan_config.dma_frame_num = kDmaFramesPerDesc;

  if (i2s_new_channel(&chan_config, nullptr, &g_i2s_rx_handle) != ESP_OK) {
    ESP_LOGE(kTag, "i2s_new_channel failed");
    return false;
  }

  i2s_std_config_t std_config{};
  std_config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz);
  std_config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  std_config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  std_config.gpio_cfg = {};
  std_config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  std_config.gpio_cfg.bclk = static_cast<gpio_num_t>(kMicBclkGpio);
  std_config.gpio_cfg.ws = static_cast<gpio_num_t>(kMicWsGpio);
  std_config.gpio_cfg.dout = I2S_GPIO_UNUSED;
  std_config.gpio_cfg.din = static_cast<gpio_num_t>(kMicSdGpio);

  if (i2s_channel_init_std_mode(g_i2s_rx_handle, &std_config) != ESP_OK) {
    ESP_LOGE(kTag, "i2s_channel_init_std_mode failed");
    return false;
  }
  if (i2s_channel_enable(g_i2s_rx_handle) != ESP_OK) {
    ESP_LOGE(kTag, "i2s_channel_enable failed");
    return false;
  }
  ESP_LOGI(kTag, "i2s rx enabled (bclk=%d ws=%d din=%d)", kMicBclkGpio,
           kMicWsGpio, kMicSdGpio);
  return true;
}

void acquisition_task(void *) {
  // The only raw-sample buffer in production firmware. Bounded and zeroized
  // by AcquisitionPipeline::ingest_block.
  static std::int32_t s_sample_block[kAcquisitionBlockSamples];
  auto &state = app_state();

  while (true) {
    std::size_t bytes_read = 0;
    const auto read_result =
        i2s_channel_read(g_i2s_rx_handle, s_sample_block,
                         sizeof(s_sample_block), &bytes_read, 1000);

    bool overrun = false;
    if (read_result == ESP_ERR_NOT_FOUND) {
      overrun = true; // timeout: a gap is reported, never fabricated
    } else if (read_result != ESP_OK) {
      ESP_LOGW(kTag, "i2s_channel_read error %d",
               static_cast<int>(read_result));
      overrun = true;
    }

    const auto samples_read = bytes_read / sizeof(std::int32_t);
    std::span<std::int32_t> block{s_sample_block, samples_read};
    for (auto &sample : block) {
      sample >>= 8; // 32-bit Philips slot -> signed 24-bit domain
    }
    const auto quality = state.clock_snapshot(now_us()).quality;
    const bool clock_uncertain = quality == ClockQuality::unknown ||
                                 quality == ClockQuality::monotonic_only;
    state.pipeline().ingest_block(block, overrun, /*dropped_interval=*/false,
                                  clock_uncertain);
  }
}

// ---------------------------------------------------------------------------
// Aggregation task: completed minute -> stored record
// ---------------------------------------------------------------------------

void aggregation_task(void *) {
  auto &state = app_state();
  auto &pipeline = state.pipeline();

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(250));
    if (!pipeline.minute_ready()) {
      continue;
    }
    const auto minute = pipeline.consume_minute();

    if (!is_storable_minute(minute)) {
      ESP_LOGW(kTag,
               "minute discarded: no valid samples or incomplete 125 ms "
               "windows (metrics.md: missing interval, not fabricated value)");
      continue;
    }

    // v1 uses the completion-time monotonic stamp minus one nominal minute.
    // Wall-clock interval_start is derived at render time from clock state.
    const auto block_ms = static_cast<std::uint64_t>(uptime_us_to_ms(now_us()) -
                                                     g_boot_uptime_us);
    const auto minute_start_ms =
        block_ms > kAggregateDurationMs ? block_ms - kAggregateDurationMs : 0;

    const auto sequence =
        g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto record =
        stored_aggregate_from(minute, sequence, minute_start_ms);
    state.store().append(record);
    ESP_LOGI(kTag, "stored aggregate sequence=%" PRIu64 " start_ms=%" PRIu64,
             sequence, minute_start_ms);
  }
}

// ---------------------------------------------------------------------------
// Control task: setup/mark button and status LED
// ---------------------------------------------------------------------------

void control_task(void *) {
  auto &state = app_state();

  gpio_config_t button_config{};
  button_config.pin_bit_mask = 1ULL << kSetupMarkGpio;
  button_config.mode = GPIO_MODE_INPUT;
  button_config.pull_up_en = GPIO_PULLUP_ENABLE;
  button_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  button_config.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&button_config);

  gpio_config_t led_config{};
  led_config.pin_bit_mask = 1ULL << kStatusLedGpio;
  led_config.mode = GPIO_MODE_OUTPUT;
  led_config.pull_up_en = GPIO_PULLUP_DISABLE;
  led_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  led_config.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&led_config);

  std::uint32_t held_ms = 0;
  bool last_held = false;
  std::int64_t led_elapsed_ms = 0;
  std::uint8_t pulses_remaining = 0;
  LedState led_state = LedState::idle;

  while (true) {
    constexpr std::int64_t kPollMs = 20;
    vTaskDelay(pdMS_TO_TICKS(kPollMs));
    const auto uptime = now_us();
    const auto now_ms = uptime_us_to_ms(uptime);

    const bool held = gpio_get_level(static_cast<gpio_num_t>(kSetupMarkGpio)) ==
                      0; // active low
    state.set_button_held(held, uptime);
    if (held) {
      held_ms = held_ms > 600'000U ? 600'000U : held_ms + kPollMs;
    } else if (last_held) {
      const auto action = classify_button_press(held_ms);
      if (action == ButtonAction::mark_annotation) {
        // v1: the marker surfaces in the console log; annotation storage
        // belongs to the deferred HTTP write surface.
        ESP_LOGI(kTag, "setup/mark button press accepted");
      } else if (action == ButtonAction::open_setup) {
        std::uint64_t nonce = 0;
        esp_fill_random(&nonce, sizeof(nonce));
        const auto token = state.setup().open(true, now_ms, nonce);
        static_cast<void>(token);
        ESP_LOGI(kTag,
                 "setup window opened (300 s TTL); tokens are intentionally "
                 "never logged (use them over the USB console)");
      }
      held_ms = 0;
    }
    last_held = held;

    const auto next_led = [&]() {
      if (state.reboot_requested()) {
        return LedState::recovery;
      }
      if (state.setup().active(now_ms)) {
        return LedState::setup_window;
      }
      return state.store().size() == 0 ? LedState::active : LedState::idle;
    }();
    if (next_led != led_state) {
      led_state = next_led;
      led_elapsed_ms = 0;
      pulses_remaining = 0;
    }

    const auto pattern = led_pattern_for(led_state);
    const auto cycle_ms =
        static_cast<std::int64_t>(pattern.on_ms) + pattern.off_ms;
    if (cycle_ms > 0) {
      led_elapsed_ms += kPollMs;
      if (pulses_remaining == 0) {
        pulses_remaining = pattern.pulses;
      }
      const auto cycle_phase = led_elapsed_ms % cycle_ms;
      bool led_on = false;
      if (cycle_phase < pattern.on_ms && pulses_remaining > 0) {
        led_on = true;
        if (cycle_phase == 0) {
          --pulses_remaining;
        }
      }
      gpio_set_level(static_cast<gpio_num_t>(kStatusLedGpio), led_on ? 1 : 0);
    }

    if (state.reboot_requested()) {
      ESP_LOGW(kTag,
               "recovery reboot requested; to enter the ROM bootloader hold "
               "BOOT (SW2) while tapping RESET (SW1)");
      vTaskDelay(pdMS_TO_TICKS(1500));
      esp_restart();
    }
  }
}

// ---------------------------------------------------------------------------
// USB-Serial-JTAG console (recovery/export path)
// ---------------------------------------------------------------------------

void console_task(void *) {
  auto &state = app_state();
  std::printf("quiet-trace console ready (aggregate-only)\n");

  std::array<char, 160> line{};
  std::size_t length = 0;
  while (true) {
    const int character = std::fgetc(stdin);
    if (character == EOF) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    if (character == '\n' || character == '\r') {
      if (length == 0) {
        continue;
      }
      line[length] = '\0';
      std::uint64_t nonce = 0;
      esp_fill_random(&nonce, sizeof(nonce));

      const bool wants_json = std::strncmp(line.data(), "export json", 11) == 0;
      const bool wants_csv = std::strncmp(line.data(), "export csv", 10) == 0;

      auto response = handle_console_line(
          state, line.data(), now_us(),
          state.physical_presence_recent(now_us(), 10'000'000LL), nonce);
      std::string reply = std::string("qt> ") + (response.ok ? "ok" : "err") +
                          " " + response.code;
      // Never echo messages for credential-adjacent commands.
      if (response.ok && !wants_json && !wants_csv &&
          response.message != "wifi_saved") {
        reply += " " + response.message;
      }
      std::printf("%s\n", reply.c_str());

      if (wants_json || wants_csv) {
        if (response.ok) {
          const auto snapshot = state.clock_snapshot(now_us());
          const auto records = state.store().export_records();
          if (wants_json) {
            std::printf("%s\n", render_usb_export_json(
                                    records, snapshot.quality,
                                    snapshot.epoch_offset_ms, now_us())
                                    .c_str());
          } else {
            std::printf("%s", render_usb_export_csv(records, snapshot.quality,
                                                    snapshot.epoch_offset_ms)
                                  .c_str());
          }
        } else {
          std::printf("export blocked\n");
        }
      }
      length = 0;
      continue;
    }
    if (character == '\b' || character == 0x7F) {
      if (length > 0) {
        --length;
      }
      continue;
    }
    if (length + 1 < line.size()) {
      line[length++] = static_cast<char>(character);
    } else {
      length = 0;
      std::printf("qt> err line_too_long\n");
    }
  }
}

// ---------------------------------------------------------------------------
// Network + HTTP (aggregate-only read API)
// ---------------------------------------------------------------------------

std::string clock_quality_text(const ClockQuality quality) {
  switch (quality) {
  case ClockQuality::synced:
    return "synced";
  case ClockQuality::host_set:
    return "host_set";
  case ClockQuality::unknown:
    return "unknown";
  case ClockQuality::monotonic_only:
    break;
  }
  return "monotonic_only";
}

std::string records_page_json(const std::vector<StoredAggregate> &all,
                              const long after, const long limit,
                              const ClockQuality quality,
                              const std::optional<std::int64_t> offset_ms) {
  std::ostringstream body;
  body << '[';
  long returned = 0;
  long last_sequence = after;
  bool has_more = false;

  for (const auto &record : all) {
    if (after > 0 && static_cast<long>(record.sequence) <= after) {
      continue;
    }
    if (returned >= limit) {
      has_more = true;
      break;
    }
    if (returned != 0) {
      body << ',';
    }
    body << render_aggregate_record_json(record, quality, offset_ms);
    last_sequence = static_cast<long>(record.sequence);
    ++returned;
  }
  body << ']';

  std::ostringstream stream;
  stream << "{\"schema\":\"quiet-trace/records-page/v1\",\"records\":"
         << body.str() << ",\"paging\":{\"after\":"
         << (after > 0 ? std::to_string(after) : std::string("null"))
         << ",\"next_after\":"
         << (has_more ? std::to_string(last_sequence) : std::string("null"))
         << ",\"limit\":" << limit
         << ",\"has_more\":" << (has_more ? "true" : "false")
         << "},\"generated_at\":null,"
         << "\"clock_quality\":\"" << clock_quality_text(quality)
         << "\",\"quality_flags\":[]}";
  return stream.str();
}

esp_err_t send_json(httpd_req_t *req, const std::string &body) {
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, body.c_str(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_status(httpd_req_t *req) {
  auto &state = app_state();
  const auto snapshot = state.clock_snapshot(now_us());
  return send_json(
      req, render_status_json(state.pipeline().counters(), state.store().size(),
                              state.store().generation(),
                              state.setup().active(uptime_us_to_ms(now_us())),
                              CalibrationState::uncalibrated, snapshot.quality,
                              false));
}

long parse_long_query(httpd_req_t *req, const char *key, long fallback) {
  char buffer[24] = {};
  if (httpd_query_key_value(req->uri, key, buffer,
                            static_cast<int>(sizeof(buffer))) <= 0) {
    return fallback;
  }
  return std::strtol(buffer, nullptr, 10);
}

esp_err_t handle_records(httpd_req_t *req) {
  auto &state = app_state();
  const auto snapshot = state.clock_snapshot(now_us());
  const auto after = parse_long_query(req, "after", 0);
  auto limit = parse_long_query(req, "limit", 50);
  limit = std::max(1L, std::min(200L, limit));
  return send_json(req, records_page_json(state.store().export_records(), after,
                                          limit, snapshot.quality,
                                          snapshot.epoch_offset_ms));
}

esp_err_t handle_export(httpd_req_t *req) {
  auto &state = app_state();
  const auto snapshot = state.clock_snapshot(now_us());
  const bool csv = std::strstr(req->uri, "csv") != nullptr;
  const auto body =
      csv ? render_usb_export_csv(state.store().export_records(),
                                  snapshot.quality, snapshot.epoch_offset_ms)
          : render_usb_export_json(state.store().export_records(),
                                   snapshot.quality, snapshot.epoch_offset_ms,
                                   now_us());
  httpd_resp_set_type(req, csv ? "text/csv" : "application/json");
  return httpd_resp_send(req, body.c_str(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_api(httpd_req_t *req) {
  if (std::strstr(req->uri, "/api/v1/status") != nullptr) {
    return handle_status(req);
  }
  if (std::strstr(req->uri, "/api/v1/records") != nullptr) {
    return handle_records(req);
  }
  if (std::strstr(req->uri, "/api/v1/export") != nullptr) {
    return handle_export(req);
  }
  httpd_resp_set_status(req, "404 Not Found");
  httpd_resp_sendstr(req, "{}");
  return ESP_OK;
}

void start_http_server() {
  if (g_http_server != nullptr) {
    return; // already serving
  }
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  // LAN-trust MVP per docs/protocol.md: no authentication yet; the exposed
  // surface is aggregate-only. TLS/auth remain open protocol work before
  // any release claim.
  if (httpd_start(&g_http_server, &config) != ESP_OK) {
    ESP_LOGE(kTag, "http server failed to start");
    g_http_server = nullptr;
    return;
  }

  const httpd_uri_t api_uri{
      .uri = const_cast<char *>("/api/v1/*"),
      .method = HTTP_GET,
      .handler = handle_api,
      .user_ctx = nullptr,
  };
  httpd_register_uri_handler(g_http_server, &api_uri);
  ESP_LOGI(kTag, "aggregate-only HTTP API listening on port 80 (LAN trust)");
}

void wifi_event_handler(void *, const esp_event_base_t base, const int32_t id,
                        void *) {
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    auto &state = app_state();
    esp_sntp_config_t sntp_config =
        ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&sntp_config) == ESP_OK) {
      if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) == ESP_OK) {
        const auto epoch_seconds =
            static_cast<std::int64_t>(std::time(nullptr));
        state.mark_time_synced(epoch_seconds * 1000LL, now_us());
        ESP_LOGI(kTag, "SNTP time synced");
      } else {
        state.mark_time_sync_failed();
        ESP_LOGW(kTag, "SNTP sync failed; clock quality demoted");
      }
    }
    start_http_server();
  }
}

void network_task(void *) {
  auto &state = app_state();

  while (true) {
    std::string ssid;
    std::string password;
    if (!state.take_wifi_credentials(ssid, password)) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    esp_netif_init();
    const auto loop_result = esp_event_loop_create_default();
    if (loop_result != ESP_OK && loop_result != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(kTag, "event loop unavailable (%d)",
               static_cast<int>(loop_result));
      return;
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wifi_init) != ESP_OK) {
      ESP_LOGE(kTag, "esp_wifi_init failed");
      return;
    }

    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), ssid.c_str(),
                 sizeof(wifi_config.sta.ssid) - 1U);
    std::strncpy(reinterpret_cast<char *>(wifi_config.sta.password),
                 password.c_str(), sizeof(wifi_config.sta.password) - 1U);
    // Scrub the volatile copy as soon as it is staged into the driver.
    std::fill(password.begin(), password.end(), '\0');
    password.clear();

    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &wifi_event_handler, nullptr);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    ESP_LOGI(kTag, "Wi-Fi connect requested (SSID %s, credential not logged)",
             ssid.c_str());
    return;
  }
}

} // namespace

bool start_all() {
  g_boot_uptime_us = now_us();
  static SystemState instance;
  g_state = &instance;

  if (!init_i2s()) {
    return false;
  }

  BaseType_t ok = pdPASS;
  ok = xTaskCreate(acquisition_task, "qt_acq", 4096, nullptr, 6, nullptr);
  ok &= xTaskCreate(aggregation_task, "qt_agg", 4096, nullptr, 5, nullptr);
  ok &= xTaskCreate(control_task, "qt_ctl", 4096, nullptr, 5, nullptr);
  ok &= xTaskCreate(console_task, "qt_con", 8192, nullptr, 4, nullptr);
  ok &= xTaskCreate(network_task, "qt_net", 6144, nullptr, 4, nullptr);
  return ok == pdPASS;
}

SystemState &state() { return *g_state; }

} // namespace quiet_trace::target
