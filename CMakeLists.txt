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

set(CMAKE_CXX_STANDARD 20)
set(ENABLE_STATIC_LIBRARY_FLAG 0)
# set(PRECOMPILED_HEADER "stdafx")

link_external_library(sharedutils)
link_external_library(mathutil)
link_external_library(vfilesystem)
link_external_library(lpng)
link_external_library(zlib)

set(COMPRESSONATOR_BASE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party_libs/compressonator")

set(DEPENDENCY_COMPRESSONATOR_INCLUDE "${COMPRESSONATOR_BASE_PATH}/cmp_compressonatorlib" CACHE PATH "Path to compressonator include directory.")
set(DEPENDENCY_COMPRESSONATOR_OPENEXR_HALF_INCLUDE "${COMPRESSONATOR_BASE_PATH}/common/lib/ext/openexr/ilmbase-2.2.0/Half" CACHE PATH "Path to compressonator_openexr_half include directory.")

add_definitions(-DUIMG_ENABLE_NVTT)
add_definitions(-DUIMG_DLL)

# TODO: Enable by default once it's fully integrated
option(USE_COMPRESSONATOR "Use Compressonator compressor instead of nvtt" OFF)


set(TEX_COMPRESSION_LIBRARY_NVTT 0)
set(TEX_COMPRESSION_LIBRARY_COMPRESSONATOR 1)

if(${USE_COMPRESSONATOR})
	set(TEX_COMPRESSION_LIBRARY TEX_COMPRESSION_LIBRARY_COMPRESSONATOR)
else()
	set(TEX_COMPRESSION_LIBRARY TEX_COMPRESSION_LIBRARY_NVTT)
endif()

add_definitions(-DTEX_COMPRESSION_LIBRARY=${TEX_COMPRESSION_LIBRARY})

if(${TEX_COMPRESSION_LIBRARY} EQUAL ${TEX_COMPRESSION_LIBRARY_NVTT})
	link_external_library(nvidia_texture_tools)
	link_external_library(nvidia_texture_tools_squish)
	link_external_library(nvidia_texture_tools_thread)
	link_external_library(nvidia_texture_tools_image)
	link_external_library(nvidia_texture_tools_bc7)
	link_external_library(nvidia_texture_tools_bc6h)
	link_external_library(nvidia_texture_tools_math)
	link_external_library(nvidia_texture_tools_core)
	link_external_library(squish)
else()
	if(WIN32)
		set(COMPRESSONATOR_BIN_PATH "${COMPRESSONATOR_BASE_PATH}/build/sdk/bin/64/bin")

		set(DEPENDENCY_COMPRESSONATOR_CORE_LIBRARY "${COMPRESSONATOR_BIN_PATH}/Release_MD_DLL/CMP_Core.lib" CACHE FILEPATH "Path to compressonator_core library.")
		set(DEPENDENCY_COMPRESSONATOR_FRAMEWORK_LIBRARY "${COMPRESSONATOR_BIN_PATH}/Release_MD_DLL/CMP_Framework.lib" CACHE FILEPATH "Path to compressonator_framework library.")
		set(DEPENDENCY_COMPRESSONATOR_LIBRARY "${COMPRESSONATOR_BIN_PATH}/Release_MD_DLL/CMP_Compressonator.lib" CACHE FILEPATH "Path to compressonator_framework library.")
		set(DEPENDENCY_COMPRESSONATOR_COMMON_LIBRARY "${COMPRESSONATOR_BIN_PATH}/Release_MD_DLL/CMP_Compressonator.lib" CACHE FILEPATH "Path to compressonator_framework library.")
	else()
		set(COMPRESSONATOR_BIN_PATH "${CMAKE_BINARY_DIR}/lib")
		set(DEPENDENCY_COMPRESSONATOR_CORE_LIBRARY "${COMPRESSONATOR_BIN_PATH}/libCMP_Core.a" CACHE FILEPATH "Path to compressonator_core library.")
		set(DEPENDENCY_COMPRESSONATOR_FRAMEWORK_LIBRARY "${COMPRESSONATOR_BIN_PATH}/libCMP_Framework.a" CACHE FILEPATH "Path to compressonator_framework library.")
		set(DEPENDENCY_COMPRESSONATOR_LIBRARY "${COMPRESSONATOR_BIN_PATH}/libCMP_Compressonator.a" CACHE FILEPATH "Path to compressonator library.")
		set(DEPENDENCY_COMPRESSONATOR_COMMON_LIBRARY "${COMPRESSONATOR_BIN_PATH}/libCMP_Common.a" CACHE FILEPATH "Path to compressonator library.")
	endif()

	link_external_library(compressonator_core)
	link_external_library(compressonator_framework)
	#Texture codecs here
	link_external_library(compressonator)
	link_external_library(compressonator_common)
	
	add_include_dir(compressonator)
	add_include_dir(compressonator_openexr_half)
endif()

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

if(${TEX_COMPRESSION_LIBRARY} EQUAL ${TEX_COMPRESSION_LIBRARY_NVTT})
	add_subdirectory("third_party_libs/nvtt")

	set(DEPENDENCY_NVIDIA_TEXTURE_TOOLS_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/third_party_libs/nvtt/src CACHE PATH "" FORCE)
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

	#HACK: Somehow this path was not included to util_image?
	target_include_directories(${PROJ_NAME} PRIVATE ${DEPENDENCY_NVIDIA_TEXTURE_TOOLS_INCLUDE})
	add_dependencies(${PROJ_NAME} squish)
else()
	set(OPTION_ENABLE_ALL_APPS OFF CACHE BOOL "Enable all apps" FORCE)
	set(OPTION_BUILD_CMP_SDK ON CACHE BOOL "Build Compressonator SDK" FORCE)
	set(OPTION_BUILD_APPS_CMP_CLI ON CACHE BOOL "Build Application: CLI" FORCE)

	add_subdirectory("third_party_libs/compressonator")
	add_dependencies(${PROJ_NAME} CMP_Compressonator CMP_Framework CMP_Common CMP_Core Image_DDS Image_EXR Image_KTX Image_TGA)

	#FIXME: This might change without warning.

	#I Hate this\

	target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/third_party_libs/compressonator/cmp_framework/common/half/)
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL ON FORCE)
