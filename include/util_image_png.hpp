/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_IMAGE_PNG_HPP__
#define __UTIL_IMAGE_PNG_HPP__

#include <memory>

class VFilePtrInternal;
namespace uimg {
	class ImageBuffer;
	namespace impl {
		std::shared_ptr<ImageBuffer> load_png_image(std::shared_ptr<VFilePtrInternal> &file);
	};
};

#endif
