#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"

#include "imgui_internal.h"
#include "theme.h"
#include "widgets.h"
#include "window.h"

using Theme::Col;
using Theme::Colors;
using Theme::S;

static const float kFadeSeconds = 0.30f;

void App::Init(bool demo_failure)
{
    m_DemoFailure = demo_failure;
    m_Screen = Screen::Connect;
    m_Fade = 0.0f;
    EnterConnect();
}

void App::Shutdown()
{
    m_Connection.Cancel();
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

void App::Frame()
{
    const float dt = ImMin(ImGui::GetIO().DeltaTime, 0.05f);

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
