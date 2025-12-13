// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.image:types;

export import std.compat;

export namespace pragma::image {
	enum class Format : uint8_t {
		None = 0u,

		R8,
		RG8,
		RGB8,
		RGBA8,

		R16,
		RG16,
		RGB16,
		RGBA16,

		R32,
		RG32,
		RGB32,
		RGBA32,
		Count,

		R_LDR = R8,
		RG_LDR = RG8,
		RGB_LDR = RGB8,
		RGBA_LDR = RGBA8,

		R_HDR = R16,
		RG_HDR = RG16,
		RGB_HDR = RGB16,
		RGBA_HDR = RGBA16,

		R_FLOAT = R32,
		RG_FLOAT = RG32,
		RGB_FLOAT = RGB32,
		RGBA_FLOAT = RGBA32
	};
	enum class Channel : uint8_t {
		Red = 0,
		Green,
		Blue,
		Alpha,

		Count,

		R = Red,
		G = Green,
		B = Blue,
		A = Alpha
	};
	struct DLLUIMG ChannelMask {
		Channel red : 8 = Channel::Red;
		Channel green : 8 = Channel::Green;
		Channel blue : 8 = Channel::Blue;
		Channel alpha : 8 = Channel::Alpha;
		bool operator==(const ChannelMask &) const = default;
		bool operator!=(const ChannelMask &) const = default;
		Channel &operator[](uint32_t idx) { return *reinterpret_cast<Channel *>(reinterpret_cast<uint8_t *>(this) + idx); }
		const Channel &operator[](uint32_t idx) const { return const_cast<ChannelMask *>(this)->operator[](idx); }
		void Reverse();
		ChannelMask GetReverse() const;
		void SwapChannels(Channel a, Channel b);
		std::optional<uint8_t> GetChannelIndex(Channel channel) const;
	};
	enum class ToneMapping : uint8_t { GammaCorrection = 0, Reinhard, HejilRichard, Uncharted, Aces, GranTurismo };
	enum class EdgeAddressMode : uint8_t {
		Clamp = 0,
		Reflect,
		Wrap,
		Zero,

		Count
	};
	enum class Filter : uint8_t {
		Default = 0,
		Box,
		Triangle,
		CubicBSpline,
		CatmullRom,
		Mitchell,

		Count
	};
	enum class ColorSpace : uint8_t {
		Auto = 0,
		Linear,
		SRGB,

		Count
	};
};
