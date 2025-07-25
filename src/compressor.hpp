// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __UTIL_IMAGE_TEXTURE_COMPRESSOR_HPP__
#define __UTIL_IMAGE_TEXTURE_COMPRESSOR_HPP__

#ifdef UIMG_ENABLE_TEXTURE_COMPRESSION

#define TEX_COMPRESSION_LIBRARY_NVTT 0
#define TEX_COMPRESSION_LIBRARY_COMPRESSONATOR 1

#ifndef TEX_COMPRESSION_LIBRARY

#define TEX_COMPRESSION_LIBRARY TEX_COMPRESSION_LIBRARY_NVTT

#endif

#include "util_image.hpp"
#include "util_texture_info.hpp"
#include <functional>
#include <string>
#include <variant>
#include <optional>

namespace uimg {
	class ITextureCompressor {
	  public:
		static std::unique_ptr<ITextureCompressor> Create(CompressorLibrary libType);
		struct CompressInfo {
			std::function<const uint8_t *(uint32_t, uint32_t, std::function<void()> &)> getImageData;
			std::function<void(const std::string &)> errorHandler;
			uimg::TextureSaveInfo textureSaveInfo;
			std::variant<uimg::TextureOutputHandler, std::string> outputHandler;
			uint32_t width;
			uint32_t height;
			uint32_t numMipmaps;
			uint32_t numLayers;
			bool absoluteFileName;
		};
		struct ResultData {
			std::optional<std::string> outputFilePath;
		};
		virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) = 0;
	};
};

#endif

#endif
