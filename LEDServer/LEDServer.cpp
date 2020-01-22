#include "LEDServer.h"
#include <boost/log/trivial.hpp>
#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;

namespace {
  const char* TAG = "LEDServer";
}

IOThread::IOThread() :
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
  accept_sock_(main_io_, tcp::endpoint(tcp::v4(), 5050))
{}

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
  for (auto&& [k, c] : clients_) {
    c->cancel();
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

void LEDServer::post_connection_error(Connection& client) {
  post(main_io_,
       [this, &client](){
	 if (clients_.erase(client.key())) {
	   LOG(info) << "Client " << client.id_str() << " disconnected";
	 } else {
	   LOG(error) << "Connection not found: " << client.key();
	 }
       });
}

void LEDServer::post_client_ready(Connection& client) {
  post(main_io_, [this, &client](){
		   client.set_ready(true);
		   if (std::all_of(clients_.begin(), clients_.end(),
				   [](auto&& c) { return c.second->ready(); })) {
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
  accept_sock_
    .async_accept(io->ctx_,
		  [this, io](const std::error_code& ec, tcp::socket client_sock) {
		    if (!ec) {
		      LOG(info) << "Connection accepted: "
				<< client_sock.remote_endpoint() << " -> "
				<< client_sock.local_endpoint();
		      auto c = std::make_unique<Connection>(*this, client_sock, io);
		      auto r = clients_.insert_or_assign(c->key(), std::move(c));
		      if (!r.second) {
			LOG(info) << "Replaced connection for client at :"
				  << c->key();
		      }
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
			 if (effect_) effect_->cancel();
		       } else {
			 LOG(error) << "Signal listen error: " << ec.message();
		       }
		     });
}

void LEDServer::send_frame() {
  effect_->wait_frame_available();
  for (auto&& [k, c] : clients_) {
    LOG(debug) << "Sending frame " << effect_->frame_count()
	       << " to client id " << c->id_str();
    c->set_ready(false);
    post(c->ctx(), [&]() { c->send(effect_->buf()); });
  }
  effect_->advance_frame();
}

int main(int argc, char *argv[]) {
  LEDServer server;
  server.start();
  while (!server.is_shutdown()) {
    server.run_effect<Test>(300);
  }
  return 0;
}
