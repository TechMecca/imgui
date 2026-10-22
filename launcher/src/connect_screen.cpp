// The first window: checks that the logon server is reachable, then hands over to the launcher.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"

#include "imgui_internal.h"
#include "theme.h"
#include "widgets.h"
#include "window.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

using Theme::Col;
using Theme::Colors;
using Theme::S;

static const float kMinSplashSeconds   = 2.4f; // keep the splash up long enough to read
static const float kOnlineHoldSeconds  = 1.0f; // check mark time before the main window

void App::EnterConnect()
{
    m_ShownProgress = 0.0f;
    m_OnlineAt = -1.0f;
    m_FailedAt = -1.0f;
    CheckLogon();
}

// Text centered horizontally on 'center_x'.
static void CenteredText(ImDrawList* dl, ImFont* font, float center_x, float y, ImU32 col, const char* text)
{
    const ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
    dl->AddText(font, font->FontSize, ImFloor(ImVec2(center_x - size.x * 0.5f, y)), col, text);
}

void App::DrawConnect(float dt)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float cx = origin.x + size.x * 0.5f;
    const float t = m_ScreenTime;

    // Progress follows the check, but never faster than the minimum splash time.
    const LogonStatus::State state = m_Logon.GetState();
    float target = 0.1f;
    if (state == LogonStatus::State::Resolving)  target = 0.35f;
    if (state == LogonStatus::State::Connecting) target = 0.8f;
    if (state == LogonStatus::State::Online)     target = 1.0f;
    if (state == LogonStatus::State::Offline)    target = m_ShownProgress;
    target = ImMin(target, t / kMinSplashSeconds);
    m_ShownProgress += (target - m_ShownProgress) * (1.0f - expf(-8.0f * dt));
    if (!Theme::Animations)
        m_ShownProgress = target;

    const bool online = state == LogonStatus::State::Online && m_ShownProgress > 0.985f;
    const bool failed = state == LogonStatus::State::Offline && t > 1.0f;
    if (online && m_OnlineAt < 0.0f)
        m_OnlineAt = t;
    if (failed && m_FailedAt < 0.0f)
        m_FailedAt = t;
    const float since_done = online ? t - m_OnlineAt : failed ? t - m_FailedAt : 0.0f;
    if (online && since_done > kOnlineHoldSeconds)
        SwitchTo(Screen::Main);

    const ImVec4 result_col = failed ? Colors.Danger : Colors.Success;
    const float result_mix = Widgets::Smooth("##result", (online || failed) ? 1.0f : 0.0f, 6.0f);
    const ImVec4 glow_col = Theme::Mix(Colors.AccentA, result_col, result_mix);

    // --- Background: aurora, snow, a low mountain range -------------------------------
    Widgets::Aurora(dl, origin, origin + ImVec2(size.x, size.y * 0.6f), m_Time, 0.9f);
    Widgets::Mountains(dl, origin + ImVec2(0.0f, size.y * 0.55f), origin + size, 11u, S(70.0f),
                       Col(Theme::Mix(Colors.Bg, Colors.AccentA, 0.10f)), Col(Colors.Bg), Col(ImVec4(1, 1, 1, 0.08f)));
    if (Theme::BackgroundEffects)
        Widgets::Snow("##connect_snow", dl, origin, origin + size, 0.9f, 60);
    Widgets::RadialGlow(dl, ImVec2(cx, origin.y + S(215.0f)), S(200.0f), Col(Colors.Bg, 0.8f), Col(Colors.Bg, 0.0f));
    const ImVec2 center(cx, origin.y + S(104.0f));
    Widgets::RadialGlow(dl, center, S(170.0f), Col(glow_col, 0.20f), Col(glow_col, 0.0f));

    // --- Window buttons --------------------------------------------------------------
    const ImVec2 button_size = S(32.0f, 28.0f);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - S(10.0f) - button_size.x * 2.0f - S(2.0f), origin.y + S(10.0f)));
    if (Widgets::WindowButton("##minimize", false, button_size))
        Window::Minimize();
    ImGui::SameLine(0.0f, S(2.0f));
    if (Widgets::WindowButton("##close", true, button_size))
        Window::Close();

    // --- Spinner / result ring + logo ------------------------------------------------
    const float ring_r = S(48.0f), ring_th = S(3.0f);
    if (!online && !failed)
    {
        Widgets::Spinner(dl, center, ring_r, ring_th, t);
    }
    else
    {
        dl->AddCircle(center, ring_r, Col(Colors.Border, 0.7f), 64, ring_th);
        Widgets::Ring(dl, center, ring_r, ring_th, Widgets::EaseOutCubic(since_done / 0.5f), Col(result_col));
    }
    const float swap = (online || failed) ? Widgets::EaseOutCubic(since_done / 0.35f) : 0.0f;
    if (swap < 1.0f)
        Widgets::Logo(dl, center, S(30.0f) * (1.0f - 0.35f * swap), t * 0.35f, 1.0f - swap);
    if (swap > 0.0f)
    {
        dl->AddCircleFilled(center, S(36.0f) * (0.6f + 0.4f * swap), Col(result_col, 0.14f * swap), 48);
        const float mark = Widgets::EaseOutCubic((since_done - 0.12f) / 0.4f);
        if (online)
            Widgets::CheckMark(dl, center, S(44.0f), S(4.0f), mark, Col(result_col));
        else
            Widgets::CrossMark(dl, center, S(40.0f), S(4.0f), mark, Col(result_col));
    }

    // --- Wordmark --------------------------------------------------------------------
    ImFont* display = Theme::Fonts.Display;
    const float name_spacing = S(6.0f);
    const ImVec2 name_size = Widgets::SpacedTextSize(display, Theme::AppName, name_spacing);
    Widgets::GradientText(dl, display, ImVec2(cx - name_size.x * 0.5f, origin.y + S(166.0f)), Theme::AppName,
                          Col(Colors.AccentA), Col(Colors.AccentB), name_spacing);
    ImFont* small = Theme::Fonts.Small;
    const float tag_spacing = S(3.0f);
    const ImVec2 tag_size = Widgets::SpacedTextSize(small, Theme::AppTagline, tag_spacing);
    Widgets::SpacedText(dl, small, ImVec2(cx - tag_size.x * 0.5f, origin.y + S(166.0f) + display->FontSize + S(4.0f)),
                        Col(Colors.TextFaint), Theme::AppTagline, tag_spacing);

    // --- Status ----------------------------------------------------------------------
    ImFont* body = Theme::Fonts.Body;
    ImFont* bold = Theme::Fonts.Bold;
    const std::string host = m_Logon.Host();
    const float status_y = origin.y + S(240.0f);
    char text[160];
    if (failed)
    {
        CenteredText(dl, bold, cx, status_y, Col(Colors.Danger), "Logon server unreachable");
        CenteredText(dl, small, cx, status_y + S(22.0f), Col(Colors.TextDim), m_Logon.Error().c_str());

        const ImVec2 btn = S(150.0f, 34.0f);
        const float gap = S(10.0f);
        ImGui::SetCursorScreenPos(ImVec2(cx - btn.x - gap * 0.5f, status_y + S(52.0f)));
        if (Widgets::PrimaryButton("Retry", btn))
            EnterConnect();
        ImGui::SameLine(0.0f, gap);
        if (Widgets::SecondaryButton("Continue offline", btn))
            SwitchTo(Screen::Main);
    }
    else
    {
        // Stage label with an animated ellipsis (centered on the full "..." width so it doesn't wobble).
        ImU32 label_col = Col(Colors.Text);
        if (online)
        {
            snprintf(text, sizeof(text), "Logon server online \xc2\xb7 %d ms", m_Logon.LatencyMs());
            label_col = Col(Colors.Success);
        }
        else if (state == LogonStatus::State::Resolving || state == LogonStatus::State::Idle)
            snprintf(text, sizeof(text), "Looking up %s", host.c_str());
        else
            snprintf(text, sizeof(text), "Connecting to logon server");

        std::string label = text;
        const float label_w = body->CalcTextSizeA(body->FontSize, FLT_MAX, 0.0f, (online ? label : label + "...").c_str()).x;
        if (!online)
            label.append((size_t)((int)(t * 2.5f) % 4), '.');
        dl->AddText(body, body->FontSize, ImFloor(ImVec2(cx - label_w * 0.5f, status_y)), label_col, label.c_str());

        const ImVec2 bar_size(S(300.0f), S(5.0f));
        const float bar_y = status_y + S(30.0f);
        ImGui::SetCursorScreenPos(ImVec2(cx - bar_size.x * 0.5f, bar_y));
        Widgets::ProgressBar(m_ShownProgress, bar_size);

        char percent[16];
        snprintf(percent, sizeof(percent), "%d%%", online ? 100 : (int)(m_ShownProgress * 100.0f));
        const float info_y = bar_y + bar_size.y + S(8.0f);
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(cx - bar_size.x * 0.5f, info_y)), Col(Colors.TextFaint), host.c_str());
        const float pw = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, percent).x;
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(cx + bar_size.x * 0.5f - pw, info_y)), Col(Colors.TextDim), percent);

        // Footer
        const float footer_y = origin.y + size.y - S(28.0f);
        char left[32], right[64];
        snprintf(left, sizeof(left), "v%s", Theme::AppVersion);
        snprintf(right, sizeof(right), "WotLK 3.3.5a \xc2\xb7 %s", Theme::BuildArch());
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(origin.x + S(18.0f), footer_y)), Col(Colors.TextFaint), left);
        const float rw = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, right).x;
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(origin.x + size.x - S(18.0f) - rw, footer_y)), Col(Colors.TextFaint), right);
    }

    // The whole window is a drag handle (except the buttons).
    DragWindowFrom(origin, origin + size);
}
