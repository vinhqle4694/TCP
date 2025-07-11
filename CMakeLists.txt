cmake_minimum_required(VERSION 3.10)

project(TcpLibrary VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
endif()

# Platform-specific definitions
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    set(PLATFORM_LIBS ws2_32)
else()
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

# SSL Support (optional)
option(TCP_SSL_SUPPORT "Enable SSL/TLS support" ON)
if(TCP_SSL_SUPPORT)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        add_definitions(-DTCP_SSL_SUPPORT)
        message(STATUS "SSL/TLS support enabled")
    else()
        message(WARNING "OpenSSL not found. SSL/TLS support disabled.")
        set(TCP_SSL_SUPPORT OFF)
    endif()
endif()

# Library source files
set(TCP_SOURCES
    tcp.cpp
    tcp_socket.cpp
    tcp_client.cpp
    tcp_server.cpp
    tcp_utils.cpp
)

# Library headers
set(TCP_HEADERS
    tcp.h
    tcp_socket.h
    tcp_client.h
    tcp_server.h
    tcp_utils.h
    ssl_context.h
)

# Create static library
add_library(tcp_static STATIC ${TCP_SOURCES})
target_include_directories(tcp_static PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(tcp_static PUBLIC ${PLATFORM_LIBS})

if(TCP_SSL_SUPPORT AND OpenSSL_FOUND)
    target_link_libraries(tcp_static PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Create shared library
add_library(tcp_shared SHARED ${TCP_SOURCES})
target_include_directories(tcp_shared PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(tcp_shared PUBLIC ${PLATFORM_LIBS})

if(TCP_SSL_SUPPORT AND OpenSSL_FOUND)
    target_link_libraries(tcp_shared PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Set library properties
set_target_properties(tcp_static PROPERTIES
    OUTPUT_NAME tcp
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

set_target_properties(tcp_shared PROPERTIES
    OUTPUT_NAME tcp
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Create alias targets
add_library(tcp::tcp_static ALIAS tcp_static)
add_library(tcp::tcp_shared ALIAS tcp_shared)

# Examples (optional)
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests (optional)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

# Install libraries
install(TARGETS tcp_static tcp_shared
    EXPORT TcpLibraryTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${TCP_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tcp
)

# Install CMake config files
install(EXPORT TcpLibraryTargets
    FILE TcpLibraryTargets.cmake
    NAMESPACE tcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TcpLibrary
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    TcpLibraryConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/TcpLibraryConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TcpLibraryConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TcpLibrary
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/TcpLibraryConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/TcpLibraryConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TcpLibrary
)

# Package configuration
set(CPACK_PACKAGE_NAME "TcpLibrary")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Comprehensive C++ TCP Library")
set(CPACK_PACKAGE_VENDOR "TCP Library Project")
set(CPACK_PACKAGE_CONTACT "tcp-library@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "TCP Library Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  SSL/TLS support: ${TCP_SSL_SUPPORT}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
