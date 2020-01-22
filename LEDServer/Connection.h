#pragma once

#include <chrono>
#include <memory>
#include <boost/asio.hpp>

class LEDServer;
class IOThread;

class Connection : public std::enable_shared_from_this<Connection> {
public:

  typedef uint8_t id_t[6];
  typedef unsigned long key_t;
    
  Connection(LEDServer& server,
	     boost::asio::ip::tcp::socket& sock,
	     std::shared_ptr<IOThread>);
  ~Connection();
  Connection(Connection&& c) = delete;
  Connection& operator=(Connection&& c) = delete;

  void post_cancel();
  void set_ready(bool ready) { ready_ = ready; }
  bool ready() const { return ready_; }
  void send(const boost::asio::const_buffer& buf);

  key_t key() const { return key_; }
  std::string id_str() const;
  boost::asio::io_context& ctx();
  
private:

  void read_header();
  void read_ready();

  std::reference_wrapper<LEDServer> server_;
  boost::asio::ip::tcp::socket sock_;
  std::shared_ptr<IOThread> io_;
  id_t id_;
  key_t key_;
  std::chrono::steady_clock::time_point start_;
  bool ready_;
  bool canceled_;
};
  
