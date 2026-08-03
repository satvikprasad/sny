cmake_minimum_required(VERSION 3.23)
project(Sydney)

add_executable(Sydney)

target_compile_features(Sydney PRIVATE cxx_std_17)

target_sources(Sydney
	PRIVATE
		src/main.cc
)

target_include_directories(Sydney 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
