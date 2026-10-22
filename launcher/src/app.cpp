#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"

#include "imgui_internal.h"
#include "theme.h"
#include "widgets.h"
#include "window.h"

#include <stdio.h>
#include <string.h>

using Theme::Col;
using Theme::Colors;
using Theme::S;

static const float kFadeSeconds = 0.30f;

void App::Init()
{
    m_Settings.Load();
    if (m_Settings.GameFolder.empty())
        m_Settings.GameFolder = Game::DetectFolder(); // launcher placed next to Wow.exe
    snprintf(m_RealmlistInput, sizeof(m_RealmlistInput), "%s", m_Settings.Realmlist.c_str());
    Theme::SetAccent(m_Settings.Accent);
    RefreshClient();

    m_Screen = Screen::Connect;
    m_Fade = 0.0f;
    EnterConnect();
}

void App::Shutdown()
{
    m_Logon.Cancel();
    m_Settings.Save();
    Game::CloseProcess(m_Process); // the game keeps running; we only drop our handle
    m_Process = nullptr;
}

ImVec4 App::ClearColor() const
{
    return Colors.Bg;
}

void App::SwitchTo(Screen screen)
{
    if (m_Switching || screen == m_Screen)
        return;
    m_NextScreen = screen;
    m_Switching = true;
}

void App::DragWindowFrom(ImVec2 min, ImVec2 max)
{
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
        ImRect(min, max).Contains(ImGui::GetIO().MousePos))
        Window::StartDrag();
}

void App::Idle()
{
    UpdateGameProcess();
}

void App::Frame()
{
    const float dt = ImMin(ImGui::GetIO().DeltaTime, 0.05f);
    m_Time += dt;

    // Settings drive a few theme globals.
    Theme::Animations = m_Settings.Animations;
    Theme::BackgroundEffects = m_Settings.Snow;
    UpdateGameProcess();
    if (m_SettingsDirty)
    {
        m_SettingsDirty = false;
        m_Settings.Save();
    }

    // Fade the current screen out, swap screens (and window size) while invisible, fade back in.
    const float fade_target = m_Switching ? 0.0f : 1.0f;
    if (Theme::Animations)
        m_Fade = ImClamp(m_Fade + (fade_target > m_Fade ? dt : -dt) / kFadeSeconds, 0.0f, 1.0f);
    else
        m_Fade = fade_target;

    if (m_Switching && m_Fade <= 0.0f)
    {
        m_Switching = false;
        m_Screen = m_NextScreen;
        m_ScreenTime = 0.0f;
        if (m_Screen == Screen::Connect)
        {
            EnterConnect();
            Window::Resize(Theme::ConnectWindowWidth, Theme::ConnectWindowHeight);
        }
        else
        {
            EnterMain();
            Window::Resize(Theme::MainWindowWidth, Theme::MainWindowHeight);
        }
    }
    m_ScreenTime += dt;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Widgets::EaseOutCubic(m_Fade));
    if (m_Screen == Screen::Connect)
        DrawConnect(dt);
    else
        DrawMain(dt);
    ImGui::PopStyleVar();

    // Windows 10 has no rounded corners or accent border on popup windows: draw a hairline border instead.
    if (!Window::HasRoundedCorners())
        ImGui::GetForegroundDrawList()->AddRect(viewport->Pos, viewport->Pos + viewport->Size, ImGui::GetColorU32(Colors.Border));

    ImGui::End();
}

//-----------------------------------------------------------------------------
// Actions
//-----------------------------------------------------------------------------

void App::RefreshClient()
{
    m_Client = Game::Inspect(m_Settings.GameFolder);
    m_AddonsScanned = false;
}

void App::BrowseForGame()
{
    // The folder dialog runs its own message loop, so open it between frames.
    Window::RunAfterFrame([this]()
    {
        std::string folder;
        if (!Game::PickFolder(Window::NativeHandle(), m_Settings.GameFolder, &folder))
            return;
        m_Settings.GameFolder = folder;
        RefreshClient();
        SaveSoon();
        if (m_Client.Found)
            ShowToast(ICON_CHECK, "Game folder set", Colors.Success);
        else
            ShowToast(ICON_WARNING, "Wow.exe was not found in that folder", Colors.Warning);
    });
}

void App::LaunchGame()
{
    if (!m_Client.Found)
    {
        BrowseForGame();
        return;
    }
    if (Game::IsRunning(m_Process))
        return;

    Game::LaunchOptions options;
    options.Folder = m_Settings.GameFolder;
    options.Realmlist = m_Settings.Realmlist;
    options.Realm = m_Settings.Realm;
    options.ClearCache = m_Settings.ClearCache;
    options.DisplayMode = m_Settings.DisplayMode;

    std::string error;
    void* process = nullptr;
    if (!Game::Launch(options, &process, &error))
    {
        ShowToast(ICON_ERROR, error, Colors.Danger);
        return;
    }
    Game::CloseProcess(m_Process);
    m_Process = process;
    ShowToast(ICON_PLAY, "Launching World of Warcraft \xc2\xb7 " + m_Settings.Realm, Colors.Success);

    if (m_Settings.AfterLaunch == 1)
        Window::Minimize();
    else if (m_Settings.AfterLaunch == 2)
        Window::Close();
}

void App::UpdateGameProcess()
{
    if (m_Process && !Game::IsRunning(m_Process))
    {
        Game::CloseProcess(m_Process);
        m_Process = nullptr;
        ShowToast(ICON_GAME, "World of Warcraft closed", Colors.TextDim);
        if (m_Settings.AfterLaunch == 1)
            Window::Restore();
    }
}

void App::CheckLogon()
{
    m_Logon.Check(m_Settings.Realmlist);
    m_LastLogonCheck = m_Time;
}

std::string App::ClientStatus(ImVec4* color) const
{
    char buf[160];
    if (m_Settings.GameFolder.empty())
    {
        *color = Colors.Warning;
        return "Choose your World of Warcraft 3.3.5a folder";
    }
    if (!m_Client.Found)
    {
        *color = Colors.Warning;
        return "Wow.exe not found in this folder";
    }
    if (m_Client.Build != 0 && m_Client.Build != Game::ExpectedBuild)
    {
        *color = Colors.Warning;
        snprintf(buf, sizeof(buf), "Build %u found \xc2\xb7 Warmane needs 3.3.5a (%u)", m_Client.Build, Game::ExpectedBuild);
        return buf;
    }
    *color = Colors.Success;
    snprintf(buf, sizeof(buf), "Ready \xc2\xb7 Wow.exe %s%s%s", m_Client.Version.empty() ? "found" : m_Client.Version.c_str(),
             m_Client.Locales.empty() ? "" : " \xc2\xb7 ", m_Client.Locales.empty() ? "" : m_Client.Locales[0].c_str());
    return buf;
}
