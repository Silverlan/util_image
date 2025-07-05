// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "compressor.hpp"
#ifdef UIMG_ENABLE_NVTT
#include "compressors/nvtt.hpp"
#endif
#ifdef UIMG_ENABLE_COMPRESSONATOR
#include "compressors/compressonator.hpp"
#endif
#ifdef UIMG_ENABLE_ISPC_TEXTURE_COMPRESSOR
#include "compressors/ispctc.hpp"
#endif

std::unique_ptr<uimg::ITextureCompressor> uimg::ITextureCompressor::Create(CompressorLibrary libType)
{
	switch(libType) {
#ifdef UIMG_ENABLE_NVTT
	case CompressorLibrary::Nvtt:
		return NvttTextureCompressor::Create();
#endif
#ifdef UIMG_ENABLE_COMPRESSONATOR
	case CompressorLibrary::Compressonator:
		return CompressonatorTextureCompressor::Create();
#endif
#ifdef UIMG_ENABLE_ISPC_TEXTURE_COMPRESSOR
	case CompressorLibrary::Ispctc:
		return IspctcTextureCompressor::Create();
#endif
	default:
		break;
	}
	return {};
}
