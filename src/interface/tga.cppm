// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

export module pragma.image:tga;

export import pragma.filesystem;

export namespace pragma::image {
	class ImageBuffer;
	namespace impl {
		std::shared_ptr<ImageBuffer> load_tga_image(std::shared_ptr<fs::VFilePtrInternal> &file);
	};
};
