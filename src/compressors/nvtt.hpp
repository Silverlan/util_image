/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
