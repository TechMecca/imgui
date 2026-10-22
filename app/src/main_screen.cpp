// The main window, shown once the connection is verified.
// Everything on these pages is demo content - wire it up to your own data.

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

static const char* kUserName = "demo_user";

static float NextRandom(unsigned int& seed)
{
    seed = seed * 1664525u + 1013904223u;
    return (seed >> 8) / 16777216.0f;
}

static void FormatDuration(char* buf, size_t buf_size, float seconds)
{
    const int s = (int)seconds;
    snprintf(buf, buf_size, "%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
}

static void FormatAgo(char* buf, size_t buf_size, float seconds)
{
    if (seconds < 10.0f)        snprintf(buf, buf_size, "just now");
    else if (seconds < 60.0f)   snprintf(buf, buf_size, "%ds ago", (int)seconds);
    else                        snprintf(buf, buf_size, "%dm ago", (int)(seconds / 60.0f));
}

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------

void App::EnterMain()
{
    m_Page = Page::Dashboard;
    m_PageTime = 0.0f;
    m_SessionTime = 0.0f;
    m_Events.clear();
    m_Toasts.clear();

    if (m_Modules.empty())
    {
        m_Modules =
        {
            { ICON_SYNC,   "Auto Update",      "Install updates in the background.",       true  },
            { ICON_CLOUD,  "Cloud Sync",       "Keep settings in sync across devices.",    true  },
            { ICON_BELL,   "Notifications",    "Desktop alerts for important events.",     false },
            { ICON_BOLT,   "Performance Mode", "Favour speed over power saving.",           false },
            { ICON_SHIELD, "Integrity Check",  "Verify local files before each launch.",   true  },
            { ICON_CODE,   "Developer Logs",   "Write verbose diagnostics to disk.",       false },
        };
    }

    for (int i = 0; i < kLatencySamples; i++)
        m_Latency[i] = 24.0f + 5.0f * sinf(i * 0.45f) + NextRandom(m_Seed) * 4.0f;
    float v = 80.0f;
    for (int i = 0; i < kActivitySamples; i++)
    {
        v = ImClamp(v + (NextRandom(m_Seed) - 0.48f) * 28.0f, 25.0f, 150.0f);
        m_Activity[i] = v;
    }

    AddEvent(ICON_GLOBE, "Connected to server", Colors.Success);
    AddEvent(ICON_SHIELD, "Session verified", Colors.AccentA);
    AddEvent(ICON_DOWNLOAD, "Profile loaded", Colors.AccentB);
    ShowToast(ICON_CHECK, std::string("Welcome back, ") + kUserName, Colors.Success);
}

void App::UpdateDemoData(float dt)
{
    m_SessionTime += dt;
    m_SampleTimer += dt;
    if (m_SampleTimer < 0.6f)
        return;
    m_SampleTimer = 0.0f;

    for (int i = 0; i < kLatencySamples - 1; i++)
        m_Latency[i] = m_Latency[i + 1];
    m_Latency[kLatencySamples - 1] = 24.0f + 5.0f * sinf(m_SessionTime * 0.7f) + NextRandom(m_Seed) * 4.0f;

    for (int i = 0; i < kActivitySamples - 1; i++)
        m_Activity[i] = m_Activity[i + 1];
    m_Activity[kActivitySamples - 1] = ImClamp(m_Activity[kActivitySamples - 2] + (NextRandom(m_Seed) - 0.48f) * 28.0f, 25.0f, 150.0f);
}

void App::AddEvent(const char* icon, const std::string& text, const ImVec4& color)
{
    m_Events.insert(m_Events.begin(), Event{ icon, text, color, m_SessionTime });
    if (m_Events.size() > 12)
        m_Events.pop_back();
}

void App::ShowToast(const char* icon, const std::string& text, const ImVec4& color)
{
    m_Toasts.push_back(Toast{ icon, text, color, 0.0f, -1.0f });
    if (m_Toasts.size() > 4)
        m_Toasts.erase(m_Toasts.begin());
}

//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------

void App::DrawMain(float dt)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float title_h = S(52.0f), sidebar_w = S(214.0f), status_h = S(30.0f);

    UpdateDemoData(dt);
    m_PageTime += dt;

    Widgets::RadialGlow(dl, origin + S(30.0f, 10.0f), S(300.0f), Col(Colors.AccentA, 0.10f), Col(Colors.AccentA, 0.0f));

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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, S(26.0f, 22.0f));
    ImGui::BeginChild("##page", panel_max - panel_min - ImVec2(S(6.0f), S(4.0f) + slide), false,
                      ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();
    switch (m_Page)
    {
    case Page::Dashboard: DrawDashboard(); break;
    case Page::Modules:   DrawModules();   break;
    case Page::Settings:  DrawSettings();  break;
    case Page::About:     DrawAbout();     break;
    default: break;
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

    Widgets::Logo(dl, ImVec2(origin.x + S(30.0f), cy), S(13.0f), m_ScreenTime * 0.25f);
    ImFont* heading = Theme::Fonts.Heading;
    const float spacing = S(3.0f);
    const ImVec2 name_size = Widgets::SpacedTextSize(heading, Theme::AppName, spacing);
    const ImVec2 name_pos(origin.x + S(54.0f), cy - name_size.y * 0.5f);
    Widgets::GradientText(dl, heading, name_pos, Theme::AppName, Col(Colors.AccentA), Col(Colors.AccentB), spacing);

    ImFont* small = Theme::Fonts.Small;
    char version[32];
    snprintf(version, sizeof(version), "v%s", Theme::AppVersion);
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(name_pos.x + name_size.x + S(10.0f), cy - small->FontSize * 0.5f + S(1.0f))),
                Col(Colors.TextFaint), version);

    // Right side: connection pill + window buttons
    const ImVec2 button_size = S(38.0f, 30.0f);
    const float buttons_x = origin.x + width - S(10.0f) - button_size.x * 2.0f - S(2.0f);
    const char* pill_label = "Connected";
    const float pill_w = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, pill_label).x + S(30.0f);
    ImGui::SetCursorScreenPos(ImVec2(buttons_x - S(14.0f) - pill_w, cy - S(13.0f)));
    Widgets::Pill(pill_label, Colors.Success, true);

    ImGui::SetCursorScreenPos(ImVec2(buttons_x, cy - button_size.y * 0.5f));
    if (Widgets::WindowButton("##minimize", false, button_size))
        Window::Minimize();
    ImGui::SameLine(0.0f, S(2.0f));
    if (Widgets::WindowButton("##close", true, button_size))
        Window::Close();
}

// Power symbol drawn with primitives (doesn't depend on the icon font).
static void PowerGlyph(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thickness)
{
    const float pi = 3.14159265f;
    dl->PathArcTo(c, r, -pi * 0.5f + 0.75f, pi * 1.5f - 0.75f, 24);
    dl->PathStroke(col, 0, thickness);
    dl->AddLine(c + ImVec2(0.0f, -r - S(1.5f)), c + ImVec2(0.0f, -r * 0.15f), col, thickness);
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
        { Page::Dashboard, ICON_HOME,     "Dashboard" },
        { Page::Modules,   ICON_APPS,     "Modules"   },
        { Page::Settings,  ICON_SETTINGS, "Settings"  },
        { Page::About,     ICON_INFO,     "About"     },
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

    // Sliding selection indicator
    const float indicator_y = Widgets::Smooth("##nav_indicator", selected_y, 16.0f);
    const ImVec2 ind_min(x, indicator_y - S(10.0f)), ind_max(x + S(3.0f), indicator_y + S(10.0f));
    Widgets::RadialGlow(dl, ImVec2(x + S(1.5f), indicator_y), S(18.0f), Col(Colors.AccentA, 0.35f), Col(Colors.AccentA, 0.0f));
    const int vtx = dl->VtxBuffer.Size;
    dl->AddRectFilled(ind_min, ind_max, IM_COL32_WHITE, S(1.5f));
    Widgets::ShadeGradient(dl, vtx, ind_min, ImVec2(ind_min.x, ind_max.y), Col(Colors.AccentA), Col(Colors.AccentB));

    // User card at the bottom
    const ImVec2 c0(origin.x + S(12.0f), origin.y + height - S(74.0f));
    const ImVec2 c1(origin.x + width - S(12.0f), origin.y + height - S(14.0f));
    const float cy = (c0.y + c1.y) * 0.5f;
    dl->AddRectFilled(c0, c1, Col(Colors.Card), S(12.0f));
    dl->AddRect(c0, c1, Col(Colors.Border), S(12.0f));

    const ImVec2 avatar(c0.x + S(28.0f), cy);
    const float avatar_r = S(17.0f);
    const int avatar_vtx = dl->VtxBuffer.Size;
    dl->AddCircleFilled(avatar, avatar_r, IM_COL32_WHITE, 32);
    Widgets::ShadeGradient(dl, avatar_vtx, avatar - ImVec2(avatar_r, avatar_r), avatar + ImVec2(avatar_r, avatar_r), Col(Colors.AccentA), Col(Colors.AccentB));
    ImFont* bold = Theme::Fonts.Bold;
    ImFont* small = Theme::Fonts.Small;
    const char* initials = "DU";
    const ImVec2 is = bold->CalcTextSizeA(bold->FontSize, FLT_MAX, 0.0f, initials);
    dl->AddText(bold, bold->FontSize, ImFloor(avatar - is * 0.5f), Col(ImVec4(1, 1, 1, 1)), initials);
    const ImVec2 online = avatar + ImVec2(avatar_r * 0.72f, avatar_r * 0.72f);
    dl->AddCircleFilled(online, S(5.5f), Col(Colors.Card), 16);
    Widgets::StatusDot(dl, online, S(3.5f), Colors.Success, false);

    const float text_x = c0.x + S(54.0f);
    const float block = bold->FontSize + S(2.0f) + small->FontSize;
    dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(text_x, cy - block * 0.5f)), Col(Colors.Text), kUserName);
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(text_x, cy - block * 0.5f + bold->FontSize + S(2.0f))), Col(Colors.TextDim), "Verified session");

    // Disconnect button
    const float bs = S(30.0f);
    ImGui::SetCursorScreenPos(ImVec2(c1.x - S(10.0f) - bs, cy - bs * 0.5f));
    ImGui::InvisibleButton("##disconnect", ImVec2(bs, bs));
    const bool hovered = ImGui::IsItemHovered();
    const float hov = Widgets::Smooth("##disconnect_hover", hovered ? 1.0f : 0.0f, 16.0f);
    const ImVec2 bmin = ImGui::GetItemRectMin(), bmax = ImGui::GetItemRectMax();
    if (hov > 0.01f)
        dl->AddRectFilled(bmin, bmax, Col(Colors.Danger, 0.18f * hov), S(8.0f));
    PowerGlyph(dl, (bmin + bmax) * 0.5f + ImVec2(0.0f, S(1.0f)), S(6.0f), Col(Mix(Colors.TextDim, Colors.Danger, hov)), S(1.6f));
    if (hovered)
        ImGui::SetTooltip("Disconnect");
    if (ImGui::IsItemClicked())
        SwitchTo(Screen::Connect);
}

void App::DrawStatusBar(ImVec2 origin, float width, float height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* small = Theme::Fonts.Small;
    const float cy = origin.y + height * 0.5f;
    const float text_y = floorf(cy - small->FontSize * 0.5f);

    Widgets::StatusDot(dl, ImVec2(origin.x + S(22.0f), cy), S(3.5f), Colors.Success, false);
    char session[16];
    FormatDuration(session, sizeof(session), m_SessionTime);
    char left[96];
    snprintf(left, sizeof(left), "Connected  \xc2\xb7  %.0f ms  \xc2\xb7  Session %s", m_Latency[kLatencySamples - 1], session);
    dl->AddText(small, small->FontSize, ImVec2(origin.x + S(34.0f), text_y), Col(Colors.TextDim), left);

    char right[128];
    snprintf(right, sizeof(right), "%s %s  \xc2\xb7  DirectX 9  \xc2\xb7  %.0f FPS", Theme::BuildArch(), Theme::BuildConfig(), ImGui::GetIO().Framerate);
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

//-----------------------------------------------------------------------------
// Pages
//-----------------------------------------------------------------------------

// Small spaced caption on the left of a stat card with an icon on the right.
static void StatCaption(const char* caption, const char* icon)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* small = Theme::Fonts.Small;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    Widgets::SpacedText(dl, small, p, Col(Colors.TextDim), caption, S(1.2f));
    Widgets::Icon(dl, icon, ImVec2(p.x + w - S(8.0f), p.y + small->FontSize * 0.5f), Col(Colors.TextFaint));
    ImGui::Dummy(ImVec2(w, small->FontSize));
}

void App::DrawDashboard()
{
    DrawPageHeader("Dashboard", "Overview of your session and the server.");

    const float gap = S(14.0f);
    const float avail = ImGui::GetContentRegionAvail().x;
    const ImVec2 stat_size(floorf((avail - gap * 2.0f) / 3.0f), S(118.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

    ImFont* heading = Theme::Fonts.Heading;
    ImFont* small = Theme::Fonts.Small;
    char buf[64];

    // Server status
    Widgets::BeginCard("##stat_server", stat_size);
    {
        StatCaption("SERVER", ICON_GLOBE);
        ImGui::Dummy(ImVec2(0.0f, S(2.0f)));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        Widgets::StatusDot(ImGui::GetWindowDrawList(), p + ImVec2(S(5.0f), heading->FontSize * 0.5f + S(1.0f)), S(4.5f), Colors.Success, true);
        ImGui::SetCursorScreenPos(p + ImVec2(S(18.0f), 0.0f));
        Widgets::TextColored(heading, Colors.Text, "Online");
        FormatDuration(buf, sizeof(buf), m_SessionTime);
        char uptime[64];
        snprintf(uptime, sizeof(uptime), "Uptime %s", buf);
        Widgets::TextColored(small, Colors.TextFaint, uptime);
    }
    Widgets::EndCard();
    ImGui::SameLine();

    // Latency with sparkline
    Widgets::BeginCard("##stat_latency", stat_size);
    {
        StatCaption("LATENCY", ICON_SPEED);
        ImGui::Dummy(ImVec2(0.0f, S(2.0f)));
        snprintf(buf, sizeof(buf), "%.0f ms", m_Latency[kLatencySamples - 1]);
        Widgets::TextColored(heading, Colors.Text, buf);
        Widgets::AreaChart("##latency_chart", m_Latency, kLatencySamples, ImGui::GetContentRegionAvail(), 15.0f, 36.0f, false);
    }
    Widgets::EndCard();
    ImGui::SameLine();

    // Session
    Widgets::BeginCard("##stat_session", stat_size);
    {
        StatCaption("SESSION", ICON_SHIELD);
        ImGui::Dummy(ImVec2(0.0f, S(2.0f)));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        Widgets::GradientText(ImGui::GetWindowDrawList(), heading, p, "Verified", Col(Colors.AccentA), Col(Colors.AccentB));
        ImGui::Dummy(ImVec2(0.0f, heading->FontSize));
        snprintf(buf, sizeof(buf), "Signed in as %s", kUserName);
        Widgets::TextColored(small, Colors.TextFaint, buf);
    }
    Widgets::EndCard();

    // Activity chart + recent events
    const float row_h = ImMax(S(230.0f), ImGui::GetContentRegionAvail().y);
    const float left_w = floorf((avail - gap) * 0.6f);
    Widgets::BeginCard("##activity", ImVec2(left_w, row_h));
    {
        const ImVec2 card_tl = ImGui::GetCursorScreenPos();
        Widgets::CardHeader(ICON_SPEED, "Server activity", "Requests per second \xc2\xb7 demo data");
        snprintf(buf, sizeof(buf), "%.0f", m_Activity[kActivitySamples - 1]);
        ImFont* hf = Theme::Fonts.Heading;
        const float vw = hf->CalcTextSizeA(hf->FontSize, FLT_MAX, 0.0f, buf).x;
        const float right = card_tl.x + ImGui::GetContentRegionAvail().x;
        Widgets::GradientText(ImGui::GetWindowDrawList(), hf, ImVec2(right - vw, card_tl.y + S(4.0f)), buf, Col(Colors.AccentA), Col(Colors.AccentB));
        ImGui::Dummy(ImVec2(0.0f, S(4.0f)));
        const ImVec2 chart = ImGui::GetContentRegionAvail();
        Widgets::AreaChart("##activity_chart", m_Activity, kActivitySamples, ImVec2(chart.x, chart.y - S(2.0f)), 0.0f, 160.0f);
    }
    Widgets::EndCard();
    ImGui::SameLine();

    Widgets::BeginCard("##events", ImVec2(avail - left_w - gap, row_h));
    {
        Widgets::CardHeader(ICON_HISTORY, "Recent events");
        ImGui::Dummy(ImVec2(0.0f, S(2.0f)));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* body = Theme::Fonts.Body;
        const float w = ImGui::GetContentRegionAvail().x;
        const float row = S(36.0f);
        int shown = 0;
        for (const Event& e : m_Events)
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (p.y + row > ImGui::GetWindowPos().y + ImGui::GetWindowHeight() - S(10.0f))
                break;
            const float cy = p.y + row * 0.5f;
            dl->AddCircleFilled(ImVec2(p.x + S(12.0f), cy), S(12.0f), Col(e.Color, 0.14f), 24);
            Widgets::Icon(dl, e.Icon, ImVec2(p.x + S(12.0f), cy), Col(e.Color));
            dl->AddText(body, body->FontSize, ImFloor(ImVec2(p.x + S(34.0f), cy - body->FontSize * 0.5f)), Col(Colors.Text), e.Text.c_str());
            FormatAgo(buf, sizeof(buf), m_SessionTime - e.Time);
            const float tw = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, buf).x;
            dl->AddText(small, small->FontSize, ImFloor(ImVec2(p.x + w - tw, cy - small->FontSize * 0.5f)), Col(Colors.TextFaint), buf);
            if (shown > 0)
                dl->AddLine(ImVec2(p.x + S(34.0f), p.y), ImVec2(p.x + w, p.y), Col(Colors.Border, 0.5f));
            ImGui::Dummy(ImVec2(w, row));
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + row));
            shown++;
        }
    }
    Widgets::EndCard();
    ImGui::PopStyleVar();
}

void App::DrawModules()
{
    int active = 0;
    for (const Module& m : m_Modules)
        active += m.Enabled ? 1 : 0;
    char subtitle[96];
    snprintf(subtitle, sizeof(subtitle), "%d of %d modules active. Changes apply instantly.", active, (int)m_Modules.size());
    DrawPageHeader("Modules", subtitle);

    const float gap = S(14.0f);
    const float avail = ImGui::GetContentRegionAvail().x;
    const ImVec2 card_size(floorf((avail - gap) * 0.5f), S(86.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

    for (int i = 0; i < (int)m_Modules.size(); i++)
    {
        Module& m = m_Modules[i];
        if (i % 2 == 1)
            ImGui::SameLine();
        ImGui::PushID(i);
        Widgets::BeginCard("##module", card_size, S(16.0f, 16.0f));
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            const float box = S(44.0f);
            const float on = Widgets::Smooth("##on", m.Enabled ? 1.0f : 0.0f, 10.0f);

            const ImVec2 box_min(p.x, p.y + floorf((ImGui::GetContentRegionAvail().y - box) * 0.5f));
            dl->AddRectFilled(box_min, box_min + ImVec2(box, box), Col(Colors.CardHi), S(10.0f));
            dl->AddRectFilled(box_min, box_min + ImVec2(box, box), Col(Colors.AccentA, 0.18f * on), S(10.0f));
            Widgets::Icon(dl, m.Icon, box_min + ImVec2(box, box) * 0.5f, Col(Mix(Colors.TextDim, Colors.AccentA, on)), true);

            ImFont* bold = Theme::Fonts.Bold;
            ImFont* small = Theme::Fonts.Small;
            const float text_x = p.x + box + S(14.0f);
            const float text_w = w - box - S(14.0f) - S(56.0f);
            const float desc_h = small->CalcTextSizeA(small->FontSize, FLT_MAX, text_w, m.Description).y;
            const float block = bold->FontSize + S(3.0f) + desc_h;
            const float ty = box_min.y + floorf((box - block) * 0.5f);
            dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(text_x, ty)), Col(Colors.Text), m.Name);
            dl->AddText(small, small->FontSize, ImFloor(ImVec2(text_x, ty + bold->FontSize + S(3.0f))), Col(Colors.TextDim), m.Description, nullptr, text_w);

            ImGui::SetCursorScreenPos(ImVec2(p.x + w - S(40.0f), box_min.y + (box - S(22.0f)) * 0.5f));
            if (Widgets::Toggle("##toggle", &m.Enabled))
            {
                const std::string text = std::string(m.Name) + (m.Enabled ? " enabled" : " disabled");
                ShowToast(m.Icon, text, m.Enabled ? Colors.Success : Colors.TextDim);
                AddEvent(m.Icon, text, m.Enabled ? Colors.Success : Colors.TextDim);
            }
        }
        Widgets::EndCard();
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
}

// Settings rows: title + description on the left, a control right-aligned.
struct SettingRow { ImVec2 Start; float Width; };
static const float kRowHeight = 54.0f;

static SettingRow BeginSettingRow(const char* title, const char* description, ImVec2 control_size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* bold = Theme::Fonts.Bold;
    ImFont* small = Theme::Fonts.Small;
    SettingRow row{ ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail().x };
    const float h = S(kRowHeight);
    const float block = bold->FontSize + S(2.0f) + small->FontSize;
    const float y = row.Start.y + (h - block) * 0.5f;
    dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(row.Start.x, y)), Col(Colors.Text), title);
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(row.Start.x, y + bold->FontSize + S(2.0f))), Col(Colors.TextDim), description);
    ImGui::SetCursorScreenPos(ImVec2(row.Start.x + row.Width - control_size.x, row.Start.y + floorf((h - control_size.y) * 0.5f)));
    return row;
}

static void EndSettingRow(const SettingRow& row, bool separator)
{
    const float h = S(kRowHeight);
    if (separator)
        ImGui::GetWindowDrawList()->AddLine(ImVec2(row.Start.x, row.Start.y + h - 0.5f), ImVec2(row.Start.x + row.Width, row.Start.y + h - 0.5f), Col(Colors.Border, 0.6f));
    ImGui::SetCursorScreenPos(ImVec2(row.Start.x, row.Start.y + h));
}

static float SettingsCardHeight(int rows)
{
    return S(16.0f) * 2.0f + S(34.0f) + S(8.0f) + S(kRowHeight) * rows;
}

void App::DrawSettings()
{
    DrawPageHeader("Settings", "Personalise the client. Settings here are for the demo only.");
    const float w = ImGui::GetContentRegionAvail().x;
    const float gap = S(14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

    // Appearance
    Widgets::BeginCard("##appearance", ImVec2(w, SettingsCardHeight(3)), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_PALETTE, "Appearance", "Colors and effects");
        ImGui::Dummy(ImVec2(0.0f, S(0.0f)));

        const float swatch_r = S(11.0f), swatch_gap = S(6.0f);
        const float swatch_w = (swatch_r + S(4.0f)) * 2.0f;
        const ImVec2 swatches_size(Theme::AccentPresetCount * swatch_w + (Theme::AccentPresetCount - 1) * swatch_gap, swatch_w);
        SettingRow row = BeginSettingRow("Accent color", Theme::AccentPresets[Theme::Accent()].Name, swatches_size);
        for (int i = 0; i < Theme::AccentPresetCount; i++)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, swatch_gap);
            const Theme::AccentPreset& preset = Theme::AccentPresets[i];
            if (Widgets::Swatch(preset.Name, preset.A, preset.B, Theme::Accent() == i, swatch_r) && Theme::Accent() != i)
            {
                Theme::SetAccent(i);
                ShowToast(ICON_PALETTE, std::string("Accent set to ") + preset.Name, Colors.AccentA);
            }
        }
        EndSettingRow(row, true);

        row = BeginSettingRow("Animations", "Smooth transitions and motion", S(40.0f, 22.0f));
        Widgets::Toggle("##animations", &Theme::Animations);
        EndSettingRow(row, true);

        row = BeginSettingRow("Background effects", "Particles on the connection screen", S(40.0f, 22.0f));
        Widgets::Toggle("##effects", &Theme::BackgroundEffects);
        EndSettingRow(row, false);
    }
    Widgets::EndCard();

    // General
    Widgets::BeginCard("##general", ImVec2(w, SettingsCardHeight(4)), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_SETTINGS, "General", "Startup, alerts and updates");

        SettingRow row = BeginSettingRow("Start with Windows", "Launch the client when you sign in", S(40.0f, 22.0f));
        Widgets::Toggle("##startup", &m_StartWithWindows);
        EndSettingRow(row, true);

        row = BeginSettingRow("Notifications", "Show a toast when something changes", S(40.0f, 22.0f));
        Widgets::Toggle("##notifications", &m_Notifications);
        EndSettingRow(row, true);

        const float combo_w = S(170.0f);
        static const char* languages[] = { "English", "Deutsch", "Espa\xc3\xb1ol", "Fran\xc3\xa7" "ais" };
        row = BeginSettingRow("Language", "Interface language", ImVec2(combo_w, ImGui::GetFrameHeight()));
        ImGui::SetNextItemWidth(combo_w);
        ImGui::Combo("##language", &m_Language, languages, IM_ARRAYSIZE(languages));
        EndSettingRow(row, true);

        static const char* channels[] = { "Stable", "Beta", "Nightly" };
        row = BeginSettingRow("Update channel", "Which builds to receive", ImVec2(combo_w, ImGui::GetFrameHeight()));
        ImGui::SetNextItemWidth(combo_w);
        ImGui::Combo("##channel", &m_UpdateChannel, channels, IM_ARRAYSIZE(channels));
        EndSettingRow(row, false);
    }
    Widgets::EndCard();

    // Session
    Widgets::BeginCard("##session", ImVec2(w, SettingsCardHeight(2)), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_LOCK, "Session", "Connection and application");
        const ImVec2 button_size = S(130.0f, 34.0f);

        SettingRow row = BeginSettingRow("Disconnect", "Close the session and go back to the connection screen", button_size);
        if (Widgets::SecondaryButton("Disconnect", button_size, &Colors.Danger))
            SwitchTo(Screen::Connect);
        EndSettingRow(row, true);

        row = BeginSettingRow("Exit", "Close the application", button_size);
        if (Widgets::SecondaryButton("Exit##app", button_size))
            Window::Close();
        EndSettingRow(row, false);
    }
    Widgets::EndCard();
    ImGui::PopStyleVar();
}

void App::DrawAbout()
{
    DrawPageHeader("About", "Version and build information.");
    const float w = ImGui::GetContentRegionAvail().x;
    const float gap = S(14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

    // Hero card
    Widgets::BeginCard("##hero", ImVec2(w, S(140.0f)), S(24.0f, 20.0f));
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetContentRegionAvail().y;
        const ImVec2 logo_c(p.x + S(50.0f), p.y + h * 0.5f);
        dl->PushClipRect(ImGui::GetWindowPos() + ImVec2(1.0f, 1.0f), ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(1.0f, 1.0f));
        Widgets::RadialGlow(dl, logo_c, S(72.0f), Col(Colors.AccentA, 0.25f), Col(Colors.AccentA, 0.0f));
        dl->PopClipRect();
        Widgets::Logo(dl, logo_c, S(40.0f), m_ScreenTime * 0.3f);

        ImFont* display = Theme::Fonts.Display;
        ImFont* small = Theme::Fonts.Small;
        const float x = p.x + S(120.0f);
        const float block = display->FontSize + S(6.0f) + small->FontSize + S(14.0f) + S(34.0f);
        const float y = p.y + (h - block) * 0.5f;
        Widgets::GradientText(dl, display, ImVec2(x, y), Theme::AppName, Col(Colors.AccentA), Col(Colors.AccentB), S(6.0f));
        char line[96];
        snprintf(line, sizeof(line), "%s  \xc2\xb7  VERSION %s", Theme::AppTagline, Theme::AppVersion);
        Widgets::SpacedText(dl, small, ImVec2(x, y + display->FontSize + S(6.0f)), Col(Colors.TextDim), line, S(1.5f));

        ImGui::SetCursorScreenPos(ImVec2(x, y + display->FontSize + S(6.0f) + small->FontSize + S(14.0f)));
        if (Widgets::PrimaryButton("Check for updates", S(170.0f, 34.0f)))
        {
            ShowToast(ICON_CHECK, "You're on the latest version", Colors.Success);
            AddEvent(ICON_DOWNLOAD, "Checked for updates", Colors.TextDim);
        }
    }
    Widgets::EndCard();

    // Build info
    const float rh = S(34.0f);
    Widgets::BeginCard("##build", ImVec2(w, S(16.0f) * 2.0f + S(34.0f) + S(8.0f) + rh * 5.0f), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_CODE, "Build", "This executable");
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* body = Theme::Fonts.Body;
        char dpi[32], arch[32];
        snprintf(dpi, sizeof(dpi), "%.0f%%", Theme::DpiScale() * 100.0f);
        snprintf(arch, sizeof(arch), "%s (%d-bit)", Theme::BuildArch(), (int)sizeof(void*) * 8);
        const char* rows[][2] =
        {
            { "Architecture", arch },
            { "Configuration", Theme::BuildConfig() },
            { "Renderer", "Direct3D 9" },
            { "Dear ImGui", IMGUI_VERSION },
            { "Display scale", dpi },
        };
        const float rw = ImGui::GetContentRegionAvail().x;
        for (int i = 0; i < IM_ARRAYSIZE(rows); i++)
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float ty = floorf(p.y + (rh - body->FontSize) * 0.5f);
            dl->AddText(body, body->FontSize, ImVec2(p.x, ty), Col(Colors.TextDim), rows[i][0]);
            const float vw = body->CalcTextSizeA(body->FontSize, FLT_MAX, 0.0f, rows[i][1]).x;
            dl->AddText(body, body->FontSize, ImVec2(floorf(p.x + rw - vw), ty), Col(Colors.Text), rows[i][1]);
            if (i > 0)
                dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + rw, p.y), Col(Colors.Border, 0.5f));
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + rh));
        }
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    }
    Widgets::EndCard();
    ImGui::PopStyleVar();
}

//-----------------------------------------------------------------------------
// Toasts
//-----------------------------------------------------------------------------

void App::DrawToasts(ImVec2 anchor, float dt)
{
    const float life = 3.2f, fade_in = 0.3f, fade_out = 0.4f;
    const float w = S(290.0f), h = S(46.0f), gap = S(8.0f);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* body = Theme::Fonts.Body;

    for (Toast& t : m_Toasts)
        t.Age += dt;
    while (!m_Toasts.empty() && m_Toasts.front().Age > life)
        m_Toasts.erase(m_Toasts.begin());
    if (!m_Notifications)
        return;

    int slot = 0;
    for (int i = (int)m_Toasts.size() - 1; i >= 0; i--, slot++)
    {
        Toast& t = m_Toasts[i];
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
