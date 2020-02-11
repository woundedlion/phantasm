#include <iostream>
#include "LedClient.h"
#include "esp_log.h"
#include "esp_system.h"
#include "LEDC.h"
#include "freertos/xtensa_timer.h"
#include "freertos/xtensa_api.h"

ESP_EVENT_DEFINE_BASE(LED_EVENT);

using namespace esp;

namespace {
	const char* TAG = "LedClient";
	const char* SERVER_ADDR = "10.10.10.1";
	const int SERVER_PORT = 5050;
	LEDClient led_client;
	DoubleBuffer<RGB, W, STRIP_H> bufs_;
	APA102Frame<STRIP_H> frame_;
//	APA102Frame<STRIP_H> background_;
}

LEDClient::LEDClient() :
	state_(STOPPED),
	x_(0)
{
	esp_timer_create_args_t args;
	args.callback = &LEDClient::handle_connect_timer;
	args.arg = this;
	args.dispatch_method = ESP_TIMER_TASK;
	args.name = "connect_timer";
	ERR_THROW(esp_timer_create(&args, &connect_timer_));
	ERR_THROW(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, LEDClient::handle_event, this));
	ERR_THROW(esp_event_handler_register(LED_EVENT, ESP_EVENT_ANY_ID, LEDClient::handle_event, this));
}

LEDClient::~LEDClient() {
	stop_connect_timer();
	if (led_task_) {
		vTaskDelete(led_task_);
	}
}

void LEDClient::start() {
	wifi_.start();
}

void IRAM_ATTR LEDClient::send_pixels() {
	*spi_ << frame_;
//	*spi_ << background_;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR LEDClient::run_leds(void* arg) {
	auto c = reinterpret_cast<LEDClient*>(arg);
	c->spi_.reset(new SPI());
	SquareWaveGenerator<W * 16, PIN_CLOCK_GEN> clock;
	c->start_gpio();
	ets_isr_mask(1ULL << XT_TIMER_INTNUM);
	while (true) {
		while (0 == (REG_READ(GPIO_IN_REG) & (1ULL << PIN_CLOCK_READ))) {}
		c->send_pixels();
		if (++c->x_ == W) {
			c->x_ = 0;
			esp_event_post(LED_EVENT, LED_EVENT_NEED_FRAME, NULL, 0, portMAX_DELAY);
		}
	}
}

void LEDClient::dump_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
	ESP_LOGI(TAG, "base: %s id: %d", base, id);
}

void LEDClient::handle_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
	try {
		auto handler = static_cast<LEDClient*>(arg);
		assert(base == IP_EVENT || base == LED_EVENT);
		switch (handler->state_) {
		case STOPPED:
			handler->state_stopped(base, id, data);
			break;
		case READY:
			handler->state_ready(base, id, data);
			break;
		case ACTIVE:
			handler->state_active(base, id, data);
			break;
		default:
			assert(false);
		}
	} catch (const std::exception & e) {
		ESP_LOGE(TAG, "Exception in LEDClient state machine: %s", e.what());
	}
}

void LEDClient::state_stopped(esp_event_base_t base, int32_t id, void* data) {
	switch (id) {
	case IP_EVENT_STA_GOT_IP:
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "STOPPED", "READY", id);
		state_ = READY;
		xTaskCreatePinnedToCore(LEDClient::run_leds, "LED_LOOP", 2048, this, configMAX_PRIORITIES - 1, &led_task_, 1);
		on_got_ip();
		break;
	case LED_EVENT_NEED_FRAME:
		// No connection yet, skip frame request
		break;
	default:
		ESP_LOGW(TAG, "Unhandled event id: %d", id);
	}
}

void LEDClient::state_ready(esp_event_base_t base, int32_t id, void* data) {
	switch (id) {
	case IP_EVENT_STA_GOT_IP:
		on_got_ip();
		break;
	case LED_EVENT_CONN_ERR:
		on_conn_err();
		break;
	case LED_EVENT_CONN_ACTIVE:
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "READY", "ACTIVE", id);
		state_ = ACTIVE;
		break;
	case LED_EVENT_NEED_FRAME:
		// No connection yet, skip frame request
		break;
	default:
		ESP_LOGW(TAG, "Unhandled event id: %d", id);
	}
}

void LEDClient::state_active(esp_event_base_t base, int32_t id, void* data) {
	switch (id) {
	case IP_EVENT_STA_GOT_IP:
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "ACTIVE", "READY", id);
		state_ = READY;
		on_got_ip();
		break;
	case LED_EVENT_CONN_ERR:
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "ACTIVE", "READY", id);
		state_ = READY;
		on_conn_err();
		break;
	case LED_EVENT_NEED_FRAME:
		assert(connection_);
		connection_->advance_frame();
		break;
	default:
		ESP_LOGW(TAG, "Unhandled event id: %d", id);
	}
}
void LEDClient::on_got_ip() {
	try {
		ESP_LOGI(TAG, "Resetting client connection on IP change");
		connection_.reset(new ServerConnection(ntohl(wifi_.ip()), ntohl(inet_addr(SERVER_ADDR)), wifi_.mac_address()));
	}
	catch (std::exception& e) {
		ESP_LOGE(TAG, "%s", e.what());
	}
}

void LEDClient::on_conn_err() {
	connection_.reset();
	start_connect_timer();
}

void LEDClient::start_connect_timer() {
	ESP_LOGI(TAG, "Reconnecting in 5 seconds...");
	ERR_THROW(esp_timer_start_once(connect_timer_, 5000000));
}

void LEDClient::stop_connect_timer() {
	ERR_LOG("esp_timer_stop", esp_timer_stop(connect_timer_));
	ERR_LOG("esp_timer_delete", esp_timer_delete(connect_timer_));
}

void LEDClient::handle_connect_timer(void* arg) {
	auto c = static_cast<LEDClient*>(arg);
	c->connection_.reset(new ServerConnection(ntohl(c->wifi_.ip()), ntohl(inet_addr(SERVER_ADDR)), c->wifi_.mac_address()));
}

void LEDClient::start_gpio() {
	try {
		gpio_config_t cfg = {};
		cfg.pin_bit_mask = 1ULL << PIN_CLOCK_READ;
		cfg.mode = GPIO_MODE_INPUT;
		cfg.pull_up_en = GPIO_PULLUP_DISABLE;
		cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
		cfg.intr_type = GPIO_INTR_DISABLE;
		ERR_THROW(gpio_config(&cfg));
	}
	catch (const std::exception& e) {
		ESP_LOGE(TAG, "%s", e.what());
	}
}

void LEDClient::stop_gpio() {
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

ServerConnection::ServerConnection(uint32_t src, uint32_t dst, const std::vector<uint8_t>& mac) :
	src_(src),
	dst_(dst),
	local_ep_(asio::ip::address_v4(src_), 0),
	remote_ep_(asio::ip::address_v4(dst_), SERVER_PORT),
	sock_(ctx_, local_ep_),
	id_(mac),
	read_pending_(false)
{
	connect();
	xTaskCreatePinnedToCore(ServerConnection::run_io, "IO_LOOP", 4096, this, 17, &io_task_, 0);
}

ServerConnection::~ServerConnection() {
	asio::post(ctx_, [this]() {
			sock_.cancel();
			sock_.close();
		});
	vTaskDelete(io_task_);
}

void ServerConnection::run_io(void* arg) {
	auto c = reinterpret_cast<ServerConnection*>(arg);
	c->ctx_.run();
	while (true) {}
}

void ServerConnection::connect() {
	ESP_LOGI(TAG, "Connecting to %s", to_string(remote_ep_).c_str());
	sock_.async_connect(remote_ep_,
		[this](const std::error_code& ec) {
			if (!ec) {
				ESP_LOGI(TAG, "Connection established: %s -> %s", to_string(local_ep_).c_str(), to_string(remote_ep_).c_str());
				send_header();
			} else if (ec != std::errc::operation_canceled) {
				ESP_LOGE(TAG, "Connection error to %s: %s", to_string(remote_ep_).c_str(), ec.message().c_str());
				post_conn_err();
			}
		}
	);
}

void ServerConnection::send_header() {
	ESP_LOGI(TAG, "ID: %02x-%02x-%02x-%02x-%02x-%02x", id_[0], id_[1], id_[2], id_[3], id_[4], id_[5]);
	asio::async_write(sock_, asio::buffer(id_),
		[this](const std::error_code& ec, std::size_t length) {
			if (!ec) {
				ESP_LOGI(TAG, "Sent HELLO: %s -> %s", to_string(local_ep_).c_str(), to_string(remote_ep_).c_str());
				read_frame();
				post_conn_active();
			} else if (ec != std::errc::operation_canceled) {
				ESP_LOGE(TAG, "Write error %s: %s", to_string(remote_ep_).c_str(), ec.message().c_str());
				post_conn_err();
			}
		});
}

void ServerConnection::read_frame() {
	asio::async_read(sock_, asio::buffer((void *)bufs_.back(), bufs_.size()),
		[this](const std::error_code& ec, std::size_t bytes) {
			if (!ec && bytes) {
				bufs_.inc_used();
				read_pending_ = false;
				ESP_LOGI(TAG, "Read complete: %d bytes", bytes);
				read_frame();
			}
			else if (ec != std::errc::operation_canceled) {
				ESP_LOGE(TAG, "Read error: %s", ec.message().c_str());
				post_conn_err();
			}
		});
}

void ServerConnection::advance_frame() {
	if (!read_pending_) {
		bufs_.swap();
		send_ready();
		frame_.load(bufs_.front());
	}
	else {
		ESP_LOGW(TAG, "Frame delayed");
/*
char dump[400];
		vTaskList(dump);
		ESP_LOGE(TAG, "%s", dump);
		*/
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

const unsigned char ServerConnection::READY_MAGIC = 0xab;

void ServerConnection::send_ready() {
	read_pending_ = true;
	asio::async_write(sock_, asio::buffer(&ServerConnection::READY_MAGIC, 1),
		[this](const std::error_code& ec, std::size_t bytes) {
			if (!ec) {
				ESP_LOGI(TAG, "Sent READY");
			}
			else if (ec != std::errc::operation_canceled) {
				ESP_LOGE(TAG, "Write error %s: %s", to_string(remote_ep_).c_str(), ec.message().c_str());
				post_conn_err();
			}
		});
}

void ServerConnection::post_conn_err() {
	ERR_THROW(esp_event_post(LED_EVENT, LED_EVENT_CONN_ERR, NULL, 0, 0));
}

void ServerConnection::post_conn_active() {
	ERR_THROW(esp_event_post(LED_EVENT, LED_EVENT_CONN_ACTIVE, NULL, 0, 0));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" void app_main(void)
{
	try {
		led_client.start();
	}
	catch (const std::exception& e) {
		ESP_LOGE(TAG, "%s", e.what());
	}
}