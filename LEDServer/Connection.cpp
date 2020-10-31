#include "Connection.h"

#include <boost/log/trivial.hpp>
#include <iomanip>
#include <sstream>

#include "LEDServer.h"
#define LOG(X) BOOST_LOG_TRIVIAL(X)

using namespace boost::asio;
using namespace boost::asio::ip;

Connection::Connection(LEDServer& server, tcp::socket& sock,
                       std::shared_ptr<IOThread> io)
    : server_(server),
      sock_(std::move(sock)),
      io_(io),
      key_(sock_.remote_endpoint().address().to_v4().to_ulong()),
      ready_(false),
      canceled_(false) {
  read_header();
}

Connection::~Connection() { cancel(); }

void Connection::post_cancel() {
  post(io_->ctx_, [this]() { cancel(); });
}

void Connection::cancel() {
  if (!canceled_) {
    canceled_ = true;
    LOG(info) << "Connection canceled: " << id_str();
    sock_.cancel();
    //    sock_.shutdown(tcp::socket::shutdown_both);
    sock_.close();
  }
}

void Connection::read_header() {
  async_read(sock_, buffer(id_, sizeof(id_)),
             [this](const std::error_code& ec, std::size_t bytes) {
               if (!ec && bytes) {
                 LOG(info) << "Header: ID = " << id_str();
                 set_ready(true);
                 server_.get().post_client_ready(shared_from_this());
               } else if (ec != std::errc::operation_canceled) {
                 LOG(error) << "Read Error: " << ec.message();
                 cancel();
               }
             });
}
void Connection::post_send(const const_buffer& buf,
                           std::function<void()> callback) {
  post(io_->ctx_, [=]() { this->send(buf, callback); });
}

void Connection::send(const const_buffer& buf, std::function<void ()> callback) {
  start_ = std::chrono::steady_clock::now();
  async_write(sock_, buf, [=](const std::error_code& ec, std::size_t bytes) {
    if (!ec) {
      LOG(info) << bytes << " bytes sent to client ID " << id_str() << " in "
                << std::dec
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start_)
                       .count()
                << "ms";
      callback();
    } else if (ec != std::errc::operation_canceled) {
      LOG(error) << "Write Error: " << ec.message();
      cancel();
    }
  });
}

std::string Connection::id_str() const {
  std::stringstream ss;
  ss << std::setfill('0') << std::hex << std::setw(2)
     << static_cast<int>(id_[0]) << "-" << std::setw(2)
     << static_cast<int>(id_[1]) << "-" << std::setw(2)
     << static_cast<int>(id_[2]) << "-" << std::setw(2)
     << static_cast<int>(id_[3]) << "-" << std::setw(2)
     << static_cast<int>(id_[4]) << "-" << std::setw(2)
     << static_cast<int>(id_[5]);
  return ss.str();
}