#include <sstream>
#include <iomanip>

#include "Connection.h"
#include "LEDServer.h"
#include <boost/log/trivial.hpp>
#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;


Connection::Connection(LEDServer& server,
		       tcp::socket& sock,
		       std::shared_ptr<IOThread> io) :
  server_(server),
  sock_(std::move(sock)),
  io_(io),
  key_(sock_.remote_endpoint().address().to_v4().to_ulong()),
  ready_(true),
  canceled_(false)
{
  read_header();
}

Connection::~Connection() {
  assert(canceled_);

}

void Connection::post_cancel() {
  post(io_->ctx_, [this]() { cancel(); });
}

void Connection::cancel() {
  if (!canceled_) {
    canceled_ = true;
    sock_.cancel();
    sock_.shutdown(tcp::socket::shutdown_both);
    sock_.close();
    LOG(info) << "Connection canceled: " << key();
    server_.get().post_connection_error(shared_from_this());
  }
}

void Connection::read_header() {
  async_read(sock_, buffer(id_, sizeof(id_)),
	     [this](const std::error_code& ec, std::size_t bytes) {
	       if (!ec && bytes) {
		 LOG(info) << "Header: ID = " << id_str();
		 read_ready();
	       } else if (ec != std::errc::operation_canceled) {
		 LOG(error) << "Read Error: " << ec.message();
		 cancel();
	       }
	     });
}

void Connection::read_ready() {
  async_read(sock_, buffer(&ready_, sizeof(ready_)),
	     [this](const std::error_code& ec, std::size_t bytes) {
	       if (!ec && bytes) {
		 auto now = std::chrono::steady_clock::now();
		 LOG(debug) << "Received READY from client: " << id_str();
		 LOG(error) <<  "Last READY: " << std::chrono::duration_cast<
		   std::chrono::milliseconds>(now - last_ready_).count() << "ms";
		 last_ready_ = now;
		 server_.get().post_client_ready(shared_from_this());
		 read_ready();
	       } else if (ec != std::errc::operation_canceled) {
		 LOG(error) << "Read Error: " << ec.message();
		 cancel();
	       }
	     });
}

void Connection::send(const const_buffer& buf) {
  start_ = std::chrono::steady_clock::now();
  async_write(sock_, buf,
	      [this](const std::error_code& ec, std::size_t bytes) {
		if (!ec) {
		  LOG(info) << bytes << " bytes sent to client ID " << id_str()
			    << " in " << std::dec
			    << std::chrono::duration_cast<
			      std::chrono::milliseconds>(std::chrono::steady_clock::now()
							 - start_).count() << "ms";
		} else if (ec != std::errc::operation_canceled) {
		  LOG(error) << "Write Error: " << ec.message();
		  cancel();
		}
	      });  
}

std::string Connection::id_str() const {
  std::stringstream ss;
  ss << std::setfill('0') << std::hex
     << std::setw(2) << static_cast<int>(id_[0]) << "-"
     << std::setw(2) << static_cast<int>(id_[1]) << "-"
     << std::setw(2) << static_cast<int>(id_[2]) << "-"
     << std::setw(2) << static_cast<int>(id_[3]) << "-"
     << std::setw(2) << static_cast<int>(id_[4]) << "-"
     << std::setw(2) << static_cast<int>(id_[5]);
  return ss.str();
}

boost::asio::io_context& Connection::ctx() {
  return io_->ctx_;
}
