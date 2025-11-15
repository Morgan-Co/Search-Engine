cmake_minimum_required(VERSION 3.10.0)
project(diploma VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED libpqxx)

add_executable(diploma 
    main.cpp 
    database/Database.h database/Database.cpp 
    file_indexer/Indexer.h file_indexer/Indexer.cpp 
    parser/Parser.h parser/Parser.cpp 
    spider/Spider.h spider/Spider.cpp
    server/Server.h server/Server.cpp
    vars.h
)

target_include_directories(diploma PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${PostgreSQL_INCLUDE_DIRS}
    ${LIBPQXX_INCLUDE_DIRS}
)

target_link_libraries(diploma PRIVATE
    Boost::boost
    boost_locale
    ${PostgreSQL_LIBRARIES}
    ${LIBPQXX_LIBRARIES}
    ssl
    crypto
    pthread
)

