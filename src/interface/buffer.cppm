// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "definitions.hpp"

export module pragma.image:buffer;

export import :types;
export import pragma.math;

export {
	namespace pragma::image {
		DLLUIMG float calc_luminance(const Vector3 &color);

		class DLLUIMG ImageBuffer : public std::enable_shared_from_this<ImageBuffer> {
		  public:
			static constexpr uint8_t FULLY_TRANSPARENT = 0u;
			static constexpr uint8_t FULLY_OPAQUE = std::numeric_limits<uint8_t>::max();
			using Offset = size_t;
			using Size = size_t;
			using PixelIndex = uint32_t;
			using LDRValue = uint8_t;
			using HDRValue = uint16_t;
			using FloatValue = float;
			class PixelIterator;
			struct DLLUIMG PixelView {
				Offset GetOffset() const;
				PixelIndex GetPixelIndex() const;
				uint32_t GetX() const;
				uint32_t GetY() const;
				const void *GetPixelData() const;
				void *GetPixelData();
				LDRValue GetLDRValue(Channel channel) const;
				HDRValue GetHDRValue(Channel channel) const;
				FloatValue GetFloatValue(Channel channel) const;
				void SetValue(Channel channel, LDRValue value);
				void SetValue(Channel channel, HDRValue value);
				void SetValue(Channel channel, FloatValue value);
				void CopyValues(const PixelView &outOther);
				void CopyValue(Channel channel, const PixelView &outOther);

				ImageBuffer &GetImageBuffer() const;
			  private:
				PixelView(ImageBuffer &imgBuffer, Offset offset);
				Offset GetAbsoluteOffset() const;
				friend PixelIterator;
				friend ImageBuffer;
				ImageBuffer &m_imageBuffer;
				Offset m_offset = 0u;
			};
			class DLLUIMG PixelIterator {
			  public:
				using iterator_category = std::forward_iterator_tag;
				using value_type = PixelView;
				using difference_type = PixelView;
				using pointer = PixelView *;
				using reference = PixelView &;

				PixelIterator &operator++();
				PixelIterator operator++(int);
				PixelView &operator*();
				PixelView *operator->();
				bool operator==(const PixelIterator &other) const;
				bool operator!=(const PixelIterator &other) const;
			  private:
				friend ImageBuffer;
				PixelIterator(ImageBuffer &imgBuffer, Offset offset);
				PixelView m_pixelView;
			};

			static std::shared_ptr<ImageBuffer> Create(void *data, uint32_t width, uint32_t height, Format format, bool ownedExternally = true);
			static std::shared_ptr<ImageBuffer> CreateWithCustomDeleter(void *data, uint32_t width, uint32_t height, Format format, const std::function<void(void *)> &customDeleter);
			static std::shared_ptr<ImageBuffer> Create(const void *data, uint32_t width, uint32_t height, Format format);
			static std::shared_ptr<ImageBuffer> Create(uint32_t width, uint32_t height, Format format);
			static std::shared_ptr<ImageBuffer> Create(ImageBuffer &parent, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
			// Order: Right, left, up, down, forward, backward
			static std::shared_ptr<ImageBuffer> CreateCubemap(const std::array<std::shared_ptr<ImageBuffer>, 6> &cubemapSides);
			static Size GetPixelSize(Format format);
			static uint8_t GetChannelSize(Format format);
			static uint8_t GetChannelCount(Format format);
			static LDRValue ToLDRValue(HDRValue value);
			static LDRValue ToLDRValue(FloatValue value);
			static HDRValue ToHDRValue(LDRValue value);
			static HDRValue ToHDRValue(FloatValue value);
			static FloatValue ToFloatValue(LDRValue value);
			static FloatValue ToFloatValue(HDRValue value);
			static Format ToLDRFormat(Format format);
			static Format ToHDRFormat(Format format);
			static Format ToFloatFormat(Format format);
			static Format ToRGBFormat(Format format);
			static Format ToRGBAFormat(Format format);

			ImageBuffer(const ImageBuffer &) = default;
			ImageBuffer &operator=(const ImageBuffer &) = default;
			Format GetFormat() const;
			uint32_t GetWidth() const;
			uint32_t GetHeight() const;
			uint8_t GetChannelCount() const;
			uint8_t GetChannelSize() const;
			Size GetPixelSize() const;
			uint32_t GetPixelCount() const;
			bool HasAlphaChannel() const;
			bool IsLDRFormat() const;
			bool IsHDRFormat() const;
			bool IsFloatFormat() const;
			const void *GetData() const;
			void *GetData();
			void Insert(const ImageBuffer &other, uint32_t x, uint32_t y, uint32_t xOther, uint32_t yOther, uint32_t wOther, uint32_t hOther);
			void Insert(const ImageBuffer &other, uint32_t x, uint32_t y);
			std::shared_ptr<ImageBuffer> Copy() const;
			std::shared_ptr<ImageBuffer> Copy(Format format) const;
			void Copy(ImageBuffer &dst, uint32_t xSrc, uint32_t ySrc, uint32_t xDst, uint32_t yDst, uint32_t w, uint32_t h) const;
			bool Copy(ImageBuffer &dst) const;
			void Convert(Format targetFormat);
			void Convert(ImageBuffer &dst);
			void SwapChannels(Channel channel0, Channel channel1);
			void SwapChannels(ChannelMask swizzle);
			void ToLDR();
			void ToHDR();
			void ToFloat();
			std::shared_ptr<ImageBuffer> ApplyToneMapping(ToneMapping toneMappingMethod);
			std::shared_ptr<ImageBuffer> ApplyToneMapping(const std::function<std::array<uint8_t, 3>(const Vector3 &)> &fToneMapper);
			void ApplyExposure(float exposure);
			void ApplyGammaCorrection(float gamma = 2.2f);
			void Reset(void *data, uint32_t width, uint32_t height, Format format);

			Size GetSize() const;

			void Clear(const Color &color);
			void Clear(const Vector4 &color);
			void ClearAlpha(LDRValue alpha = std::numeric_limits<LDRValue>::max());

			PixelIndex GetPixelIndex(uint32_t x, uint32_t y) const;
			Offset GetPixelOffset(uint32_t x, uint32_t y) const;
			Offset GetPixelOffset(PixelIndex index) const;
			void ClampPixelCoordinatesToBounds(uint32_t &inOutX, uint32_t &inOutY) const;
			void ClampBounds(uint32_t &inOutX, uint32_t &inOutY, uint32_t &inOutW, uint32_t &inOutH) const;

			void Read(Offset offset, Size size, void *outData);
			void Write(Offset offset, Size size, const void *inData);
			void Resize(Size width, Size height, EdgeAddressMode addressMode = EdgeAddressMode::Clamp, Filter filter = Filter::Default, ColorSpace colorSpace = ColorSpace::Auto);

			size_t GetRowStride() const;
			size_t GetPixelStride() const;
			void FlipHorizontally();
			void FlipVertically();
			void Flip(bool horizontally, bool vertically);
			void Crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h, void *optOutCroppedData = nullptr);

			void InitPixelView(uint32_t x, uint32_t y, PixelView &pxView);
			PixelView GetPixelView(Offset offset = 0);
			PixelView GetPixelView(uint32_t x, uint32_t y);

			void SetPixelColor(uint32_t x, uint32_t y, const std::array<uint8_t, 4> &color);
			void SetPixelColor(PixelIndex index, const std::array<uint8_t, 4> &color);
			void SetPixelColor(uint32_t x, uint32_t y, const std::array<uint16_t, 4> &color);
			void SetPixelColor(PixelIndex index, const std::array<uint16_t, 4> &color);
			void SetPixelColor(uint32_t x, uint32_t y, const Vector4 &color);
			void SetPixelColor(PixelIndex index, const Vector4 &color);

			ImageBuffer *GetParent();
			const std::pair<uint64_t, uint64_t> &GetPixelCoordinatesRelativeToParent() const;
			Offset GetAbsoluteOffset(Offset localOffset) const;

			void CalcLuminance(float &outAvgLuminance, float &outMinLuminance, float &outMaxLuminance, Vector3 &outAvgIntensity, float *optOutLogAvgLuminance = nullptr) const;

			PixelIterator begin();
			PixelIterator end();
		  private:
			static void Convert(ImageBuffer &srcImg, ImageBuffer &dstImg, Format targetFormat);
			ImageBuffer(const std::shared_ptr<void> &data, uint32_t width, uint32_t height, Format format);
			std::pair<uint32_t, uint32_t> GetPixelCoordinates(Offset offset) const;
			void Reallocate();
			std::shared_ptr<void> m_data = nullptr;
			uint32_t m_width = 0u;
			uint32_t m_height = 0u;
			Format m_format = Format::None;

			std::weak_ptr<ImageBuffer> m_parent = {};
			std::pair<uint64_t, uint64_t> m_offsetRelToParent = {};
		};

		struct DLLUIMG ImageLayerSet {
			std::unordered_map<std::string, std::shared_ptr<ImageBuffer>> images;
		};

		DLLUIMG std::optional<ToneMapping> string_to_tone_mapping(const std::string &str);
	};
	DLLUIMG std::ostream &operator<<(std::ostream &out, const pragma::image::ImageBuffer &o);
}
