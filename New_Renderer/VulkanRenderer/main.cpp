#include "Core/window.h"
#include "Core/graphics_context.h"

int main()
{
    Window window(1280, 720, "Vulkan Renderer");
    GraphicsContext context(window);

    while (!window.ShouldClose())
    {
        window.PollEvents();
        context.BeginFrame();
        context.EndFrame();
    }

    return 0;
}