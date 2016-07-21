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
set(OPEN62541_DIR "${CMAKE_SOURCE_DIR}/external/open62541")
ExternalProject_Add(open62541
DOWNLOAD_COMMAND ""
CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/external
           -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
           -DUA_ENABLE_AMALGAMATION=ON
SOURCE_DIR ${OPEN62541_DIR}
BINARY_DIR ${CMAKE_SOURCE_DIR}/external_generated
INSTALL_COMMAND ""
)
include_directories(${CMAKE_SOURCE_DIR}/external_generated)
link_directories(${PROJECT_SOURCE_DIR}/external_generated)
include_directories(${PROJECT_BINARY_DIR}) #for nodeset.h

#nodeset generation
add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/nodeset.h ${PROJECT_BINARY_DIR}/nodeset.c
                  PRE_BUILD
                  COMMAND ${PYTHON_EXECUTABLE} ${OPEN62541_DIR}/tools/pyUANamespace/generate_open62541CCode.py
                                                -i ${OPEN62541_DIR}/tools/pyUANamespace/NodeID_Blacklist_FullNS0.txt
                                                ${OPEN62541_DIR}/tools/schema/namespace0/Opc.Ua.NodeSet2.xml
                                                ${PROJECT_SOURCE_DIR}/server_nodeset.xml
                                                ${PROJECT_BINARY_DIR}/nodeset
                  DEPENDS ${OPEN62541_DIR}/tools/pyUANamespace/generate_open62541CCode.py
                          ${OPEN62541_DIR}/tools/pyUANamespace/open62541_MacroHelper.py
                          ${OPEN62541_DIR}/tools/pyUANamespace/ua_builtin_types.py
                          ${OPEN62541_DIR}/tools/pyUANamespace/ua_constants.py
                          ${OPEN62541_DIR}/tools/pyUANamespace/ua_namespace.py
                          ${OPEN62541_DIR}/tools/pyUANamespace/ua_node_types.py
                          ${OPEN62541_DIR}/tools/pyUANamespace/NodeID_Blacklist_FullNS0.txt
                          
                          ${PROJECT_SOURCE_DIR}/server_nodeset.xml
                  )

add_executable(dht22    dht22.c 
                        ${PROJECT_BINARY_DIR}/nodeset.c
                        ${DHT22_PLATFORM_PREFIX}pi_dht_read.c
                        ${DHT22_PLATFORM_PREFIX}pi_mmio.c
                        ${DHT22_PLATFORM_PREFIX}../common_dht_read.c
                        )
target_include_directories(dht22 PRIVATE ${CMAKE_SOURCE_DIR}/external/open62541/plugins ${CMAKE_SOURCE_DIR}/external/open62541/include ${CMAKE_SOURCE_DIR}/external_generated/src_generated ${CMAKE_SOURCE_DIR}/external/open62541/src ${CMAKE_SOURCE_DIR}/external/open62541/deps) # needs an internal header
target_link_libraries(dht22 libopen62541.a)          
add_dependencies(dht22 open62541)
