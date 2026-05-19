#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ImageLoader
{
	struct ImageData
	{
		std::vector<uint8_t> pixels;
		int width = 0;
		int height = 0;
		int channels = 4; // RBGA
	};

	ImageData Load(const std::string& path);
}
