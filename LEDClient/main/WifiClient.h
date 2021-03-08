#pragma once

#include <vector>
#include "esp_event.h"
#include "esp_timer.h"

namespace esp {

	class WifiClient {
	public:


		WifiClient();
		~WifiClient();
		void start();
		uint32_t ip() { return netinfo_.ip.addr; }
		uint32_t gateway() { return netinfo_.gw.addr; }
		std::vector<uint8_t> mac_address();

	private:

		enum State {
			STOPPED,
			DISCONNECTED,
			CONNECTED, 
			ACTIVE,
		};

		State state_;
		esp_netif_t* netif_;
		esp_netif_ip_info_t netinfo_;
		esp_timer_handle_t connect_timer_;

		void connect();
		void connect_timer_start();
		static void handle_connect_timer(void *arg);
		static void handle_csi(void* ctx, wifi_csi_info_t* data);
		static void handle_event(void* arg, esp_event_base_t base, int32_t id, void* data);
		void state_stopped(void * data, int32_t id);
		void state_disconnected(void* data, int32_t id);
		void state_connected(void* data, int32_t id);
		void state_active(void* data, int32_t id);
	};
}