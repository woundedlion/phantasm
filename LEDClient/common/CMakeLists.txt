file(GLOB srcs *.cpp *.cc)
idf_component_register( SRCS ${srcs} 
			INCLUDE_DIRS "."
			REQUIRES nvs_flash)
