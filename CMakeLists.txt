cmake_minimum_required(VERSION 3.5)

function(GET_VERSION VERSION_PART VERSION_NUM)
    set(VERSION_REGEX "^const int VERSION_${VERSION_PART} = (.+);$")
    file(STRINGS "src/sw/redis++/version.h" VERSION_INFO REGEX "${VERSION_REGEX}")
    string(REGEX REPLACE "${VERSION_REGEX}" "\\1" VERSION_RESULT ${VERSION_INFO})
    set(${VERSION_NUM} ${VERSION_RESULT} PARENT_SCOPE)
endfunction()

GET_VERSION("MAJOR" REDIS_PLUS_PLUS_VERSION_MAJOR)
GET_VERSION("MINOR" REDIS_PLUS_PLUS_VERSION_MINOR)
GET_VERSION("PATCH" REDIS_PLUS_PLUS_VERSION_PATCH)

set(REDIS_PLUS_PLUS_VERSION "${REDIS_PLUS_PLUS_VERSION_MAJOR}.${REDIS_PLUS_PLUS_VERSION_MINOR}.${REDIS_PLUS_PLUS_VERSION_PATCH}")
message(STATUS "redis-plus-plus version: ${REDIS_PLUS_PLUS_VERSION}")

project(redis++ LANGUAGES CXX VERSION ${REDIS_PLUS_PLUS_VERSION})

set(REDIS_PLUS_PLUS_DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE ${REDIS_PLUS_PLUS_DEFAULT_BUILD_TYPE} CACHE STRING "Set build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()
message(STATUS "redis-plus-plus build type: ${CMAKE_BUILD_TYPE}")

set(REDIS_PLUS_PLUS_DEFAULT_CXX_STANDARD 17)
if(NOT REDIS_PLUS_PLUS_CXX_STANDARD)
    set(REDIS_PLUS_PLUS_CXX_STANDARD ${REDIS_PLUS_PLUS_DEFAULT_CXX_STANDARD} CACHE STRING "Set CXX standard" FORCE)
    set_property(CACHE REDIS_PLUS_PLUS_CXX_STANDARD PROPERTY STRINGS "11" "14" "17" "20")
endif()
message(STATUS "redis-plus-plus build with CXX standard: c++${REDIS_PLUS_PLUS_CXX_STANDARD}")

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${REDIS_PLUS_PLUS_CXX_STANDARD}")
else()
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++${REDIS_PLUS_PLUS_CXX_STANDARD}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${REDIS_PLUS_PLUS_CXX_STANDARD}")
    endif()
endif()

if(REDIS_PLUS_PLUS_BUILD_ASYNC)
    if(REDIS_PLUS_PLUS_BUILD_ASYNC STREQUAL "libuv")
        message(STATUS "redis-plus-plus build async interface with libuv")

        # libuv dependency
        find_path(REDIS_PLUS_PLUS_ASYNC_LIB_HEADER NAMES uv.h)
        find_library(REDIS_PLUS_PLUS_ASYNC_LIB uv)
    else()
        message(FATAL_ERROR "invalid REDIS_PLUS_PLUS_BUILD_ASYNC")
    endif()
endif()

if(REDIS_PLUS_PLUS_BUILD_CORO)
    if(NOT REDIS_PLUS_PLUS_BUILD_ASYNC)
        message(FATAL_ERROR "In order to build coroutine support, you must set REDIS_PLUS_PLUS_BUILD_ASYNC")
    endif()

    message(STATUS "redis-plus-plus build coroutine support: " ${REDIS_PLUS_PLUS_BUILD_CORO})
endif()

set(REDIS_PLUS_PLUS_HEADER_DIR src)

set(REDIS_PLUS_PLUS_SOURCE_DIR src/sw/redis++)

set(REDIS_PLUS_PLUS_SOURCES
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/command.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/command_options.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/connection.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/connection_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/crc16.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/errors.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/pipeline.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/redis.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/redis_cluster.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/redis_uri.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/reply.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/sentinel.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/shards.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/shards_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/subscriber.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/transaction.cpp"
)

set(PATTERNS_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/patterns")

list(APPEND REDIS_PLUS_PLUS_SOURCES
        "${PATTERNS_DIR}/redlock.cpp")

if(REDIS_PLUS_PLUS_BUILD_ASYNC)
    list(APPEND REDIS_PLUS_PLUS_SOURCES
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_connection.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_connection_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_redis.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/event_loop.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_sentinel.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_redis_cluster.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_shards_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_subscriber.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_subscriber_impl.cpp"
    )

    if(NOT REDIS_PLUS_PLUS_ASYNC_FUTURE)
        set(REDIS_PLUS_PLUS_ASYNC_FUTURE "std")
    endif()

    if(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "std")
        set(REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER "${REDIS_PLUS_PLUS_SOURCE_DIR}/future/std")
    elseif(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "boost")
        set(REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER "${REDIS_PLUS_PLUS_SOURCE_DIR}/future/boost")
        find_package(Boost REQUIRED COMPONENTS system thread)
    else()
        message(FATAL_ERROR "invalid REDIS_PLUS_PLUS_ASYNC_FUTURE")
    endif()
endif()

# cxx utils
if(REDIS_PLUS_PLUS_CXX_STANDARD LESS 17)
    set(CXX_UTILS_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/cxx11")
else()
    set(CXX_UTILS_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/cxx17")
endif()

# TLS support
option(REDIS_PLUS_PLUS_USE_TLS "Build with TLS support" OFF)
message(STATUS "redis-plus-plus TLS support: ${REDIS_PLUS_PLUS_USE_TLS}")

if(REDIS_PLUS_PLUS_USE_TLS)
    set(TLS_SUB_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/tls")

    list(APPEND REDIS_PLUS_PLUS_SOURCES "${TLS_SUB_DIR}/sw/redis++/tls.cpp")

    set(REDIS_PLUS_PLUS_DEPENDS "hiredis,hiredis_ssl")
else()
    set(TLS_SUB_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/no_tls")

    set(REDIS_PLUS_PLUS_DEPENDS "hiredis")
endif()

# hiredis dependency
include(${CMAKE_CURRENT_LIST_DIR}/cmake/FindHiredis.cmake)

# Check hiredis features
message(STATUS "redis-plus-plus check hiredis features")
if(hiredis_FOUND)
    set(HIREDIS_FEATURE_TEST_INCLUDE ${hiredis_INCLUDE_DIRS})
else()
    set(HIREDIS_FEATURE_TEST_INCLUDE ${HIREDIS_HEADER})
endif()
set(HIREDIS_FEATURE_TEST_HEADER "${HIREDIS_FEATURE_TEST_INCLUDE}/hiredis/hiredis.h")

file(READ "${HIREDIS_FEATURE_TEST_HEADER}" HIREDIS_HEADER_CONTENT)

string(FIND "${HIREDIS_HEADER_CONTENT}" redisEnableKeepAliveWithInterval redisEnableKeepAliveWithInterval_POS)
if(${redisEnableKeepAliveWithInterval_POS} GREATER -1)
    set(REDIS_PLUS_PLUS_HAS_redisEnableKeepAliveWithInterval ON)
endif()

set(REDIS_PLUS_PLUS_GENERATED_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/${REDIS_PLUS_PLUS_HEADER_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/hiredis_features.h.in ${CMAKE_CURRENT_BINARY_DIR}/${REDIS_PLUS_PLUS_SOURCE_DIR}/hiredis_features.h)

# Build static library
option(REDIS_PLUS_PLUS_BUILD_STATIC "Build static library" ON)
message(STATUS "redis-plus-plus build static library: ${REDIS_PLUS_PLUS_BUILD_STATIC}")

if(REDIS_PLUS_PLUS_BUILD_STATIC)
    set(STATIC_LIB redis++_static)

    # For the static build, link to the static version of hiredis
    set(REDIS_PLUS_PLUS_HIREDIS_LIBS_STATIC ${REDIS_PLUS_PLUS_HIREDIS_LIBS})

    if (TARGET hiredis::hiredis_static)
      string(REPLACE "hiredis::hiredis" "hiredis::hiredis_static" REDIS_PLUS_PLUS_HIREDIS_LIBS_STATIC
        "${REDIS_PLUS_PLUS_HIREDIS_LIBS_STATIC}")
    endif()

    if (TARGET hiredis::hiredis_ssl_static)
      # If SSL is not enabled, this line will have no effect
      string(REPLACE "hiredis::hiredis_static_ssl" "hiredis::hiredis_ssl_static" REDIS_PLUS_PLUS_HIREDIS_LIBS_STATIC
        "${REDIS_PLUS_PLUS_HIREDIS_LIBS_STATIC}")
    endif()

    add_library(${STATIC_LIB} STATIC ${REDIS_PLUS_PLUS_SOURCES})
    add_library(redis++::${STATIC_LIB} ALIAS ${STATIC_LIB})

    list(APPEND REDIS_PLUS_PLUS_TARGETS ${STATIC_LIB})

    target_include_directories(${STATIC_LIB} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_HEADER_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TLS_SUB_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${CXX_UTILS_DIR}>
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_GENERATED_HEADER_DIR}>)

    if(hiredis_FOUND)
        target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${hiredis_INCLUDE_DIRS}>)
        target_link_libraries(${STATIC_LIB} PUBLIC ${REDIS_PLUS_PLUS_HIREDIS_LIBS_STATIC})
    else()
        target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${HIREDIS_HEADER}>)
    endif()

    if(REDIS_PLUS_PLUS_BUILD_ASYNC)
        target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER}>)
        target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_ASYNC_LIB_HEADER}>)
        if(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "boost")
            target_include_directories(${STATIC_LIB} SYSTEM PUBLIC $<BUILD_INTERFACE:${Boost_INCLUDE_DIR}>)
        endif()
    endif()

    if (WIN32)
        target_compile_definitions(${STATIC_LIB} PRIVATE NOMINMAX)
        set_target_properties(${STATIC_LIB} PROPERTIES CXX_STANDARD ${REDIS_PLUS_PLUS_CXX_STANDARD})
        set_target_properties(${STATIC_LIB} PROPERTIES OUTPUT_NAME redis++_static)
    else()
        target_compile_options(${STATIC_LIB} PRIVATE "-Wall" "-Wextra" "-Werror")
        set_target_properties(${STATIC_LIB} PROPERTIES OUTPUT_NAME redis++)
    endif()

    set_target_properties(${STATIC_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(${STATIC_LIB} PROPERTIES CXX_EXTENSIONS OFF)

    option(REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC "Build static library with position independent code" ON)
    message(STATUS "redis-plus-plus build static library with position independent code: ${REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC}")

    if(REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC)
        set_target_properties(${STATIC_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# Build shared library
option(REDIS_PLUS_PLUS_BUILD_SHARED "Build shared library" ON)
message(STATUS "redis-plus-plus build shared library: ${REDIS_PLUS_PLUS_BUILD_SHARED}")

if(REDIS_PLUS_PLUS_BUILD_SHARED)
    set(SHARED_LIB redis++)

    add_library(${SHARED_LIB} SHARED ${REDIS_PLUS_PLUS_SOURCES})
    add_library(redis++::${SHARED_LIB} ALIAS ${SHARED_LIB})
    list(APPEND REDIS_PLUS_PLUS_TARGETS ${SHARED_LIB})

    target_include_directories(${SHARED_LIB} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_HEADER_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TLS_SUB_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${CXX_UTILS_DIR}>
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_GENERATED_HEADER_DIR}>)

    if(hiredis_FOUND)
        target_include_directories(${SHARED_LIB} PUBLIC $<BUILD_INTERFACE:${hiredis_INCLUDE_DIRS}>)
    else()
        target_include_directories(${SHARED_LIB} PUBLIC $<BUILD_INTERFACE:${HIREDIS_HEADER}>)
    endif()
    target_link_libraries(${SHARED_LIB} PUBLIC ${REDIS_PLUS_PLUS_HIREDIS_LIBS})

    if(REDIS_PLUS_PLUS_BUILD_ASYNC)
        target_include_directories(${SHARED_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER}>)
        target_include_directories(${SHARED_LIB} PUBLIC $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_ASYNC_LIB_HEADER}>)
        target_link_libraries(${SHARED_LIB} PUBLIC ${REDIS_PLUS_PLUS_ASYNC_LIB})
        if(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "boost")
            target_include_directories(${SHARED_LIB} SYSTEM PUBLIC $<BUILD_INTERFACE:${Boost_INCLUDE_DIR}>)
        endif()
    endif()

    if(WIN32)
        target_compile_definitions(${SHARED_LIB} PRIVATE NOMINMAX)
        set_target_properties(${SHARED_LIB} PROPERTIES CXX_STANDARD ${REDIS_PLUS_PLUS_CXX_STANDARD})
        set_target_properties(${SHARED_LIB} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    else()
        target_compile_options(${SHARED_LIB} PRIVATE "-Wall" "-Wextra" "-Werror")
    endif()

    set_target_properties(${SHARED_LIB} PROPERTIES OUTPUT_NAME redis++)
    set_target_properties(${SHARED_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(${SHARED_LIB} PROPERTIES CXX_EXTENSIONS OFF)
    set_target_properties(${SHARED_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(${SHARED_LIB} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

option(REDIS_PLUS_PLUS_BUILD_TEST "Build tests for redis++" ON)
message(STATUS "redis-plus-plus build test: ${REDIS_PLUS_PLUS_BUILD_TEST}")

option(REDIS_PLUS_PLUS_BUILD_ASYNC_TEST "Build async tests for redis++" OFF)
if(REDIS_PLUS_PLUS_BUILD_ASYNC AND REDIS_PLUS_PLUS_BUILD_TEST)
    set(REDIS_PLUS_PLUS_BUILD_ASYNC_TEST ON)
endif()

if(REDIS_PLUS_PLUS_BUILD_TEST)
    add_subdirectory(test)
endif()

include(GNUInstallDirs)

install(TARGETS ${REDIS_PLUS_PLUS_TARGETS}
        EXPORT redis++-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(REDIS_PLUS_PLUS_CMAKE_DESTINATION share/cmake/redis++)

install(EXPORT redis++-targets
        FILE redis++-targets.cmake
        NAMESPACE redis++::
        DESTINATION ${REDIS_PLUS_PLUS_CMAKE_DESTINATION})

# Install headers.
set(HEADER_PATH "sw/redis++")
file(GLOB HEADERS
    "${REDIS_PLUS_PLUS_SOURCE_DIR}/*.h*"
    "${TLS_SUB_DIR}/sw/redis++/*.h"
    "${CXX_UTILS_DIR}/sw/redis++/*.h"
    "${REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER}/sw/redis++/*.h"
    "${REDIS_PLUS_PLUS_GENERATED_HEADER_DIR}/sw/redis++/*.h")

if(NOT REDIS_PLUS_PLUS_BUILD_ASYNC)
    file(GLOB ASYNC_HEADERS "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_*.h" "${REDIS_PLUS_PLUS_SOURCE_DIR}/event_*.h")
    list(REMOVE_ITEM HEADERS ${ASYNC_HEADERS})
endif()

if(NOT REDIS_PLUS_PLUS_BUILD_CORO)
    file(GLOB CORO_HEADERS "${REDIS_PLUS_PLUS_SOURCE_DIR}/co_*.h")
    list(REMOVE_ITEM HEADERS ${CORO_HEADERS})
endif()

install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${HEADER_PATH})

file(GLOB PATTERNS_HEADERS "${PATTERNS_DIR}/*.h*")
install(FILES ${PATTERNS_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${HEADER_PATH}/patterns)

include(CMakePackageConfigHelpers)

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion)

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/redis++-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config.cmake"
        INSTALL_DESTINATION ${REDIS_PLUS_PLUS_CMAKE_DESTINATION})

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config-version.cmake"
        DESTINATION ${REDIS_PLUS_PLUS_CMAKE_DESTINATION})

export(EXPORT redis++-targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-targets.cmake"
        NAMESPACE redis++::)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/redis++.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++.pc" @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# All the Debian-specific cpack defines.
if(${CMAKE_VERSION} VERSION_GREATER 3.6)
  SET(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS "ON")
endif()
if(NOT DEFINED CPACK_DEBIAN_PACKAGE_DEPENDS)
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6, libhiredis-dev")
endif()
SET(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
SET(CPACK_DEBIAN_PACKAGE_VERSION "${REDIS_PLUS_PLUS_VERSION}")
SET(CPACK_DEBIAN_PACKAGE_SOURCE "https://github.com/sewenew/redis-plus-plus")
message(STATUS "Debian package name: ${CPACK_PACKAGE_FILE_NAME}.deb")

# All the common cpack defines.
if(NOT DEFINED CPACK_PACKAGE_NAME)
    SET(CPACK_PACKAGE_NAME "libredis++-dev")
endif()
SET(CPACK_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
SET(CPACK_PACKAGE_DESCRIPTION "A pure C++ client for Redis, based on hiredis.")
SET(CPACK_PACKAGE_CONTACT "anonymous")
SET(CPACK_GENERATOR "DEB")
INCLUDE(CPack)
