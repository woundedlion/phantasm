#pragma once

#include <sstream>

#include "App.h"
#include "LEDC.h"
#include "LEDServer/Types.h"
#include "ServerConnection.h"
#include "SPI.h"
#include "WifiClient.h"

#include "driver/gpio.h"

ESP_EVENT_DECLARE_BASE(LED_EVENT);

enum {
  LED_EVENT_CONN_ERR = 10000,
  LED_EVENT_CONN_ACTIVE = 10001,
  LED_EVENT_PREFETCH_TIMER = 10002,
  LED_EVENT_NEED_FRAME = 10003,
  LED_EVENT_READ_COMPLETE = 10004,
};

class LEDClient : public esp::App {
 public:
  LEDClient();
  ~LEDClient();
  void start();
  void send_pixels();

 private:

  enum State {
    STOPPED,
    READY,
    PREFETCH,
    ACTIVE,
  };

  static const gpio_num_t PIN_CLOCK_GEN = GPIO_NUM_25;
  static const gpio_num_t PIN_CLOCK_READ = GPIO_NUM_26;

  State state_;
  esp::WifiClient wifi_;
  std::unique_ptr<ServerConnection> connection_;
  esp_timer_handle_t connect_timer_;
  esp_timer_handle_t prefetch_timer_;
  APA102Frame<STRIP_H> frame_[W];
  std::unique_ptr<SPI> spi_;
  volatile uint32_t x_;
  TaskHandle_t led_task_;
  std::unique_ptr<SquareWaveGenerator<W * 16, PIN_CLOCK_GEN>> led_clock_;
  std::unique_ptr<JitterBuffer> bufs_;
  bool read_pending_;

  static void run_leds(void* arg);
  static void on_clock_isr(void* arg);

  static void dump_event(void* arg, esp_event_base_t base, int32_t id,
                         void* data);
  static void handle_event(void* arg, esp_event_base_t base, int32_t id,
                           void* data);
  void state_stopped(esp_event_base_t base, int32_t id, void* data);
  void state_ready(esp_event_base_t base, int32_t id, void* data);
  void state_prefetch(esp_event_base_t base, int32_t id, void* data);
  void state_active(esp_event_base_t base, int32_t id, void* data);

  void start_connect_timer();
  void stop_connect_timer();
  static void handle_connect_timer(void* arg);

  void start_prefetch_timer();
  void stop_prefetch_timer();
  static void handle_prefetch_timer(void* arg);

  void on_got_ip();
  void on_conn_err();
  void advance_frame();

  void start_gpio();
  void stop_gpio();
};