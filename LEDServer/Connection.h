#pragma once

#include <chrono>
#include <boost/asio.hpp>

class LEDServer;
class IOThread;

class Connection {
public:

  typedef uint8_t id_t[6];
  typedef unsigned long key_t;
    
  Connection(Connection&& c);
  Connection(LEDServer& server,
	     boost::asio::ip::tcp::socket& sock,
	     std::shared_ptr<IOThread>);
  ~Connection();
  bool operator==(const Connection& c) { return id_ == c.id_; }
  Connection& operator=(Connection&& c);
  void set_ready(bool ready) { ready_ = ready; }
  bool ready() const { return ready_; }
  void send(const boost::asio::const_buffer& buf);

  key_t key() const { return sock_.remote_endpoint().address().to_v4().to_ulong(); }
  std::string id_str() const;
  boost::asio::io_context& ctx();
  
private:

  void read_header();
  void read_ready();

  std::reference_wrapper<LEDServer> server_;
  boost::asio::ip::tcp::socket sock_;
  std::shared_ptr<IOThread> io_;
  id_t id_;
  std::chrono::steady_clock::time_point start_;
  bool ready_;
};
  
