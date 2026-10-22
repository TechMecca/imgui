#pragma once

#include "armory.h"
#include "game.h"
#include "imgui.h"
#include "logon_status.h"
#include "settings.h"

#include <string>
#include <vector>

// The launcher UI: a "connecting" window that checks the logon server, then the main launcher window.
class App
{
public:
    void   Init();
    void   Shutdown();
    void   Frame();              // builds the UI for one frame (between ImGui::NewFrame and ImGui::Render)
    void   Idle();               // called instead of Frame() while the window is minimized
    ImVec4 ClearColor() const;

private:
    enum class Screen { Connect, Main };
    enum class Page   { Play, Armory, Addons, Settings, About };

    struct Toast { const char* Icon; std::string Text; ImVec4 Color; float Age; float Y; };

    void SwitchTo(Screen screen);
    void DragWindowFrom(ImVec2 min, ImVec2 max); // clicking empty space in this rect moves the window

    // connect_screen.cpp
    void EnterConnect();
    void DrawConnect(float dt);

    // main_screen.cpp
    void EnterMain();
    void DrawMain(float dt);
    void DrawTitleBar(ImVec2 origin, float width, float height);
    void DrawSidebar(ImVec2 origin, float width, float height);
    void DrawStatusBar(ImVec2 origin, float width, float height);
    void DrawPageHeader(const char* title, const char* subtitle);
    void DrawToasts(ImVec2 anchor, float dt);
    void ShowToast(const char* icon, const std::string& text, const ImVec4& color);

    // pages.cpp
    void DrawPlay();
    void DrawBanner(ImVec2 size);
    void DrawRealmCards();
    void DrawLaunchBar();
    void DrawArmory();
    void StartArmorySearch(const std::string& name, int realm_index);
    void DrawAddons();
    void DrawSettings();
    void DrawAbout();

    // Actions
    void RefreshClient();
    void BrowseForGame();
    void LaunchGame();
    void UpdateGameProcess();
    void CheckLogon();
    void SaveSoon() { m_SettingsDirty = true; }
    std::string ClientStatus(ImVec4* color) const;

    // State
    Settings         m_Settings;
    bool             m_SettingsDirty = false;
    LogonStatus      m_Logon;
    float            m_LastLogonCheck = 0.0f;
    Game::ClientInfo m_Client;
    void*            m_Process = nullptr;  // running Wow.exe
    std::vector<Game::Addon> m_Addons;
    bool             m_AddonsScanned = false;
    char             m_AddonFilter[64] = {};
    char             m_RealmlistInput[128] = {};
    char             m_ArmoryName[48] = {};
    int              m_ArmoryRealm = 0;
    Armory           m_Armory;

    // Screen flow
    Screen m_Screen      = Screen::Connect;
    Screen m_NextScreen  = Screen::Connect;
    bool   m_Switching   = false;
    float  m_Fade        = 0.0f;  // opacity of the current screen
    float  m_ScreenTime  = 0.0f;  // seconds since the current screen appeared
    float  m_Time        = 0.0f;  // seconds since start

    // Connect screen
    float  m_ShownProgress = 0.0f;
    float  m_OnlineAt      = -1.0f;
    float  m_FailedAt      = -1.0f;

    // Main screen
    Page   m_Page          = Page::Play;
    float  m_PageTime      = 0.0f;
    float  m_RealmChangedAt = -10.0f;
    std::vector<Toast> m_Toasts;
};
