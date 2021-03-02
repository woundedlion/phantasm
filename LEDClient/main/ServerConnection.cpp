#include "ServerConnection.h"

#include "LEDClient.h"

namespace {
const char* TAG = "ServerConnection";
}

ServerConnection::ServerConnection(uint32_t src, uint32_t dst,
                                   uint16_t dst_port,
                                   const std::vector<uint8_t>& mac)
    : src_(src),
      dst_(dst),
      local_ep_(asio::ip::address_v4(src_), 0),
      remote_ep_(asio::ip::address_v4(dst_), dst_port),
      sock_(ctx_, local_ep_),
      id_(mac),
      read_pending_(false),
      op_id_(0)
{
  connect();
  xTaskCreatePinnedToCore(ServerConnection::run_io, "IO_LOOP", 4096, this, 17,
                          &io_task_, 0);
}

ServerConnection::~ServerConnection() {
  asio::post(ctx_, [this]() {
    sock_.cancel();
    sock_.close();
  });
  vTaskDelete(io_task_);
}

void ServerConnection::run_io(void* arg) {
  auto c = reinterpret_cast<ServerConnection*>(arg);
  auto work = asio::make_work_guard(c->ctx_);
  c->ctx_.run();
}

void ServerConnection::connect() {
  ESP_LOGI(TAG, "Connecting to %s", to_string(remote_ep_).c_str());
  sock_.async_connect(remote_ep_, [this](const std::error_code& ec) {
    if (!ec) {
      ESP_LOGI(TAG, "Connection established: %s -> %s",
               to_string(local_ep_).c_str(), to_string(remote_ep_).c_str());
      send_header();
    } else if (ec != std::errc::operation_canceled) {
      ESP_LOGE(TAG, "Connection error to %s: %s", to_string(remote_ep_).c_str(),
               ec.message().c_str());
      post_conn_err();
    }
  });
}

void ServerConnection::send_header() {
  ESP_LOGI(TAG, "ID: %02x-%02x-%02x-%02x-%02x-%02x", id_[0], id_[1], id_[2],
           id_[3], id_[4], id_[5]);
  asio::async_write(
      sock_, asio::buffer(id_),
      [this](const std::error_code& ec, std::size_t length) {
        if (!ec) {
          ESP_LOGI(TAG, "Sent HELLO: %s -> %s", to_string(local_ep_).c_str(),
                   to_string(remote_ep_).c_str());
          post_conn_active();
        } else if (ec != std::errc::operation_canceled) {
          ESP_LOGE(TAG, "Write error %s: %s", to_string(remote_ep_).c_str(),
                   ec.message().c_str());
          post_conn_err();
        }
      });
}

void ServerConnection::read_frame(JitterBuffer& bufs) {
  assert(bufs.level() < bufs.depth());
  auto op_id = op_id_++;
  ESP_LOGI(TAG, "Read %d started - Jitter buffer level: %d/%d", op_id, bufs.level(), bufs.depth());
  asio::async_read(
      sock_, asio::buffer((void*)bufs.next(), bufs.datum_size()),
      [&, this, op_id](const std::error_code& ec, std::size_t bytes) {
        if (!ec && bytes) {
          ESP_LOGI(TAG, "Read %d complete: %d bytes",
                   op_id, bytes);
          bufs.push();
          ESP_LOGI(TAG, "Push %d - Jitter buffer level: %d/%d",
                   op_id, bufs.level(), bufs.depth());
          ERR_THROW(
              esp_event_post(LED_EVENT, LED_EVENT_READ_COMPLETE, NULL, 0, 0));
        } else if (ec != std::errc::operation_canceled) {
          ESP_LOGE(TAG, "Read error: %s", ec.message().c_str());
          read_pending_ = false;
          post_conn_err();
        }
      });
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

void ServerConnection::post_conn_err() {
  ERR_THROW(esp_event_post(LED_EVENT, LED_EVENT_CONN_ERR, NULL, 0, 0));
}

void ServerConnection::post_conn_active() {
  ERR_THROW(esp_event_post(LED_EVENT, LED_EVENT_CONN_ACTIVE, NULL, 0, 0));
}
