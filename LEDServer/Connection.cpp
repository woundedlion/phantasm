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
      frame_num_(0),
      slice_idx_(0),
      io_(io),
      key_(sock_.remote_endpoint().address().to_v4().to_ulong()),
      ready_(false),
      canceled_(false) {}

Connection::~Connection() { cancel(); }

void Connection::post_cancel() {
  post(io_->ctx_, [this]() { cancel(); });
}

void Connection::cancel() {
  if (!canceled_) {
    canceled_ = true;
    LOG(info) << "Connection canceled: " << id_str();
    sock_.shutdown(tcp::socket::shutdown_both);
    sock_.cancel();
    sock_.close();
  }
}

void Connection::read_header() {
  async_read(sock_, buffer(id_, sizeof(id_)),
             [this](const std::error_code& ec, std::size_t bytes) {
               if (!ec && bytes) {
                 LOG(info) << "Header: ID = " << id_str();
                 server_.get().post_client_ready(shared_from_this());
               } else if (ec != std::errc::operation_canceled) {
                 LOG(error) << "Read Error: " << ec.message();
                 cancel();
               }
             });
}
void Connection::start_send(RGBFrameBuffer& frames, int slice_idx) {
  slice_idx_ = slice_idx;
  post(io_->ctx_, [&frames, this]() { this->send(frames); });
}

void Connection::send(RGBFrameBuffer& frames) {
  while (!canceled_) {
    auto frame = frames.pop(frame_num_);
    auto slice = frame->slice_data(slice_idx_);
    try {
      auto bytes = write(sock_, slice);
      LOG(info) << "Frame " << frame_num_ << " sent to client ID " << id_str()
                << " [" << bytes << " bytes]";
      ++frame_num_;
    } catch (boost::system::system_error& e) {
      if (e.code() != std::errc::operation_canceled) {
        LOG(error) << "Write Error: " << e.what();
      }
      cancel();
    }
  }
  /*
  async_write(sock_, slice,
              [&frames, frame, this](const std::error_code& ec, std::size_t
  bytes) { if (!ec) { LOG(info) << "Frame " << frame_num_ << " sent to client
  ID "
                               << id_str() << " [" << bytes << " bytes]";
                  ++frame_num_;
                  this->send(frames);
                } else if (ec != std::errc::operation_canceled) {
                  LOG(error) << "Write Error: " << ec.message();
                  cancel();
                }
              });
  */
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
