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

LEDServer::LEDServer() :
  shutdown_(false),
  signals_(main_io_, SIGINT, SIGTERM),
  accept_sock_(main_io_, tcp::endpoint(tcp::v4(), 5050))
{

}

LEDServer::~LEDServer() {
  stop();
}

void LEDServer::start() {
  LOG(info) << "Starting server";
  subscribe_signals();
  accept();
  int num_client_io_threads = std::max((uint)1, std::thread::hardware_concurrency() - 1);
  for (int i = 0; i < num_client_io_threads; ++i) {
    workers_.emplace_back(new IOThread());
  }
  main_io_thread_ = std::thread([this]() { main_io_.run(); });
}

void LEDServer::stop() {
  LOG(info) << "Stopping server...";        
  for (auto&& w : workers_) {
    w->guard_.reset();
  }
  for (auto&& w : workers_) { 
    w->thread_.join();
  }
  LOG(debug) << "worker threads joined";      
  accept_sock_.close();
  signals_.cancel();
  main_io_thread_.join();
  LOG(info) << "Server stopped";
}

LEDServer::IOThread& LEDServer::io_schedule() {
  assert(workers_.size());
  return **std::min_element(workers_.begin(), workers_.end(),
			    [](const auto& a, const auto& b) {
			      return a->clients_.size() < b->clients_.size();
			    });
}

void LEDServer::accept() {
  accept_sock_
    .async_accept([this](const std::error_code& ec, tcp::socket client_sock) {
		    if (!ec) {
		      LOG(info) << "Connection accepted: "
				<< client_sock.remote_endpoint() << " -> "
				<< client_sock.local_endpoint();
		      auto& io = io_schedule();
		      io.clients_.emplace_back(std::move(client_sock), io.ctx_);
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

void LEDServer::send_frame(Effect& e)
{}


int main(int argc, char *argv[]) {
  LEDServer server;
  server.start();
  while (!server.is_shutdown()) {
    
  }
  return 0;
}
