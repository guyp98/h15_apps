cmake_minimum_required(VERSION 3.12)
project(camera_streaming_detection)

set(CMAKE_CXX_STANDARD 17)

set(YSROOT /opt/poky/4.0.2/sysroots/armv8a-poky-linux)

set(CMAKE_CXX_FLAGS "-fstack-protector-strong -Wformat -Wformat-security -Werror=format-security -pipe -g -feliminate-unused-debug-types -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --sysroot=${YSROOT}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${YSROOT}/usr/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${YSROOT}/usr/include/gstreamer-1.0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${YSROOT}/usr/lib/glib-2.0/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${YSROOT}/usr/include/glib-2.0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${YSROOT}/usr/incude/gst-hailo")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${YSROOT}/usr/incude/gsthailometa")

add_executable(camera_streaming_detection camera_streaming_detection.cpp)

target_link_libraries(camera_streaming_detection pthread gstreamer-1.0 gobject-2.0 glib-2.0 zmq)