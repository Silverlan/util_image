/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_image.hpp"
#include "util_image_buffer.hpp"
#include "util_image_tga.hpp"
#include "util_image_png.hpp"
#include <sharedutils/util_file.h>
#include <sharedutils/util_string.h>
#include <fsys/filesystem.h>

#if 0
std::shared_ptr<uimg::ImageBuffer> uimg::load_image(const std::string &fileName)
{
	std::string ext;
	if(ufile::get_extension(fileName,&ext) == false)
		return nullptr;
	auto f = FileManager::OpenFile(fileName.c_str(),"rb");
	if(f == nullptr)
		return nullptr;
	ustring::to_lower(ext);
	if(ext == "tga")
		return load_image(f,ImageFormat::TGA);
	else if(ext == "png")
		return load_image(f,ImageFormat::PNG);
	return nullptr;
}
std::shared_ptr<uimg::ImageBuffer> uimg::load_image(std::shared_ptr<VFilePtrInternal> &file,ImageFormat format)
{
	switch(format)
	{
		case ImageFormat::PNG:
		{
			auto img = uimg::impl::load_png_image(file);
			img->SwapChannels(uimg::ImageBuffer::Channel::Red,uimg::ImageBuffer::Channel::Blue);
			return img;
		}
		case ImageFormat::TGA:
			return uimg::impl::load_tga_image(file);
	}
	return nullptr;
}
#endif
