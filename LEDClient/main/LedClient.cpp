#include <iostream>
#include <thread>
#include "LedClient.h"
#include "esp_log.h"
#include "esp_system.h"

ESP_EVENT_DEFINE_BASE(LED_EVENT);

using namespace esp;

namespace {
	const char* TAG = "LedClient";
	const char* SERVER_ADDR = "192.168.0.200";
	const int SERVER_PORT = 5050;
	LEDClient app;
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

	esp_event_loop_args_t led_loop_cfg = {};
	led_loop_cfg.queue_size = 1;
	led_loop_cfg.task_name = "LED";
	led_loop_cfg.task_priority = 1;
	led_loop_cfg.task_stack_size = 2048;
	led_loop_cfg.task_core_id = 1;
	ERR_THROW(esp_event_loop_create(&led_loop_cfg, &led_loop_));
	ERR_THROW(esp_event_handler_register_with(
		led_loop_, LED_EVENT, LED_EVENT_START_TIMER, LEDClient::handle_led_event, this));
	ERR_THROW(esp_event_handler_register_with(
		led_loop_, LED_EVENT, LED_EVENT_STOP_TIMER, LEDClient::handle_led_event, this));
}

LEDClient::~LEDClient() {
	ERR_LOG("esp_event_handler_unregister", esp_event_handler_unregister_with(
		led_loop_, LED_EVENT, LED_EVENT_START_TIMER, LEDClient::handle_led_event));
	ERR_LOG("esp_event_handler_unregister", esp_event_handler_unregister_with(
		led_loop_, LED_EVENT, LED_EVENT_STOP_TIMER, LEDClient::handle_led_event));
	ERR_LOG("esp_event_loop_delete", esp_event_loop_delete(led_loop_));
	ERR_LOG("esp_timer_stop", esp_timer_stop(connect_timer_));
	ERR_LOG("esp_timer_delete", esp_timer_delete(connect_timer_));
	// TODO: Stop LED timer on core 1
}

void LEDClient::start() {
	wifi_.start();
}

void LEDClient::send_pixels() {
	if (connection_) {
		spi_ << APA102Frame<STRIP_H>(connection_->get_frame());
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LEDClient::handle_led_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
	auto handler = static_cast<LEDClient*>(arg);
	assert(base == LED_EVENT);
	switch (id) {
	case LED_EVENT_START_TIMER:
		handler->start_led_timer();
		break;
	case LED_EVENT_STOP_TIMER:
		handler->stop_led_timer();
		break;
	case LED_EVENT_SHOW:
		handler->send_pixels();
		break;
	default:
		assert(false);
	}
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
		on_got_ip();
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
		ESP_LOGI(TAG, "Connection active, starting LED timer");
		ERR_THROW(esp_event_post_to(led_loop_, LED_EVENT, LED_EVENT_START_TIMER, NULL, 0, portMAX_DELAY));
		break;
	case LED_EVENT_NEED_FRAME:
		if (connection_) {
			connection_->advance_frame();
		}
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
	connect_timer_start();
}

void LEDClient::connect_timer_start() {
	ESP_LOGI(TAG, "Reconnecting in 5 seconds...");
	ERR_THROW(esp_timer_start_once(connect_timer_, 5000000));
}
	
void LEDClient::handle_connect_timer(void* arg) {
	auto c = static_cast<LEDClient*>(arg);
	c->connection_.reset(new ServerConnection(ntohl(c->wifi_.ip()), ntohl(inet_addr(SERVER_ADDR)), c->wifi_.mac_address()));
}

void LEDClient::start_led_timer() {
	timer_config_t cfg;
	cfg.alarm_en = TIMER_ALARM_EN;
	cfg.counter_en = TIMER_PAUSE;
	cfg.intr_type = TIMER_INTR_LEVEL;
	cfg.counter_dir = TIMER_COUNT_UP;
	cfg.auto_reload = TIMER_AUTORELOAD_EN;
	cfg.divider = LED_TIMER_DIVIDER;

	ERR_THROW(timer_init(LED_TIMER_GROUP, LED_TIMER, &cfg));
	ERR_THROW(timer_set_counter_value(LED_TIMER_GROUP, LED_TIMER, 0));
	ERR_THROW(timer_set_alarm_value(LED_TIMER_GROUP, LED_TIMER, 1));
	ERR_THROW(timer_enable_intr(LED_TIMER_GROUP, LED_TIMER));
	ERR_THROW(timer_isr_register(LED_TIMER_GROUP, LED_TIMER, 
		LEDClient::handle_led_timer_ISR, this, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3, NULL));
	ERR_THROW(timer_start(LED_TIMER_GROUP, LED_TIMER));
	ESP_LOGI(TAG, "LED Timer started on core %d", xPortGetCoreID());
}

void LEDClient::stop_led_timer() {

	timer_deinit(LED_TIMER_GROUP, LED_TIMER);
}

void IRAM_ATTR LEDClient::handle_led_timer_ISR(void* arg) {
	timer_spinlock_take(LED_TIMER_GROUP);
	timer_group_clr_intr_status_in_isr(LED_TIMER_GROUP, LED_TIMER);
	timer_group_enable_alarm_in_isr(LED_TIMER_GROUP, LED_TIMER);
	auto c = reinterpret_cast<LEDClient *>(arg);
	c->send_pixels();
	//esp_event_post_to(c->led_loop_, LED_EVENT, LED_EVENT_SHOW, NULL, 0, portMAX_DELAY);
	if (++c->x_ == W) {
		c->x_ = 0;
		esp_event_post(LED_EVENT, LED_EVENT_NEED_FRAME, NULL, 0, portMAX_DELAY);
	}
	timer_spinlock_give(LED_TIMER_GROUP);
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
	io_thread_ = std::thread([this]() { 
		ctx_.run();
		ESP_LOGI(TAG, "IO thread exiting");
		});
}

ServerConnection::~ServerConnection() {
	asio::post(ctx_, [this]() {
			sock_.cancel();
			sock_.close();
		});
	io_thread_.join();
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
				ESP_LOGI(TAG, "Read complete: %d bytes", bytes);
				read_pending_ = false;
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
	}
	else {
		ESP_LOGW(TAG, "Frame delayed");
	}
}

RGB* ServerConnection::get_frame() {
	return bufs_.front();
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
		app.start();
	}
	catch (const std::exception& e) {
		ESP_LOGE(TAG, "%s", e.what());
	}
}