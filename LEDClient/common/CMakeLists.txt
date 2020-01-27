file(GLOB srcs *.cpp *.cc)
idf_component_register( SRCS ${srcs} 
			INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/.."
			REQUIRES nvs_flash)
