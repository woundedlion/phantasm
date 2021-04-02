#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "Connection.h"
#include "Effect.h"

struct IOThread {
  IOThread();

  boost::asio::io_context ctx_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      guard_;
  std::thread thread_;
};

typedef std::vector<std::function<void()>> Sequence;

class LEDServer {
 public:
  LEDServer();
  ~LEDServer();
  void start();
  void stop();
  void post_drop_client(std::shared_ptr<Connection> client);
  void post_connection_error(std::shared_ptr<Connection> client);
  void post_client_ready(std::shared_ptr<Connection> client);
  bool is_shutdown() { return shutdown_; }

  void run(const Sequence& sequence);
  template <typename Effect, size_t secs>
  std::function<void()> play_secs();

 private:
  std::shared_ptr<IOThread> io_schedule();
  void accept();
  void subscribe_signals();
  int get_slice(const std::string& client_id);
  void send_frame();
  void post_send_frame_complete();
  bool all_clients_ready();

  int frames_in_flight_;
  std::shared_ptr<Effect> effect_;
  bool shutdown_;
  boost::asio::io_context main_io_;
  boost::asio::signal_set signals_;
  std::thread main_io_thread_;
  boost::asio::ip::tcp::acceptor accept_sock_;
  boost::asio::ip::tcp::endpoint accept_ep_;
  std::vector<std::shared_ptr<IOThread>> workers_;
  std::vector<std::shared_ptr<Connection>> clients_;
  static const std::unordered_map<std::string, int> slices_;
};

template <typename EffectDerived, size_t secs>
std::function<void()> LEDServer::play_secs() {
  return [this] {
    auto effect = std::atomic_load(&effect_);
    if (effect) {
      effect->cancel();
    }
    std::atomic_exchange(&effect_,
                         std::shared_ptr<Effect>(new EffectDerived()));
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(secs);
    while (std::chrono::steady_clock::now() < end) {
      if (is_shutdown()) {
        return;
      }
      std::atomic_load(&effect_)->draw_frame();
    }
  };
}
