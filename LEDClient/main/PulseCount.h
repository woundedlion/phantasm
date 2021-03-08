#pragma once

#include "App.h"

class PulseCount {
public:
	PulseCount() {
        pcnt_config_t cfg = {};
        cfg.pulse_gpio_num = PCNT_GPIO;             /*!< Pulse input GPIO number, if you want to use GPIO16, enter pulse_gpio_num = 16, a negative value will be ignored */
        cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;              /*!< Control signal input GPIO number, a negative value will be ignored */
        cfg.lctrl_mode = PCNT_MODE_KEEP;    /*!< PCNT low control mode */
        cfg.hctrl_mode = PCNT_MODE_KEEP;    /*!< PCNT high control mode */
        cfg.pos_mode = PCNT_COUNT_INC;     /*!< PCNT positive edge count mode */
        cfg.neg_mode = PCNT_COUNT_DIS;     /*!< PCNT negative edge count mode */
        cfg.counter_h_lim = 288;          /*!< Maximum counter value */
        cfg.counter_l_lim = 0;          /*!< Minimum counter value */
        cfg.unit = PCNT_UNIT;               /*!< PCNT unit number */
        cfg.channel = PCNT_CHANNEL_0;         /*!< the PCNT channel */

        ERR_THROW(pcnt_unit_config(&cfg));
        ERR_THROW(pcnt_isr_handler_add(pcnt_unit_t unit, void (*isr_handler)(void*), void* args, ));
        ERR_THROW(pcnt_isr_service_install(ESP_INTR_FLAG_IRAM));
        ERR_THROW(pcnt_isr_handler_add(PCNT_UNIT, PulseCount::handle_event, this));
        ERR_THROW(pcnt_intr_enable(PCNT_UNIT));
	}

    ~PulseCount() {
        pcnt_isr_handler_remove(PCNT_UNIT);
        pcnt_isr_service_uninstall();
    }

private:

    static void handle_event(void* arg) {
        auto pc = reinterpret_cast<PulseCount*>(arg);

        pcnt_counter_clear(PCNT_UNIT);
    }

    int PCNT_GPIO = 15;
    int PCNT_UNIT = 0;

    pcnt_isr_handle_t handle_;
};