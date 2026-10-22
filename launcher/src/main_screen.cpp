// The main launcher window: title bar, sidebar, status bar and toasts. Pages live in pages.cpp.

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
using Theme::Mix;
using Theme::S;

static const float kLogonRecheckSeconds = 60.0f;

void App::EnterMain()
{
    m_Page = Page::Play;
    m_PageTime = 0.0f;
    m_RealmChangedAt = m_Time;
    m_Toasts.clear();
    if (m_Logon.GetState() == LogonStatus::State::Online)
        ShowToast(ICON_GLOBE, "Connected to " + m_Logon.Host(), Colors.Success);
}

void App::ShowToast(const char* icon, const std::string& text, const ImVec4& color)
{
    m_Toasts.push_back(Toast{ icon, text, color, 0.0f, -1.0f });
    if (m_Toasts.size() > 4)
        m_Toasts.erase(m_Toasts.begin());
}

void App::DrawMain(float dt)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float title_h = S(52.0f), sidebar_w = S(214.0f), status_h = S(30.0f);

    m_PageTime += dt;
    if (!m_Logon.Busy() && m_Time - m_LastLogonCheck > kLogonRecheckSeconds)
        CheckLogon();

    Widgets::RadialGlow(dl, origin + S(30.0f, 10.0f), S(300.0f), Col(Colors.AccentA, 0.10f), Col(Colors.AccentA, 0.0f));
    if (Theme::BackgroundEffects)
        Widgets::Snow("##sidebar_snow", dl, origin, origin + ImVec2(sidebar_w, size.y), 0.45f, 24);

    // Content panel
    const ImVec2 panel_min = origin + ImVec2(sidebar_w, title_h);
    const ImVec2 panel_max = origin + ImVec2(size.x - S(10.0f), size.y - status_h);
    dl->AddRectFilled(panel_min, panel_max, Col(Colors.Surface), S(14.0f));
    dl->AddRect(panel_min, panel_max, Col(Colors.Border, 0.7f), S(14.0f));

    DrawTitleBar(origin, size.x, title_h);
    DrawSidebar(origin + ImVec2(0.0f, title_h), sidebar_w, size.y - title_h - status_h);
    DrawStatusBar(origin + ImVec2(0.0f, size.y - status_h), size.x, status_h);

    // Current page: fades and slides in when it changes.
    const float appear = Widgets::EaseOutCubic(Theme::Animations ? m_PageTime / 0.3f : 1.0f);
    const float slide = S(12.0f) * (1.0f - appear);
    ImGui::SetCursorScreenPos(panel_min + ImVec2(1.0f, S(2.0f) + slide));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * appear);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, S(24.0f, 20.0f));
    ImGui::BeginChild("##page", panel_max - panel_min - ImVec2(S(6.0f), S(4.0f) + slide), false,
                      ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    switch (m_Page)
    {
    case Page::Play:     DrawPlay();     break;
    case Page::Armory:   DrawArmory();   break;
    case Page::Addons:   DrawAddons();   break;
    case Page::Settings: DrawSettings(); break;
    case Page::About:    DrawAbout();    break;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    DrawToasts(panel_max - S(16.0f, 16.0f), dt);
    DragWindowFrom(origin, origin + ImVec2(size.x, title_h));
}

void App::DrawTitleBar(ImVec2 origin, float width, float height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cy = origin.y + height * 0.5f;

    Widgets::Logo(dl, ImVec2(origin.x + S(30.0f), cy), S(13.0f), m_Time * 0.2f);
    ImFont* heading = Theme::Fonts.Heading;
    const float spacing = S(3.0f);
    const ImVec2 name_size = Widgets::SpacedTextSize(heading, Theme::AppName, spacing);
    const ImVec2 name_pos(origin.x + S(54.0f), cy - name_size.y * 0.5f);
    Widgets::GradientText(dl, heading, name_pos, Theme::AppName, Col(Colors.AccentA), Col(Colors.AccentB), spacing);

    ImFont* small = Theme::Fonts.Small;
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(name_pos.x + name_size.x + S(10.0f), cy - small->FontSize * 0.5f + S(1.0f))),
                Col(Colors.TextFaint), "for Warmane");

    // Right side: logon status pill + window buttons
    const ImVec2 button_size = S(38.0f, 30.0f);
    const float buttons_x = origin.x + width - S(10.0f) - button_size.x * 2.0f - S(2.0f);
    char pill[64];
    ImVec4 pill_col = Colors.Warning;
    switch (m_Logon.GetState())
    {
    case LogonStatus::State::Online:  snprintf(pill, sizeof(pill), "Online \xc2\xb7 %d ms", m_Logon.LatencyMs()); pill_col = Colors.Success; break;
    case LogonStatus::State::Offline: snprintf(pill, sizeof(pill), "Logon offline"); pill_col = Colors.Danger; break;
    default:                          snprintf(pill, sizeof(pill), "Checking..."); break;
    }
    const float pill_w = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, pill).x + S(30.0f);
    ImGui::SetCursorScreenPos(ImVec2(buttons_x - S(14.0f) - pill_w, cy - S(13.0f)));
    Widgets::Pill(pill, pill_col, m_Logon.GetState() == LogonStatus::State::Online);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s:3724 (rechecked every minute)", m_Logon.Host().c_str());

    ImGui::SetCursorScreenPos(ImVec2(buttons_x, cy - button_size.y * 0.5f));
    if (Widgets::WindowButton("##minimize", false, button_size))
        Window::Minimize();
    ImGui::SameLine(0.0f, S(2.0f));
    if (Widgets::WindowButton("##close", true, button_size))
        Window::Close();
}

void App::DrawSidebar(ImVec2 origin, float width, float height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float x = origin.x + S(14.0f);
    const float item_w = width - S(28.0f);

    ImGui::SetCursorScreenPos(ImVec2(x + S(10.0f), origin.y + S(12.0f)));
    Widgets::SectionLabel("MENU");

    struct NavEntry { Page page; const char* icon; const char* label; };
    static const NavEntry entries[] =
    {
        { Page::Play,     ICON_PLAY,     "Play"     },
        { Page::Armory,   ICON_SHIELD,   "Armory"   },
        { Page::Addons,   ICON_PUZZLE,   "Addons"   },
        { Page::Settings, ICON_SETTINGS, "Settings" },
        { Page::About,    ICON_INFO,     "About"    },
    };

    float selected_y = origin.y;
    ImGui::SetCursorScreenPos(ImVec2(x, origin.y + S(34.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, S(4.0f)));
    for (const NavEntry& e : entries)
    {
        ImGui::SetCursorScreenPos(ImVec2(x, ImGui::GetCursorScreenPos().y));
        if (Widgets::NavItem(e.icon, e.label, m_Page == e.page, item_w) && m_Page != e.page)
        {
            m_Page = e.page;
            m_PageTime = 0.0f;
        }
        if (m_Page == e.page)
            selected_y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    }
    ImGui::PopStyleVar();

    // Vote for points: opens Warmane's vote page in the browser (manual, within the rules).
    ImGui::SetCursorScreenPos(ImVec2(x, ImGui::GetItemRectMax().y + S(16.0f)));
    if (Widgets::SecondaryButton("Vote for points", ImVec2(item_w, S(36.0f))))
    {
        Game::OpenURL(Game::VoteURL);
        ShowToast(ICON_LINK, "Opened the Warmane vote page", Colors.AccentA);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Opens warmane.com in your browser so you can vote and collect points");

    // Sliding selection indicator
    const float indicator_y = Widgets::Smooth("##nav_indicator", selected_y, 16.0f);
    const ImVec2 ind_min(x, indicator_y - S(10.0f)), ind_max(x + S(3.0f), indicator_y + S(10.0f));
    Widgets::RadialGlow(dl, ImVec2(x + S(1.5f), indicator_y), S(18.0f), Col(Colors.AccentA, 0.35f), Col(Colors.AccentA, 0.0f));
    const int vtx = dl->VtxBuffer.Size;
    dl->AddRectFilled(ind_min, ind_max, IM_COL32_WHITE, S(1.5f));
    Widgets::ShadeGradient(dl, vtx, ind_min, ImVec2(ind_min.x, ind_max.y), Col(Colors.AccentA), Col(Colors.AccentB));

    // Game client card at the bottom (click: settings)
    const ImVec2 c0(origin.x + S(12.0f), origin.y + height - S(74.0f));
    const ImVec2 c1(origin.x + width - S(12.0f), origin.y + height - S(14.0f));
    ImGui::SetCursorScreenPos(c0);
    ImGui::InvisibleButton("##client_card", c1 - c0);
    const float hov = Widgets::Smooth("##client_card_hover", ImGui::IsItemHovered() ? 1.0f : 0.0f, 16.0f);
    if (ImGui::IsItemClicked() && m_Page != Page::Settings)
    {
        m_Page = Page::Settings;
        m_PageTime = 0.0f;
    }
    const bool ready = m_Client.Found && (m_Client.Build == 0 || m_Client.Build == Game::ExpectedBuild);
    const bool running = Game::IsRunning(m_Process);
    const ImVec4 state_col = running ? Colors.AccentB : ready ? Colors.Success : Colors.Warning;
    dl->AddRectFilled(c0, c1, Col(Mix(Colors.Card, Colors.CardHi, hov)), S(12.0f));
    dl->AddRect(c0, c1, Col(Mix(Colors.Border, Colors.AccentA, 0.5f * hov)), S(12.0f));

    const float cy = (c0.y + c1.y) * 0.5f;
    const float box = S(36.0f);
    const ImVec2 box_min(c0.x + S(10.0f), cy - box * 0.5f);
    dl->AddRectFilled(box_min, box_min + ImVec2(box, box), Col(state_col, 0.14f), S(9.0f));
    Widgets::Icon(dl, ICON_GAME, box_min + ImVec2(box, box) * 0.5f, Col(state_col), true);
    Widgets::StatusDot(dl, box_min + ImVec2(box - S(2.0f), box - S(2.0f)), S(3.5f), state_col, running);

    ImFont* bold = Theme::Fonts.Bold;
    ImFont* small = Theme::Fonts.Small;
    const float text_x = box_min.x + box + S(10.0f);
    const float block = bold->FontSize + S(2.0f) + small->FontSize;
    dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(text_x, cy - block * 0.5f)), Col(Colors.Text), "WotLK 3.3.5a");
    const char* sub = running ? "In game" : ready ? "Ready to play" : m_Client.Found ? "Unexpected build" : "Client not set";
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(text_x, cy - block * 0.5f + bold->FontSize + S(2.0f))), Col(state_col), sub);
}

void App::DrawStatusBar(ImVec2 origin, float width, float height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* small = Theme::Fonts.Small;
    const float cy = origin.y + height * 0.5f;
    const float text_y = floorf(cy - small->FontSize * 0.5f);

    char left[160];
    ImVec4 dot = Colors.Warning;
    const std::string host = m_Logon.Host();
    switch (m_Logon.GetState())
    {
    case LogonStatus::State::Online:
        snprintf(left, sizeof(left), "%s  \xc2\xb7  Online  \xc2\xb7  %d ms", host.c_str(), m_Logon.LatencyMs());
        dot = Colors.Success;
        break;
    case LogonStatus::State::Offline:
        snprintf(left, sizeof(left), "%s  \xc2\xb7  %s", host.c_str(), m_Logon.Error().c_str());
        dot = Colors.Danger;
        break;
    default:
        snprintf(left, sizeof(left), "%s  \xc2\xb7  Checking...", host.c_str());
        break;
    }
    if (Game::IsRunning(m_Process))
    {
        const size_t len = strlen(left);
        snprintf(left + len, sizeof(left) - len, "  \xc2\xb7  World of Warcraft is running");
    }
    Widgets::StatusDot(dl, ImVec2(origin.x + S(22.0f), cy), S(3.5f), dot, false);
    dl->AddText(small, small->FontSize, ImVec2(origin.x + S(34.0f), text_y), Col(Colors.TextDim), left);

    char right[128];
    snprintf(right, sizeof(right), "Realm %s  \xc2\xb7  %s  \xc2\xb7  v%s", m_Settings.Realm.c_str(), Theme::BuildArch(), Theme::AppVersion);
    const float rw = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, right).x;
    dl->AddText(small, small->FontSize, ImVec2(floorf(origin.x + width - S(20.0f) - rw), text_y), Col(Colors.TextFaint), right);
}

void App::DrawPageHeader(const char* title, const char* subtitle)
{
    Widgets::TextColored(Theme::Fonts.Heading, Colors.Text, title);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(6.0f));
    Widgets::TextColored(Theme::Fonts.Body, Colors.TextDim, subtitle);
    ImGui::Dummy(ImVec2(0.0f, S(8.0f)));
}

void App::DrawToasts(ImVec2 anchor, float dt)
{
    const float life = 3.6f, fade_in = 0.3f, fade_out = 0.4f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* body = Theme::Fonts.Body;
    const float h = S(46.0f), gap = S(8.0f);

    for (Toast& t : m_Toasts)
        t.Age += dt;
    while (!m_Toasts.empty() && m_Toasts.front().Age > life)
        m_Toasts.erase(m_Toasts.begin());

    int slot = 0;
    for (int i = (int)m_Toasts.size() - 1; i >= 0; i--, slot++)
    {
        Toast& t = m_Toasts[i];
        const float w = ImMax(S(260.0f), body->CalcTextSizeA(body->FontSize, FLT_MAX, 0.0f, t.Text.c_str()).x + S(72.0f));
        const float in = Widgets::EaseOutCubic(t.Age / fade_in);
        const float out = Widgets::EaseOutCubic((t.Age - (life - fade_out)) / fade_out);
        const float alpha = in * (1.0f - out);

        const float target_y = anchor.y - h - slot * (h + gap);
        t.Y = (t.Y < 0.0f || !Theme::Animations) ? target_y : t.Y + (target_y - t.Y) * (1.0f - expf(-14.0f * dt));
        const ImVec2 p0(anchor.x - w + S(30.0f) * (1.0f - in), t.Y + S(8.0f) * out);
        const ImVec2 p1 = p0 + ImVec2(w, h);

        dl->AddRectFilled(p0 + ImVec2(0.0f, S(4.0f)), p1 + ImVec2(0.0f, S(6.0f)), Col(ImVec4(0, 0, 0, 0.35f), alpha), S(12.0f));
        dl->AddRectFilled(p0, p1, Col(Colors.CardHi, alpha), S(10.0f));
        dl->AddRect(p0, p1, Col(Colors.Border, alpha), S(10.0f));
        dl->AddRectFilled(p0 + ImVec2(S(10.0f), S(12.0f)), ImVec2(p0.x + S(13.0f), p1.y - S(12.0f)), Col(t.Color, alpha), S(2.0f));
        Widgets::Icon(dl, t.Icon, ImVec2(p0.x + S(32.0f), (p0.y + p1.y) * 0.5f), Col(t.Color, alpha));
        dl->AddText(body, body->FontSize, ImFloor(ImVec2(p0.x + S(52.0f), (p0.y + p1.y - body->FontSize) * 0.5f)), Col(Colors.Text, alpha), t.Text.c_str());
    }
}
