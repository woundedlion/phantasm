#pragma once

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

  template <typename T>
  void run_effect(size_t seconds) {
    T effect;
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < start) {
      effect.draw_frame();
      send_frame(effect);
    }
  }
  
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
  void send_frame(Effect& e);
  
  boost::asio::io_context main_io_;
  std::thread main_io_thread_;
  boost::asio::ip::tcp::acceptor accept_sock_;
  boost::asio::ip::tcp::endpoint accept_ep_;
  std::vector<std::shared_ptr<IOThread>> workers_;
};
