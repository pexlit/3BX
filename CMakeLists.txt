#
# Specify minimum CMake version and project name
#
cmake_minimum_required (VERSION 3.21)
project (3bx)

#
# Enable ccache if available
#
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

#find_package(imgui CONFIG REQUIRED)

#
# CMake setup
#
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set (CMAKE_VERBOSE_MAKEFILE 0) # 1 should be used for debugging
set (CMAKE_SUPPRESS_REGENERATION TRUE) # Suppresses ZERO_CHECK
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-fmodules-ts)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
  if(NOT WIN32)
    set(GLAD_LIBRARIES dl)
  endif()
endif()


#
# GLFW options
#
option (GLFW_INSTALL OFF)
option (GLFW_BUILD_DOCS OFF)
option (GLFW_BUILD_EXAMPLES OFF)
option (GLFW_BUILD_TESTS OFF)
#add_subdirectory (gloom/vendor/glfw)

MACRO(HEADER_DIRECTORIES return_list)
    FILE(GLOB_RECURSE new_list src/*.h)
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        SET(dir_list ${dir_list} ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list})
ENDMACRO()

HEADER_DIRECTORIES(header_dir_list)

#
# Set include paths
#
include_directories (${header_dir_list})


#
# Add files
#
file (GLOB_RECURSE PROJECT_HEADERS src/*.hpp
                                   src/*.h)
file (GLOB_RECURSE PROJECT_SOURCES src/*.cpp
                                   src/*.cxx
                                   src/*.cc
                                   src/*.c)
file (GLOB         PROJECT_CONFIGS CMakeLists.txt
                                   README.md
                                  .gitignore)

#
# Organizing files
#
source_group ("headers" FILES ${PROJECT_HEADERS})
source_group ("sources" FILES ${PROJECT_SOURCES})

#
# Set executable and target link libraries
#
add_definitions (-DGLFW_INCvLUDE_NONE
                 -DPROJECT_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\")
add_executable (${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS}
                                ${PROJECT_CONFIGS})
                                
#target_include_directories(${PROJECT_NAME}
#PRIVATE  ${Stb_INCLUDE_DIR}
#)
#target_link_libraries (${PROJECT_NAME}
#PRIVATE
#nlohmann_json::nlohmann_json
#)
set_target_properties (${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)