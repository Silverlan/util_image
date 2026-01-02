// SPDX-FileCopyrightText: (c) 2025 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module pragma.image;

import :buffer;

pragma::image::ImageBuffer::PixelView::PixelView(ImageBuffer &imgBuffer, Offset offset) : m_imageBuffer {imgBuffer}, m_offset {offset} {}
pragma::image::ImageBuffer::Offset pragma::image::ImageBuffer::PixelView::GetOffset() const { return m_offset; }
pragma::image::ImageBuffer::Offset pragma::image::ImageBuffer::PixelView::GetAbsoluteOffset() const { return m_imageBuffer.GetAbsoluteOffset(m_offset); }
pragma::image::ImageBuffer::PixelIndex pragma::image::ImageBuffer::PixelView::GetPixelIndex() const { return m_offset / m_imageBuffer.GetPixelSize(); }
uint32_t pragma::image::ImageBuffer::PixelView::GetX() const { return GetPixelIndex() % m_imageBuffer.GetWidth(); }
uint32_t pragma::image::ImageBuffer::PixelView::GetY() const { return GetPixelIndex() / m_imageBuffer.GetWidth(); }
const void *pragma::image::ImageBuffer::PixelView::GetPixelData() const { return const_cast<PixelView *>(this)->GetPixelData(); }
void *pragma::image::ImageBuffer::PixelView::GetPixelData() { return static_cast<uint8_t *>(m_imageBuffer.GetData()) + GetAbsoluteOffset(); }
pragma::image::ImageBuffer::LDRValue pragma::image::ImageBuffer::PixelView::GetLDRValue(Channel channel) const
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return 0;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return std::numeric_limits<LDRValue>::max();
	auto *data = static_cast<const void *>(static_cast<const uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * pragma::math::to_integral(channel));
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
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return 0;
}
pragma::image::ImageBuffer::HDRValue pragma::image::ImageBuffer::PixelView::GetHDRValue(Channel channel) const
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return 0;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return pragma::math::float32_to_float16_glm(std::numeric_limits<FloatValue>::max());
	auto *data = static_cast<const void *>(static_cast<const uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * pragma::math::to_integral(channel));
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
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return 0;
}
pragma::image::ImageBuffer::FloatValue pragma::image::ImageBuffer::PixelView::GetFloatValue(Channel channel) const
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return 0;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return std::numeric_limits<FloatValue>::max();
	auto *data = static_cast<const void *>(static_cast<const uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * pragma::math::to_integral(channel));
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
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
	return 0.f;
}
void pragma::image::ImageBuffer::PixelView::SetValue(Channel channel, LDRValue value)
{
	if(m_imageBuffer.m_width == 0 || m_imageBuffer.m_height == 0)
		return;
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return;
	auto *data = static_cast<void *>(static_cast<uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * pragma::math::to_integral(channel));
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
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
}
void pragma::image::ImageBuffer::PixelView::SetValue(Channel channel, HDRValue value)
{
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return;
	auto *data = static_cast<void *>(static_cast<uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * pragma::math::to_integral(channel));
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
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
}
void pragma::image::ImageBuffer::PixelView::SetValue(Channel channel, FloatValue value)
{
	if(channel == Channel::Alpha && m_imageBuffer.HasAlphaChannel() == false)
		return;
	auto *data = static_cast<void *>(static_cast<uint8_t *>(GetPixelData()) + m_imageBuffer.GetChannelSize() * pragma::math::to_integral(channel));
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
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
}
pragma::image::ImageBuffer &pragma::image::ImageBuffer::PixelView::GetImageBuffer() const { return m_imageBuffer; }
void pragma::image::ImageBuffer::PixelView::CopyValue(Channel channel, const PixelView &outOther)
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
	static_assert(pragma::math::to_integral(Format::Count) == 13u);
}
void pragma::image::ImageBuffer::PixelView::CopyValues(const PixelView &outOther)
{
	auto format = outOther.GetImageBuffer().GetFormat();
	auto numChannels = pragma::math::to_integral(Channel::Count);
	for(auto i = decltype(numChannels) {0u}; i < numChannels; ++i)
		CopyValue(static_cast<Channel>(i), outOther);
}

/////

pragma::image::ImageBuffer::PixelIterator::PixelIterator(ImageBuffer &imgBuffer, Offset offset) : m_pixelView {imgBuffer, offset} {}
pragma::image::ImageBuffer::PixelIterator &pragma::image::ImageBuffer::PixelIterator::operator++()
{
	m_pixelView.m_offset = pragma::math::min(m_pixelView.m_offset + m_pixelView.m_imageBuffer.GetPixelSize(), m_pixelView.m_imageBuffer.GetSize());
	return *this;
}
pragma::image::ImageBuffer::PixelIterator pragma::image::ImageBuffer::PixelIterator::operator++(int)
{
	auto it = *this;
	it.operator++();
	return it;
}
pragma::image::ImageBuffer::PixelView &pragma::image::ImageBuffer::PixelIterator::operator*() { return m_pixelView; }
pragma::image::ImageBuffer::PixelView *pragma::image::ImageBuffer::PixelIterator::operator->() { return &m_pixelView; }
bool pragma::image::ImageBuffer::PixelIterator::operator==(const PixelIterator &other) const { return &other.m_pixelView.m_imageBuffer == &m_pixelView.m_imageBuffer && other.m_pixelView.m_offset == m_pixelView.m_offset; }
bool pragma::image::ImageBuffer::PixelIterator::operator!=(const PixelIterator &other) const { return !operator==(other); }
