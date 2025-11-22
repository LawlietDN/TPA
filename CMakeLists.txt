cmake_minimum_required(VERSION 3.22)
project(TransitAnalytics VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system json headers)
find_package(Protobuf REQUIRED)
find_package(SQLite3 REQUIRED)

set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/gtfs-realtime.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/nyct-subway.proto
)

foreach(PFILE ${PROTO_FILES})
    if(NOT EXISTS ${PFILE})
        message(FATAL_ERROR "Missing protocol buffer definition: ${PFILE}")
    endif()
endforeach()

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_executable(transitAnalyzer
    main.cpp
    src/ConfigurationManager.cpp
    src/Parser.cpp
    client/MtaClient.cpp 
    src/SQLiteStore.cpp     
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)
target_compile_definitions(transitAnalyzer PRIVATE
    BOOST_ASIO_NO_DEPRECATED
    BOOST_ASIO_NO_TS_EXECUTORS
    _WIN32_WINNT=0x0A00
)

target_include_directories(transitAnalyzer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(transitAnalyzer PRIVATE
    Boost::system
    Boost::json
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    Threads::Threads
    SQLite::SQLite3
)

message(STATUS "Build configuration complete.")
message(STATUS "Protobuf Compiler: ${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
