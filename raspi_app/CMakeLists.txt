cmake_minimum_required (VERSION 2.8)
 
set(CMAKE_MODULE_PATH "/usr/local/lib/cmake/${CMAKE_MODULE_PATH}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -lncurses -lrt -pthread -lmosquitto -lm ")

find_package(raspicam REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Mosquitto REQUIRED IMPORTED_TARGET libmosquitto)

add_executable ( main main.c main.h function.c camera.h camera.cpp )
 
target_link_libraries (main ${raspicam_LIBS} PkgConfig::Mosquitto)