/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_TEXTURE_INFO_HPP__
#define __UTIL_TEXTURE_INFO_HPP__

#include <cinttypes>
#include <mathutil/umath.h>
#include <string>
#include "util_image_definitions.hpp"
#include "util_image_types.hpp"

namespace uimg
{
	struct DLLUIMG TextureInfo
	{
		enum class InputFormat : uint8_t
		{
			KeepInputImageFormat = 0u,
			R8G8B8A8_UInt,
			R16G16B16A16_Float,
			R32G32B32A32_Float,
			R32_Float,
			B8G8R8A8_UInt,
			Count
		};
		enum class OutputFormat : uint8_t
		{
			KeepInputImageFormat,
			RGB,
			RGBA,
			DXT1,
			DXT1a,
			DXT3,
			DXT5,
			DXT5n,
			BC1,
			BC1a,
			BC2,
			BC3,
			BC3n,
			BC4,
			BC5,
			DXT1n,
			CTX1,
			BC6,
			BC7,
			BC3_RGBM,
			ETC1,
			ETC2_R,
			ETC2_RG,
			ETC2_RGB,
			ETC2_RGBA,
			ETC2_RGB_A1,
			ETC2_RGBM,

			Count,

			// Helper types for common use-cases
			ColorMap = DXT1,
			ColorMap1BitAlpha = DXT1a,
			ColorMapSharpAlpha = DXT3,
			ColorMapSmoothAlpha = DXT5,
			NormalMap = DXT5, // TODO: BC5 might be a better choice, but is currently not supported by cycles/oiio! DXT5n may also be a better choice but has some strange rules associated.
			HDRColorMap = BC6,
			GradientMap = BC4
		};
		enum class ContainerFormat : uint8_t
		{
			DDS = 0u,
			KTX,

			Count
		};
		enum class Flags : uint32_t
		{
			None = 0u,
			ConvertToNormalMap = 1u,
			SRGB = ConvertToNormalMap<<1u,
			GenerateMipmaps = SRGB<<1u
		};
		enum class MipmapFilter : uint8_t
		{
			Box = 0u,
			Kaiser
		};
		enum class WrapMode : uint8_t
		{
			Clamp = 0u,
			Repeat,
			Mirror
		};
		enum class AlphaMode : uint8_t
		{
			Auto = 0u,
			Transparency,
			None
		};
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
		bool IsNormalMap() const {return m_normalMap;}
	private:
		bool m_normalMap = false;
	};
	DLLUIMG std::string get_absolute_path(const std::string &fileName,uimg::TextureInfo::ContainerFormat containerFormat);
};
REGISTER_BASIC_BITWISE_OPERATORS(uimg::TextureInfo::Flags)

#endif
