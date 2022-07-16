cmake_minimum_required(VERSION 3.0.0)
project(client.c VERSION 0.1.0)

include(CTest)
enable_testing()

set(OPENSSL_ROOT_DIR /usr/include/openssl)

add_executable(varnish-encrypto src/encrypto.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(THREADS_PREFER_PTHREAD_FLAG ON)
include(CPack)
set( OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@1.1")
find_package(OpenSSL REQUIRED)
if ( OPENSSL_FOUND )
    message(STATUS "OpenSSL Found: ${OPENSSL_VERSION}")
    message(STATUS "OpenSSL Include: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
endif()
target_link_libraries(varnish-encrypto pthread)
target_link_libraries(varnish-encrypto ${OPENSSL_LIBRARIES})
target_include_directories(varnish-encrypto PUBLIC
    /opt/homebrew/opt/openssl@1.1/lib
    /opt/homebrew/opt/openssl@1.1/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    Threads::Threads
)

