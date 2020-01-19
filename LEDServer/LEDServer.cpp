#include "LEDServer.h"
#include <boost/log/trivial.hpp>
#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;

namespace {
  const char* TAG = "LEDServer";
}

LEDServer::IOThread::IOThread() :
  guard_(make_work_guard(ctx_)),
  thread_([this]() {
	    LOG(info) << "IO thread start: " << std::hex << std::this_thread::get_id();
	    ctx_.run();
	    LOG(info) << "IO thread exit: " << std::hex << std::this_thread::get_id();
	  })
{}

/////////////////////////////////////////////////////////////////////////////////////////

LEDServer::LEDServer() :
  shutdown_(false),
  signals_(main_io_, SIGINT, SIGTERM),
  accept_sock_(main_io_, tcp::endpoint(tcp::v4(), 5050)),
  client_count_(0),
  client_ready_count_(0)
{

}

LEDServer::~LEDServer() {
  stop();
}

void LEDServer::start() {
  LOG(info) << "Starting server";
  subscribe_signals();
  int num_client_io_threads = std::max((uint)1, std::thread::hardware_concurrency() - 1);
  for (int i = 0; i < num_client_io_threads; ++i) {
    workers_.emplace_back(new IOThread());
  }
  accept();
  main_io_thread_ = std::thread([this]() { main_io_.run(); });
}

void LEDServer::stop() {
  LOG(info) << "Stopping server...";        
  for (auto&& w : workers_) {
    w->guard_.reset();
    w->clients_.clear();
    client_count_ = 0;
    w->thread_.join();
  }
  LOG(debug) << "worker threads joined";      
  accept_sock_.close();
  signals_.cancel();
  main_io_thread_.join();
  LOG(info) << "Server stopped";
}

void LEDServer::post_connection_error(LEDController& client) {
  if (workers_.size() && client_count_) {
    for (auto&& w : workers_) {
      LOG(info) << "Searching for 0x" << std::hex << client.id();
      auto c = std::find(w->clients_.begin(), w->clients_.end(), client);
      if (c != w->clients_.end()) {
	w->clients_.erase(c);
	client_count_--;
	LOG(info) << "Client 0x" << std::hex << client.id() << " disconnected";
	return;
      }
    }
    LOG(error) << client_count_;
    LOG(error) << "Connection not found: 0x" << std::hex << client.id();
  }
}

void LEDServer::post_client_ready() {
  client_ready_count_++;
}

LEDServer::IOThread& LEDServer::io_schedule() {
  assert(workers_.size());
  return **std::min_element(workers_.begin(), workers_.end(),
			    [](const auto& a, const auto& b) {
			      return a->clients_.size() < b->clients_.size();
			    });
}

void LEDServer::accept() {
  auto& io = io_schedule();
  accept_sock_
    .async_accept(io.ctx_,
		  [this, &io](const std::error_code& ec, tcp::socket client_sock) {
		    if (!ec) {
		      LOG(info) << "Connection accepted: "
				<< client_sock.remote_endpoint() << " -> "
				<< client_sock.local_endpoint();
		      io.clients_.emplace_back(*this, std::move(client_sock), io.ctx_);
		      client_count_++;
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

void LEDServer::subscribe_signals()
{
  signals_
    .async_wait([this](const std::error_code& ec,
			    int signal_number)
		     {
		       if (!ec) {
			 LOG(info) << "Received signal " << signal_number;
			 shutdown_ = true;
		       } else {
			 LOG(error) << "Signal listen error: " << ec.message();
		       }
		     });
}

bool LEDServer::clients_ready() {
  return client_ready_count_ >= client_count_;
}

void LEDServer::send_frame(const_buffer buf) {
  for (auto&& w : workers_) {
    for (auto&& c : w->clients_) {
      LOG(debug) << "Sending frame to client id " << std::hex << c.id();
      post(w->ctx_, [&]() { c.send(buf); });
    }
  }
  client_ready_count_ = 0;
}

int main(int argc, char *argv[]) {
  LEDServer server;
  server.start();
  while (!server.is_shutdown()) {
    server.run_effect<Test<144>>(300);
  }
  return 0;
}
