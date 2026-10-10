cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{IDF_PATH})
	message(FATAL_ERROR "Environment variable IDF_PATH is not set. Please set IDF_PATH to your esp-idf installation path.")
endif()

if(NOT EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
	message(FATAL_ERROR "Could not find project.cmake at $ENV{IDF_PATH}/tools/cmake/project.cmake. Check your IDF_PATH.")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(epaper_hello)
