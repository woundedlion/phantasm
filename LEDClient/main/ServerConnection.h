#pragma once

#include <vector>
#include "asio.hpp"
#include "JitterBuffer.h"
#include <sstream>

template <typename T>
std::string to_string(const T& t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}

class ServerConnection {
 public:
  ServerConnection(uint32_t src, uint32_t dst, uint16_t dst_port, 
    const std::vector<uint8_t>& id);
  ~ServerConnection();
  void connect();
  void send_header();
  void read_frame(JitterBuffer& bufs);

 private:

  static void run_io(void* arg);
  void post_conn_err();
  void post_conn_active();

  uint32_t src_;
  uint32_t dst_;
  asio::io_context ctx_;
  TaskHandle_t io_task_;
  asio::ip::tcp::endpoint local_ep_;
  asio::ip::tcp::endpoint remote_ep_;
  asio::ip::tcp::socket sock_;
  std::vector<uint8_t> id_;
  bool read_pending_;
  uint32_t op_id_;
};
