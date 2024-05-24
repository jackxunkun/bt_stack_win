cmake_minimum_required(VERSION 3.28)
project(bt_stack_copy)

#set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wunused-but-set-variable -Wunused-variable -Wswitch-default -Werror")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_PORTAUDIO")

find_package(PkgConfig QUIET)


include_directories(FreeRTOS_Source/include)
include_directories(FreeRTOS_Source/portable/MSVC-MingW)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(FreeRTOS_Source)

set(SOURCES_EXAMPLE main.c main_blinky.c Run-time-stats-utils.c)

add_executable(bt_stack_copy ${SOURCES_EXAMPLE})
target_link_libraries(bt_stack_copy setupapi winusb freertos_kernel)

message("${CMAKE_C_FLAGS}")
