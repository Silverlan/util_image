// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

module pragma.image;

import :buffer;

std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::Create(const void *data, uint32_t width, uint32_t height, Format format) { return Create(const_cast<void *>(data), width, height, format, false); }
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::CreateWithCustomDeleter(void *data, uint32_t width, uint32_t height, Format format, const std::function<void(void *)> &customDeleter)
{
	if(customDeleter == nullptr) {
		auto buf = Create(width, height, format);
		if(data)
			buf->Write(0, width * height * GetPixelSize(format), data);
		return buf;
	}
	auto ptrData = std::shared_ptr<void> {data, customDeleter};
	return std::shared_ptr<ImageBuffer> {new ImageBuffer {ptrData, width, height, format}};
}
void pragma::image::ImageBuffer::Reset(void *data, uint32_t width, uint32_t height, Format format)
{
	m_data.reset(data, [](void *) {});
	m_width = width;
	m_height = height;
	m_format = format;
}
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::Create(void *data, uint32_t width, uint32_t height, Format format, bool ownedExternally)
{
	if(ownedExternally == false)
		return CreateWithCustomDeleter(data, width, height, format, nullptr);
	return CreateWithCustomDeleter(data, width, height, format, [](void *) {});
}
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::Create(uint32_t width, uint32_t height, Format format)
{
	auto buf = std::shared_ptr<ImageBuffer> {new ImageBuffer {nullptr, width, height, format}};
	buf->Reallocate();
	return buf;
}
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::Create(ImageBuffer &parent, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	auto xMax = parent.GetWidth();
	auto yMax = parent.GetHeight();
	x = pragma::math::min(x, xMax);
	y = pragma::math::min(y, yMax);
	w = pragma::math::min(x + w, xMax) - x;
	h = pragma::math::min(y + h, yMax) - y;

	auto buf = std::shared_ptr<ImageBuffer> {new ImageBuffer {nullptr, w, h, parent.GetFormat()}};
	buf->m_offsetRelToParent = {x, y};
	buf->m_parent = parent.shared_from_this();
	buf->m_data = parent.m_data;
	return buf;
}
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::CreateCubemap(const std::array<std::shared_ptr<ImageBuffer>, 6> &cubemapSides)
{
	auto &img0 = cubemapSides.front();
	auto w = img0->GetWidth();
	auto h = img0->GetHeight();
	auto format = img0->GetFormat();
	for(auto &img : cubemapSides) {
		// Make sure all sides have the same format
		if(img->GetWidth() != w || img->GetHeight() != h || img->GetFormat() != format)
			return nullptr;
	}
	auto cubemap = Create(w * 4, h * 3, format);
	cubemap->Clear(Vector4 {0.f, 0.f, 0.f, 0.f});

	std::array<std::pair<uint32_t, uint32_t>, 6> pxOffsetCoords = {
	  std::pair<uint32_t, uint32_t> {w * 2, h}, // Right
	  std::pair<uint32_t, uint32_t> {0, h},     // Left
	  std::pair<uint32_t, uint32_t> {w, 0},     // Top
	  std::pair<uint32_t, uint32_t> {w, h * 2}, // Bottom
	  std::pair<uint32_t, uint32_t> {w, h},     // Front
	  std::pair<uint32_t, uint32_t> {w * 3, h}  // Back
	};
	std::array<std::thread, 6> threads {};
	for(auto i = decltype(pxOffsetCoords.size()) {0u}; i < pxOffsetCoords.size(); ++i) {
		auto coords = pxOffsetCoords.at(i);
		auto imgSide = cubemapSides.at(i);
		threads.at(i) = std::thread {[i, coords, cubemap, &cubemapSides, w, h]() { cubemapSides.at(i)->Copy(*cubemap, 0, 0, coords.first, coords.second, w, h); }};
	}
	for(auto &t : threads)
		t.join();
	return cubemap;
}
pragma::image::ImageBuffer::LDRValue pragma::image::ImageBuffer::ToLDRValue(HDRValue value) { return pragma::math::clamp<uint32_t>(pragma::math::float16_to_float32_glm(value) * static_cast<float>(std::numeric_limits<uint8_t>::max()), 0, std::numeric_limits<uint8_t>::max()); }
pragma::image::ImageBuffer::LDRValue pragma::image::ImageBuffer::ToLDRValue(FloatValue value) { return pragma::math::clamp<uint32_t>(value * static_cast<float>(std::numeric_limits<uint8_t>::max()), 0, std::numeric_limits<uint8_t>::max()); }
pragma::image::ImageBuffer::HDRValue pragma::image::ImageBuffer::ToHDRValue(LDRValue value) { return ToHDRValue(ToFloatValue(value)); }
pragma::image::ImageBuffer::HDRValue pragma::image::ImageBuffer::ToHDRValue(FloatValue value) { return pragma::math::float32_to_float16_glm(value); }
pragma::image::ImageBuffer::FloatValue pragma::image::ImageBuffer::ToFloatValue(LDRValue value) { return value / static_cast<float>(std::numeric_limits<LDRValue>::max()); }
pragma::image::ImageBuffer::FloatValue pragma::image::ImageBuffer::ToFloatValue(HDRValue value) { return pragma::math::float16_to_float32_glm(value); }
pragma::image::Format pragma::image::ImageBuffer::ToLDRFormat(Format format)
{
	switch(format) {
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return Format::R8;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return Format::RG8;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB8;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA8;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return Format::None;
}
pragma::image::Format pragma::image::ImageBuffer::ToHDRFormat(Format format)
{
	switch(format) {
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return Format::R16;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return Format::RG16;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB16;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA16;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return Format::None;
}
pragma::image::Format pragma::image::ImageBuffer::ToFloatFormat(Format format)
{
	switch(format) {
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return Format::R32;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return Format::RG32;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return Format::RGB32;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return Format::RGBA32;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return Format::None;
}
pragma::image::Format pragma::image::ImageBuffer::ToRGBFormat(Format format)
{
	switch(format) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return Format::RGB8;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return Format::RGB16;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return Format::RGB32;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return Format::None;
}
pragma::image::Format pragma::image::ImageBuffer::ToRGBAFormat(Format format)
{
	switch(format) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return Format::RGBA8;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return Format::RGBA16;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return Format::RGBA32;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return Format::None;
}
size_t pragma::image::ImageBuffer::GetRowStride() const { return GetPixelSize() * GetWidth(); }
size_t pragma::image::ImageBuffer::GetPixelStride() const { return GetPixelSize(); }
void pragma::image::ImageBuffer::FlipHorizontally()
{
	auto w = GetWidth();
	auto h = GetHeight();
	auto rowStride = GetRowStride();
	auto pxStride = GetPixelStride();
	auto *p = static_cast<uint8_t *>(GetData());
	for(auto y = decltype(h) {0u}; y < h; ++y) {
		pragma::util::flip_item_sequence(p, w * pxStride, w, pxStride);
		p += rowStride;
	}
}
void pragma::image::ImageBuffer::FlipVertically()
{
	auto w = GetWidth();
	auto h = GetHeight();
	pragma::util::flip_item_sequence(GetData(), w * h * GetPixelStride(), h, GetRowStride());
}
void pragma::image::ImageBuffer::Flip(bool horizontally, bool vertically)
{
	if(horizontally && vertically) {
		// Optimized algorithm for flipping both axes at once
		auto w = GetWidth();
		auto h = GetHeight();
		auto rowStride = GetRowStride();
		auto pxStride = GetPixelStride();
		auto *sequence = GetData();
		auto *tmp = new uint8_t[rowStride];
		auto *row0 = static_cast<uint8_t *>(sequence);
		auto *row1 = row0 + GetSize() - rowStride;
		for(auto y = decltype(h) {0u}; y < h / 2; ++y) {
			memcpy(tmp, row1, rowStride);
			memcpy(row1, row0, rowStride);
			memcpy(row0, tmp, rowStride);

			// Flip both rows horizontally
			for(auto *rowStart : {row0, row1}) {
				auto *px0 = rowStart;
				auto *px1 = rowStart + rowStride - pxStride;
				for(auto x = decltype(w) {0u}; x < w / 2; ++x) {
					memcpy(tmp, px1, pxStride);
					memcpy(px1, px0, pxStride);
					memcpy(px0, tmp, pxStride);

					px0 += pxStride;
					px1 -= pxStride;
				}
			}

			row0 += rowStride;
			row1 -= rowStride;
		}
		if((h % 2) != 0) {
			// Uneven height; We still have to flip the center row horizontally
			auto *p = static_cast<uint8_t *>(sequence) + (h / 2) * GetRowStride();
			pragma::util::flip_item_sequence(p, GetRowStride(), GetWidth(), GetPixelStride());
		}
		delete[] tmp;
		return;
	}
	if(horizontally)
		FlipHorizontally();
	if(vertically)
		FlipVertically();
}
void pragma::image::ImageBuffer::Crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h, void *optOutCroppedData)
{
	std::shared_ptr<void> newData = nullptr;
	void *newDataPtr = optOutCroppedData;
	if(optOutCroppedData == nullptr) {
		newData = std::shared_ptr<void> {new uint8_t[w * h * GetPixelStride()], [](const void *ptr) { delete[] static_cast<const uint8_t *>(ptr); }};
		newDataPtr = newData.get();
	}
	auto *srcData = static_cast<uint8_t *>(GetData());
	auto *dstData = static_cast<uint8_t *>(newDataPtr);
	auto srcRowStride = GetRowStride();
	auto dstRowStride = w * GetPixelStride();
	for(auto yc = y; yc < (y + h); ++yc) {
		memmove(dstData, srcData, dstRowStride);
		srcData += srcRowStride;
		dstData += dstRowStride;
	}
	if(newData)
		m_data = newData;
	m_width = w;
	m_height = h;
}
void pragma::image::ImageBuffer::InitPixelView(uint32_t x, uint32_t y, PixelView &pxView) { pxView.m_offset = GetPixelOffset(x, y); }
pragma::image::ImageBuffer::PixelView pragma::image::ImageBuffer::GetPixelView(Offset offset) { return PixelView {*this, offset}; }
pragma::image::ImageBuffer::PixelView pragma::image::ImageBuffer::GetPixelView(uint32_t x, uint32_t y) { return GetPixelView(GetPixelOffset(x, y)); }
void pragma::image::ImageBuffer::SetPixelColor(uint32_t x, uint32_t y, const std::array<uint8_t, 4> &color) { SetPixelColor(GetPixelIndex(x, y), color); }
void pragma::image::ImageBuffer::SetPixelColor(PixelIndex index, const std::array<uint8_t, 4> &color)
{
	auto pxView = GetPixelView(GetPixelOffset(index));
	for(uint8_t i = 0; i < 4; ++i)
		pxView.SetValue(static_cast<Channel>(i), color.at(i));
}
void pragma::image::ImageBuffer::SetPixelColor(uint32_t x, uint32_t y, const std::array<uint16_t, 4> &color) { SetPixelColor(GetPixelIndex(x, y), color); }
void pragma::image::ImageBuffer::SetPixelColor(PixelIndex index, const std::array<uint16_t, 4> &color)
{
	auto pxView = GetPixelView(GetPixelOffset(index));
	for(uint8_t i = 0; i < 4; ++i)
		pxView.SetValue(static_cast<Channel>(i), color.at(i));
}
void pragma::image::ImageBuffer::SetPixelColor(uint32_t x, uint32_t y, const Vector4 &color) { SetPixelColor(GetPixelIndex(x, y), color); }
void pragma::image::ImageBuffer::SetPixelColor(PixelIndex index, const Vector4 &color)
{
	auto pxView = GetPixelView(GetPixelOffset(index));
	for(uint8_t i = 0; i < 4; ++i)
		pxView.SetValue(static_cast<Channel>(i), color[i]);
}

pragma::image::ImageBuffer *pragma::image::ImageBuffer::GetParent() { return (m_parent.expired() == false) ? m_parent.lock().get() : nullptr; }
const std::pair<uint64_t, uint64_t> &pragma::image::ImageBuffer::GetPixelCoordinatesRelativeToParent() const { return m_offsetRelToParent; }
pragma::image::ImageBuffer::Offset pragma::image::ImageBuffer::GetAbsoluteOffset(Offset localOffset) const
{
	if(m_parent.expired())
		return localOffset;
	auto parent = m_parent.lock();
	auto pxCoords = GetPixelCoordinates(localOffset);
	pxCoords.first += m_offsetRelToParent.first;
	pxCoords.second += m_offsetRelToParent.second;
	return parent->GetAbsoluteOffset(parent->GetPixelOffset(pxCoords.first, pxCoords.second));
}
float pragma::image::calc_luminance(const Vector3 &color) { return 0.212671 * color.r + 0.71516 * color.g + 0.072169 * color.b; }
void pragma::image::ImageBuffer::CalcLuminance(float &outAvgLuminance, float &outMinLuminance, float &outMaxLuminance, Vector3 &outAvgIntensity, float *optOutLogAvgLuminance) const
{
	float delta = 1e-4f;
	outAvgLuminance = 0.f;
	if(optOutLogAvgLuminance)
		*optOutLogAvgLuminance = 0.f;
	outMinLuminance = std::numeric_limits<float>::max();
	outMaxLuminance = std::numeric_limits<float>::lowest();
	outAvgIntensity = Vector3 {};

	for(auto &pxView : const_cast<ImageBuffer &>(*this)) {
		Vector3 col {pxView.GetFloatValue(Channel::Red), pxView.GetFloatValue(Channel::Green), pxView.GetFloatValue(Channel::Blue)};
		outAvgIntensity += col;
		auto lum = calc_luminance(col);
		outAvgLuminance += lum;
		if(optOutLogAvgLuminance)
			*optOutLogAvgLuminance += std::log(delta + lum);
		if(lum > outMaxLuminance)
			outMaxLuminance = lum;
		if(lum < outMinLuminance)
			outMinLuminance = lum;
	}

	auto numPixels = static_cast<float>(GetPixelCount());
	outAvgIntensity = outAvgIntensity / numPixels;
	outAvgLuminance = outAvgLuminance / numPixels;
	if(optOutLogAvgLuminance)
		*optOutLogAvgLuminance = std::exp(*optOutLogAvgLuminance / numPixels);
}
uint8_t pragma::image::ImageBuffer::GetChannelCount(Format format)
{
	switch(format) {
	case Format::R8:
	case Format::R16:
	case Format::R32:
		return 1;
	case Format::RG8:
	case Format::RG16:
	case Format::RG32:
		return 2;
	case Format::RGB8:
	case Format::RGB16:
	case Format::RGB32:
		return 3;
	case Format::RGBA8:
	case Format::RGBA16:
	case Format::RGBA32:
		return 4;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return 0;
}
uint8_t pragma::image::ImageBuffer::GetChannelSize(Format format)
{
	switch(format) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return 1;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return 2;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return 4;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return 0;
}
pragma::image::ImageBuffer::Size pragma::image::ImageBuffer::GetPixelSize(Format format) { return GetChannelCount(format) * GetChannelSize(format); }
pragma::image::ImageBuffer::ImageBuffer(const std::shared_ptr<void> &data, uint32_t width, uint32_t height, Format format) : m_data {data}, m_width {width}, m_height {height}, m_format {format} {}
std::pair<uint32_t, uint32_t> pragma::image::ImageBuffer::GetPixelCoordinates(Offset offset) const
{
	offset /= GetPixelSize();
	auto x = offset % GetWidth();
	auto y = offset / GetWidth();
	return {x, y};
}
void pragma::image::ImageBuffer::Insert(const ImageBuffer &other, uint32_t x, uint32_t y, uint32_t xOther, uint32_t yOther, uint32_t wOther, uint32_t hOther)
{
	other.ClampBounds(xOther, yOther, wOther, hOther);
	ClampPixelCoordinatesToBounds(x, y);
	auto w = GetWidth();
	auto h = GetHeight();
	if(x + wOther > w)
		wOther = w - x;
	if(y + hOther > h)
		hOther = h - y;
	for(auto i = xOther; i < (xOther + wOther); ++i) {
		for(auto j = yOther; j < (yOther + hOther); ++j) {
			GetPixelView(x + (i - xOther), y + (j - yOther)).CopyValues(const_cast<ImageBuffer &>(other).GetPixelView(i, j));
		}
	}
}
void pragma::image::ImageBuffer::Insert(const ImageBuffer &other, uint32_t x, uint32_t y) { Insert(other, x, y, 0, 0, other.GetWidth(), other.GetHeight()); }
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::Copy() const { return ImageBuffer::Create(m_data.get(), m_width, m_height, m_format, false); }
std::shared_ptr<pragma::image::ImageBuffer> pragma::image::ImageBuffer::Copy(Format format) const
{
	// Optimized copy that performs copy +format change in one go
	auto cpy = ImageBuffer::Create(m_width, m_height, m_format);
	Convert(const_cast<ImageBuffer &>(*this), *cpy, format);
	return cpy;
}
bool pragma::image::ImageBuffer::Copy(ImageBuffer &dst) const
{
	auto w = GetWidth();
	auto h = GetHeight();
	if(w != dst.GetWidth() || h != dst.GetHeight())
		return false;
	Copy(dst, 0, 0, 0, 0, w, h);
	return true;
}
void pragma::image::ImageBuffer::Copy(ImageBuffer &dst, uint32_t xSrc, uint32_t ySrc, uint32_t xDst, uint32_t yDst, uint32_t w, uint32_t h) const
{
	if(GetFormat() == dst.GetFormat() && xSrc == 0 && ySrc == 0 && xDst == 0 && yDst == 0 && w == GetWidth() && h == GetHeight()) {
		memcpy(dst.GetData(), GetData(), GetSize());
		return;
	}
	auto imgViewSrc = Create(const_cast<ImageBuffer &>(*this), xSrc, ySrc, w, h);
	auto imgViewDst = Create(dst, xDst, yDst, w, h);
	for(auto &px : *imgViewSrc) {
		auto pxDst = imgViewDst->GetPixelView(px.GetX(), px.GetY());
		pxDst.CopyValues(px);
	}
}
pragma::image::Format pragma::image::ImageBuffer::GetFormat() const { return m_format; }
uint32_t pragma::image::ImageBuffer::GetWidth() const { return m_width; }
uint32_t pragma::image::ImageBuffer::GetHeight() const { return m_height; }
pragma::image::ImageBuffer::Size pragma::image::ImageBuffer::GetPixelSize() const { return GetPixelSize(GetFormat()); }
uint32_t pragma::image::ImageBuffer::GetPixelCount() const { return m_width * m_height; }
bool pragma::image::ImageBuffer::HasAlphaChannel() const { return GetChannelCount() >= pragma::math::to_integral(Channel::Alpha) + 1; }
bool pragma::image::ImageBuffer::IsLDRFormat() const
{
	switch(GetFormat()) {
	case Format::R_LDR:
	case Format::RG_LDR:
	case Format::RGB_LDR:
	case Format::RGBA_LDR:
		return true;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return false;
}
bool pragma::image::ImageBuffer::IsHDRFormat() const
{
	switch(GetFormat()) {
	case Format::R_HDR:
	case Format::RG_HDR:
	case Format::RGB_HDR:
	case Format::RGBA_HDR:
		return true;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return false;
}
bool pragma::image::ImageBuffer::IsFloatFormat() const
{
	switch(GetFormat()) {
	case Format::R_FLOAT:
	case Format::RG_FLOAT:
	case Format::RGB_FLOAT:
	case Format::RGBA_FLOAT:
		return true;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return false;
}
uint8_t pragma::image::ImageBuffer::GetChannelCount() const { return GetChannelCount(GetFormat()); }
uint8_t pragma::image::ImageBuffer::GetChannelSize() const { return GetChannelSize(GetFormat()); }
pragma::image::ImageBuffer::PixelIndex pragma::image::ImageBuffer::GetPixelIndex(uint32_t x, uint32_t y) const { return y * GetWidth() + x; }
pragma::image::ImageBuffer::Offset pragma::image::ImageBuffer::GetPixelOffset(uint32_t x, uint32_t y) const { return GetPixelOffset(GetPixelIndex(x, y)); }
pragma::image::ImageBuffer::Offset pragma::image::ImageBuffer::GetPixelOffset(PixelIndex index) const { return index * GetPixelSize(); }
void pragma::image::ImageBuffer::ClampPixelCoordinatesToBounds(uint32_t &inOutX, uint32_t &inOutY) const
{
	inOutX = pragma::math::clamp(inOutX, 0u, GetWidth() - 1);
	inOutY = pragma::math::clamp(inOutY, 0u, GetHeight() - 1);
}
void pragma::image::ImageBuffer::ClampBounds(uint32_t &inOutX, uint32_t &inOutY, uint32_t &inOutW, uint32_t &inOutH) const
{
	ClampPixelCoordinatesToBounds(inOutX, inOutY);
	auto w = GetWidth();
	auto h = GetHeight();
	if(inOutX + inOutW > w)
		inOutW = w - inOutX;
	if(inOutY + inOutH > h)
		inOutH = h - inOutY;
}
const void *pragma::image::ImageBuffer::GetData() const { return const_cast<ImageBuffer *>(this)->GetData(); }
void *pragma::image::ImageBuffer::GetData() { return m_data.get(); }
void pragma::image::ImageBuffer::Reallocate()
{
	m_data = std::shared_ptr<void> {new uint8_t[GetSize()], [](const void *ptr) { delete[] static_cast<const uint8_t *>(ptr); }};
}
pragma::image::ImageBuffer::PixelIterator pragma::image::ImageBuffer::begin() { return PixelIterator {*this, 0}; }
pragma::image::ImageBuffer::PixelIterator pragma::image::ImageBuffer::end() { return PixelIterator {*this, GetSize()}; }

void pragma::image::ImageBuffer::Convert(ImageBuffer &srcImg, ImageBuffer &dstImg, Format targetFormat)
{
	if(dstImg.GetFormat() != targetFormat) {
		dstImg.m_format = targetFormat;
		dstImg.Reallocate();
	}
	auto itSrc = srcImg.begin();
	auto itDst = dstImg.begin();
	while(itSrc != srcImg.end()) {
		auto &srcPixel = *itSrc;
		auto &dstPixel = *itDst;
		auto numChannels = pragma::math::to_integral(Channel::Count);
		for(auto i = decltype(numChannels) {0u}; i < numChannels; ++i) {
			auto channel = static_cast<Channel>(i);
			switch(targetFormat) {
			case Format::R8:
			case Format::RG8:
			case Format::RGB8:
			case Format::RGBA8:
				dstPixel.SetValue(channel, srcPixel.GetLDRValue(channel));
				break;
			case Format::R16:
			case Format::RG16:
			case Format::RGB16:
			case Format::RGBA16:
				dstPixel.SetValue(channel, srcPixel.GetHDRValue(channel));
				break;
			case Format::R32:
			case Format::RG32:
			case Format::RGB32:
			case Format::RGBA32:
				dstPixel.SetValue(channel, srcPixel.GetFloatValue(channel));
				break;
			}
			static_assert(pragma::math::to_integral(Format::Count) == 13u);
		}
		++itSrc;
		++itDst;
	}
}
void pragma::image::ImageBuffer::Convert(Format targetFormat)
{
	if(GetFormat() == targetFormat)
		return;
	auto cpy = *this;
	Convert(cpy, *this, targetFormat);
}
void pragma::image::ImageBuffer::Convert(ImageBuffer &dst) { Convert(*this, dst, dst.GetFormat()); }
void pragma::image::ImageBuffer::SwapChannels(ChannelMask swizzle)
{
	if(swizzle == ChannelMask {})
		return;
	auto channelSize = GetChannelSize();
	auto numChannels = GetChannelCount();
	auto pxSize = channelSize * numChannels;
	std::vector<uint8_t> tmpChannelData {};
	tmpChannelData.resize(pxSize);
	for(auto &px : *this) {
		auto *pxData = px.GetPixelData();
		auto *channelData = static_cast<uint8_t *>(pxData);

		for(auto i = decltype(numChannels) {0u}; i < numChannels; ++i)
			memcpy(tmpChannelData.data() + i * channelSize, channelData + pragma::math::to_integral(swizzle[i]) * channelSize, channelSize);

		memcpy(channelData, tmpChannelData.data(), pxSize);
	}
}
void pragma::image::ImageBuffer::SwapChannels(Channel channel0, Channel channel1)
{
	auto channelSize = GetChannelSize();
	std::vector<uint8_t> tmpChannelData {};
	tmpChannelData.resize(channelSize);
	for(auto &px : *this) {
		auto *pxData = px.GetPixelData();
		auto &channelData0 = *(static_cast<uint8_t *>(pxData) + pragma::math::to_integral(channel0) * channelSize);
		auto &channelData1 = *(static_cast<uint8_t *>(pxData) + pragma::math::to_integral(channel1) * channelSize);
		memcpy(tmpChannelData.data(), &channelData0, channelSize);
		memcpy(&channelData0, &channelData1, channelSize);
		memcpy(&channelData1, tmpChannelData.data(), channelSize);
	}
}
void pragma::image::ImageBuffer::ToLDR()
{
	switch(m_format) {
	case Format::R16:
	case Format::R32:
		Convert(Format::R8);
		return;
	case Format::RG16:
	case Format::RG32:
		Convert(Format::RG8);
		return;
	case Format::RGB16:
	case Format::RGB32:
		Convert(Format::RGB8);
		return;
	case Format::RGBA16:
	case Format::RGBA32:
		Convert(Format::RGBA8);
		return;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
}
void pragma::image::ImageBuffer::ToHDR()
{
	switch(m_format) {
	case Format::R8:
	case Format::R32:
		Convert(Format::R16);
		return;
	case Format::RG8:
	case Format::RG32:
		Convert(Format::RG16);
		return;
	case Format::RGB8:
	case Format::RGB32:
		Convert(Format::RGB16);
		return;
	case Format::RGBA8:
	case Format::RGBA32:
		Convert(Format::RGBA16);
		return;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
}
void pragma::image::ImageBuffer::ToFloat()
{
	switch(m_format) {
	case Format::R8:
	case Format::R16:
		Convert(Format::R32);
		return;
	case Format::RG8:
	case Format::RG16:
		Convert(Format::RG32);
		return;
	case Format::RGB8:
	case Format::RGB16:
		Convert(Format::RGB32);
		return;
	case Format::RGBA8:
	case Format::RGBA16:
		Convert(Format::RGBA32);
		return;
	}
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
}
void pragma::image::ImageBuffer::Clear(const Color &color) { Clear(color.ToVector4()); }
void pragma::image::ImageBuffer::Clear(const Vector4 &color)
{
	for(auto &px : *this) {
		for(uint8_t i = 0; i < 4; ++i)
			px.SetValue(static_cast<Channel>(i), color[i]);
	}
}
void pragma::image::ImageBuffer::ClearAlpha(LDRValue alpha)
{
	if(HasAlphaChannel() == false)
		return;
	if(IsLDRFormat()) {
		for(auto &px : *this)
			px.SetValue(Channel::Alpha, alpha);
		return;
	}
	if(IsHDRFormat()) {
		auto value = ToHDRValue((alpha / static_cast<float>(std::numeric_limits<LDRValue>::max()))); // *std::numeric_limits<float>::max());
		for(auto &px : *this)
			px.SetValue(Channel::Alpha, value);
		return;
	}
	if(IsFloatFormat()) {
		auto value = (alpha / static_cast<float>(std::numeric_limits<LDRValue>::max())); // *std::numeric_limits<float>::max();
		for(auto &px : *this)
			px.SetValue(Channel::Alpha, value);
		return;
	}
}
pragma::image::ImageBuffer::Size pragma::image::ImageBuffer::GetSize() const { return GetPixelCount() * GetPixelSize(GetFormat()); }
void pragma::image::ImageBuffer::Read(Offset offset, Size size, void *outData)
{
	auto *srcPtr = static_cast<uint8_t *>(m_data.get()) + offset;
	memcpy(outData, srcPtr, size);
}
void pragma::image::ImageBuffer::Write(Offset offset, Size size, const void *inData)
{
	auto *dstPtr = static_cast<uint8_t *>(m_data.get()) + offset;
	memcpy(dstPtr, inData, size);
}
void pragma::image::ImageBuffer::Resize(Size width, Size height, EdgeAddressMode addressMode, Filter filter, ColorSpace colorSpace)
{
	if(width == m_width && height == m_height)
		return;
	auto imgResized = Create(width, height, GetFormat());
	stbir_datatype stformat;
	if(IsLDRFormat())
		stformat = STBIR_TYPE_UINT8;
	else if(IsHDRFormat())
		stformat = STBIR_TYPE_UINT16;
	else if(IsFloatFormat())
		stformat = STBIR_TYPE_FLOAT;
	else
		return;

	stbir_edge stedge;
	switch(addressMode) {
	case EdgeAddressMode::Clamp:
		stedge = STBIR_EDGE_CLAMP;
		break;
	case EdgeAddressMode::Reflect:
		stedge = STBIR_EDGE_REFLECT;
		break;
	case EdgeAddressMode::Wrap:
		stedge = STBIR_EDGE_WRAP;
		break;
	case EdgeAddressMode::Zero:
		stedge = STBIR_EDGE_ZERO;
		break;
	}
	static_assert(pragma::math::to_integral(EdgeAddressMode::Count) == 4);

	stbir_filter stfilter;
	switch(filter) {
	case Filter::Default:
		stfilter = STBIR_FILTER_DEFAULT;
		break;
	case Filter::Box:
		stfilter = STBIR_FILTER_BOX;
		break;
	case Filter::Triangle:
		stfilter = STBIR_FILTER_TRIANGLE;
		break;
	case Filter::CubicBSpline:
		stfilter = STBIR_FILTER_CUBICBSPLINE;
		break;
	case Filter::CatmullRom:
		stfilter = STBIR_FILTER_CATMULLROM;
		break;
	case Filter::Mitchell:
		stfilter = STBIR_FILTER_MITCHELL;
		break;
	}
	static_assert(pragma::math::to_integral(Filter::Count) == 6);

	stbir_colorspace stColorspace;
	switch(colorSpace) {
	case ColorSpace::Auto:
		stColorspace = !IsLDRFormat() ? STBIR_COLORSPACE_LINEAR : STBIR_COLORSPACE_SRGB;
		break;
	case ColorSpace::Linear:
		stColorspace = STBIR_COLORSPACE_LINEAR;
		break;
	case ColorSpace::SRGB:
		stColorspace = STBIR_COLORSPACE_SRGB;
		break;
	}
	static_assert(pragma::math::to_integral(ColorSpace::Count) == 3);

	auto res = stbir_resize(static_cast<uint8_t *>(GetData()), GetWidth(), GetHeight(), 0 /* stride */, static_cast<uint8_t *>(imgResized->GetData()), imgResized->GetWidth(), imgResized->GetHeight(), 0 /* stride */, stformat, GetChannelCount(),
	  HasAlphaChannel() ? pragma::math::to_integral(Channel::Alpha) : STBIR_ALPHA_CHANNEL_NONE, 0 /* flags */, stedge, stedge, stfilter, stfilter, stColorspace, nullptr);
	if(res == 0)
		return;
	*this = *imgResized;
}

std::ostream &operator<<(std::ostream &out, const pragma::image::ImageBuffer &o)
{
	out << "ImageBuffer";
	out << "[Format:" << magic_enum::enum_name(o.GetFormat()) << "]";
	out << "[Resolution:" << o.GetWidth() << "x" << o.GetHeight() << "]";
	out << "[Channels:" << o.GetChannelCount() << "]";
	return out;
}
