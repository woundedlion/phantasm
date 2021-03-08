file(GLOB srcs *.cpp *.cc)
idf_component_register(SRCS ${srcs} 
			REQUIRES esp_netif asio nvs_flash
			INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/..")
