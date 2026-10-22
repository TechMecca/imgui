#pragma once

#include "connection.h"
#include "imgui.h"

#include <string>
#include <vector>

// The whole UI: a "Connecting" window that turns into the main window once the
// server connection is verified.
class App
{
public:
    void   Init(bool demo_failure);
    void   Shutdown();
    void   Frame();              // builds the UI for one frame (between ImGui::NewFrame and ImGui::Render)
    ImVec4 ClearColor() const;

private:
    enum class Screen { Connect, Main };
    enum class Page   { Dashboard, Modules, Settings, About, Count };

    struct Module { const char* Icon; const char* Name; const char* Description; bool Enabled; };
    struct Event  { const char* Icon; std::string Text; ImVec4 Color; float Time; };
    struct Toast  { const char* Icon; std::string Text; ImVec4 Color; float Age; float Y; };

    void SwitchTo(Screen screen);
    void DragWindowFrom(ImVec2 min, ImVec2 max); // clicking empty space in this rect moves the window

    // connect_screen.cpp
    void EnterConnect();
    void DrawConnect(float dt);

    // main_screen.cpp
    void EnterMain();
    void DrawMain(float dt);
    void UpdateDemoData(float dt);
    void DrawTitleBar(ImVec2 origin, float width, float height);
    void DrawSidebar(ImVec2 origin, float width, float height);
    void DrawStatusBar(ImVec2 origin, float width, float height);
    void DrawPageHeader(const char* title, const char* subtitle);
    void DrawDashboard();
    void DrawModules();
    void DrawSettings();
    void DrawAbout();
    void DrawToasts(ImVec2 anchor, float dt);
    void AddEvent(const char* icon, const std::string& text, const ImVec4& color);
    void ShowToast(const char* icon, const std::string& text, const ImVec4& color);

    // Screen flow
    ServerConnection m_Connection;
    bool   m_DemoFailure   = false;
    Screen m_Screen        = Screen::Connect;
    Screen m_NextScreen    = Screen::Connect;
    bool   m_Switching     = false;
    float  m_Fade          = 0.0f;  // opacity of the current screen
    float  m_ScreenTime    = 0.0f;  // seconds since the current screen appeared

    // Connect screen
    float  m_ShownProgress = 0.0f;
    float  m_VerifiedAt    = -1.0f;
    float  m_FailedAt      = -1.0f;

    // Main screen
    Page   m_Page          = Page::Dashboard;
    float  m_PageTime      = 0.0f;
    float  m_SessionTime   = 0.0f;
    std::vector<Module> m_Modules;
    std::vector<Event>  m_Events;
    std::vector<Toast>  m_Toasts;

    // Demo data for the dashboard
    static constexpr int kLatencySamples  = 40;
    static constexpr int kActivitySamples = 48;
    float  m_Latency[kLatencySamples]   = {};
    float  m_Activity[kActivitySamples] = {};
    float  m_SampleTimer   = 0.0f;
    unsigned int m_Seed    = 0x9E3779B9u;

    // Settings page (demo values)
    bool   m_StartWithWindows = false;
    bool   m_Notifications    = true;
    int    m_Language         = 0;
    int    m_UpdateChannel    = 0;
};
