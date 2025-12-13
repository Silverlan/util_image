// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.image:compressors.compressonator;

export import :compressor;

export namespace pragma::image {
	class CompressonatorTextureCompressor : public ITextureCompressor {
	  public:
		static std::unique_ptr<ITextureCompressor> Create();
		virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) override;
	};
};
