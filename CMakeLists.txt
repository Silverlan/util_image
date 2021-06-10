cmake_minimum_required(VERSION 3.12)

set(INCLUDE_DIRS)
function(add_include_dir IDENTIFIER)
	set(PRETTYNAME ${IDENTIFIER})
	set(ARGV ${ARGN})
	list(LENGTH ARGV ARGC)
	if(${ARGC} GREATER 0)
		list(GET ARGV 0 PRETTYNAME)
	endif()
	string(TOUPPER ${IDENTIFIER} UIDENTIFIER)

	set(${DEFAULT_DIR} "")
	set(DEPENDENCY_${UIDENTIFIER}_INCLUDE ${DEFAULT_DIR} CACHE PATH "Path to ${PRETTYNAME} include directory.")
	set(INCLUDE_DIRS ${INCLUDE_DIRS} DEPENDENCY_${UIDENTIFIER}_INCLUDE PARENT_SCOPE)
endfunction(add_include_dir)

set(LIBRARIES)
function(add_external_library IDENTIFIER)
	set(PRETTYNAME ${IDENTIFIER})
	set(ARGV ${ARGN})
	list(LENGTH ARGV ARGC)
	if(${ARGC} GREATER 0)
		list(GET ARGV 0 PRETTYNAME)
	endif()
	string(TOUPPER ${IDENTIFIER} UIDENTIFIER)

	set(DEPENDENCY_${UIDENTIFIER}_LIBRARY "" CACHE FILEPATH "Path to ${PRETTYNAME} library.")
	set(LIBRARIES ${LIBRARIES} DEPENDENCY_${UIDENTIFIER}_LIBRARY PARENT_SCOPE)
endfunction(add_external_library)

function(link_external_library IDENTIFIER)
	set(PRETTYNAME ${IDENTIFIER})
	set(ARGV ${ARGN})
	list(LENGTH ARGV ARGC)
	if(${ARGC} GREATER 0)
		list(GET ARGV 0 PRETTYNAME)
	endif()
	string(TOUPPER ${IDENTIFIER} UIDENTIFIER)

	set(${DEFAULT_DIR} "")
	set(DEPENDENCY_${UIDENTIFIER}_INCLUDE ${DEFAULT_DIR} CACHE PATH "Path to ${PRETTYNAME} include directory.")
	set(INCLUDE_DIRS ${INCLUDE_DIRS} DEPENDENCY_${UIDENTIFIER}_INCLUDE PARENT_SCOPE)

	set(DEPENDENCY_${UIDENTIFIER}_LIBRARY "" CACHE FILEPATH "Path to ${PRETTYNAME} library.")
	set(LIBRARIES ${LIBRARIES} DEPENDENCY_${UIDENTIFIER}_LIBRARY PARENT_SCOPE)
endfunction(link_external_library)

##### CONFIGURATION #####

set(PROJ_NAME util_image)

project(${PROJ_NAME} CXX)

set(CMAKE_CXX_STANDARD 17)
set(ENABLE_STATIC_LIBRARY_FLAG 0)
# set(PRECOMPILED_HEADER "stdafx")

link_external_library(sharedutils)
link_external_library(mathutil)
link_external_library(vfilesystem)
link_external_library(lpng)
link_external_library(zlib)
link_external_library(nvidia_texture_tools)

add_external_library(nvidia_texture_tools)
add_external_library(nvidia_texture_tools_image)
add_external_library(nvidia_texture_tools_core)
add_external_library(nvidia_texture_tools_math)
add_external_library(nvidia_texture_tools_bc7)
add_external_library(nvidia_texture_tools_bc6h)
add_external_library(nvidia_texture_tools_thread)
add_external_library(nvidia_texture_tools_squish)
add_external_library(squish)

add_definitions(-DUIMG_ENABLE_NVTT)
add_definitions(-DUIMG_DLL)

add_include_dir(glm)
add_include_dir(lpng_build)

set(DEFINITIONS)

##### CONFIGURATION #####

set(LIB_TYPE SHARED)
if(ENABLE_STATIC_LIBRARY_FLAG)
	option(CONFIG_STATIC_LIBRARY "Build as static library?" OFF)
endif()
if(DEFINED ${CONFIG_STATIC_LIBRARY} AND ${CONFIG_STATIC_LIBRARY})
	set(LIB_TYPE STATIC)
endif()

foreach(def IN LISTS DEFINITIONS)
	add_definitions(-D${def})
endforeach(def)

function(def_vs_filters FILE_LIST)
	foreach(source IN LISTS FILE_LIST)
	    get_filename_component(source_path "${source}" PATH)
	    string(REPLACE "${CMAKE_CURRENT_LIST_DIR}" "" source_path_relative "${source_path}")
	    string(REPLACE "/" "\\" source_path_msvc "${source_path_relative}")
	    source_group("${source_path_msvc}" FILES "${source}")
	endforeach()
endfunction(def_vs_filters)

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)
add_library(${PROJ_NAME} ${LIB_TYPE} ${SRC_FILES})
if(WIN32)
	target_compile_options(${PROJ_NAME} PRIVATE /wd4251)
	target_compile_options(${PROJ_NAME} PRIVATE /wd4996)
endif()
def_vs_filters("${SRC_FILES}")

foreach(LIB IN LISTS LIBRARIES)
	target_link_libraries(${PROJ_NAME} ${${LIB}})
endforeach(LIB)

target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

foreach(INCLUDE_PATH IN LISTS INCLUDE_DIRS)
	target_include_directories(${PROJ_NAME} PRIVATE ${${INCLUDE_PATH}})
endforeach(INCLUDE_PATH)

set(TARGET_PROPERTIES LINKER_LANGUAGE CXX)
if(DEFINED PRECOMPILED_HEADER)
	include(PrecompiledHeader.cmake)
	add_precompiled_header(${PROJ_NAME} "src/${PRECOMPILED_HEADER}.h" c++17 FORCEINCLUDE)
endif()
set_target_properties(${PROJ_NAME} PROPERTIES ${TARGET_PROPERTIES})

set(BUILD_SHARED_LIBS OFF CACHE BOOL OFF FORCE)
add_subdirectory("third_party_libs/nvtt")
set(BUILD_SHARED_LIBS ON CACHE BOOL ON FORCE)

set(DEPENDENCY_NVIDIA_TEXTURE_TOOLS_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/third_party_libs/nvtt/src CACHE STRING "" FORCE)
set_target_properties(nvtt PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(bc6h PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(bc7 PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(CMP_Core PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(cubemaptest PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(EtcLib PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(filtertest PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(imperativeapi PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvassemble PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvcompress PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvcore PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvddsinfo PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvdecompress PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nv-gnome-thumbnailer PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvhdrtest PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvimage PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvimgdiff PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvmath PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvsquish PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvtestsuite PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvthread PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(nvzoom PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(posh PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(process_alpha_map PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(rg_etc1 PROPERTIES FOLDER third_party_libs/nvtt)
set_target_properties(squish PROPERTIES FOLDER third_party_libs/nvtt)

add_dependencies(${PROJ_NAME} nvtt)
add_dependencies(${PROJ_NAME} squish)
