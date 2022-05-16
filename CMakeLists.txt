cmake_minimum_required(VERSION 3.0.0)
project(client.c VERSION 0.1.0)

include(CTest)
enable_testing()

set(OPENSSL_ROOT_DIR /usr/include/openssl)

add_executable(varnish-encrypto client.c)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
find_package(OpenSSL REQUIRED)
target_link_libraries(varnish-encrypto OpenSSL::SSL)

