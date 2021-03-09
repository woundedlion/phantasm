cmake_minimum_required(VERSION 3.5)
set(CMAKE_BUILD_TYPE Debug)

project(LEDServer)

file(GLOB bin_srcs *.cpp)
add_executable(ledserve ${bin_srcs})

file(GLOB libcolorspace_srcs ../libs/ColorSpace/src/*.cpp)
add_library(libcolorspace ${libcolorspace_srcs})

target_include_directories(ledserve PUBLIC ../libs/ColorSpace/src)
target_link_libraries(ledserve boost_system boost_log pthread libcolorspace)
target_compile_options(ledserve PUBLIC -DBOOST_LOG_DYN_LINK -std=c++17)
