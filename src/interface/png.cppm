// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <memory>

export module pragma.image:png;

export import pragma.filesystem;

export namespace uimg {
	class ImageBuffer;
	namespace impl {
		std::shared_ptr<ImageBuffer> load_png_image(std::shared_ptr<VFilePtrInternal> &file);
	};
};
