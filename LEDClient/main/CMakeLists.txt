file(GLOB srcs *.cpp *.cc)
idf_component_register(SRCS ${srcs} 
			REQUIRES common esp_netif asio
			INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/..")
message(${CMAKE_SOURCE_DIR})