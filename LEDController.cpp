#include "LEDController.h"
#include <boost/log/trivial.hpp>
#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;


LEDController::LEDController(tcp::socket sock, io_context& io) :
  sock_(std::move(sock)),
  io_(io)
{
  read_header();
}

LEDController::LEDController(LEDController&& c) :
  sock_(std::move(c.sock_)),
  io_(c.io_)
{}

LEDController::~LEDController() {
  sock_.close();
}

void LEDController::read_header() {
  async_read(sock_, buffer(&id_, sizeof(id_)),
	     [this](const std::error_code& ec, std::size_t bytes) {
	       if (!ec) {
		 LOG(info) << "Header: ID = 0x" << std::hex << ntohl(id_);
	       } else {
		 LOG(error) << "Read Error: " << ec.message();
		 // TODO: Tear down connection?
	       }
	     });
}

void LEDController::send_frame(const const_buffer& buf) {
    async_write(sock_, buf,
	      [this](const std::error_code& ec, std::size_t bytes) {
		if (!ec) {
		  LOG(info) << bytes << " sent to " << std::hex << ntohl(id_);
		} else {
		  LOG(error) << "Write Error: " << ec.message();
		  // TODO: Tear down connection?
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
		  // TODO: Tear down connection?
       		}
	      });  
}

