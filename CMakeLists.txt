cmake_minimum_required(VERSION 3.22)
project(TransitAnalytics VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system headers)
find_package(Protobuf REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)

set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/gtfs-realtime.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/nyct-subway.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(tz STATIC third_party/date/src/tz.cpp)
target_include_directories(tz PUBLIC third_party/date/include)

add_executable(transitAnalyzer
    src/main.cpp
    src/ConfigurationManager.cpp
    src/Parser.cpp
    src/SQLiteStore.cpp 
    src/VirtualClock.cpp  
    src/Dashboard.cpp
    src/StopManager.cpp
    src/MtaClient.cpp
    src/ReplayEngine.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_compile_definitions(transitAnalyzer PRIVATE
    BOOST_ASIO_NO_DEPRECATED
    BOOST_ASIO_NO_TS_EXECUTORS
    DATE_MANUAL_TZDB_PATH="${CMAKE_SOURCE_DIR}/third_party/tzdata"
)

target_include_directories(transitAnalyzer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/date/include
)

target_link_libraries(transitAnalyzer PRIVATE
    Boost::system
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    Threads::Threads
    SQLite::SQLite3
    tz
    CURL::libcurl
)
