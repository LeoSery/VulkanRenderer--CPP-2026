#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec3 inViewPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textureSampler;

layout(set = 0, binding = 1) uniform LightData
{
    vec3  lightDir;
    float ambientStrength;
    vec3  lightColor;
    float specularStrength;
    float shininess;
} light;

void main()
{
    vec3 objectColor = texture(textureSampler, inUV).rgb;

    vec3 normal = normalize(inNormal);
    vec3 viewDir = normalize(inViewPos - inFragPos);
    vec3 lightDir = normalize(light.lightDir);

    // Ambient
    vec3 ambient = light.ambientStrength * light.lightColor;

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.lightColor;

    // Specular (Blinn-Phong)
    vec3  halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), light.shininess);
    vec3  specular = light.specularStrength * spec * light.lightColor;

    outColor = vec4((ambient + diffuse + specular) * objectColor, 1.0);
}