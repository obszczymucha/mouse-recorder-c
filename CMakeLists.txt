cmake_minimum_required(VERSION 3.10)
project(mouse-recorder C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

include_directories(${LIBUSB_INCLUDE_DIRS})

add_executable(mouse-recorder main.c)

target_link_libraries(mouse-recorder ${LIBUSB_LIBRARIES})

target_compile_options(mouse-recorder PRIVATE ${LIBUSB_CFLAGS_OTHER})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_link_libraries(mouse-recorder rt)
endif()

install(TARGETS mouse-recorder DESTINATION bin)
