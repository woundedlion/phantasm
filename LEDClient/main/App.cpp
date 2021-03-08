#include "App.h"
#include "esp_log.h"

using namespace esp;
namespace {
	const char* TAG = "App";
}

App::App() {
	ERR_THROW(esp_event_loop_create_default());
	ESP_LOGI(TAG, "App Initialized");
}

App::~App() {
	ERR_LOG("esp_event_loop_delete_default", esp_event_loop_delete_default());
	ESP_LOGI(TAG, "App Destroyed");
}
