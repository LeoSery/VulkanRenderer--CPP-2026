#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textureSampler;

void main()
{
    // Hardcoded light and object settings
    vec3 lightDir   = normalize(vec3(1.0, 2.0, -1.0));    // direction of light
    vec3 lightColor = vec3(1.0, 1.0, 1.0);                // white light
    vec3 objectColor = texture(textureSampler, inUV).rgb; // object's base color

    float ambientStrength  = 0.15;
    float specularStrength = 0.5;
    float shininess        = 32.0;

    vec3 normal  = normalize(inNormal);
    vec3 viewDir = normalize(vec3(0.0, 0.0, -1.0)); // This is an approximation, this will be refined based on the camera's actual position.

    // Ambient
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    float diff   = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec      = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specular   = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * objectColor;
    outColor    = vec4(result, 1.0);
}