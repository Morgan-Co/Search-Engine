cmake_minimum_required(VERSION 3.10.0)
project(diploma VERSION 0.1.0 LANGUAGES C CXX)

cmake_policy(SET CMP0167 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
find_package(Boost REQUIRED COMPONENTS system locale thread)
find_package(OpenSSL REQUIRED)
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
)

# add_executable(diploma main.cpp)


target_link_libraries(diploma PRIVATE 
    Boost::system 
    Boost::locale 
    Boost::thread 
    OpenSSL::SSL 
    OpenSSL::Crypto
    pthread
    ${LIBPQXX_LIBRARIES}
		${PostgreSQL_LIBRARIES}
)

