idf_component_register(
  SRCS
    "target_core.cpp"
    "target_app.cpp"
  INCLUDE_DIRS "include"
  REQUIRES domain esp_driver_i2s esp_driver_gpio esp_event esp_http_server esp_netif esp_wifi esp_timer
)
