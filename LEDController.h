#pragma once
#include "boost/asio.hpp"

class LEDController {
public:

  LEDController(LEDController&& c) : sock_(std::move(c.sock_)) {}
  LEDController(boost::asio::ip::tcp::socket sock);
  ~LEDController();
  
private:

  void read_header();
  
  boost::asio::ip::tcp::socket sock_;
};
  
