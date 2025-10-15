// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <cinttypes>
#include <string>
#include "util_image_definitions.hpp"

export module pragma.image:texture_info;

import pragma.math;

export {
	namespace uimg {
		struct DLLUIMG TextureInfo {
			enum class InputFormat : uint8_t { KeepInputImageFormat = 0u, R8G8B8A8_UInt, R16G16B16A16_Float, R32G32B32A32_Float, R32_Float, B8G8R8A8_UInt, Count };
			enum class OutputFormat : uint8_t {
				KeepInputImageFormat,
				RGB,
				RGBA,
				BC1,
				BC1a,
				BC2,
				BC3,
				BC3n,
				BC4,
				BC5,
				BC6,
				BC7,
				CTX1,
				ETC1,
				ETC2_R,
				ETC2_RG,
				ETC2_RGB,
				ETC2_RGBA,
				ETC2_RGB_A1,
				ETC2_RGBM,

				Count,

				// Helper types for common use-cases
				DXT1 = BC1,
				DXT1a = BC1a,
				DXT3 = BC2,
				DXT5 = BC3,
				DXT5n = BC3n,
				ATIN = BC4,
				ATI2N = BC5,

				BCFirst = BC1,
				BCLast = BC7,

				ColorMap = DXT1,
				ColorMap1BitAlpha = DXT1a,
				ColorMapSharpAlpha = DXT3,
				ColorMapSmoothAlpha = DXT5,
				NormalMap = DXT5, // TODO: BC5 might be a better choice, but is currently not supported by cycles/oiio! DXT5n may also be a better choice but has some strange rules associated.
				HDRColorMap = BC6,
				GradientMap = BC4
			};
			enum class ContainerFormat : uint8_t {
				DDS = 0u,
				KTX,

				Count
			};
			enum class Flags : uint32_t { None = 0u, ConvertToNormalMap = 1u, SRGB = ConvertToNormalMap << 1u, GenerateMipmaps = SRGB << 1u };
			enum class MipmapFilter : uint8_t { Box = 0u, Kaiser };
			enum class WrapMode : uint8_t { Clamp = 0u, Repeat, Mirror };
			enum class AlphaMode : uint8_t { Auto = 0u, Transparency, None };
			InputFormat inputFormat = InputFormat::R8G8B8A8_UInt;
			OutputFormat outputFormat = OutputFormat::BC3;
			ContainerFormat containerFormat = ContainerFormat::KTX;
			Flags flags = Flags::None;
			MipmapFilter mipMapFilter = MipmapFilter::Box;
			WrapMode wrapMode = WrapMode::Mirror;
			AlphaMode alphaMode = AlphaMode::Auto;
			void SetNormalMap()
			{
				m_normalMap = true;
				outputFormat = OutputFormat::NormalMap;
				mipMapFilter = MipmapFilter::Kaiser;
			}
			bool IsNormalMap() const { return m_normalMap; }
		private:
			bool m_normalMap = false;
		};
		DLLUIMG std::string get_absolute_path(const std::string &fileName, uimg::TextureInfo::ContainerFormat containerFormat);
		using namespace umath::scoped_enum::bitwise;
	};
	namespace umath::scoped_enum::bitwise {
		template<>
		struct enable_bitwise_operators<uimg::TextureInfo::Flags> : std::true_type {};
	}
}
