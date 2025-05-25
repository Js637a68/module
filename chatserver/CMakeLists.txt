cmake_minimum_required(VERSION 3.10)
project(chat)

set(CMAKE_CXX_FLAGES ${CMAKE_CXX_FLAGES} -g -lmysqlclient)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/server)
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis)
include_directories(${PROJECT_SOURCE_DIR}/include/server/model)
include_directories($(PROJECT_SOURCE_DIR)/lib)

link_directories()

add_subdirectory(src)