cmake_minimum_required(VERSION 3.0.0)
project(client.c VERSION 0.1.0)

include(CTest)
enable_testing()

set(OPENSSL_ROOT_DIR /usr/include/openssl)

add_executable(varnish-encrypto src/client.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(OPENSSL_CRYPTO_LIBRARY /opt/homebrew/Cellar/openssl@1.1/1.1.1m)
set(OPENSSL_INCLUDE_DIR /opt/homebrew/Cellar/openssl@1.1/1.1.1m/include)
set(THREADS_PREFER_PTHREAD_FLAG ON)
include(CPack)
find_package(OpenSSL REQUIRED)
find_library(OPENSSL_LIB ssl PATHS /opt/homebrew/Cellar/openssl@1.1/1.1.1p/lib NO_DEFAULT_PATH) 
find_library(CRYPTO_LIB crypto PATHS /opt/homebrew/Cellar/openssl@1.1/1.1.1p/lib NO_DEFAULT_PATH)
target_link_libraries(varnish-encrypto ssl)
target_link_libraries(varnish-encrypto crypto)
target_link_libraries(varnish-encrypto pthread)
target_include_directories(varnish-encrypto PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src 
    /opt/homebrew/opt/openssl@1.1/include
    /opt/homebrew/Cellar/openssl@1.1/1.1.1m/include
    /opt/homebrew/opt/openssl@1.1/include
    /opt/homebrew/opt/openssl@1.1/lib
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/lib/include
    ${CMAKE_CURRENT_LIST_DIR}/lib/include/boost/numeric/conversion/include
    Threads::Threads
)

