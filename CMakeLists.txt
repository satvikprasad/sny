cmake_minimum_required(VERSION 3.23)
project(sny)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(md4c STATIC third_party/md4c/md4c.c)
add_library(md4c::md4c ALIAS md4c)

target_include_directories(md4c SYSTEM PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c>
)

set_target_properties(md4c
	PROPERTIES C_STANDARD 99
	POSITION_INDEPENDENT_CODE ON
)

add_executable(templater)
target_compile_features(templater PRIVATE cxx_std_20)
target_sources(templater
	PRIVATE
		src/templater.cc
)
target_include_directories(templater
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(SYDNEY_TEMPLATE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/templates)
set(SYDNEY_TEMPLATE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/src/meta/template.h)

add_custom_command(
	OUTPUT ${SYDNEY_TEMPLATE_HEADER}
	COMMAND templater ${SYDNEY_TEMPLATE_DIR}
	DEPENDS templater ${SYDNEY_TEMPLATE_DIR}/tmpl.sny
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "Generating src/meta/template.h from templates/tmpl.sny"
	VERBATIM
)

add_custom_target(template DEPENDS ${SYDNEY_TEMPLATE_HEADER})

add_executable(sny)

target_compile_features(sny PRIVATE cxx_std_20)

target_sources(sny
	PRIVATE
		src/main.cc
		src/parser.cc
		src/linter.cc
		src/emitter.cc
)

target_include_directories(sny
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sny PRIVATE md4c::md4c)

add_dependencies(sny template)
