// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __UTIL_IMAGE_COMPRESSOR_NVTT_HPP__
#define __UTIL_IMAGE_COMPRESSOR_NVTT_HPP__

#include "util_image_definitions.hpp"
#include "compressor.hpp"

namespace uimg {
	class NvttTextureCompressor : public ITextureCompressor {
	  public:
		static std::unique_ptr<ITextureCompressor> Create();
		virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) override;
	};
};

#endif
