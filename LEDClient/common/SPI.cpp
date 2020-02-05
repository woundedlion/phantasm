#include "SPI.h"
#include "esp_intr_alloc.h"

SPI::SPI() {
	spi_bus_config_t bus_cfg = {};
	bus_cfg.miso_io_num = -1;
	bus_cfg.mosi_io_num = 23;
	bus_cfg.sclk_io_num = 18;
	bus_cfg.quadwp_io_num = -1;
	bus_cfg.quadhd_io_num = -1;
	bus_cfg.max_transfer_sz = 0; // Defaults to 4094
	bus_cfg.intr_flags = ESP_INTR_FLAG_IRAM;
	ERR_THROW(spi_bus_initialize(VSPI_HOST, &bus_cfg, DMA_CHAN));

	spi_device_interface_config_t dev_cfg = {};
	dev_cfg.command_bits = 0;
	dev_cfg.address_bits = 0;
	dev_cfg.dummy_bits = 0;
	dev_cfg.mode = 0;
	dev_cfg.duty_cycle_pos = 128;
	dev_cfg.cs_ena_pretrans = 0;
	dev_cfg.cs_ena_posttrans = 0;
	dev_cfg.clock_speed_hz = 24 * 1000000;
	dev_cfg.input_delay_ns = 0;
	dev_cfg.spics_io_num = -1;
	dev_cfg.flags = 0;
	dev_cfg.queue_size = 1;
	dev_cfg.pre_cb = NULL;
	dev_cfg.post_cb = NULL;
	ERR_THROW(spi_bus_add_device(VSPI_HOST, &dev_cfg, &device_));
	ERR_THROW(spi_device_acquire_bus(device_, portMAX_DELAY));
}

SPI::~SPI() {
	spi_device_release_bus(device_);
	spi_bus_remove_device(device_);
	spi_bus_free(VSPI_HOST);
}