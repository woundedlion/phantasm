#pragma once

#include "App.h"
#include "driver/spi_master.h"
#include "LEDServer/Types.h"

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
	APA102Frame() {
		auto w = frame_;
		std::fill(w, w + S * 4, 0);
		w += 4 + S * 4;
		std::fill(w, frame_ + size() / 2, 1);
		w = frame_ + size() / 2;
		std::copy(frame_, w, w);
	}

	APA102Frame& IRAM_ATTR load(const RGB* data) {
		auto w = frame_ + 4;
		for (auto p = data; p < data + S; ++p) {
			*w++ = 0xff; // 0xe0 + 5 bits brightness 
			*w++ = p->b_;
			*w++ = p->g_;
			*w++ = p->r_;
		}
		return *this;
	}

	constexpr size_t IRAM_ATTR size() const { return sizeof(frame_); }
	const uint8_t* IRAM_ATTR data() const { return frame_; }

private:

	uint8_t frame_[ 2 * (
		4 // start frame
		+ (S * 4) // LED frames
		+ ((((S + 1) / 2) + 7) / 8) // end frame
	)];
};