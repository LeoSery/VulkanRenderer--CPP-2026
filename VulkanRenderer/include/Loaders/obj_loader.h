#pragma once

#include <memory>
#include <string>

class Mesh;
class GraphicsContext;

namespace ObjLoader
{
	std::unique_ptr<Mesh> Load(GraphicsContext& ctx, const std::string& path);
};
