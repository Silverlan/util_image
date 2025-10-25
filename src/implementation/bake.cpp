module;

#include <cinttypes>
#include <vector>
#include <memory>

#include <string>
#include <cstring>

module pragma.image;

import :core;

struct ImBuf {
	int x, y;
	std::shared_ptr<uimg::ImageBuffer> rect;
};

static int filter_make_index(const int x, const int y, const int w, const int h)
{
	if(x < 0 || x >= w || y < 0 || y >= h)
		return -1;
	else
		return y * w + x;
}

static int check_pixel_assigned(const void *buffer, const char *mask, const int index, const int depth, const bool is_float)
{
	int res = 0;

	if(index >= 0) {
		const int alpha_index = depth * index + (depth - 1);

		if(mask != NULL)
			res = mask[index] != 0 ? 1 : 0;
		else if((is_float && ((const float *)buffer)[alpha_index] != 0.0f) || (!is_float && ((const unsigned char *)buffer)[alpha_index] != 0)) {
			res = 1;
		}
	}

	return res;
}

static void filter_extend(struct ImBuf *ibuf, std::vector<uint8_t> &vmask, int filter, int depth = 4)
{
	auto *mask = reinterpret_cast<char *>(vmask.data());

	const int width = ibuf->x;
	const int height = ibuf->y;
	const bool is_float = ibuf->rect->IsFloatFormat();
	const int chsize = is_float ? sizeof(float) : sizeof(unsigned char);
	const size_t bsize = ((size_t)width) * height * depth * chsize;

	std::vector<uint8_t> vdstbuf;
	auto *data = ibuf->rect->GetData();
	vdstbuf.resize(ibuf->rect->GetSize());
	memcpy(vdstbuf.data(), data, vdstbuf.size() * sizeof(vdstbuf.front()));

	void *dstbuf = vdstbuf.data();
	auto vdstmask = vmask;
	char *dstmask = reinterpret_cast<char *>(vdstmask.data());
	void *srcbuf = data;
	char *srcmask = mask;
	int cannot_early_out = 1, r, n, k, i, j, c;
	float weight[25];

	n = 1;

	weight[0] = 1;
	weight[1] = 2;
	weight[2] = 1;
	weight[3] = 2;
	weight[4] = 0;
	weight[5] = 2;
	weight[6] = 1;
	weight[7] = 2;
	weight[8] = 1;

	for(r = 0; cannot_early_out == 1 && r < filter; r++) {
		int x, y;
		cannot_early_out = 0;

		for(y = 0; y < height; y++) {
			for(x = 0; x < width; x++) {
				const int index = filter_make_index(x, y, width, height);

				if(!check_pixel_assigned(srcbuf, srcmask, index, depth, is_float)) {
					float tmp[4];
					float wsum = 0;
					float acc[4] = {0, 0, 0, 0};
					k = 0;

					if(check_pixel_assigned(srcbuf, srcmask, filter_make_index(x - 1, y, width, height), depth, is_float) || check_pixel_assigned(srcbuf, srcmask, filter_make_index(x + 1, y, width, height), depth, is_float)
					  || check_pixel_assigned(srcbuf, srcmask, filter_make_index(x, y - 1, width, height), depth, is_float) || check_pixel_assigned(srcbuf, srcmask, filter_make_index(x, y + 1, width, height), depth, is_float)) {
						for(i = -n; i <= n; i++) {
							for(j = -n; j <= n; j++) {
								if(i != 0 || j != 0) {
									const int tmpindex = filter_make_index(x + i, y + j, width, height);
									if(check_pixel_assigned(srcbuf, srcmask, tmpindex, depth, is_float)) {
										if(is_float) {
											for(c = 0; c < depth; c++)
												tmp[c] = ((const float *)srcbuf)[depth * tmpindex + c];
										}
										else {
											for(c = 0; c < depth; c++)
												tmp[c] = (float)((const unsigned char *)srcbuf)[depth * tmpindex + c];
										}

										wsum += weight[k];

										for(c = 0; c < depth; c++)
											acc[c] += weight[k] * tmp[c];
									}
								}
								k++;
							}
						}

						if(wsum != 0) {
							for(c = 0; c < depth; c++)
								acc[c] /= wsum;

							if(is_float) {
								for(c = 0; c < depth; c++)
									((float *)dstbuf)[depth * index + c] = acc[c];
							}
							else {
								for(c = 0; c < depth; c++) {
									((unsigned char *)dstbuf)[depth * index + c] = acc[c] > 255 ? 255 : (acc[c] < 0 ? 0 : ((unsigned char)(acc[c] + 0.5f)));
								}
							}

							if(dstmask != NULL)
								dstmask[index] = util::baking::FILTER_MASK_MARGIN;
							cannot_early_out = 1;
						}
					}
				}
			}
		}

		memcpy(srcbuf, dstbuf, bsize);
		if(dstmask != NULL)
			memcpy(srcmask, dstmask, ((size_t)width) * height);
	}
}

void uimg::bake_margin(ImageBuffer &imgBuffer, std::vector<uint8_t> &mask, const int margin)
{
	ImBuf imgBuf {};
	imgBuf.rect = imgBuffer.shared_from_this();
	imgBuf.x = imgBuffer.GetWidth();
	imgBuf.y = imgBuffer.GetHeight();
	filter_extend(&imgBuf, mask, margin, imgBuffer.GetChannelCount());
}
