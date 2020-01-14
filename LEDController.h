#pragma once

#include <chrono>
#include <boost/asio.hpp>

class LEDServer;

class LEDController {
public:

  LEDController(LEDController&& c);
  LEDController(LEDServer& server,
		boost::asio::ip::tcp::socket sock,
		boost::asio::io_context& io);
  ~LEDController();
  bool operator==(const LEDController& c) { return id_ == c.id_; }
  LEDController& operator=(LEDController&& c);
  uint32_t id() { return id_; }
  void send(const boost::asio::const_buffer& buf);
  void send_sync();
  
private:

  void read_header();

  std::reference_wrapper<LEDServer> server_;
  boost::asio::ip::tcp::socket sock_;
  std::reference_wrapper<boost::asio::io_context> io_;
  uint32_t id_;
  std::chrono::steady_clock::time_point start_;
};
  
