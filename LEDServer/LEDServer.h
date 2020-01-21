#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <boost/asio.hpp>
#include "Connection.h"
#include "Effect.h"

struct IOThread {
  IOThread();
  
  boost::asio::io_context ctx_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard_;
  std::thread thread_;
};

class LEDServer {
public:

  LEDServer();
  ~LEDServer();
  void start();
  void stop();
  void post_connection_error(Connection& client); 
  void post_client_ready(Connection& client); 
  bool is_shutdown() { return shutdown_; }
  
  template <typename T>
  void run_effect(size_t seconds);
  
private:
  
  std::shared_ptr<IOThread> io_schedule();
  void accept();
  void subscribe_signals();
  void send_frame();

  std::unique_ptr<Effect> effect_;
  bool shutdown_;
  boost::asio::io_context main_io_;
  boost::asio::signal_set signals_;
  std::thread main_io_thread_;
  boost::asio::ip::tcp::acceptor accept_sock_;
  boost::asio::ip::tcp::endpoint accept_ep_;
  std::vector<std::shared_ptr<IOThread>> workers_;
  std::unordered_map<Connection::key_t, Connection> clients_;
};

template <typename T>
void LEDServer::run_effect(size_t seconds) {
  effect_.reset(new T());
  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::seconds(seconds);
  while (!is_shutdown() && std::chrono::steady_clock::now() < end) {
    effect_->draw_frame();
  }  
}
