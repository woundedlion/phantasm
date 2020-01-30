#include <cassert>
#include <cstring>
extern "C" {
	#include "esp_netif.h"
	#include "esp_wifi.h"
	#include "nvs_flash.h"
}
#include "WifiClient.h"
#include "App.h"

using namespace esp;

namespace {
	const char* TAG = "WifiClient";
}

WifiClient::WifiClient() :
state_(STOPPED),
netif_(NULL)
{
	esp_timer_create_args_t args;
	args.callback = &WifiClient::handle_connect_timer;
	args.arg = this;
	args.dispatch_method = ESP_TIMER_TASK;
	args.name = "connect_timer";
	ERR_THROW(esp_timer_create(&args, &connect_timer_));
	ERR_THROW(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, WifiClient::handle_event, this));
	ERR_THROW(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, WifiClient::handle_event, this));
}

WifiClient::~WifiClient() {
	esp_timer_stop(connect_timer_);
	ERR_LOG("esp_timer_delete", esp_timer_delete(connect_timer_));
	ERR_LOG("esp_netif_deinit", esp_netif_deinit());
}

void WifiClient::start() {
	ERR_THROW(nvs_flash_init());
	ERR_THROW(esp_netif_init());
	netif_ = esp_netif_create_default_wifi_sta();

	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
	ERR_THROW(esp_wifi_init(&init_cfg));


	ERR_THROW(esp_wifi_set_ps(WIFI_PS_NONE));
	ERR_THROW(esp_wifi_set_mode(WIFI_MODE_STA));

	auto wifi_cfg = wifi_config_t();
	strcpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid), "DEATHVALLEY");
	strcpy(reinterpret_cast<char*>(wifi_cfg.sta.password), "iliketheas");
	ERR_THROW(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));

	ERR_THROW(esp_wifi_start());
}

void WifiClient::handle_csi(void* ctx, wifi_csi_info_t* data) {
	ESP_LOGI(TAG, "sig: %d rssi: %d rate: %d", data->rx_ctrl.sig_mode, data->rx_ctrl.rssi, data->rx_ctrl.rate);
}

std::vector<uint8_t> WifiClient::mac_address() {
	uint8_t mac[6];
	ERR_THROW(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
	ESP_LOGI(TAG, "MAC: %02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return std::vector<uint8_t>(mac, mac + sizeof(mac));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void WifiClient::handle_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
	auto handler = static_cast<WifiClient*>(arg);
	assert(base == WIFI_EVENT || base == IP_EVENT);
	switch (handler->state_) {
	case STOPPED:
		handler->state_stopped(data, id);
		break;
	case DISCONNECTED:
		handler->state_disconnected(data, id);
		break;
	case CONNECTED:
		handler->state_connected(data, id);
		break;
	case ACTIVE:
		handler->state_active(data, id);
		break;
	default:
		assert(false);
	}
}

void WifiClient::state_stopped(void* data, int32_t id) {
	switch (id) {
	case WIFI_EVENT_STA_START:
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "STOPPED", "DISCONNECTED", id);
		state_ = DISCONNECTED;
		connect();
		break;
	default:
		ESP_LOGW(TAG, "Unhandled event id: %d", id);
	}
}

void WifiClient::state_disconnected(void* data, int32_t id) {
	switch (id) {
	case WIFI_EVENT_STA_CONNECTED:
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "DISCONNECTED", "CONNECTED", id);
		state_ = CONNECTED;
		break;
	case WIFI_EVENT_STA_DISCONNECTED:
		ESP_LOGE(TAG, "Wifi connect failed: %s %d", 
			reinterpret_cast<wifi_event_sta_disconnected_t*>(data)->ssid,
			reinterpret_cast<wifi_event_sta_disconnected_t *>(data)->reason);
		connect_timer_start();
		break;
	default:
		ESP_LOGW(TAG, "Unhandled event id: %d", id);
	}
}

void WifiClient::state_connected(void* data, int32_t id) {
	switch (id) {
	case IP_EVENT_STA_GOT_IP:
	{
		auto d = reinterpret_cast<ip_event_got_ip_t*>(data);
		assert(d->esp_netif == netif_);
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "CONNECTED", "ACTIVE", id);
		state_ = ACTIVE;
		netinfo_ = d->ip_info;
		break;
	}
	case WIFI_EVENT_STA_DISCONNECTED:
		ESP_LOGE(TAG, "Wifi disconnected: %s %d",
			reinterpret_cast<wifi_event_sta_disconnected_t*>(data)->ssid,
			reinterpret_cast<wifi_event_sta_disconnected_t*>(data)->reason);
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "CONNECTED", "DISCONNECTED", id);
		state_ = DISCONNECTED;
		connect_timer_start();
		break;
	default:
		ESP_LOGW(TAG, "Unhandled event id: %d", id);
	}
}

void WifiClient::state_active(void* data, int32_t id) {
	switch (id) {
	case IP_EVENT_STA_GOT_IP:
	{
		auto d = reinterpret_cast<ip_event_got_ip_t*>(data);
		assert(d->esp_netif == netif_);
		netinfo_ = d->ip_info;
		break;
	}
	case WIFI_EVENT_STA_DISCONNECTED:
		ESP_LOGE(TAG, "Wifi disconnected: %s %d",
			reinterpret_cast<wifi_event_sta_disconnected_t*>(data)->ssid,
			reinterpret_cast<wifi_event_sta_disconnected_t*>(data)->reason);
		ESP_LOGI(TAG, "State transition: %s -> %s on %d", "ACTIVE", "DISCONNECTED", id);
		state_ = DISCONNECTED;
		connect_timer_start();
		break;
	default:
		ESP_LOGW(TAG, "Unhandled event id: %d", id);
	}
}

void WifiClient::connect() {
	ESP_LOGI(TAG, "Initiating Wifi connection");
	ERR_THROW(esp_wifi_connect());
}

void WifiClient::connect_timer_start() {
	ESP_LOGI(TAG, "Reconnecting to Wifi in 5 seconds...");
	ERR_THROW(esp_timer_start_once(connect_timer_, 5000000));
}

void WifiClient::handle_connect_timer(void* arg) {
	auto wifi = static_cast<WifiClient*>(arg);
	wifi->connect();
}
