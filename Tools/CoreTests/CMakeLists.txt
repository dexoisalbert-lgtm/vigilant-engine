cmake_minimum_required(VERSION 3.16)
project(PaleotrenchCoreTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/PaleotrenchCore)

# Only the top level of Private/ is engine-free. Private/Module/ holds the
# single Unreal-dependent file and is deliberately not included here.
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS ${CORE_DIR}/Private/*.cpp)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(core_tests ${CORE_SOURCES} ${TEST_SOURCES})
target_include_directories(core_tests PRIVATE ${CORE_DIR}/Public)

# Mirror Unreal's build: no exceptions, no RTTI, warnings are errors.
if(MSVC)
	target_compile_options(core_tests PRIVATE /W4 /WX /GR- /EHs-c- /D_HAS_EXCEPTIONS=0)
else()
	target_compile_options(core_tests PRIVATE
		-Wall -Wextra -Wshadow -Wconversion -Werror -fno-exceptions -fno-rtti)
endif()
