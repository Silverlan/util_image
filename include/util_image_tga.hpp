// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __UTIL_IMAGE_TGA_HPP__
#define __UTIL_IMAGE_TGA_HPP__

#include <memory>

class VFilePtrInternal;
namespace uimg {
	class ImageBuffer;
	namespace impl {
		std::shared_ptr<ImageBuffer> load_tga_image(std::shared_ptr<VFilePtrInternal> &file);
	};
};

#endif
