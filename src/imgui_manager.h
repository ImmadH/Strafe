#pragma once

namespace ImGuiManager
{
    bool Init();
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Render(void* cmd);
}
