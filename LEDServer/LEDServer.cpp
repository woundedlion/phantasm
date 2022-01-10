#include "LEDServer.h"

#include <algorithm>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <unordered_map>

#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;

namespace {
const char* TAG = "LEDServer";
}  // namespace

const char * Config::_slices[Config::SLICE_COUNT] = {"24-0a-c4-c0-6b-f0",
                     //  "24-0a-c4-c0-66-b8",
                     //"24-0a-c4-c0-4b-6c"
};

IOThread::IOThread()
    : guard_(make_work_guard(ctx_)), thread_([this]() {
        LOG(info) << "IO thread start: " << std::hex
                  << std::this_thread::get_id();
        ctx_.run();
        LOG(info) << "IO thread exit: " << std::hex
                  << std::this_thread::get_id();
      }) {}

LEDServer::LEDServer()
    : frame_num_(0),
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

void LEDServer::post_drop_client(std::shared_ptr<Connection> client) {
  post(main_io_, [this, client]() {
    clients_.erase(std::remove_if(
        clients_.begin(), clients_.end(),
        [&](auto& c) { return c->id_str() == client->id_str(); }));
  });
}

void LEDServer::post_connection_error(std::shared_ptr<Connection> client) {
  post(main_io_, [this, client]() { clients_.clear(); });
}

bool LEDServer::all_clients_ready() {
  for (auto& slice : Config::_slices) {
    if (std::find_if(clients_.begin(), clients_.end(), [&](auto& client) {
          return client->id_str() == slice && client->ready();
        }) == clients_.end()) {
      return false;
    }
  }
  return true;
}

void LEDServer::post_client_ready(std::shared_ptr<Connection> client) {
  post(main_io_, [this, client]() {
    client->set_ready(true);
    if (clients_.end() !=
        std::find_if(clients_.begin(), clients_.end(), [&](auto& c) -> bool {
          return c->id_str() == client->id_str() && c != client;
        })) {
      LOG(warning) << "Client " << client->id_str() << " at " << client->key()
                   << " reconnected";
      post_connection_error(client);
      return;
    }
    if (std::find(Config::_slices, std::end(Config::_slices), client->id_str()) ==
        std::end(Config::_slices)) {
      post_drop_client(client);
      return;
    }
    if (all_clients_ready()) {
      start_sending();
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
          socket_base::send_buffer_size option(1024000);
          client_sock.set_option(option);
          auto c = std::make_shared<Connection>(*this, client_sock, io);
          c->read_header();
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
      frames_.cancel();
    } else {
      LOG(error) << "Signal listen error: " << ec.message();
    }
  });
}

int LEDServer::slice_index(const std::string& client_id) {
  auto iter = std::find(Config::_slices, std::end(Config::_slices), client_id);
  assert(iter != std::end(Config::_slices));
  return std::distance(Config::_slices, iter);
}

void LEDServer::start_sending() {
  for (auto& c : clients_) {
    c->start_send(frames_, slice_index(c->id_str()));
  }
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
  boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                      boost::log::trivial::info);

  LEDServer server;
  server.start();
  Sequence show = {
      server.play_secs<Test, 10>(),
      //      server.play_secs<RainbowHSV, 10>(),
      //     server.play_secs<RainbowTwistHSV, 10>(),
      //    server.play_secs<RainbowHSL, 3>(),
  };
  while (!server.is_shutdown()) {
    server.run(show);
  }
  return 0;
}
