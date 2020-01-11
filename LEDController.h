#pragma once

#include <boost/asio.hpp>
#include "LEDController.h"

class LEDController {
public:

  LEDController(LEDController&& c);
  LEDController(boost::asio::ip::tcp::socket sock, boost::asio::io_context& io);
  ~LEDController();
  void send_frame(const boost::asio::const_buffer& buf);
  void send_sync();
  
private:

  void read_header();
  
  boost::asio::ip::tcp::socket sock_;
  boost::asio::io_context& io_;
  uint32_t id_;
};
  
