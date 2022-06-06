cmake_minimum_required(VERSION 3.5)

project(XenxivePACO2 LANGUAGES C)

file(GLOB SRCS "${CMAKE_SOURCE_DIR}/*.c")
add_executable(XenxivePACO2 ${SRCS})
