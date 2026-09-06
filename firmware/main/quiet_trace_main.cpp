#include "esp_log.h"

namespace {
constexpr char kTag[] = "quiet_trace";
}

extern "C" void app_main() {
  ESP_LOGI(
      kTag,
      "aggregate-only firmware slice booted; hardware microphone transport "
      "and bench validation remain pending physical bring-up");
}
