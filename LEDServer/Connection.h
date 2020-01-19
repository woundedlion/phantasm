#pragma once

#include <chrono>
#include <boost/asio.hpp>

class LEDServer;

class Connection {
public:

  Connection(Connection&& c);
  Connection(LEDServer& server,
		boost::asio::ip::tcp::socket sock,
		boost::asio::io_context& io);
  ~Connection();
  bool operator==(const Connection& c) { return id_ == c.id_; }
  Connection& operator=(Connection&& c);
  const uint8_t* id() { return id_; }
  void send(const boost::asio::const_buffer& buf);
  std::string id_str();
  
private:

  void read_header();
  void read_ready();

  std::reference_wrapper<LEDServer> server_;
  boost::asio::ip::tcp::socket sock_;
  std::reference_wrapper<boost::asio::io_context> io_;
  uint8_t id_[6];
  std::chrono::steady_clock::time_point start_;
};
  
