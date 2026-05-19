#include "VulkanRenderer.h"

int main()
{
    GraphicsRenderer application("Vulkan Renderer", 1280, 720);

    Scene& mainScene = application.GetScene();
    mainScene.GetCamera().SetPosition({ 0.0f, 0.0f, 3.0f });
    mainScene.GetCamera().SetRotation(-90.0f, 0.0f);

    // 3D Frog Model
    RenderObject* frogObject = mainScene.AddObject("Frog");
    frogObject->SetMesh("assets/Meshs/FrogThisWay/Frog.obj");
    frogObject->SetTexture("assets/Textures/FrogThisWay/Tx_Frogv1_D.jpg");
    frogObject->SetRotation(0.0f, 0.0f, 0.0f);

    RenderObject* frogObject2 = mainScene.AddObject("Frog2");
    frogObject2->SetMesh("assets/Meshs/FrogThisWay/Frog.obj");
    frogObject2->SetTexture("assets/Textures/FrogThisWay/Tx_Frogv1_D.jpg");
    frogObject2->SetRotation(3.0f, 0.0f, 0.0f);

    // Key light > warm, main, high front-right
    Light* keyLight = mainScene.AddLight("Key Light");
    keyLight->SetDirection(0.639f, 0.426f, 0.639f);
    keyLight->SetColor(1.0f, 0.92f, 0.78f);    // Warm white, slightly golden
    keyLight->SetAmbientStrength(0.15f);
    keyLight->SetSpecularStrength(0.5f);
    keyLight->SetShininess(32.0f);

    // Fill light > cool, soft, front-left
    Light* fillLight = mainScene.AddLight("Fill Light");
    fillLight->SetDirection(-0.534f, 0.267f, 0.801f);
    fillLight->SetColor(0.7f, 0.82f, 1.0f);    // Slightly blueish white, cool
    fillLight->SetAmbientStrength(0.0f);
    fillLight->SetSpecularStrength(0.1f);
    fillLight->SetShininess(16.0f);

    // Rim light > neutral, high from behind, outline
    Light* rimLight = mainScene.AddLight("Rim Light");
    rimLight->SetDirection(0.0f, 0.8f, -0.6f);
    rimLight->SetColor(0.4f, 0.52f, 0.65f);    // Pale cool blue, cinematic rim
    rimLight->SetAmbientStrength(0.0f);
    rimLight->SetSpecularStrength(0.5f);
    rimLight->SetShininess(64.0f);

    application.Run();
    return 0;
}
