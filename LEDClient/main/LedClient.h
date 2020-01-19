#pragma once
#include <sstream>
#include "asio.hpp"
#include "App.h"
#include "WifiClient.h"

template <typename T>
std::string to_string(const T& t) {
	std::stringstream ss;
	ss << t;
	return ss.str();
}

class ClientConnection {
public:

	ClientConnection(uint32_t src, uint32_t dst, const std::vector<uint8_t>& id);
	~ClientConnection();
	void connect();
	void send_header();
	void read_frame();
	void send_ready();

private:

	void post_conn_err();

	static const unsigned char READY = 0xab;
	uint32_t src_;
	uint32_t dst_;
	asio::io_context ctx_;
	std::thread io_thread_;
	asio::ip::tcp::endpoint local_ep_;
	asio::ip::tcp::endpoint remote_ep_;
	asio::ip::tcp::socket sock_;
	unsigned char bufs_[2][288 * 144 * 3];
	std::vector<uint8_t> id_;
};

ESP_EVENT_DECLARE_BASE(LED_EVENT);

enum {
	LED_EVENT_CONN_ERR = 10000,
};

class LEDClient : public esp::App {
public:

	LEDClient();
	~LEDClient();
	void start();

private:

	enum State {
		STOPPED,
		READY,
	};

	static const uint16_t LED_TIMER_GROUP = TIMER_GROUP_0;
	static const uint16_t LED_TIMER = TIMER_0;
	static const uint16_t LED_TIMER_DIVIDER = 17361;

	State state_;
	esp::WifiClient wifi_;
	std::unique_ptr<ClientConnection> connection_;
	esp_timer_handle_t connect_timer_;

	static void handle_event(void* arg, esp_event_base_t base, int32_t id, void* data);
	void state_stopped(esp_event_base_t base, int32_t id, void* data);
	void state_ready(esp_event_base_t base, int32_t id, void* data);

	void connect_timer_start();
	static void handle_connect_timer(void *arg);
	void start_led_timer();
	void stop_led_timer();
	static void handle_led_timer_ISR(void *);

	void on_got_ip();
	void on_conn_err();
	void read();

};