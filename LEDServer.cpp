#include "LEDServer.h"

#include <iostream>
#include "boost/asio.hpp"

using namespace boost::asio::ip;

namespace {
  const char* TAG = "LEDServer";
}

LEDServer::LEDServer() :
  accept_sock_(io_, tcp::endpoint(tcp::v4(), 5050))
{
}

LEDServer::~LEDServer() {
  io_thread_.join();
}

void LEDServer::start() {
  start_accept();
  io_thread_ = std::thread([this]() { io_.run(); });  
}

void LEDServer::start_accept() {
  accept_sock_.async_accept(std::bind(&LEDServer::handle_accept,
				      this, std::placeholders::_1, std::placeholders::_2));
}

void LEDServer::handle_accept(const std::error_code& ec,
			      tcp::socket client_sock)
{
  if (!ec) {
    std::cout << "Connection accepted: "
      	      << client_sock.remote_endpoint() << " -> "
	      << client_sock.local_endpoint() << std::endl;
    controllers_.emplace_back(std::move(client_sock));
  } else {
    std::cout << ec.message() << std::endl;
  }
  start_accept();
}

int main(int argc, char *argv[]) {
  LEDServer server;
  server.start();
}
