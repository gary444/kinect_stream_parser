cmake_minimum_required(VERSION 2.8)

set (PROJECT_NAME parse_image)
project( ${PROJECT_NAME} )


set(CMAKE_CXX_FLAGS "-std=c++11")


find_package( OpenCV REQUIRED )
add_executable( ${PROJECT_NAME} main.cpp )
target_link_libraries( ${PROJECT_NAME} ${OpenCV_LIBS} )