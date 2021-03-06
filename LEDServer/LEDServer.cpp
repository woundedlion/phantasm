#include "LEDServer.h"

#include <boost/log/trivial.hpp>
#include <cassert>
#include <unordered_map>
#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;

namespace {
const char* TAG = "LEDServer";
const std::unordered_map<std::string, int> _slices = {{"24-0a-c4-c0-6b-f0", 0},
                                                      {"24-0a-c4-c0-66-b8", 1}};
}

IOThread::IOThread()
    : guard_(make_work_guard(ctx_)), thread_([this]() {
        LOG(info) << "IO thread start: " << std::hex
                  << std::this_thread::get_id();
        ctx_.run();
        LOG(info) << "IO thread exit: " << std::hex
                  << std::this_thread::get_id();
      }) {}

LEDServer::LEDServer()
    : frames_in_flight_(0),
      shutdown_(false),
      signals_(main_io_, SIGINT, SIGTERM),
      accept_sock_(main_io_, tcp::endpoint(tcp::v4(), 5050)) {}

LEDServer::~LEDServer() { stop(); }

void LEDServer::start() {
  LOG(info) << "Starting server";
  subscribe_signals();
  int num_client_io_threads =
      std::max((uint)1, std::thread::hardware_concurrency() - 1);
  for (int i = 0; i < num_client_io_threads; ++i) {
    workers_.emplace_back(new IOThread());
  }
  accept();
  main_io_thread_ = std::thread([this]() { main_io_.run(); });
}

void LEDServer::stop() {
  LOG(info) << "Stopping server...";
  for (auto& c : clients_) {
    c->post_cancel();
  }
  for (auto&& w : workers_) {
    w->guard_.reset();
    w->thread_.join();
  }
  LOG(debug) << "worker threads joined";
  accept_sock_.close();
  signals_.cancel();
  main_io_thread_.join();
  LOG(info) << "Server stopped";
}

void LEDServer::post_connection_error(std::shared_ptr<Connection> client) {
  post(main_io_, [this, client]() {
    clients_.clear();
    frames_in_flight_ = 0;
  });
}

void LEDServer::post_client_ready(std::shared_ptr<Connection> client) {
  post(main_io_, [this, client]() {
    client->set_ready(true);
    if (clients_.end() != std::find_if(clients_.begin(), clients_.end(), [&](auto& c) -> bool {
          return c->id_str() == client->id_str() && c != client;
        })) {
        LOG(warning) << "Client " << client->id_str() << " at " << client->key()
                     << " reconnected";
      post_connection_error(client);
    }
    if (std::all_of(clients_.begin(), clients_.end(),
                    [](auto& c) { return c->ready(); })) {
      frames_in_flight_ = 0;
      send_frame();
    }
  });
}

std::shared_ptr<IOThread> LEDServer::io_schedule() {
  assert(workers_.size());
  return *std::min_element(workers_.begin(), workers_.end(),
                           [](const auto& a, const auto& b) {
                             return a.use_count() < b.use_count();
                           });
} 

void LEDServer::accept() {
  auto io = io_schedule();
  accept_sock_.async_accept(
      io->ctx_, [this, io](const std::error_code& ec, tcp::socket client_sock) {
        if (!ec) {
          LOG(info) << "Connection accepted: " << client_sock.remote_endpoint()
                    << " -> " << client_sock.local_endpoint();
          socket_base::send_buffer_size option(512000);
          client_sock.set_option(option);
          auto c = std::make_shared<Connection>(*this, client_sock, io);
          clients_.emplace_back(std::move(c));
          accept();
        } else {
          if (ec != std::errc::operation_canceled) {
            LOG(error) << ec.message();
          } else {
            LOG(debug) << ec.message();
          }
        }
      });
}

void LEDServer::subscribe_signals() {
  signals_.async_wait([this](const std::error_code& ec, int signal_number) {
    if (!ec) {
      LOG(info) << "Received signal " << signal_number;
      shutdown_ = true;
      auto effect = std::atomic_load(&effect_);
      if (effect) effect->cancel();
    } else {
      LOG(error) << "Signal listen error: " << ec.message();
    }
  });
}

int LEDServer::get_slice(const std::string& client_id) {
  return _slices.at(client_id);
}

void LEDServer::send_frame() {
  auto effect = std::atomic_load(&effect_);
  while (!shutdown_ && !effect->wait_frame_available()) {
    effect = std::atomic_load(&effect_);
  }
  assert(frames_in_flight_ == 0);
  frames_in_flight_ = clients_.size();
  for (auto& c : clients_) {
    LOG(debug) << "Sending frame " << effect->frame_count() << " to client id "
               << c->id_str();
    c->post_send(effect->buf(get_slice(c->id_str())), 
      [this]() { this->post_send_frame_complete(); });
  }
  effect->advance_frame();
}

void LEDServer::post_send_frame_complete() {
  post(main_io_, [&]() {
    if (--frames_in_flight_ == 0) {
      this->send_frame();
    }
  });
}

void LEDServer::run(const Sequence& sequence) {
  while (true) {
    for (auto& play_effect : sequence) {
      play_effect();
      if (is_shutdown()) {
        return;
      }
    }
  }
}

int main(int argc, char* argv[]) {
  LEDServer server;
  server.start();
  Sequence show = {
    server.play_secs<Test, 5>(),
    server.play_secs<Test2, 5>(),
  };
  while (!server.is_shutdown()) {
    server.run(show);
  }
  return 0;
}
