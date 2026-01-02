// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module pragma.image;

import :compressor;
#ifdef UIMG_ENABLE_NVTT
import :compressors.nvtt;
#endif
#ifdef UIMG_ENABLE_COMPRESSONATOR
import :compressors.compressonator;
#endif
#ifdef UIMG_ENABLE_ISPC_TEXTURE_COMPRESSOR
import :compressors.ispctc;
#endif

std::unique_ptr<pragma::image::ITextureCompressor> pragma::image::ITextureCompressor::Create(CompressorLibrary libType)
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
