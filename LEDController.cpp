#include <sstream>
#include <iomanip>

#include "LEDController.h"
#include "LEDServer.h"
#include <boost/log/trivial.hpp>
#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;


LEDController::LEDController(LEDServer& server, tcp::socket sock, io_context& io) :
  server_(server),
  sock_(std::move(sock)),
  io_(io)
{
  read_header();
}

LEDController::LEDController(LEDController&& c) :
  server_(c.server_),
  sock_(std::move(c.sock_)),
  io_(c.io_)
{}

LEDController::~LEDController() {
  sock_.close();
}

LEDController& LEDController::operator=(LEDController&& c) {
  server_ = c.server_;
  sock_ = std::move(c.sock_);
  io_ = c.io_;
  return *this;
}

void LEDController::read_header() {
  async_read(sock_, buffer(id_, sizeof(id_)),
	     [this](const std::error_code& ec, std::size_t bytes) {
	       if (!ec && bytes) {
		 LOG(info) << "Header: ID = " << id_str();
		 read_header();
	       } else {
		 LOG(error) << "Read Error: " << ec.message();
		 server_.get().post_connection_error(*this);
	       }
	     });
}

void LEDController::send(const const_buffer& buf) {
  start_ = std::chrono::steady_clock::now();
  async_write(sock_, buf,
	      [this](const std::error_code& ec, std::size_t bytes) {
		if (!ec) {
		  LOG(info) << bytes << " bytes sent to client ID " << id_str()
			    << " in " << std::dec
			    << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count() << "ms";
		} else {
		  LOG(error) << "Write Error: " << ec.message();
		  server_.get().post_connection_error(*this);
		}
	      });
  
}

void LEDController::send_sync() { 
  async_write(sock_, buffer("SYNC", 4),
	      [this](const std::error_code& ec, std::size_t bytes) {
		if (!ec) {
		  LOG(info) << "SYNC sent to " << id_str();
		} else {
		  LOG(error) << "Write Error: " << ec.message();
		  server_.get().post_connection_error(*this);
       		}
	      });  
}

std::string LEDController::id_str() {
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
