#pragma once

#include "driver/ledc.h"

template <size_t HZ, int PIN>
class SquareWaveGenerator {
public:

    SquareWaveGenerator() {
        ledc_timer_config_t timer_cfg = {};
        timer_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
        timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
        timer_cfg.timer_num = LEDC_TIMER_0;
        timer_cfg.freq_hz = HZ;
        timer_cfg.clk_cfg = LEDC_AUTO_CLK;
        ERR_THROW(ledc_timer_config(&timer_cfg));

        ledc_channel_config_t channel_cfg = {};
        channel_cfg.gpio_num = PIN;
        channel_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
        channel_cfg.channel = LEDC_CHANNEL_0;
        channel_cfg.intr_type = LEDC_INTR_DISABLE;
        channel_cfg.timer_sel = LEDC_TIMER_0;
        channel_cfg.duty = 128;
        channel_cfg.hpoint = 0;
        ERR_THROW(ledc_channel_config(&channel_cfg));
	}

    ~SquareWaveGenerator() {
        ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
    }
};