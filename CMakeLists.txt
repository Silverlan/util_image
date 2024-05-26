include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

# TODO: Enable by default once it's fully integrated
option(USE_COMPRESSONATOR "Use Compressonator compressor instead of nvtt" OFF)

set(PROJ_NAME util_image)
pr_add_library(${PROJ_NAME} SHARED)

pr_add_dependency(${PROJ_NAME} vfilesystem TARGET PUBLIC)
pr_add_external_dependency(${PROJ_NAME} zlib LIBRARY)
pr_add_external_dependency(${PROJ_NAME} lpng LIBRARY)
pr_add_external_dependency(${PROJ_NAME} lpng_build HEADER_ONLY)

pr_add_headers(${PROJ_NAME} "include/")
pr_add_sources(${PROJ_NAME} "src/")

pr_add_compile_definitions(${PROJ_NAME} -DUIMG_ENABLE_NVTT PUBLIC)
pr_add_compile_definitions(${PROJ_NAME} -DUIMG_DLL)

set(COMPRESSONATOR_BASE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party_libs/compressonator")

set(DEPENDENCY_COMPRESSONATOR_INCLUDE "${COMPRESSONATOR_BASE_PATH}/cmp_compressonatorlib" CACHE PATH "Path to compressonator include directory.")
set(DEPENDENCY_COMPRESSONATOR_OPENEXR_HALF_INCLUDE "${COMPRESSONATOR_BASE_PATH}/common/lib/ext/openexr/ilmbase-2.2.0/Half" CACHE PATH "Path to compressonator_openexr_half include directory.")

set(TEX_COMPRESSION_LIBRARY_NVTT 0)
set(TEX_COMPRESSION_LIBRARY_COMPRESSONATOR 1)

if(${USE_COMPRESSONATOR})
	set(TEX_COMPRESSION_LIBRARY TEX_COMPRESSION_LIBRARY_COMPRESSONATOR)
else()
	set(TEX_COMPRESSION_LIBRARY TEX_COMPRESSION_LIBRARY_NVTT)
endif()

pr_add_compile_definitions(${PROJ_NAME} -DTEX_COMPRESSION_LIBRARY=${TEX_COMPRESSION_LIBRARY} PUBLIC)

if(${TEX_COMPRESSION_LIBRARY} EQUAL ${TEX_COMPRESSION_LIBRARY_NVTT})
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools_squish LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools_thread LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools_image LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools_bc7 LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools_bc6h LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools_math LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} nvidia_texture_tools_core LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} squish LIBRARY LINK_ONLY)
else()
	if(WIN32)
		set(COMPRESSONATOR_BIN_PATH "${COMPRESSONATOR_BASE_PATH}/build/sdk/bin/64/bin")

		set(DEPENDENCY_COMPRESSONATOR_CORE_LIBRARY "${COMPRESSONATOR_BIN_PATH}/Release_MD_DLL/CMP_Core.lib" CACHE FILEPATH "Path to compressonator_core library.")
		set(DEPENDENCY_COMPRESSONATOR_FRAMEWORK_LIBRARY "${COMPRESSONATOR_BIN_PATH}/Release_MD_DLL/CMP_Framework.lib" CACHE FILEPATH "Path to compressonator_framework library.")
	endif()
	
	pr_add_external_dependency(${PROJ_NAME} compressonator_core LIBRARY LINK_ONLY)
	pr_add_external_dependency(${PROJ_NAME} compressonator_framework LIBRARY LINK_ONLY)
	
	pr_add_external_dependency(${PROJ_NAME} compressonator HEADER_ONLY)
	pr_add_external_dependency(${PROJ_NAME} compressonator_openexr_half HEADER_ONLY)
endif()

pr_finalize(${PROJ_NAME})


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

	add_subdirectory("third_party_libs/compressonator")
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL ON FORCE)
