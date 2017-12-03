cmake_minimum_required(VERSION 3.4.1)
project(hDHT
	VERSION 3.25.92
	LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(PrintableOptions)
include(FindPkgConfig)

add_printable_variable(LIB_SUFFIX "Library directory suffix, usually defined to '64' for x86_64 systems" "")
add_printable_variable_bare(CMAKE_INSTALL_PREFIX)
add_printable_variable_path(BIN_INSTALL_DIR "Install directory for binary files, defaults to CMAKE_INSTALL_PREFIX/bin" "")
add_printable_variable_path(INCLUDE_INSTALL_DIR "Install directory for header files, defaults to CMAKE_INSTALL_PREFIX/include" "")
add_printable_variable_path(LIB_INSTALL_DIR "Install directory for library files, defaults to CMAKE_INSTALL_PREFIX/lib{LIB_SUFFIX}" "")
add_printable_variable_path(LIBEXEC_INSTALL_DIR "Install directory for library executable files, defaults to CMAKE_INSTALL_PREFIX/libexec" "")
add_printable_variable_path(SHARE_INSTALL_PREFIX "Install directory for shared files, defaults to CMAKE_INSTALL_PREFIX/share" "")
add_printable_variable_path(LOCALE_INSTALL_DIR "Install directory for locale files, defaults to SHARE_INSTALL_PREFIX/locale" "")
add_printable_variable_path(SYSCONF_INSTALL_DIR "Install directory for system configuration files, defaults to CMAKE_INSTALL_PREFIX/etc" "")

macro(ensure_default_value _var _defvalue)
        if(${_var} STREQUAL "")
                set(${_var} ${_defvalue})
        endif(${_var} STREQUAL "")
endmacro(ensure_default_value)

ensure_default_value(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
ensure_default_value(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")
ensure_default_value(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")
ensure_default_value(LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/libexec")
ensure_default_value(SHARE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share")
ensure_default_value(LOCALE_INSTALL_DIR "${SHARE_INSTALL_PREFIX}/locale")
ensure_default_value(SYSCONF_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/etc")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++14 -Wall -Werror")

pkg_check_modules(UV REQUIRED libuv)
pkg_check_modules(SYSTEMD libsystemd)

set(HEADERS include/libhdht/libhdht.hpp)

add_library(hdht SHARED
	lib/client.cpp
	lib/dht.cpp
	lib/libhdht.cpp
	lib/geo.cpp
	lib/marshal.cpp
	lib/net.cpp
	lib/node.cpp
	lib/protocol.cpp
	lib/rpc.cpp
	lib/server.cpp
	lib/uv.cpp
  lib/rtree/rtree.cpp
  lib/rtree/rtree-helper.cpp
  lib/rtree/node.cpp
  lib/rtree/rectangle.cpp
  lib/rtree/node-entry.cpp
  lib/rtree/leaf-entry.cpp
  lib/rtree/internal-entry.cpp
  lib/rtree/hilbert-value.cpp
)
target_include_directories(hdht PUBLIC "include/")
target_compile_options(hdht PUBLIC ${UV_CFLAGS})
target_include_directories(hdht PUBLIC ${UV_INCLUDE_DIRS})
target_link_libraries(hdht PUBLIC ${UV_LIBRARIES})
if(SYSTEMD_FOUND)
	target_compile_options(hdht PUBLIC "-DHAVE_SYSTEMD=1 ${SYSTEMD_CFLAGS}")
	target_include_directories(hdht PUBLIC ${SYSTEMD_INCLUDE_DIRS})
	target_link_libraries(hdht PUBLIC ${SYSTEMD_LIBRARIES})
endif(SYSTEMD_FOUND)

add_executable(hdhtd server/main.cpp)
target_link_libraries(hdhtd hdht)

add_executable(hdht-cli client/main.cpp)
target_link_libraries(hdht-cli hdht)

install (TARGETS hdhtd DESTINATION bin)
install (TARGETS hdht-cli DESTINATION bin)
install (TARGETS hdht DESTINATION ${LIB_INSTALL_DIR})
install (FILES ${HEADERS} DESTINATION ${INCLUDE_INSTALL_DIR}/libhdht)
