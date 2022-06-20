cmake_minimum_required(VERSION 3.0.0)
project(client.c VERSION 0.1.0)

include(CTest)
enable_testing()

set(OPENSSL_ROOT_DIR /usr/include/openssl)

add_executable(varnish-encrypto src/client.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(THREADS_PREFER_PTHREAD_FLAG ON)
include(CPack)
find_package(OpenSSL REQUIRED)
target_link_libraries(varnish-encrypto OpenSSL::SSL)
target_link_libraries(varnish-encrypto pthread)
target_include_directories(varnish-encrypto PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src 
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/lib/include
)

