#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>
#include "LEDController.h"
#include "Effect.h"

class LEDServer {
public:

  LEDServer();
  ~LEDServer();
  void start();
  void stop();
  void post_connection_error(LEDController& client); 
  void post_client_ready(); 
  bool is_shutdown() { return shutdown_; }
  
  template <typename T>
  void run_effect(size_t seconds);
  
private:

  struct IOThread {
    IOThread();

    boost::asio::io_context ctx_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard_;
    std::thread thread_;
    std::vector<LEDController> clients_;
  };
  
  IOThread& io_schedule();
  void accept();
  void subscribe_signals();
  bool clients_ready();
  void send_frame(boost::asio::const_buffer buf);
  
  bool shutdown_;
  boost::asio::io_context main_io_;
  boost::asio::signal_set signals_;
  std::thread main_io_thread_;
  boost::asio::ip::tcp::acceptor accept_sock_;
  boost::asio::ip::tcp::endpoint accept_ep_;
  std::vector<std::shared_ptr<IOThread>> workers_;
  uint32_t client_count_;
  int client_ready_count_;
};

template <typename T>
void LEDServer::run_effect(size_t seconds) {
  T effect;
  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::seconds(seconds);
  effect.draw_frame();
  while (!is_shutdown() && std::chrono::steady_clock::now() < end) {
    if (clients_ready()) {
      send_frame(effect.buf());
      effect.draw_frame();
    }
  }
}
