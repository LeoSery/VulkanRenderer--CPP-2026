#include "Loaders/image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>

namespace ImageLoader
{
	ImageData ImageLoader::Load(const std::string& path)
	{
		ImageData result;

		stbi_uc* data = stbi_load(path.c_str(), &result.width, &result.height, nullptr, STBI_rgb_alpha);

		if (!data)
		{
			throw std::runtime_error("ImageLoader > Load(): Failed to load image : " + path + " - " + stbi_failure_reason());
		}

		result.channels = 4;
		result.pixels.assign(data, data + result.width * result.height * 4);

		stbi_image_free(data);
		return result;
	}
}
