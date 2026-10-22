// Launcher pages: Play, Addons, Settings, About.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"

#include "imgui_internal.h"
#include "theme.h"
#include "widgets.h"
#include "window.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

using Theme::Col;
using Theme::Colors;
using Theme::Mix;
using Theme::S;

static bool ContainsNoCase(const std::string& haystack, const char* needle)
{
    if (!needle[0])
        return true;
    const size_t n = strlen(needle);
    for (size_t i = 0; i + n <= haystack.size(); i++)
    {
        size_t j = 0;
        while (j < n && tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j]))
            j++;
        if (j == n)
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Play
//-----------------------------------------------------------------------------

void App::DrawPlay()
{
    const float gap = S(14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
    DrawBanner(ImVec2(ImGui::GetContentRegionAvail().x, S(232.0f)));
    DrawRealmCards();
    DrawLaunchBar();
    ImGui::PopStyleVar();
}

// Northrend at night: aurora, moon, two mountain ranges and snow, with the selected realm on top.
void App::DrawBanner(ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1 = p0 + size;
    ImGui::Dummy(size);
    const float rounding = S(14.0f);
    const Game::Realm& realm = Game::Realms[Game::FindRealm(m_Settings.Realm)];

    dl->PushClipRect(p0, p1, true);
    const ImU32 sky_top = Col(Mix(Colors.Bg, Colors.AccentA, 0.22f)), sky_bottom = Col(Colors.Bg);
    dl->AddRectFilledMultiColor(p0, p1, sky_top, sky_top, sky_bottom, sky_bottom);
    Widgets::Aurora(dl, p0, p1, m_Time, 1.0f);

    const ImVec2 moon(p1.x - S(110.0f), p0.y + S(58.0f));
    Widgets::RadialGlow(dl, moon, S(110.0f), Col(Colors.AccentB, 0.22f), Col(Colors.AccentB, 0.0f));
    dl->AddCircleFilled(moon, S(20.0f), Col(Mix(ImVec4(1, 1, 1, 1), Colors.AccentB, 0.25f), 0.92f), 48);
    dl->AddCircleFilled(moon + S(-6.0f, -5.0f), S(4.0f), Col(Colors.AccentA, 0.12f), 16);
    dl->AddCircleFilled(moon + S(7.0f, 6.0f), S(3.0f), Col(Colors.AccentA, 0.10f), 16);

    Widgets::Mountains(dl, p0 + ImVec2(0.0f, size.y * 0.35f), p1, 7u, S(120.0f),
                       Col(Mix(Colors.Bg, Colors.AccentA, 0.28f)), Col(Mix(Colors.Bg, Colors.AccentA, 0.08f)), Col(ImVec4(1, 1, 1, 0.14f)));
    Widgets::Mountains(dl, p0 + ImVec2(0.0f, size.y * 0.55f), p1, 23u, S(75.0f),
                       Col(Mix(Colors.Bg, Colors.AccentA, 0.09f)), Col(Colors.Bg), Col(ImVec4(1, 1, 1, 0.16f)));
    if (Theme::BackgroundEffects)
        Widgets::Snow("##banner_snow", dl, p0, p1, 1.0f, 80);

    // Darken behind the text so it stays readable.
    const ImU32 clear = Col(Colors.Bg, 0.0f);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, p0.y + size.y * 0.45f), p1, clear, clear, Col(Colors.Bg, 0.75f), Col(Colors.Bg, 0.75f));
    dl->AddRectFilledMultiColor(p0, ImVec2(p0.x + size.x * 0.6f, p1.y), Col(Colors.Bg, 0.5f), clear, clear, Col(Colors.Bg, 0.5f));

    // Realm chip (top-left)
    ImFont* small = Theme::Fonts.Small;
    const ImVec2 chip_text = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, realm.Tag);
    const ImVec2 c0 = p0 + S(24.0f, 22.0f), c1 = c0 + ImVec2(chip_text.x + S(20.0f), S(24.0f));
    dl->AddRectFilled(c0, c1, Col(Colors.Bg, 0.6f), S(12.0f));
    dl->AddRect(c0, c1, Col(Colors.AccentB, 0.45f), S(12.0f));
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(c0.x + S(10.0f), (c0.y + c1.y - chip_text.y) * 0.5f)), Col(Colors.AccentB), realm.Tag);

    // Expansion line, realm name, description - they slide in when the realm changes.
    const float e = Widgets::EaseOutCubic(Theme::Animations ? (m_Time - m_RealmChangedAt) / 0.45f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * e);
    ImFont* display = Theme::Fonts.Display;
    ImFont* body = Theme::Fonts.Body;
    const float x = p0.x + S(28.0f) + S(18.0f) * (1.0f - e);
    float y = p1.y - S(26.0f) - body->FontSize - S(6.0f) - display->FontSize - S(8.0f) - small->FontSize;
    Widgets::SpacedText(dl, small, ImVec2(x, y), Col(Colors.AccentB, 0.95f), "WRATH OF THE LICH KING  \xc2\xb7  3.3.5a", S(2.0f));
    y += small->FontSize + S(8.0f);
    std::string name = realm.Name;
    for (char& c : name)
        c = (char)toupper((unsigned char)c);
    Widgets::GradientText(dl, display, ImVec2(x, y), name.c_str(), Col(ImVec4(1, 1, 1, 1)), Col(Colors.AccentB), S(5.0f));
    y += display->FontSize + S(6.0f);
    dl->AddText(body, body->FontSize, ImFloor(ImVec2(x, y)), Col(Colors.TextDim), realm.Description);
    ImGui::PopStyleVar();
    dl->PopClipRect();

    Widgets::MaskCorners(dl, p0, p1, rounding, Col(Colors.Surface));
    dl->AddRect(p0, p1, Col(Colors.Border, 0.8f), rounding);
}

void App::DrawRealmCards()
{
    Widgets::SectionLabel("CHOOSE YOUR REALM");
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(4.0f));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* bold = Theme::Fonts.Bold;
    ImFont* small = Theme::Fonts.Small;
    const float gap = S(12.0f);
    const ImVec2 card((ImGui::GetContentRegionAvail().x - gap * (Game::RealmCount - 1)) / Game::RealmCount, S(100.0f));
    const int selected_index = Game::FindRealm(m_Settings.Realm);
    const float rounding = S(12.0f);

    for (int i = 0; i < Game::RealmCount; i++)
    {
        const Game::Realm& realm = Game::Realms[i];
        const bool selected = i == selected_index;
        if (i > 0)
            ImGui::SameLine(0.0f, gap);
        ImGui::PushID(i);
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##realm", card) && !selected)
        {
            m_Settings.Realm = realm.Name;
            m_RealmChangedAt = m_Time;
            SaveSoon();
        }
        const float hov = Widgets::Smooth("##hover", ImGui::IsItemHovered() ? 1.0f : 0.0f, 16.0f);
        const float sel = Widgets::Smooth("##selected", selected ? 1.0f : 0.0f, 12.0f);
        ImGui::PopID();

        const ImVec2 q0 = p0 - ImVec2(0.0f, S(2.0f) * hov), q1 = q0 + card;
        dl->AddRectFilled(q0, q1, Col(Mix(Colors.Card, Colors.CardHi, hov * 0.6f)), rounding);
        if (sel > 0.01f)
        {
            const int vtx = dl->VtxBuffer.Size;
            dl->AddRectFilled(q0, q1, IM_COL32_WHITE, rounding);
            Widgets::ShadeGradient(dl, vtx, q0, ImVec2(q0.x, q1.y), Col(Colors.AccentA, 0.20f * sel), Col(Colors.AccentA, 0.02f * sel));
        }
        dl->AddRect(q0, q1, Col(Mix(Mix(Colors.Border, Colors.TextFaint, 0.5f * hov), Colors.AccentA, sel)), rounding, 0, 1.0f + sel);

        const float x = q0.x + S(16.0f);
        dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(x, q0.y + S(14.0f))), Col(Colors.Text), realm.Name);
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(x, q0.y + S(16.0f) + bold->FontSize)), Col(Mix(Colors.TextFaint, Colors.AccentB, ImMax(sel, hov * 0.5f))), realm.Tag);
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(x, q0.y + S(56.0f))), Col(Colors.TextDim), realm.Description, nullptr, card.x - S(32.0f));

        if (sel > 0.01f)
        {
            const ImVec2 badge(q1.x - S(18.0f), q0.y + S(18.0f));
            dl->AddCircleFilled(badge, S(9.0f), Col(Colors.AccentA, sel), 24);
            Widgets::CheckMark(dl, badge, S(12.0f), S(1.8f), sel, Col(ImVec4(1, 1, 1, 1)));
        }
    }
}

void App::DrawLaunchBar()
{
    Widgets::BeginCard("##launch", ImVec2(ImGui::GetContentRegionAvail().x, S(92.0f)), S(18.0f, 16.0f));
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 inner = ImGui::GetContentRegionAvail();
        const bool running = Game::IsRunning(m_Process);

        // Client icon
        const float box = S(52.0f);
        const ImVec2 box_min(p.x, p.y + (inner.y - box) * 0.5f);
        dl->AddRectFilled(box_min, box_min + ImVec2(box, box), Col(Colors.AccentA, 0.14f), S(12.0f));
        Widgets::Icon(dl, ICON_GAME, box_min + ImVec2(box, box) * 0.5f, Col(Colors.AccentA), true);

        // Play button (right), cache toggle (left of it)
        const ImVec2 play_size = S(196.0f, 54.0f);
        const float play_x = p.x + inner.x - play_size.x;
        ImFont* bold = Theme::Fonts.Bold;
        ImFont* small = Theme::Fonts.Small;
        const ImVec2 toggle_size = S(40.0f, 22.0f);
        const float toggle_x = play_x - S(28.0f) - toggle_size.x;
        const char* toggle_label = "Clear cache";
        const float label_w = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, toggle_label).x;
        const float label_x = toggle_x - S(10.0f) - label_w;
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(label_x, p.y + (inner.y - small->FontSize) * 0.5f)), Col(Colors.TextDim), toggle_label);
        ImGui::SetCursorScreenPos(ImVec2(toggle_x, p.y + (inner.y - toggle_size.y) * 0.5f));
        if (Widgets::Toggle("##clear_cache", &m_Settings.ClearCache))
            SaveSoon();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Delete the Cache folder before starting the game");

        ImGui::SetCursorScreenPos(ImVec2(play_x, p.y + (inner.y - play_size.y) * 0.5f));
        const char* play_label = running ? "PLAYING" : m_Client.Found ? "PLAY" : "FIND GAME";
        if (Widgets::PlayButton(play_label, play_size, running))
            LaunchGame();

        // Client text
        const float text_x = box_min.x + box + S(14.0f);
        const float text_w = label_x - S(20.0f) - text_x;
        const float block = bold->FontSize + small->FontSize * 2.0f + S(6.0f);
        float y = p.y + (inner.y - block) * 0.5f;
        dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(text_x, y)), Col(Colors.Text), "World of Warcraft 3.3.5a");
        y += bold->FontSize + S(3.0f);
        const std::string folder = m_Settings.GameFolder.empty() ? std::string("No game folder selected") : Widgets::Ellipsize(small, m_Settings.GameFolder.c_str(), text_w);
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(text_x, y)), Col(Colors.TextFaint), folder.c_str());
        y += small->FontSize + S(3.0f);
        ImVec4 status_col;
        const std::string status = running ? std::string("In game \xc2\xb7 ") + m_Settings.Realm : ClientStatus(&status_col);
        if (running)
            status_col = Colors.AccentB;
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(text_x, y)), Col(status_col), Widgets::Ellipsize(small, status.c_str(), text_w).c_str());
    }
    Widgets::EndCard();
}

//-----------------------------------------------------------------------------
// Addons
//-----------------------------------------------------------------------------

void App::DrawAddons()
{
    if (!m_AddonsScanned)
    {
        m_Addons = Game::ScanAddons(m_Settings.GameFolder);
        m_AddonsScanned = true;
    }
    char subtitle[96];
    if (m_Client.Found)
        snprintf(subtitle, sizeof(subtitle), "%d installed in Interface\\AddOns", (int)m_Addons.size());
    else
        snprintf(subtitle, sizeof(subtitle), "Choose your game folder to see installed addons.");
    DrawPageHeader("Addons", subtitle);

    const float w = ImGui::GetContentRegionAvail().x;
    if (!m_Client.Found)
    {
        if (Widgets::PrimaryButton("Choose game folder", S(190.0f, 38.0f)))
            BrowseForGame();
        return;
    }

    // Toolbar: search, rescan, open folder
    const float frame_h = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(S(280.0f));
    ImGui::InputTextWithHint("##filter", "Search addons", m_AddonFilter, sizeof(m_AddonFilter));
    ImGui::SameLine(0.0f, S(8.0f));
    if (Widgets::IconButton("##rescan", ICON_REFRESH, ImVec2(frame_h, frame_h), "Rescan"))
        m_AddonsScanned = false;
    const float open_w = S(170.0f);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - open_w);
    if (Widgets::SecondaryButton("Open AddOns folder", ImVec2(open_w, frame_h)))
        Game::OpenInExplorer(Game::AddonsFolder(m_Settings.GameFolder));
    ImGui::Dummy(ImVec2(0.0f, S(4.0f)));

    // List
    Widgets::BeginCard("##addon_list", ImVec2(w, ImMax(ImGui::GetContentRegionAvail().y, S(200.0f))), S(6.0f, 6.0f));
    ImGui::BeginChild("##rows", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* bold = Theme::Fonts.Bold;
        ImFont* small = Theme::Fonts.Small;
        const float row_h = S(56.0f);
        int shown = 0;
        for (int i = 0; i < (int)m_Addons.size(); i++)
        {
            const Game::Addon& addon = m_Addons[i];
            if (!ContainsNoCase(addon.Title, m_AddonFilter) && !ContainsNoCase(addon.Folder, m_AddonFilter))
                continue;
            ImGui::PushID(i);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float rw = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("##row", ImVec2(rw, row_h));
            const bool hovered = ImGui::IsItemHovered();
            const float hov = Widgets::Smooth("##hover", hovered ? 1.0f : 0.0f, 16.0f);
            ImGui::PopID();

            if (hov > 0.01f)
                dl->AddRectFilled(p, p + ImVec2(rw, row_h), Col(Colors.CardHi, hov), S(8.0f));
            else if (shown > 0)
                dl->AddLine(ImVec2(p.x + S(56.0f), p.y), ImVec2(p.x + rw - S(12.0f), p.y), Col(Colors.Border, 0.5f));

            const float cy = p.y + row_h * 0.5f;
            const float box = S(34.0f);
            const ImVec2 box_min(p.x + S(10.0f), cy - box * 0.5f);
            dl->AddRectFilled(box_min, box_min + ImVec2(box, box), Col(Colors.AccentA, 0.12f), S(8.0f));
            Widgets::Icon(dl, ICON_PUZZLE, box_min + ImVec2(box, box) * 0.5f, Col(Colors.AccentA));

            const float vw = addon.Version.empty() ? 0.0f : small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, addon.Version.c_str()).x;
            const float text_x = p.x + S(56.0f);
            const float text_w = rw - S(56.0f) - S(24.0f) - vw;
            const bool has_notes = !addon.Notes.empty();
            const float block = bold->FontSize + (has_notes ? S(2.0f) + small->FontSize : 0.0f);
            dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(text_x, cy - block * 0.5f)), Col(Colors.Text), Widgets::Ellipsize(bold, addon.Title.c_str(), text_w).c_str());
            if (has_notes)
                dl->AddText(small, small->FontSize, ImFloor(ImVec2(text_x, cy - block * 0.5f + bold->FontSize + S(2.0f))), Col(Colors.TextDim),
                            Widgets::Ellipsize(small, addon.Notes.c_str(), text_w).c_str());
            if (vw > 0.0f)
                dl->AddText(small, small->FontSize, ImFloor(ImVec2(p.x + rw - S(14.0f) - vw, cy - small->FontSize * 0.5f)), Col(Colors.TextFaint), addon.Version.c_str());
            if (hovered)
                ImGui::SetTooltip("%s\nFolder: %s%s%s", addon.Title.c_str(), addon.Folder.c_str(),
                                  addon.Author.empty() ? "" : "\nAuthor: ", addon.Author.c_str());
            shown++;
        }
        if (shown == 0)
        {
            const char* empty = m_Addons.empty() ? "No addons installed yet." : "No addons match your search.";
            const ImVec2 ts = ImGui::CalcTextSize(empty);
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - ts.x) * 0.5f, S(40.0f)));
            ImGui::TextDisabled("%s", empty);
        }
    }
    ImGui::EndChild();
    Widgets::EndCard();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

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
    const float text_w = row.Width - control_size.x - S(24.0f);
    dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(row.Start.x, y)), Col(Colors.Text), title);
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(row.Start.x, y + bold->FontSize + S(2.0f))), Col(Colors.TextDim),
                Widgets::Ellipsize(small, description, text_w).c_str());
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
    DrawPageHeader("Settings", "Game client, launch behaviour and appearance.");
    const float w = ImGui::GetContentRegionAvail().x;
    const float gap = S(14.0f);
    const ImVec2 toggle = S(40.0f, 22.0f);
    const ImVec2 button = S(130.0f, 34.0f);
    const float combo_w = S(190.0f);
    const float frame_h = ImGui::GetFrameHeight();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

    // Game client
    Widgets::BeginCard("##client", ImVec2(w, SettingsCardHeight(5)), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_GAME, "Game client", "World of Warcraft 3.3.5a (build 12340)");

        SettingRow row = BeginSettingRow("Game folder", m_Settings.GameFolder.empty() ? "Not set - pick the folder that contains Wow.exe" : m_Settings.GameFolder.c_str(), button);
        if (Widgets::SecondaryButton("Browse...", button))
            BrowseForGame();
        EndSettingRow(row, true);

        const ImVec2 realmlist_size(S(220.0f) + S(8.0f) + frame_h, frame_h);
        row = BeginSettingRow("Realmlist", "Written to realmlist.wtf and Config.wtf on launch", realmlist_size);
        ImGui::SetNextItemWidth(S(220.0f));
        ImGui::InputText("##realmlist", m_RealmlistInput, sizeof(m_RealmlistInput));
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            m_Settings.Realmlist = m_RealmlistInput[0] ? m_RealmlistInput : Game::DefaultRealmlist;
            SaveSoon();
            CheckLogon();
        }
        ImGui::SameLine(0.0f, S(8.0f));
        if (Widgets::IconButton("##reset_realmlist", ICON_REFRESH, ImVec2(frame_h, frame_h), "Reset to logon.warmane.com"))
        {
            m_Settings.Realmlist = Game::DefaultRealmlist;
            snprintf(m_RealmlistInput, sizeof(m_RealmlistInput), "%s", Game::DefaultRealmlist);
            SaveSoon();
            CheckLogon();
        }
        EndSettingRow(row, true);

        row = BeginSettingRow("Clear cache before launch", "Fixes stale item and quest data after realm changes", toggle);
        if (Widgets::Toggle("##clear_cache_setting", &m_Settings.ClearCache))
            SaveSoon();
        EndSettingRow(row, true);

        static const char* display_modes[] = { "Use game setting", "Windowed", "Fullscreen" };
        row = BeginSettingRow("Display mode", "Applied to Config.wtf when you press Play", ImVec2(combo_w, frame_h));
        ImGui::SetNextItemWidth(combo_w);
        if (ImGui::Combo("##display_mode", &m_Settings.DisplayMode, display_modes, IM_ARRAYSIZE(display_modes)))
            SaveSoon();
        EndSettingRow(row, true);

        static const char* after_launch[] = { "Keep launcher open", "Minimize launcher", "Close launcher" };
        row = BeginSettingRow("After launching", "What the launcher does once the game starts", ImVec2(combo_w, frame_h));
        ImGui::SetNextItemWidth(combo_w);
        if (ImGui::Combo("##after_launch", &m_Settings.AfterLaunch, after_launch, IM_ARRAYSIZE(after_launch)))
            SaveSoon();
        EndSettingRow(row, false);
    }
    Widgets::EndCard();

    // Appearance
    Widgets::BeginCard("##appearance", ImVec2(w, SettingsCardHeight(3)), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_PALETTE, "Appearance", "Colors and effects");

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
                m_Settings.Accent = i;
                SaveSoon();
            }
        }
        EndSettingRow(row, true);

        row = BeginSettingRow("Animations", "Smooth transitions and motion", toggle);
        if (Widgets::Toggle("##animations", &m_Settings.Animations))
            SaveSoon();
        EndSettingRow(row, true);

        row = BeginSettingRow("Snow and aurora", "Falling snow on the banner and connection screen", toggle);
        if (Widgets::Toggle("##snow", &m_Settings.Snow))
            SaveSoon();
        EndSettingRow(row, false);
    }
    Widgets::EndCard();

    // Maintenance
    Widgets::BeginCard("##maintenance", ImVec2(w, SettingsCardHeight(2)), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_DELETE, "Maintenance", "Housekeeping for the game folder");

        SettingRow row = BeginSettingRow("Clear cache now", "Deletes the client's Cache folder", button);
        if (Widgets::SecondaryButton("Clear cache", button, &Colors.Warning) && m_Client.Found)
        {
            std::string error;
            if (Game::IsRunning(m_Process))
                ShowToast(ICON_WARNING, "Close the game before clearing the cache", Colors.Warning);
            else if (Game::ClearCache(m_Settings.GameFolder, &error))
                ShowToast(ICON_CHECK, "Cache cleared", Colors.Success);
            else
                ShowToast(ICON_ERROR, error, Colors.Danger);
        }
        EndSettingRow(row, true);

        row = BeginSettingRow("Game folder", "Open it in Explorer", button);
        if (Widgets::SecondaryButton("Open folder", button) && !m_Settings.GameFolder.empty())
            Game::OpenInExplorer(m_Settings.GameFolder);
        EndSettingRow(row, false);
    }
    Widgets::EndCard();
    ImGui::PopStyleVar();
}

//-----------------------------------------------------------------------------
// About
//-----------------------------------------------------------------------------

void App::DrawAbout()
{
    DrawPageHeader("About", "Version, build and what the launcher does.");
    const float w = ImGui::GetContentRegionAvail().x;
    const float gap = S(14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

    // Hero
    Widgets::BeginCard("##hero", ImVec2(w, S(132.0f)), S(24.0f, 20.0f));
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetContentRegionAvail().y;
        const ImVec2 logo_c(p.x + S(46.0f), p.y + h * 0.5f);
        dl->PushClipRect(ImGui::GetWindowPos() + ImVec2(1.0f, 1.0f), ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(1.0f, 1.0f));
        Widgets::RadialGlow(dl, logo_c, S(70.0f), Col(Colors.AccentA, 0.25f), Col(Colors.AccentA, 0.0f));
        dl->PopClipRect();
        Widgets::Logo(dl, logo_c, S(38.0f), m_Time * 0.25f);

        ImFont* display = Theme::Fonts.Display;
        ImFont* small = Theme::Fonts.Small;
        const float x = p.x + S(112.0f);
        const float block = display->FontSize + S(6.0f) + small->FontSize + S(8.0f) + small->FontSize;
        const float y = p.y + (h - block) * 0.5f;
        Widgets::GradientText(dl, display, ImVec2(x, y), Theme::AppName, Col(Colors.AccentA), Col(Colors.AccentB), S(6.0f));
        char line[96];
        snprintf(line, sizeof(line), "%s  \xc2\xb7  VERSION %s", Theme::AppTagline, Theme::AppVersion);
        Widgets::SpacedText(dl, small, ImVec2(x, y + display->FontSize + S(6.0f)), Col(Colors.TextDim), line, S(1.5f));
        dl->AddText(small, small->FontSize, ImFloor(ImVec2(x, y + display->FontSize + S(14.0f) + small->FontSize)), Col(Colors.Warning),
                    "Unofficial \xc2\xb7 not affiliated with Warmane or Blizzard Entertainment");
    }
    Widgets::EndCard();

    // What it does
    const char* paragraphs[] =
    {
        "Pressing Play writes your realmlist to Data\\<locale>\\realmlist.wtf and WTF\\Config.wtf, remembers the realm you picked, "
        "optionally clears the Cache folder, and starts Wow.exe.",
        "The launcher never reads or changes game memory and never asks for your account details: you log in inside the game as usual.",
    };
    ImFont* body = Theme::Fonts.Body;
    const float wrap = w - S(36.0f);
    float text_h = 0.0f;
    for (const char* p : paragraphs)
        text_h += body->CalcTextSizeA(body->FontSize, FLT_MAX, wrap, p).y + S(8.0f);
    Widgets::BeginCard("##what", ImVec2(w, S(16.0f) * 2.0f + S(34.0f) + S(8.0f) + text_h), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_SHIELD, "What the launcher changes");
        ImGui::PushStyleColor(ImGuiCol_Text, Colors.TextDim);
        ImGui::PushTextWrapPos(0.0f);
        for (const char* p : paragraphs)
            ImGui::TextUnformatted(p);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    Widgets::EndCard();

    // Build info
    const float rh = S(34.0f);
    Widgets::BeginCard("##build", ImVec2(w, S(16.0f) * 2.0f + S(34.0f) + S(8.0f) + rh * 4.0f), S(18.0f, 16.0f));
    {
        Widgets::CardHeader(ICON_CODE, "Build", "This executable");
        ImDrawList* dl = ImGui::GetWindowDrawList();
        char arch[32];
        snprintf(arch, sizeof(arch), "%s (%d-bit)", Theme::BuildArch(), (int)sizeof(void*) * 8);
        const char* rows[][2] =
        {
            { "Architecture", arch },
            { "Configuration", Theme::BuildConfig() },
            { "Renderer", "Direct3D 9" },
            { "Dear ImGui", IMGUI_VERSION },
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
