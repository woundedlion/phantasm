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
  async_read(sock_, buffer(&id_, sizeof(id_)),
	     [this](const std::error_code& ec, std::size_t bytes) {
	       if (!ec && bytes) {
		 id_ = ntohl(id_);
		 LOG(info) << "Header: ID = 0x" << std::hex << id_;
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
		  LOG(info) << bytes << " bytes sent to client ID " << std::hex << ntohl(id_)
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
		  LOG(info) << "SYNC sent to " << std::hex << ntohl(id_);
		} else {
		  LOG(error) << "Write Error: " << ec.message();
		  server_.get().post_connection_error(*this);
       		}
	      });  
}

