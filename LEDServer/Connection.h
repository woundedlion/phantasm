#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include "Types.h"
#include "FrameBuffer.h"

class LEDServer;
class IOThread;

class Connection : public std::enable_shared_from_this<Connection> {
 public:
  typedef uint8_t id_t[6];
  typedef unsigned long key_t;

  Connection(LEDServer& server, boost::asio::ip::tcp::socket& sock,
             std::shared_ptr<IOThread>);
  ~Connection();
  Connection(Connection&& c) = delete;
  Connection& operator=(Connection&& c) = delete;
  void read_header();

  void post_cancel();
  void set_ready(bool ready) { ready_ = ready; }
  bool ready() const { return ready_; }
  void start_send(RGBFrameBuffer& frames_, int slice_idx);
  key_t key() const { return key_; }
  std::string id_str() const;

 private:
  void send(RGBFrameBuffer& frames);
  void cancel();

  std::reference_wrapper<LEDServer> server_;
  boost::asio::ip::tcp::socket sock_;
  uint64_t frame_num_;
  std::shared_ptr<IOThread> io_;
  id_t id_;
  key_t key_;
  int slice_idx_;
  bool ready_;
  bool canceled_;
};
