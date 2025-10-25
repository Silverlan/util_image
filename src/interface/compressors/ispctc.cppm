// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "util_image_definitions.hpp"
#include <memory>
#include <optional>

export module pragma.image:compressors.ispctc;

export import :compressor;

export namespace uimg {
	class IspctcTextureCompressor : public ITextureCompressor {
	  public:
		static std::unique_ptr<ITextureCompressor> Create();
		virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) override;
	};
};
