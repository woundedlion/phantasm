#pragma once

#include <sstream>
#include <vector>

#include "JitterBuffer.h"
#include "asio.hpp"

template <typename T>
std::string to_string(const T& t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}

class ServerConnection {
 public:
  ServerConnection(asio::io_context& ctx, uint32_t src, uint32_t dst,
                   uint16_t dst_port, const std::vector<uint8_t>& id);
  ~ServerConnection();
  static void start_io();
  void connect();
  void send_header();
  void read_frame(JitterBuffer& bufs);

 private:
  void post_conn_err();
  void post_conn_active();

  asio::io_context& ctx_;
  uint32_t src_;
  uint32_t dst_;
  asio::ip::tcp::endpoint local_ep_;
  asio::ip::tcp::endpoint remote_ep_;
  asio::ip::tcp::socket sock_;
  std::vector<uint8_t> id_;
  bool read_pending_;
  uint32_t op_id_;
};
