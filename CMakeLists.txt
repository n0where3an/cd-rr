# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.16)

project(cd-rr
	VERSION 1.30
	DESCRIPTION "CD-R eRRoR coRRectoR"
	LANGUAGES C)

option(CDRR_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
# Off by default: a newer compiler may add warnings and must not break user builds.
option(CDRR_WERROR "Treat compiler warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)	# gnu99: the code uses POSIX/GNU APIs

# Shared code (Reed-Solomon math, file chain, CLI helpers)
add_library(rrcore STATIC
	rr.c
	rrfile.c
	rrdecode.c
	rrmath.c
)
target_include_directories(rrcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(rrcore PUBLIC _CONSOLE)

if(WIN32)
	# getopt.c/getopt1.c replace the missing libc getopt on Windows
	target_sources(rrcore PRIVATE getopt.c getopt1.c)
endif()
if(NOT MSVC)
	target_compile_definitions(rrcore PUBLIC __GCC__)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(rrcore PUBLIC -Wall)
	if(CDRR_SANITIZE)
		target_compile_options(rrcore PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
		target_link_options(rrcore PUBLIC -fsanitize=address,undefined)
	endif()
endif()

if(CDRR_WERROR)
	if(MSVC)
		target_compile_options(rrcore PUBLIC /WX)
	else()
		target_compile_options(rrcore PUBLIC -Werror)
	endif()
endif()

add_executable(rrenc rrenc.c rrlist.c rrencode.c)
target_link_libraries(rrenc PRIVATE rrcore)

add_executable(rrdec rrdec.c)
target_link_libraries(rrdec PRIVATE rrcore)

include(GNUInstallDirs)
install(TARGETS rrenc rrdec RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CTest)
if(BUILD_TESTING AND UNIX)
	add_test(NAME roundtrip
		COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/roundtrip.sh
			$<TARGET_FILE:rrenc> $<TARGET_FILE:rrdec>)
endif()
