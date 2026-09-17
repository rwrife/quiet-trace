#include "esp_log.h"

#include "quiet_trace/target.hpp"

namespace {
constexpr char kTag[] = "quiet_trace";
}

extern "C" void app_main() {
  if (!quiet_trace::target::start_all()) {
    ESP_LOGE(
        kTag,
        "target integration failed to start; microphone transport not live");
    return;
  }
  ESP_LOGI(kTag,
           "aggregate-only firmware integration started; hardware validation "
           "remains pending physical bring-up (issue #6)");
}
