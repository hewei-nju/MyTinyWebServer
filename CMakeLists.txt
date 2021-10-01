cmake_minimum_required(VERSION 2.8.4)
project(MyTinyWebServer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W -Wall -lpthread -std=c99")

add_executable(webserver webserver.c)