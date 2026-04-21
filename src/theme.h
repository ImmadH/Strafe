#pragma once
#include "imgui.h"

inline void applyOrangeTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    
    ImVec4 orange      = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);  
    ImVec4 orangeHigh  = ImVec4(1.00f, 0.60f, 0.10f, 1.00f);  
    ImVec4 orangeMid   = ImVec4(0.60f, 0.25f, 0.00f, 1.00f);  
    ImVec4 orangeLow   = ImVec4(0.25f, 0.10f, 0.00f, 1.00f);  
    ImVec4 bg          = ImVec4(0.08f, 0.04f, 0.02f, 1.00f);  
    ImVec4 bgWindow    = ImVec4(0.11f, 0.06f, 0.03f, 1.00f);  
    ImVec4 text        = ImVec4(1.00f, 0.95f, 0.90f, 1.00f);  

    colors[ImGuiCol_Text]                  = text;
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.35f, 0.20f, 1.00f);
    colors[ImGuiCol_WindowBg]              = bgWindow;
    colors[ImGuiCol_ChildBg]               = bg;
    colors[ImGuiCol_PopupBg]               = bgWindow;
    colors[ImGuiCol_Border]                = orangeMid;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = orangeLow;
    colors[ImGuiCol_FrameBgHovered]        = orangeMid;
    colors[ImGuiCol_FrameBgActive]         = orange;
    colors[ImGuiCol_TitleBg]               = orangeMid;
    colors[ImGuiCol_TitleBgActive]         = orange;
    colors[ImGuiCol_TitleBgCollapsed]      = orangeLow;
    colors[ImGuiCol_MenuBarBg]             = orangeLow;
    colors[ImGuiCol_ScrollbarBg]           = bg;
    colors[ImGuiCol_ScrollbarGrab]         = orangeMid;
    colors[ImGuiCol_ScrollbarGrabHovered]  = orange;
    colors[ImGuiCol_ScrollbarGrabActive]   = orangeHigh;
    colors[ImGuiCol_CheckMark]             = orangeHigh;
    colors[ImGuiCol_SliderGrab]            = orange;
    colors[ImGuiCol_SliderGrabActive]      = orangeHigh;
    colors[ImGuiCol_Button]                = orangeMid;
    colors[ImGuiCol_ButtonHovered]         = orange;
    colors[ImGuiCol_ButtonActive]          = orangeHigh;
    colors[ImGuiCol_Header]                = orangeMid;
    colors[ImGuiCol_HeaderHovered]         = orange;
    colors[ImGuiCol_HeaderActive]          = orangeHigh;
    colors[ImGuiCol_Separator]             = orangeMid;
    colors[ImGuiCol_SeparatorHovered]      = orange;
    colors[ImGuiCol_SeparatorActive]       = orangeHigh;
    colors[ImGuiCol_ResizeGrip]            = orangeLow;
    colors[ImGuiCol_ResizeGripHovered]     = orange;
    colors[ImGuiCol_ResizeGripActive]      = orangeHigh;
    colors[ImGuiCol_Tab]                   = orangeLow;
    colors[ImGuiCol_TabHovered]            = orange;
    colors[ImGuiCol_TabActive]             = orangeMid;
    colors[ImGuiCol_TabUnfocused]          = orangeLow;
    colors[ImGuiCol_TabUnfocusedActive]    = orangeMid;
    colors[ImGuiCol_PlotLines]             = orange;
    colors[ImGuiCol_PlotLinesHovered]      = orangeHigh;
    colors[ImGuiCol_PlotHistogram]         = orange;
    colors[ImGuiCol_PlotHistogramHovered]  = orangeHigh;
    colors[ImGuiCol_TableHeaderBg]         = orangeLow;
    colors[ImGuiCol_TableBorderStrong]     = orangeMid;
    colors[ImGuiCol_TableBorderLight]      = orangeLow;
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(1.00f, 0.45f, 0.00f, 0.35f);
    colors[ImGuiCol_NavHighlight]          = orange;

    
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 0.0f;
    style.TabRounding       = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 4);
}