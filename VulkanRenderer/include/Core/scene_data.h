#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>

class SceneData
{
public:
	static constexpr uint32_t MAX_LIGHTS = 8;

	struct LightData
	{
		glm::vec3 direction;
		float ambientStrength;   // Ambient light intensity [0, 1]

		glm::vec3 color;    // RGB
		float specularStrength;  //Specular highlight intensity [0, 1]

		float shininess;         // Specular shininess exponent
		float _pad[3];           // padding to reach 16-byte aligment
	};

	struct SceneUBOData
	{
		int numberLights;
		float _pad[3];
		LightData lights[MAX_LIGHTS];
	};
};

