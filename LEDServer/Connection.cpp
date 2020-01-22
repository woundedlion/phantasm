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
  ready_(true)
{
  read_header();
}

Connection::Connection(Connection&& c) :
  server_(c.server_),
  sock_(std::move(c.sock_)),
  io_(std::move(c.io_))
{}

Connection::~Connection() {
  sock_.shutdown(tcp::socket::shutdown_both);
  sock_.close();
}

Connection& Connection::operator=(Connection&& c) {
  server_ = c.server_;
  sock_ = std::move(c.sock_);
  io_ = std::move(c.io_);
  return *this;
}

void Connection::read_header() {
  async_read(sock_, buffer(id_, sizeof(id_)),
	     [this](const std::error_code& ec, std::size_t bytes) {
	       if (!ec && bytes) {
		 LOG(info) << "Header: ID = " << id_str();
		 read_ready();
	       } else {
		 LOG(error) << "Read Error: " << ec.message();
		 server_.get().post_connection_error(*this);
	       }
	     });
}

void Connection::read_ready() {
  unsigned char ready;
  async_read(sock_, buffer(&ready, sizeof(ready)),
	     [this](const std::error_code& ec, std::size_t bytes) {
	       if (!ec && bytes) {
		 LOG(debug) << "Received READY from client: " << id_str();
		 server_.get().post_client_ready(*this);
	       } else {
		 LOG(error) << "Read Error: " << ec.message();
		 server_.get().post_connection_error(*this);		 
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
		} else {
		  LOG(error) << "Write Error: " << ec.message();
		  server_.get().post_connection_error(*this);
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
