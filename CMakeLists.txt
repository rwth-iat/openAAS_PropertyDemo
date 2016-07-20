cmake_minimum_required(VERSION 2.8.11)
project(dht22ua C)
include(ExternalProject)

set (CMAKE_C_FLAGS "--std=gnu99 ${CMAKE_C_FLAGS}") #dht22 driver needs gnu99

# Set default build type.
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE not given; setting to 'Release'.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

#dht22 driver business
set(DHT22_PLATFORM_PREFIX "${PROJECT_SOURCE_DIR}/external/Adafruit_Python_DHT/source/Raspberry_Pi/")
include_directories(${DHT22_PLATFORM_PREFIX})

#open62541 business
ExternalProject_Add(open62541
DOWNLOAD_COMMAND ""
CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/external
           -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
           -DUA_ENABLE_AMALGAMATION=ON
SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/open62541
BINARY_DIR ${CMAKE_SOURCE_DIR}/external_generated
INSTALL_COMMAND ""
)
include_directories(${CMAKE_SOURCE_DIR}/external_generated)

add_executable(dht22    dht22.c 
                        ${DHT22_PLATFORM_PREFIX}pi_dht_read.c
                        ${DHT22_PLATFORM_PREFIX}pi_mmio.c
                        ${DHT22_PLATFORM_PREFIX}../common_dht_read.c
                        
                        ${CMAKE_SOURCE_DIR}/external_generated/open62541.c
                        )
                        
add_dependencies(dht22 open62541)
