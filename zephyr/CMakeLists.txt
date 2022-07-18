cmake_minimum_required(VERSION 3.13.1)
project(RART)

zephyr_library()
zephyr_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/rart.c)
