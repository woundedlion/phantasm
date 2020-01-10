#include "LEDController.h"

LEDController::LEDController(boost::asio::ip::tcp::socket sock) :
  sock_(std::move(sock))
{
  read_header();
}

LEDController::~LEDController() {
  sock_.close();
}

void LEDController::read_header() {

}
