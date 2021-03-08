#include "LedClient.h"

#include <iostream>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/xtensa_api.h"
#include "freertos/xtensa_timer.h"
#include "freertos/timers.h"

ESP_EVENT_DEFINE_BASE(LED_EVENT);

using namespace esp;

namespace {
const char* TAG = "LedClient";
const char* SERVER_ADDR = "10.10.10.1";
const int SERVER_PORT = 5050;
std::unique_ptr<LEDClient> led_client;
}  // namespace

LEDClient::LEDClient() : 
  state_(STOPPED), 
  x_(0),
  bufs_(new JitterBuffer()), 
  read_pending_(false),
  dropped_frames_(0) 
{
  assert(bufs_);
  esp_timer_create_args_t args;
  args.callback = &LEDClient::handle_connect_timer;
  args.arg = this;
  args.dispatch_method = ESP_TIMER_TASK;
  args.name = "connect_timer";
  ERR_THROW(esp_timer_create(&args, &connect_timer_));

  args.callback = &LEDClient::handle_prefetch_timer;
  args.arg = this;
  args.dispatch_method = ESP_TIMER_TASK;
  args.name = "prefetch_timer";
  ERR_THROW(esp_timer_create(&args, &prefetch_timer_));
  ERR_THROW(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                       LEDClient::handle_event, this));
  ERR_THROW(esp_event_handler_register(LED_EVENT, ESP_EVENT_ANY_ID,
                                       LEDClient::handle_event, this));
}

LEDClient::~LEDClient() {
  stop_connect_timer();
  ERR_LOG("esp_timer_delete", esp_timer_delete(connect_timer_));
  stop_prefetch_timer();
  ERR_LOG("esp_timer_delete", esp_timer_delete(prefetch_timer_));
  if (led_task_) {
    vTaskDelete(led_task_);
  }
}

void LEDClient::start() { wifi_.start(); }

void IRAM_ATTR LEDClient::send_pixels() {
  *spi_ << frame_[x_];
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR LEDClient::run_leds(void* arg) {
  auto c = reinterpret_cast<LEDClient*>(arg);
  c->spi_.reset(new SPI());
  c->start_gpio();
  ets_isr_mask(1ULL << XT_TIMER_INTNUM);
  while (true) {
    /*
    while (0 == (REG_READ(GPIO_IN_REG) & 1 << PIN_CLOCK_READ)) {
    }
    c->send_pixels();
    if (++c->x_ == W) {
      c->x_ = 0;
      esp_event_post(LED_EVENT, LED_EVENT_NEED_FRAME, NULL, 0, 0);
    }
    */
  }
}

void IRAM_ATTR LEDClient::on_clock_isr(void* arg) {
  auto c = reinterpret_cast<LEDClient*>(arg);
  c->send_pixels();
  if (++c->x_ == W) {
    c->x_ = 0;
    int yield = 0;
    esp_event_isr_post(LED_EVENT, LED_EVENT_NEED_FRAME, NULL, 0, &yield);
    if (yield) {
      portYIELD_FROM_ISR();
    }
  }
}

void LEDClient::dump_event(void* arg, esp_event_base_t base, int32_t id,
                           void* data) {
  ESP_LOGI(TAG, "base: %s id: %d", base, id);
}

void LEDClient::handle_event(void* arg, esp_event_base_t base, int32_t id,
                             void* data) {
  try {
    auto handler = static_cast<LEDClient*>(arg);
    assert(base == IP_EVENT || base == LED_EVENT);
    switch (handler->state_) {
      case STOPPED:
        handler->state_stopped(base, id, data);
        break;
      case READY:
        handler->state_ready(base, id, data);
        break;
      case PREFETCH:
        handler->state_prefetch(base, id, data);
        break;
      case ACTIVE:
        handler->state_active(base, id, data);
        break;
      default:
        assert(false);
    }
  } catch (const std::exception& e) {
    ESP_LOGE(TAG, "Exception in LEDClient state machine: %s", e.what());
  }
}

void LEDClient::state_stopped(esp_event_base_t base, int32_t id, void* data) {
  switch (id) {
    case IP_EVENT_STA_GOT_IP:
      ESP_LOGI(TAG, "State transition: %s -> %s on %d", "STOPPED", "READY", id);
      state_ = READY;
      on_got_ip();
      break;
    default:
      ESP_LOGW(TAG, "Unhandled event id: %d", id);
  }
}

void LEDClient::state_ready(esp_event_base_t base, int32_t id, void* data) {
  switch (id) {
    case IP_EVENT_STA_GOT_IP:
      on_got_ip();
      break;
    case LED_EVENT_CONN_ERR:
      on_conn_err();
      break;
    case LED_EVENT_CONN_ACTIVE:
      ESP_LOGI(TAG, "State transition: %s -> %s on %d", "READY", "PREFETCH", id);
      state_ = PREFETCH;
      xTaskCreatePinnedToCore(LEDClient::run_leds, "LED_LOOP", 2048, this,
                              configMAX_PRIORITIES - 1, &led_task_, 1);
      start_prefetch_timer();
      read_pending_ = true;
      connection_->read_frame(*bufs_);
      break;
    case LED_EVENT_NEED_FRAME:
      // ignore clock before we are active
      break;
    default:
      ESP_LOGW(TAG, "Unhandled event id: %d", id);
  }
}

void LEDClient::state_prefetch(esp_event_base_t base, int32_t id, void* data) {
  switch (id) {
    case IP_EVENT_STA_GOT_IP:
      ESP_LOGI(TAG, "State transition: %s -> %s on %d", "PREFETCH", "READY", id);
      state_ = READY;
      on_got_ip();
      break;
    case LED_EVENT_CONN_ERR:
      ESP_LOGI(TAG, "State transition: %s -> %s on %d", "PREFETCH", "READY", id);
      state_ = READY;
      stop_prefetch_timer();
      on_conn_err();
      break;
    case LED_EVENT_PREFETCH_TIMER:
      // Start led clock if rcv buffer is full
      if (bufs_->level() == bufs_->depth()) {
        ESP_LOGI(TAG, "State transition: %s -> %s on %d", "PREFETCH", "ACTIVE", id);
        state_ = ACTIVE;
        for (int i = 0; i < W; ++i) {
          frame_[i].load(bufs_->front() + i * STRIP_H);
        }
        led_clock_.reset(new SquareWaveGenerator<W * 16, PIN_CLOCK_GEN>());
      } else {
        start_prefetch_timer();
      }
      break;
    case LED_EVENT_NEED_FRAME:
      // ignore clock before we are active
      break;
    case LED_EVENT_READ_COMPLETE:
read_pending_ = false;
if (bufs_->level() < bufs_->depth()) {
  read_pending_ = true;
  connection_->read_frame(*bufs_);
}
break;
    default:
      ESP_LOGW(TAG, "Unhandled event id: %d", id);
  }
}

void LEDClient::state_active(esp_event_base_t base, int32_t id, void* data) {
  switch (id) {
  case IP_EVENT_STA_GOT_IP:
    ESP_LOGI(TAG, "State transition: %s -> %s on %d", "ACTIVE", "READY", id);
    state_ = READY;
    on_got_ip();
    break;
  case LED_EVENT_CONN_ERR:
    ESP_LOGI(TAG, "State transition: %s -> %s on %d", "ACTIVE", "READY", id);
    state_ = READY;
    on_conn_err();
    break;
  case LED_EVENT_NEED_FRAME:
    advance_frame();
    break;
  case LED_EVENT_READ_COMPLETE:
    read_pending_ = false;
    if (bufs_->level() < bufs_->depth()) {
      read_pending_ = true;
      connection_->read_frame(*bufs_);
    }
    break;
  default:
    ESP_LOGW(TAG, "Unhandled event id: %d", id);
  }
}

void LEDClient::on_got_ip() {
  try {
    ESP_LOGI(TAG, "Resetting client connection on IP change");
    connection_.reset(new ServerConnection(
      ntohl(wifi_.ip()), ntohl(inet_addr(SERVER_ADDR)), SERVER_PORT, wifi_.mac_address()));
  }
  catch (std::exception& e) {
    ESP_LOGE(TAG, "%s", e.what());
  }
}

void LEDClient::on_conn_err() {
  connection_.reset();
  led_clock_.reset();
  start_connect_timer();
}

void LEDClient::start_connect_timer() {
  ESP_LOGI(TAG, "Reconnecting in 1 second...");
  ERR_THROW(esp_timer_start_once(connect_timer_, 1000000));
}

void LEDClient::stop_connect_timer() {
  ERR_LOG("esp_timer_stop", esp_timer_stop(connect_timer_));
}

void LEDClient::handle_connect_timer(void* arg) {
  auto c = static_cast<LEDClient*>(arg);
  c->connection_.reset(new ServerConnection(ntohl(c->wifi_.ip()),
    ntohl(inet_addr(SERVER_ADDR)),
    SERVER_PORT,
    c->wifi_.mac_address()));
}

void LEDClient::start_prefetch_timer() {
  ESP_LOGI(TAG, "Waiting for frame prefetch to complete...");
  ERR_THROW(esp_timer_start_once(prefetch_timer_, 500000));
}

void LEDClient::stop_prefetch_timer() {
  ESP_LOGI(TAG, "Stopping prefetch_timer...");
  ERR_LOG("esp_timer_stop", esp_timer_stop(prefetch_timer_));
}

void LEDClient::handle_prefetch_timer(void* arg) {
  esp_event_post(LED_EVENT, LED_EVENT_PREFETCH_TIMER, NULL, 0, 0);
}

void LEDClient::advance_frame() {
  assert(connection_);
  if (bufs_->level()) {
    if (bufs_->level() < bufs_->depth()) {
      ESP_LOGW(TAG, "jitter buffer level: %d/%d",
               bufs_->level(), bufs_->depth());    
    }
    ESP_LOGD(TAG, "Frame advanced -> jitter buffer level: %d/%d",
             bufs_->level(), bufs_->depth());
    int fastforward = bufs_->pop(1 + dropped_frames_) - 1;
    if (fastforward) {
      dropped_frames_ -= fastforward;
      ESP_LOGW(TAG, "Caught up by %d frames -> jitter buffer level: %d/%d",
               fastforward, bufs_->level(), bufs_->depth());
    }
    if (!read_pending_) {
      read_pending_ = true;
      connection_->read_frame(*bufs_);
    }
    for (int i = 0; i < W; ++i) {
      frame_[i].load(bufs_->front() + i * STRIP_H);
    }
  }
  else {
    dropped_frames_++;
    ESP_LOGE(TAG, "Frame dropped (%d)", dropped_frames_);
    if (dropped_frames_ > 16 * 10) {
        assert(0);
    }
  }
}

void LEDClient::start_gpio() {
  try {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << PIN_CLOCK_READ;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    cfg.intr_type = GPIO_INTR_POSEDGE;
//    cfg.intr_type = GPIO_INTR_DISABLE;
    ERR_THROW(gpio_config(&cfg));
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3);
    gpio_isr_handler_add(PIN_CLOCK_READ, on_clock_isr, this);
  } catch (const std::exception& e) {
    ESP_LOGE(TAG, "%s", e.what());
  }
}

void LEDClient::stop_gpio() {
  // TODO
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" void app_main(void) {
  try {
    led_client.reset(new LEDClient());
    led_client->start();
  } catch (const std::exception& e) {
    ESP_LOGE(TAG, "%s", e.what());
  }
}