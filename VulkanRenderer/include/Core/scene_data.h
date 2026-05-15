#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>

class SceneData
{
public:
	struct LightData
	{
		glm::vec3 lightDirection;
		float ambientStrength;   // Ambient light intensity [0, 1]

		glm::vec3 lightColor;    // RGB
		float specularStrength;  //Specular highlight intensity [0, 1]

		float shininess;         // Specular shininess exponent
		float _pad[3];           // padding to reach 16-byte aligment
	};
};

