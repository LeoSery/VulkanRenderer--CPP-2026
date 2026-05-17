#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inFragPos;
layout(location = 3) in vec3 inViewPos;

layout(location = 0) out vec4 outColor;

struct LightData
{
    vec3 direction;
    float ambientStrength;
    vec3 color;
    float specularStrength;
    float shininess;
};

layout(set = 0, binding = 0) uniform SceneData
{
    int numberLights;
    LightData lights[8];
} scene;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

void main()
{
    vec3 objectColor = texture(textureSampler, inUV).rgb;

    vec3 normal = normalize(inNormal);
    vec3 viewDirection = normalize(inViewPos - inFragPos);
    
    vec3 ambient = vec3(0.0);
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < scene.numberLights; i++)
    {
        vec3 lightDirection = normalize(scene.lights[i].direction);
        ambient += scene.lights[i].ambientStrength * scene.lights[i].color;

        float diff = max(dot(normal, lightDirection), 0.0);
        diffuse += diff * scene.lights[i].color;
        
        vec3 halfwayDirection = normalize(lightDirection + viewDirection);
        float spec = pow(max(dot(normal, halfwayDirection), 0.0), scene.lights[i].shininess);
        specular += scene.lights[i].specularStrength * spec * scene.lights[i].color;
    }

    outColor = vec4((ambient + diffuse + specular) * objectColor, 1.0);
}