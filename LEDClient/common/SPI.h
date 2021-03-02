#pragma once

#include "App.h"
#include "LEDServer/Types.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"

class SPI {
 public:
  SPI();
  ~SPI();

  template <typename T>
  SPI& operator<<(const T& t);

 private:
  const int DMA_CHAN = 1;
  spi_device_handle_t device_;
};

template <typename T>
inline SPI& IRAM_ATTR SPI::operator<<(const T& t) {
  spi_transaction_t txn = {};
  txn.tx_buffer = t.data();
  txn.length = t.size() * 8;
  ERR_THROW(spi_device_polling_transmit(device_, &txn));
  return *this;
}

template <size_t S>
class APA102Frame {
 public:
  APA102Frame()
      : frame_((SPIFrame*)heap_caps_malloc(sizeof(SPIFrame),
                                           MALLOC_CAP_DMA | MALLOC_CAP_32BIT)) {
    std::fill(*frame_, *frame_ + sizeof(SPIFrame) / 2, 0);
    for (int i = 0; i < S; ++i) {
      *(*frame_ + 4 + i * 4) = 0xff;  // 0xe0 + 5 bits brightness
    }
    // copy black pixels into 2nd slot for background
    std::copy(*frame_, *frame_ + sizeof(SPIFrame) / 2,
              *frame_ + sizeof(SPIFrame) / 2);
  }

  ~APA102Frame() { heap_caps_free(frame_); }

  APA102Frame& IRAM_ATTR load(const RGB* data) {
    auto w = *frame_ + 4;
    for (auto p = data; p < data + S; ++p) {
      *w++ = 0xff;  // 0xe0 + 5 bits brightness
      std::copy(reinterpret_cast<const uint8_t*>(p),
                reinterpret_cast<const uint8_t*>(p + 3), w);
      w += 3;
    }
    return *this;
  }

  constexpr size_t IRAM_ATTR size() const { return sizeof(SPIFrame); }
  const uint8_t* IRAM_ATTR data() const { return *frame_; }

 private:
  typedef uint8_t SPIFrame[2 * (4                            // start frame
                                + (S * 4)                    // LED frames
                                + ((((S + 1) / 2) + 7) / 8)  // end frame
                                )];
  SPIFrame* frame_;
};
