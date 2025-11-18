// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.image:compressors.nvtt;

export import :compressor;

export namespace uimg {
	class NvttTextureCompressor : public ITextureCompressor {
	  public:
		static std::unique_ptr<ITextureCompressor> Create();
		virtual std::optional<ResultData> Compress(const CompressInfo &compressInfo) override;
	};
};
