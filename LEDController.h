#pragma once
#include "boost/asio.hpp"

class LEDController {
public:

  LEDController(LEDController&& c) : sock_(std::move(c.sock_)) {}
  LEDController(boost::asio::ip::tcp::socket sock);
  ~LEDController();
  
private:

  boost::asio::ip::tcp::socket sock_;
};
  
