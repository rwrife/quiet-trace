#include "esp_log.h"

namespace {
constexpr char kTag[] = "quiet_trace";
}

extern "C" void app_main() {
  ESP_LOGI(kTag,
           "foundation scaffold booted; microphone acquisition and persisted "
           "records are not implemented");
}
