include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

set(ENABLE_NVTT_OPTION OFF)
if(WIN32)
	# NVTT is the fastest option on Windows
    set(DEFAULT_ENABLE_NVTT ON)
    set(DEFAULT_ENABLE_COMPRESSONATOR OFF)
    set(DEFAULT_ENABLE_ISPC_TEXTURE_COMPRESSOR OFF)

	# nvtt does not work with clang on windows
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   		set(DEFAULT_ENABLE_NVTT OFF)
		set(DEFAULT_ENABLE_ISPC_TEXTURE_COMPRESSOR ON)
	else()
		set(ENABLE_NVTT_OPTION ON)
	endif()
else()
	# NVTT is not available on Linux.
	# Compressonator does not support GPU acceleration on Linux and can be very slow for large images.
	# ISPC TC does not support all BC compression types (e.g. BC2), but will fall back on compatible other types.
    set(DEFAULT_ENABLE_NVTT OFF)
    set(DEFAULT_ENABLE_COMPRESSONATOR OFF)
    set(DEFAULT_ENABLE_ISPC_TEXTURE_COMPRESSOR ON)
endif()

if(ENABLE_NVTT_OPTION)
	option(ENABLE_NVTT "Enable nvtt library" ${DEFAULT_ENABLE_NVTT})
else()
	set(ENABLE_NVTT ${DEFAULT_ENABLE_NVTT})
endif()
option(ENABLE_COMPRESSONATOR "Enable compressonator library" ${DEFAULT_ENABLE_COMPRESSONATOR})
option(ENABLE_ISPC_TEXTURE_COMPRESSOR "Enable ISPC Texture Compressor library" ${DEFAULT_ENABLE_ISPC_TEXTURE_COMPRESSOR})

option(ENABLE_SVG_SUPPORT "Enable SVG support" ON)

set(PROJ_NAME util_image)
pr_add_library(${PROJ_NAME} SHARED)

pr_add_dependency(${PROJ_NAME} gli TARGET PRIVATE)
pr_add_dependency(${PROJ_NAME} vfilesystem TARGET PUBLIC)
if(ENABLE_SVG_SUPPORT)
	pr_add_dependency(${PROJ_NAME} lunasvg TARGET)
endif()

pr_add_external_dependency(${PROJ_NAME} libpng LIBRARY PRIVATE)
pr_add_external_dependency(${PROJ_NAME} libpng_build HEADER_ONLY PRIVATE)
pr_add_external_dependency(${PROJ_NAME} zlib LIBRARY PRIVATE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

if(ENABLE_ISPC_TEXTURE_COMPRESSOR)
	find_package(ispctc REQUIRED)
endif()

set(SOURCE_EXCLUSION_FILTER)
if(NOT ENABLE_NVTT)
    list(APPEND SOURCE_EXCLUSION_FILTER "src/interface/compressors/nvtt.cppm")
    list(APPEND SOURCE_EXCLUSION_FILTER "src/implementation/compressors/nvtt.cpp")
endif()
if(NOT ENABLE_COMPRESSONATOR)
    list(APPEND SOURCE_EXCLUSION_FILTER "src/interface/compressors/compressonator.cppm")
    list(APPEND SOURCE_EXCLUSION_FILTER "src/implementation/compressors/compressonator.cpp")
endif()
if(NOT ENABLE_ISPC_TEXTURE_COMPRESSOR)
    list(APPEND SOURCE_EXCLUSION_FILTER "src/interface/compressors/ispctc.cppm")
    list(APPEND SOURCE_EXCLUSION_FILTER "src/implementation/compressors/ispctc.cpp")
endif()

if(DEFAULT_ENABLE_NVTT OR DEFAULT_ENABLE_COMPRESSONATOR OR DEFAULT_ENABLE_ISPC_TEXTURE_COMPRESSOR)
	pr_add_compile_definitions(${PROJ_NAME} -DUIMG_ENABLE_TEXTURE_COMPRESSION PUBLIC)
else()
	list(APPEND SOURCE_EXCLUSION_FILTER "src/interface/compressor.cppm")
	list(APPEND SOURCE_EXCLUSION_FILTER "src/implementation/texture.cpp")
endif()
if(ENABLE_SVG_SUPPORT)
	pr_add_compile_definitions(${PROJ_NAME} -DUIMG_ENABLE_SVG PUBLIC)
endif()
pr_add_compile_definitions(${PROJ_NAME} -DUIMG_DLL)

pr_init_module(${PROJ_NAME} EXCLUDE ${SOURCE_EXCLUSION_FILTER})

set(COMPRESSONATOR_BASE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party_libs/compressonator")

set(DEPENDENCY_COMPRESSONATOR_INCLUDE "${COMPRESSONATOR_BASE_PATH}/cmp_compressonatorlib" CACHE PATH "Path to compressonator include directory.")
set(DEPENDENCY_COMPRESSONATOR_OPENEXR_HALF_INCLUDE "${COMPRESSONATOR_BASE_PATH}/../common/lib/ext/openexr/ilmbase-2.2.0/Half" CACHE PATH "Path to compressonator_openexr_half include directory.")

if(ENABLE_NVTT)
	pr_add_compile_definitions(${PROJ_NAME} -DUIMG_ENABLE_NVTT=1)
endif()
if(ENABLE_COMPRESSONATOR)
	pr_add_compile_definitions(${PROJ_NAME} -DUIMG_ENABLE_COMPRESSONATOR=1)
endif()
if(ENABLE_ISPC_TEXTURE_COMPRESSOR)
	pr_add_compile_definitions(${PROJ_NAME} -DUIMG_ENABLE_ISPC_TEXTURE_COMPRESSOR=1)
endif()

if(ENABLE_NVTT)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL OFF FORCE)
	add_subdirectory("third_party_libs/nvtt")
	set(BUILD_SHARED_LIBS ON CACHE BOOL ON FORCE)

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

	pr_add_dependency(${PROJ_NAME} nvtt TARGET PRIVATE)
	pr_add_dependency(${PROJ_NAME} nvsquish TARGET PRIVATE)
	pr_add_dependency(${PROJ_NAME} nvthread TARGET PRIVATE)
	pr_add_dependency(${PROJ_NAME} nvimage TARGET PRIVATE)
	pr_add_dependency(${PROJ_NAME} bc7 TARGET PRIVATE)
	pr_add_dependency(${PROJ_NAME} bc6h TARGET PRIVATE)
	pr_add_dependency(${PROJ_NAME} nvmath TARGET PRIVATE)
	pr_add_dependency(${PROJ_NAME} nvcore TARGET PRIVATE)
	#TODO: This would get shoved in. If TEX_COMPRESSION_LIBRARY would change to compressonator, this dep would be lost.
	pr_add_dependency(${PROJ_NAME} squish TARGET PRIVATE)
endif()

if(ENABLE_COMPRESSONATOR)
	if(UNIX)
		set(DEPENDENCY_COMPRESSONATOR_FRAMEWORK_INCLUDE "${DEPENDENCY_COMPRESSONATOR_SOURCE_DIR}/cmp_compressonatorlib")
		set(DEPENDENCY_COMPRESSONATOR_FRAMEWORK_LIBRARY "${DEPENDENCY_COMPRESSONATOR_LIBRARY_DIR}/libCMP_Framework.so")
		pr_add_external_dependency(${PROJ_NAME} compressonator_framework LIBRARY PRIVATE)

		set(DEPENDENCY_COMPRESSONATOR_LIBRARY "${DEPENDENCY_COMPRESSONATOR_LIBRARY_DIR}/libCMP_Compressonator.so")
		pr_add_external_dependency(${PROJ_NAME} compressonator LIBRARY PRIVATE)
	else()
		set(DEPENDENCY_COMPRESSONATOR_FRAMEWORK_INCLUDE "${DEPENDENCY_COMPRESSONATOR_SOURCE_DIR}/cmp_compressonatorlib")
		set(DEPENDENCY_COMPRESSONATOR_FRAMEWORK_LIBRARY "${DEPENDENCY_COMPRESSONATOR_LIBRARY_DIR}/CMP_Framework.lib")
		pr_add_external_dependency(${PROJ_NAME} compressonator_framework LIBRARY PRIVATE)

		set(DEPENDENCY_COMPRESSONATOR_LIBRARY "${DEPENDENCY_COMPRESSONATOR_LIBRARY_DIR}/CMP_Compressonator.lib")
		pr_add_external_dependency(${PROJ_NAME} compressonator LIBRARY PRIVATE)

		set(DEPENDENCY_COMPRESSONATOR_CORE_LIBRARY "${DEPENDENCY_COMPRESSONATOR_LIBRARY_DIR}/CMP_Core.lib")
		pr_add_external_dependency(${PROJ_NAME} compressonator_core LIBRARY PRIVATE)

		set(DEPENDENCY_COMPRESSONATOR_COMMON_LIBRARY "${DEPENDENCY_COMPRESSONATOR_LIBRARY_DIR}/CMP_Common.lib")
		pr_add_external_dependency(${PROJ_NAME} compressonator_common LIBRARY PRIVATE)
	endif()

	set(DEPENDENCY_COMPRESSONATOR_HALF_INCLUDE "${DEPENDENCY_COMPRESSONATOR_SOURCE_DIR}/cmp_framework/common/half")
	pr_add_external_dependency(${PROJ_NAME} compressonator_half HEADER_ONLY PRIVATE)
endif()

if(ENABLE_ISPC_TEXTURE_COMPRESSOR)
	pr_add_third_party_dependency(${PROJ_NAME} ispctc)
endif()

pr_finalize(${PROJ_NAME})
