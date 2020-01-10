#pragma once

#include <thread>
#include "boost/asio.hpp"
#include "LEDController.h"

class LEDServer {
public:

  LEDServer();
  ~LEDServer();
  void start();
  void start_accept();
  void handle_accept(const std::error_code& ec,
		     boost::asio::ip::tcp::socket client_sock);
  
private:

  boost::asio::io_context io_;
  boost::asio::ip::tcp::acceptor accept_sock_;
  boost::asio::ip::tcp::endpoint accept_ep_;
  std::thread io_thread_;
  std::vector<LEDController> controllers_;
};
