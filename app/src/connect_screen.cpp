// The first window: connects to the server, shows progress, and hands over to the main window.

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

static const float kVerifiedHoldSeconds = 1.1f; // how long the check mark shows before the main window

void App::EnterConnect()
{
    m_ShownProgress = 0.0f;
    m_VerifiedAt = -1.0f;
    m_FailedAt = -1.0f;
    m_Connection.SetDemoFailure(m_DemoFailure);
    m_Connection.Start();
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

    const ConnectStage stage = m_Connection.Stage();
    const bool verified = stage == ConnectStage::Verified;
    const bool failed = stage == ConnectStage::Failed;
    if (verified && m_VerifiedAt < 0.0f)
        m_VerifiedAt = t;
    if (failed && m_FailedAt < 0.0f)
        m_FailedAt = t;
    const float since_done = verified ? t - m_VerifiedAt : failed ? t - m_FailedAt : 0.0f;

    // Verified: let the check mark play, then fade over to the main window.
    if (verified && since_done > kVerifiedHoldSeconds)
        SwitchTo(Screen::Main);

    m_ShownProgress += (m_Connection.Progress() - m_ShownProgress) * (1.0f - expf(-10.0f * dt));
    if (!Theme::Animations)
        m_ShownProgress = m_Connection.Progress();

    const ImVec4 result_col = failed ? Colors.Danger : Colors.Success;
    const float result_mix = Widgets::Smooth("##result", (verified || failed) ? 1.0f : 0.0f, 6.0f);
    const ImVec4 glow_col = Theme::Mix(Colors.AccentA, result_col, result_mix);

    // --- Background ------------------------------------------------------
    if (Theme::BackgroundEffects)
        Widgets::Particles(dl, origin, origin + size, 1.0f);
    // Calm the particles down behind the text.
    Widgets::RadialGlow(dl, ImVec2(cx, origin.y + S(215.0f)), S(200.0f), Col(Colors.Bg, 0.85f), Col(Colors.Bg, 0.0f));
    const ImVec2 center(cx, origin.y + S(104.0f));
    Widgets::RadialGlow(dl, center, S(180.0f), Col(glow_col, 0.18f), Col(glow_col, 0.0f));

    // --- Window buttons ----------------------------------------------------
    const ImVec2 button_size = S(32.0f, 28.0f);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - S(10.0f) - button_size.x * 2.0f - S(2.0f), origin.y + S(10.0f)));
    if (Widgets::WindowButton("##minimize", false, button_size))
        Window::Minimize();
    ImGui::SameLine(0.0f, S(2.0f));
    if (Widgets::WindowButton("##close", true, button_size))
        Window::Close();

    // --- Spinner / result ring + logo ------------------------------------------
    const float ring_r = S(48.0f), ring_th = S(3.0f);
    if (!verified && !failed)
    {
        Widgets::Spinner(dl, center, ring_r, ring_th, t);
    }
    else
    {
        dl->AddCircle(center, ring_r, Col(Colors.Border, 0.7f), 64, ring_th);
        Widgets::Ring(dl, center, ring_r, ring_th, Widgets::EaseOutCubic(since_done / 0.5f), Col(result_col));
    }

    const float swap = (verified || failed) ? Widgets::EaseOutCubic(since_done / 0.35f) : 0.0f;
    if (swap < 1.0f)
        Widgets::Logo(dl, center, S(30.0f) * (1.0f - 0.35f * swap), t * 0.5f, 1.0f - swap);
    if (swap > 0.0f)
    {
        dl->AddCircleFilled(center, S(36.0f) * (0.6f + 0.4f * swap), Col(result_col, 0.14f * swap), 48);
        const float mark = Widgets::EaseOutCubic((since_done - 0.12f) / 0.4f);
        if (verified)
            Widgets::CheckMark(dl, center, S(44.0f), S(4.0f), mark, Col(result_col));
        else
            Widgets::CrossMark(dl, center, S(40.0f), S(4.0f), mark, Col(result_col));
    }

    // --- Wordmark ---------------------------------------------------------------
    ImFont* display = Theme::Fonts.Display;
    const float name_spacing = S(7.0f);
    const ImVec2 name_size = Widgets::SpacedTextSize(display, Theme::AppName, name_spacing);
    Widgets::GradientText(dl, display, ImVec2(cx - name_size.x * 0.5f, origin.y + S(166.0f)), Theme::AppName,
                          Col(Colors.AccentA), Col(Colors.AccentB), name_spacing);

    ImFont* small = Theme::Fonts.Small;
    const float tag_spacing = S(3.0f);
    const ImVec2 tag_size = Widgets::SpacedTextSize(small, Theme::AppTagline, tag_spacing);
    Widgets::SpacedText(dl, small, ImVec2(cx - tag_size.x * 0.5f, origin.y + S(166.0f) + display->FontSize + S(4.0f)),
                        Col(Colors.TextFaint), Theme::AppTagline, tag_spacing);

    // --- Status -------------------------------------------------------------------
    ImFont* body = Theme::Fonts.Body;
    ImFont* bold = Theme::Fonts.Bold;
    const float status_y = origin.y + S(240.0f);
    if (failed)
    {
        CenteredText(dl, bold, cx, status_y, Col(Colors.Danger), ConnectStageLabel(stage));

        const std::string error = m_Connection.Error();
        const float max_w = size.x - S(60.0f);
        const ImVec2 err_size = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, error.c_str());
        if (err_size.x <= max_w)
            CenteredText(dl, small, cx, status_y + S(22.0f), Col(Colors.TextDim), error.c_str());
        else
            dl->AddText(small, small->FontSize, ImVec2(origin.x + S(30.0f), status_y + S(22.0f)), Col(Colors.TextDim), error.c_str(), nullptr, max_w);

        const ImVec2 btn = S(120.0f, 34.0f);
        const float gap = S(10.0f);
        ImGui::SetCursorScreenPos(ImVec2(cx - btn.x - gap * 0.5f, status_y + S(52.0f)));
        if (Widgets::PrimaryButton("Retry", btn))
            EnterConnect();
        ImGui::SameLine(0.0f, gap);
        if (Widgets::SecondaryButton("Exit", btn))
            Window::Close();
    }
    else
    {
        // Stage label with an animated ellipsis (centered on the full "..." width so it doesn't wobble).
        const char* label = verified ? "Connection verified" : ConnectStageLabel(stage);
        const ImU32 label_col = Col(verified ? Colors.Success : Colors.Text);
        char text[96];
        snprintf(text, sizeof(text), "%s...", label);
        const float full_w = body->CalcTextSizeA(body->FontSize, FLT_MAX, 0.0f, text).x;
        const int dots = verified ? 0 : (int)(t * 2.5f) % 4;
        snprintf(text, sizeof(text), "%s%.*s", label, dots, "...");
        dl->AddText(body, body->FontSize, ImFloor(ImVec2(cx - full_w * 0.5f, status_y)), label_col, text);

        // Progress bar + step counter
        const ImVec2 bar_size(S(300.0f), S(5.0f));
        const float bar_y = status_y + S(30.0f);
        ImGui::SetCursorScreenPos(ImVec2(cx - bar_size.x * 0.5f, bar_y));
        Widgets::ProgressBar(m_ShownProgress, bar_size);

        char step[48], percent[16];
        if (verified)
            snprintf(step, sizeof(step), "Launching");
        else
            snprintf(step, sizeof(step), "Step %d of %d", ImMax(ConnectStageIndex(stage), 1), ConnectStageCount);
        snprintf(percent, sizeof(percent), "%d%%", verified ? 100 : (int)(m_ShownProgress * 100.0f));
        const float info_y = bar_y + bar_size.y + S(8.0f);
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(cx - bar_size.x * 0.5f, info_y)), Col(Colors.TextFaint), step);
        const float pw = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, percent).x;
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(cx + bar_size.x * 0.5f - pw, info_y)), Col(Colors.TextDim), percent);
    }

    // --- Footer (hidden while the Retry / Exit buttons need the space) ----------------
    if (!failed)
    {
        const float footer_y = origin.y + size.y - S(28.0f);
        char left[32], right[64];
        snprintf(left, sizeof(left), "v%s", Theme::AppVersion);
        snprintf(right, sizeof(right), "%s \xc2\xb7 DirectX 9", Theme::BuildArch());
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(origin.x + S(18.0f), footer_y)), Col(Colors.TextFaint), left);
        const float rw = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, right).x;
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(origin.x + size.x - S(18.0f) - rw, footer_y)), Col(Colors.TextFaint), right);
    }

    // The whole window is a drag handle (except the buttons).
    DragWindowFrom(origin, origin + size);
}
