#pragma once

#include <stdexcept>
#include <iostream>
#include "esp_event.h"
#include "esp_log.h"

#define ERR_THROW(x) do {															\
        esp_err_t rc = (x);															\
        if (rc != ESP_OK) {															\
			throw std::runtime_error(esp_err_to_name(rc));	\
        }																			\
    } while(0);

#define ERR_LOG(t, x) do {															\
        esp_err_t rc = (x);															\
        if (rc != ESP_OK) {															\
			ESP_LOGE(TAG, "%s: %s", (t), esp_err_to_name(rc));						\
        }																			\
    } while(0);

namespace esp {

	class App {
	public:

		App();
		virtual ~App();

	private:

	};
}
