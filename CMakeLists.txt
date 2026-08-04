cmake_minimum_required(VERSION 3.23)
project(sydney)

add_library(md4c STATIC third_party/md4c/md4c.c)
add_library(md4c::md4c ALIAS md4c)

target_include_directories(md4c SYSTEM PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c>
)

set_target_properties(md4c 
	PROPERTIES C_STANDARD 99 
	POSITION_INDEPENDENT_CODE ON
)

add_executable(sydney)

target_compile_features(sydney PRIVATE cxx_std_20)

target_sources(sydney
	PRIVATE
		src/main.cc
)

target_include_directories(sydney 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sydney PRIVATE md4c::md4c)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
