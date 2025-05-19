/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image_buffer.hpp"
#include <mathutil/umath.h>

uimg::ImageBuffer::PixelView::PixelView(ImageBuffer &imgBuffer, Offset offset) : m_imageBuffer {imgBuffer}, m_offset {offset} {}
uimg::ImageBuffer::Offset uimg::ImageBuffer::PixelView::GetOffset() const { return m_offset; }
uimg::ImageBuffer::Offset uimg::ImageBuffer::PixelView::GetAbsoluteOffset() const { return m_imageBuffer.GetAbsoluteOffset(m_offset); }
uimg::ImageBuffer::PixelIndex uimg::ImageBuffer::PixelView::GetPixelIndex() const { return m_offset / m_imageBuffer.GetPixelSize(); }
uint32_t uimg::ImageBuffer::PixelView::GetX() const { return GetPixelIndex() % m_imageBuffer.GetWidth(); }
uint32_t uimg::ImageBuffer::PixelView::GetY() const { return GetPixelIndex() / m_imageBuffer.GetWidth(); }
const void *uimg::ImageBuffer::PixelView::GetPixelData() const { return const_cast<PixelView *>(this)->GetPixelData(); }
void *uimg::ImageBuffer::PixelView::GetPixelData() { return static_cast<uint8_t *>(m_imageBuffer.GetData()) + GetAbsoluteOffset(); }
uimg::ImageBuffer::LDRValue uimg::ImageBuffer::PixelView::GetLDRValue(Channel channel) const
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return 0;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return std::numeric_limits<LDRValue>::max();
	auto *data = static_cast<const void *>(static_cast<const uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * umath::to_integral(channel));
	switch(m_imageBuffer.GetFormat()) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return *static_cast<const LDRValue *>(data);
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return ToLDRValue(*static_cast<const HDRValue *>(data));
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return ToLDRValue(*static_cast<const FloatValue *>(data));
	default:
		break;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return 0;
}
uimg::ImageBuffer::HDRValue uimg::ImageBuffer::PixelView::GetHDRValue(Channel channel) const
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return 0;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return umath::float32_to_float16_glm(std::numeric_limits<FloatValue>::max());
	auto *data = static_cast<const void *>(static_cast<const uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * umath::to_integral(channel));
	switch(m_imageBuffer.GetFormat()) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return ToHDRValue(*static_cast<const LDRValue *>(data));
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return *static_cast<const HDRValue *>(data);
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return ToHDRValue(*static_cast<const FloatValue *>(data));
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return 0;
}
uimg::ImageBuffer::FloatValue uimg::ImageBuffer::PixelView::GetFloatValue(Channel channel) const
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return 0;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return std::numeric_limits<FloatValue>::max();
	auto *data = static_cast<const void *>(static_cast<const uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * umath::to_integral(channel));
	switch(m_imageBuffer.GetFormat()) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		return ToFloatValue(*static_cast<const LDRValue *>(data));
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		return ToFloatValue(*static_cast<const HDRValue *>(data));
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		return *static_cast<const FloatValue *>(data);
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
	return 0.f;
}
void uimg::ImageBuffer::PixelView::SetValue(Channel channel, LDRValue value)
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return;
	auto *data = static_cast<void *>(static_cast<uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * umath::to_integral(channel));
	switch(m_imageBuffer.GetFormat()) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		*static_cast<LDRValue *>(data) = value;
		return;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		*static_cast<HDRValue *>(data) = ToHDRValue(value);
		return;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		*static_cast<HDRValue *>(data) = ToFloatValue(value);
		return;
	default:
		break;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
}
void uimg::ImageBuffer::PixelView::SetValue(Channel channel, HDRValue value)
{
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return;
	auto *data = static_cast<void *>(static_cast<uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * umath::to_integral(channel));
	switch(m_imageBuffer.GetFormat()) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		*static_cast<LDRValue *>(data) = ToLDRValue(value);
		return;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		*static_cast<HDRValue *>(data) = value;
		return;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		*static_cast<FloatValue *>(data) = ToFloatValue(value);
		return;
	default:
		break;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
}
void uimg::ImageBuffer::PixelView::SetValue(Channel channel, FloatValue value)
{
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return;
	auto *data = static_cast<void *>(static_cast<uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * umath::to_integral(channel));
	switch(m_imageBuffer.GetFormat()) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		*static_cast<LDRValue *>(data) = ToLDRValue(value);
		return;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		*static_cast<HDRValue *>(data) = ToHDRValue(value);
		return;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		*static_cast<FloatValue *>(data) = value;
		return;
	default:
		break;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
}
uimg::ImageBuffer &uimg::ImageBuffer::PixelView::GetImageBuffer() const { return m_imageBuffer; }
void uimg::ImageBuffer::PixelView::CopyValue(Channel channel, const PixelView &outOther)
{
	auto format = outOther.GetImageBuffer().GetFormat();
	switch(format) {
	case Format::R8:
	case Format::RG8:
	case Format::RGB8:
	case Format::RGBA8:
		SetValue(channel, outOther.GetLDRValue(channel));
		break;
	case Format::R16:
	case Format::RG16:
	case Format::RGB16:
	case Format::RGBA16:
		SetValue(channel, outOther.GetHDRValue(channel));
		break;
	case Format::R32:
	case Format::RG32:
	case Format::RGB32:
	case Format::RGBA32:
		SetValue(channel, outOther.GetFloatValue(channel));
		break;
	default:
		break;
	}
	static_assert(umath::to_integral(Format::Count) == 13u);
}
void uimg::ImageBuffer::PixelView::CopyValues(const PixelView &outOther)
{
	auto format = outOther.GetImageBuffer().GetFormat();
	auto numChannels = umath::to_integral(Channel::Count);
	for(auto i = decltype(numChannels) {0u}; i < numChannels; ++i)
		CopyValue(static_cast<Channel>(i), outOther);
}

/////

uimg::ImageBuffer::PixelIterator::PixelIterator(ImageBuffer &imgBuffer, Offset offset) : m_pixelView {imgBuffer, offset} {}
uimg::ImageBuffer::PixelIterator &uimg::ImageBuffer::PixelIterator::operator++()
{
	m_pixelView.m_offset = umath::min(m_pixelView.m_offset + m_pixelView.m_imageBuffer.GetPixelSize(), m_pixelView.m_imageBuffer.GetSize());
	return *this;
}
uimg::ImageBuffer::PixelIterator uimg::ImageBuffer::PixelIterator::operator++(int)
{
	auto it = *this;
	it.operator++();
	return it;
}
uimg::ImageBuffer::PixelView &uimg::ImageBuffer::PixelIterator::operator*() { return m_pixelView; }
uimg::ImageBuffer::PixelView *uimg::ImageBuffer::PixelIterator::operator->() { return &m_pixelView; }
bool uimg::ImageBuffer::PixelIterator::operator==(const PixelIterator &other) const { return &other.m_pixelView.m_imageBuffer == &m_pixelView.m_imageBuffer && other.m_pixelView.m_offset == m_pixelView.m_offset; }
bool uimg::ImageBuffer::PixelIterator::operator!=(const PixelIterator &other) const { return !operator==(other); }
