// Launcher pages: Play, Armory, Addons, Settings, About.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"

#include "imgui_internal.h"
#include "theme.h"
#include "widgets.h"
#include "window.h"

#include <algorithm>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
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
// Armory (public character search - opens the browser, nothing scraped)
//-----------------------------------------------------------------------------

// Class colors (WoW 3.3.5a).
static ImVec4 ClassColor(const std::string& klass)
{
    struct C { const char* name; unsigned int rgb; };
    static const C colors[] = {
        {"Warrior",0xC79C6E},{"Paladin",0xF58CBA},{"Hunter",0xABD473},{"Rogue",0xFFF569},
        {"Priest",0xFFFFFF},{"Death Knight",0xC41F3B},{"Shaman",0x0070DE},{"Mage",0x69CCF0},
        {"Warlock",0x9482C9},{"Druid",0xFF7D0A},
    };
    for (const C& c : colors)
        if (klass == c.name)
            return ImVec4(((c.rgb >> 16) & 0xFF) / 255.0f, ((c.rgb >> 8) & 0xFF) / 255.0f, (c.rgb & 0xFF) / 255.0f, 1.0f);
    return Colors.Text;
}

// Item quality colors and names (poor .. heirloom).
static ImVec4 QualityColor(int quality)
{
    static const unsigned int rgb[] = { 0x9D9D9D, 0xFFFFFF, 0x1EFF00, 0x0070DD, 0xA335EE, 0xFF8000, 0xE6CC80, 0xE6CC80 };
    if (quality < 0 || quality > 7)
        return Colors.TextDim;
    const unsigned int c = rgb[quality];
    return ImVec4(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, 1.0f);
}

static const char* QualityName(int quality)
{
    static const char* names[] = { "Poor", "Common", "Uncommon", "Rare", "Epic", "Legendary", "Artifact", "Heirloom" };
    return (quality >= 0 && quality <= 7) ? names[quality] : "";
}

void App::StartArmorySearch(const std::string& name, int realm_index)
{
    std::string trimmed = name;
    const size_t bgn = trimmed.find_first_not_of(" \t");
    const size_t end = trimmed.find_last_not_of(" \t");
    trimmed = (bgn == std::string::npos) ? std::string() : trimmed.substr(bgn, end - bgn + 1);
    if (trimmed.empty())
    {
        ShowToast(ICON_WARNING, "Type a character name first", Colors.Warning);
        return;
    }
    realm_index = ImClamp(realm_index, 0, Game::RealmCount - 1);
    m_ArmoryRealm = realm_index;
    snprintf(m_ArmoryName, sizeof(m_ArmoryName), "%s", trimmed.c_str());
    const std::string realm = Game::Realms[realm_index].Name;

    m_Armory.Search(trimmed, realm);

    std::string entry = trimmed + "/" + realm;
    std::vector<std::string>& h = m_Settings.ArmoryHistory;
    h.erase(std::remove(h.begin(), h.end(), entry), h.end());
    h.insert(h.begin(), entry);
    if (h.size() > 8)
        h.resize(8);
    SaveSoon();
}

// Race portraits are regular icons on the same CDN as the gear icons:
// .../icons/large/achievement_character_<race>_<gender>.jpg
std::string App::PortraitUrl(const Character& c) const
{
    std::string base = "http://cdn.warmane.com/wotlk/icons/large/";
    for (const GearItem& g : c.Gear)
        if (!g.IconUrl.empty())
        {
            base = g.IconUrl.substr(0, g.IconUrl.find_last_of('/') + 1);
            break;
        }
    std::string race;
    for (char ch : c.Race)
        if (ch != ' ')
            race += (char)tolower((unsigned char)ch); // "Night Elf" -> "nightelf"
    if (race.empty())
        return std::string();
    const bool female = !c.Gender.empty() && (c.Gender[0] == 'F' || c.Gender[0] == 'f');
    return base + "achievement_character_" + race + (female ? "_female" : "_male") + ".jpg";
}

// One labelled stat tile (achievement points, honorable kills, ...).
static void StatTile(ImVec2 size, const char* caption, const std::string& value, const char* icon)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, p + size, Col(Colors.CardHi, 0.6f), S(10.0f));
    dl->AddRect(p, p + size, Col(Colors.Border, 0.7f), S(10.0f));
    ImFont* small = Theme::Fonts.Small;
    ImFont* heading = Theme::Fonts.Heading;
    Widgets::SpacedText(dl, small, p + S(12.0f, 11.0f), Col(Colors.TextDim), caption, S(1.0f));
    Widgets::Icon(dl, icon, ImVec2(p.x + size.x - S(16.0f), p.y + S(15.0f)), Col(Colors.AccentA));
    const std::string v = value.empty() ? "0" : value;
    dl->AddText(heading, heading->FontSize, ImFloor(p + ImVec2(S(12.0f), size.y - heading->FontSize - S(11.0f))), Col(Colors.Text), v.c_str());
    ImGui::Dummy(size);
}

// "Sanctified Dark Coven Ho..." - item names read better cut at the end.
static std::string EllipsizeEnd(ImFont* font, const std::string& text, float max_width)
{
    auto width = [font](const std::string& s) { return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, s.c_str()).x; };
    if (width(text) <= max_width)
        return text;
    std::string out = text;
    while (!out.empty())
    {
        out.pop_back();
        while (!out.empty() && ((unsigned char)out.back() & 0xC0) == 0x80) // don't split a UTF-8 character
            out.pop_back();
        if (!out.empty() && ((unsigned char)out.back() & 0x80))
            out.pop_back();
        const size_t end = out.find_last_not_of(' ');
        const std::string candidate = (end == std::string::npos ? std::string() : out.substr(0, end + 1)) + "...";
        if (width(candidate) <= max_width)
            return candidate;
    }
    return "...";
}

// Icon square with a quality-colored frame (or an empty-slot placeholder).
static void DrawItemIcon(ImDrawList* dl, IconCache& icons, const GearItem& item, ImVec2 p, float size)
{
    const float r = S(6.0f);
    dl->AddRectFilled(p, p + ImVec2(size, size), Col(Colors.Bg), r);
    if (item.ItemId == 0)
    {
        dl->AddRect(p, p + ImVec2(size, size), Col(Colors.Border, 0.8f), r);
        Widgets::Icon(dl, ICON_PUZZLE, p + ImVec2(size, size) * 0.5f, Col(Colors.TextFaint, 0.5f));
        return;
    }
    const IconCache::Icon* icon = icons.Get(item.IconUrl);
    if (icon && icon->Texture)
        dl->AddImageRounded(icon->Texture, p + ImVec2(1.5f, 1.5f), p + ImVec2(size - 1.5f, size - 1.5f), ImVec2(0, 0), icon->Uv1,
                            Col(ImVec4(1, 1, 1, 1)), r - 1.0f);
    else if (!icon || !icon->Failed) // still loading: soft pulse
        dl->AddRectFilled(p + ImVec2(2, 2), p + ImVec2(size - 2, size - 2),
                          Col(QualityColor(item.Quality), 0.08f + 0.06f * sinf((float)ImGui::GetTime() * 4.0f)), r);
    const ImVec4 qc = QualityColor(item.Quality);
    dl->AddRect(p, p + ImVec2(size, size), Col(qc, 0.95f), r, 0, S(1.6f));
    // a little glow for epic and better
    if (item.Quality >= 4)
        dl->AddRect(p - ImVec2(1.5f, 1.5f), p + ImVec2(size + 1.5f, size + 1.5f), Col(qc, 0.25f), r + 1.5f, 0, S(2.0f));
}

static void ItemTooltip(const GearItem& item, int slot)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, S(12.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, S(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, S(6.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.03f, 0.04f, 0.08f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, Theme::WithAlpha(item.ItemId ? QualityColor(item.Quality) : Colors.Border, 0.7f));
    ImGui::BeginTooltip();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (item.ItemId == 0)
    {
        ImGui::TextDisabled("%s - empty", GearSlotName(slot));
        ImGui::EndTooltip();
        return;
    }
    ImGui::PushFont(Theme::Fonts.Bold);
    ImGui::PushStyleColor(ImGuiCol_Text, QualityColor(item.Quality));
    ImGui::TextUnformatted(item.Name.empty() ? ("Item #" + std::to_string(item.ItemId)).c_str() : item.Name.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::TextDisabled("%s %s", QualityName(item.Quality), GearSlotName(slot));
    if (item.Enchanted)
        ImGui::TextColored(QualityColor(2), "Enchanted");
    if (item.Gems > 0)
        ImGui::TextColored(Colors.TextDim, "%d gem%s socketed", item.Gems, item.Gems == 1 ? "" : "s");
    ImGui::TextDisabled("Item ID %u", item.ItemId);
    ImGui::EndTooltip();
}

// One side slot: icon plus name and details (mirrored on the right column, like the armory).
void App::DrawGearSlot(const GearItem& item, int slot, ImVec2 pos, float width, bool icon_right)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float icon = S(36.0f);
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(slot);
    ImGui::InvisibleButton("##slot", ImVec2(width, icon));
    const bool hovered = ImGui::IsItemHovered();
    const float hov = Widgets::Smooth("##hover", hovered ? 1.0f : 0.0f, 16.0f);
    ImGui::PopID();

    if (hov > 0.01f)
        dl->AddRectFilled(pos - S(4.0f, 3.0f), pos + ImVec2(width, icon) + S(4.0f, 3.0f), Col(Colors.CardHi, 0.75f * hov), S(8.0f));
    const ImVec2 ip = icon_right ? ImVec2(pos.x + width - icon, pos.y) : pos;
    DrawItemIcon(dl, m_Icons, item, ip, icon);

    ImFont* bold = Theme::Fonts.Bold;
    ImFont* small = Theme::Fonts.Small;
    const float text_w = width - icon - S(10.0f);
    std::string name = item.ItemId == 0 ? std::string("Empty") : item.Name.empty() ? "Item #" + std::to_string(item.ItemId) : item.Name;
    std::string sub = GearSlotName(slot);
    if (item.Enchanted)
        sub += "  \xc2\xb7  Enchanted";
    if (item.Gems > 0)
        sub += "  \xc2\xb7  " + std::to_string(item.Gems) + (item.Gems == 1 ? " gem" : " gems");
    name = EllipsizeEnd(bold, name, text_w);
    sub = Widgets::Ellipsize(small, sub.c_str(), text_w);

    const float block = bold->FontSize + S(2.0f) + small->FontSize;
    const float y = pos.y + (icon - block) * 0.5f;
    const ImU32 name_col = Col(item.ItemId ? QualityColor(item.Quality) : Colors.TextFaint);
    const float name_w = bold->CalcTextSizeA(bold->FontSize, FLT_MAX, 0.0f, name.c_str()).x;
    const float sub_w = small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, sub.c_str()).x;
    const float x_name = icon_right ? ip.x - S(10.0f) - name_w : ip.x + icon + S(10.0f);
    const float x_sub = icon_right ? ip.x - S(10.0f) - sub_w : ip.x + icon + S(10.0f);
    dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(x_name, y)), name_col, name.c_str());
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(x_sub, y + bold->FontSize + S(2.0f))), Col(Colors.TextFaint), sub.c_str());

    if (hovered)
        ItemTooltip(item, slot);
}

// The center of the paper-doll: race portrait framed in class colors, spec, and the three weapons.
void App::DrawToon(const Character& c, ImVec2 pos, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec4 cc = ClassColor(c.Class);
    const ImVec2 end = pos + size;
    const float rounding = S(14.0f);

    dl->AddRectFilled(pos, end, Col(Colors.Bg, 0.6f), rounding);
    dl->PushClipRect(pos, end, true);
    const ImVec2 center(pos.x + size.x * 0.5f, pos.y + S(78.0f));
    Widgets::RadialGlow(dl, center, size.x * 0.75f, Col(cc, 0.22f), Col(cc, 0.0f));
    if (Theme::BackgroundEffects)
        Widgets::Snow("##toon_snow", dl, pos, end, 0.45f, 18);
    dl->PopClipRect();
    dl->AddRect(pos, end, Col(cc, 0.28f), rounding);

    // Portrait in a ring, with a slowly turning class-colored arc.
    const float r = S(48.0f);
    const float t = m_Time * 0.8f;
    dl->AddCircleFilled(center, r + S(6.0f), Col(Colors.Bg), 48);
    dl->AddCircle(center, r + S(6.0f), Col(cc, 0.35f), 48, S(1.5f));
    dl->PathArcTo(center, r + S(6.0f), t, t + 1.4f, 24);
    dl->PathStroke(Col(cc, 0.95f), 0, S(2.5f));
    dl->PathArcTo(center, r + S(6.0f), t + 3.14159f, t + 3.14159f + 0.6f, 12);
    dl->PathStroke(Col(cc, 0.6f), 0, S(2.0f));

    const IconCache::Icon* portrait = m_Icons.Get(PortraitUrl(c));
    if (portrait && portrait->Texture)
    {
        dl->AddImageRounded(portrait->Texture, center - ImVec2(r, r), center + ImVec2(r, r), ImVec2(0, 0), portrait->Uv1,
                            Col(ImVec4(1, 1, 1, 1)), r);
    }
    else
    {
        // No portrait (yet): the race's initial in class colors.
        dl->AddCircleFilled(center, r, Col(cc, 0.16f), 48);
        ImFont* display = Theme::Fonts.Display;
        const std::string initial = c.Race.empty() ? "?" : c.Race.substr(0, 1);
        const ImVec2 ts = display->CalcTextSizeA(display->FontSize, FLT_MAX, 0.0f, initial.c_str());
        dl->AddText(display, display->FontSize, ImFloor(center - ts * 0.5f), Col(cc), initial.c_str());
    }

    // Class, level/race, spec
    ImFont* heading = Theme::Fonts.Heading;
    ImFont* small = Theme::Fonts.Small;
    auto centered = [&](ImFont* f, float y, ImU32 col, const std::string& text)
    {
        const std::string fit = Widgets::Ellipsize(f, text.c_str(), size.x - S(16.0f));
        const float w = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0.0f, fit.c_str()).x;
        dl->AddText(f, f->FontSize, ImFloor(ImVec2(pos.x + (size.x - w) * 0.5f, y)), col, fit.c_str());
    };
    float y = center.y + r + S(16.0f);
    centered(heading, y, Col(cc), c.Class.empty() ? std::string("Unknown") : c.Class);
    y += heading->FontSize + S(2.0f);
    centered(small, y, Col(Colors.TextDim), "Level " + c.Level + " " + c.Race);
    y += small->FontSize + S(4.0f);
    if (!c.Talents.empty())
    {
        // Either one entry per tree ("Affliction" / "57") or per spec with "57 / 3 / 11".
        std::string spec;
        if (c.Talents[0].second.find('/') != std::string::npos)
            spec = c.Talents[0].first + "  " + c.Talents[0].second;
        else
        {
            size_t best = 0;
            std::string points;
            for (size_t i = 0; i < c.Talents.size(); i++)
            {
                if (atoi(c.Talents[i].second.c_str()) > atoi(c.Talents[best].second.c_str()))
                    best = i;
                points += (i ? " / " : "") + (c.Talents[i].second.empty() ? std::string("0") : c.Talents[i].second);
            }
            spec = c.Talents[best].first + "  " + points;
        }
        centered(small, y, Col(Colors.AccentA), spec);
    }

    // Weapons along the bottom (main hand, off hand, ranged)
    const float icon = S(36.0f), gap = S(10.0f);
    const float row_w = icon * 3.0f + gap * 2.0f;
    const ImVec2 wp(pos.x + (size.x - row_w) * 0.5f, end.y - icon - S(14.0f));
    for (int i = 0; i < 3; i++)
    {
        const int slot = 16 + i;
        const ImVec2 p(wp.x + i * (icon + gap), wp.y);
        ImGui::SetCursorScreenPos(p);
        ImGui::PushID(slot);
        ImGui::InvisibleButton("##weapon", ImVec2(icon, icon));
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        DrawItemIcon(dl, m_Icons, c.Gear[slot], p, icon);
        if (hovered)
        {
            dl->AddRect(p - ImVec2(2, 2), p + ImVec2(icon + 2, icon + 2), Col(Colors.Text, 0.6f), S(7.0f));
            ItemTooltip(c.Gear[slot], slot);
        }
    }
}

void App::DrawGear(const Character& c, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();

    if (c.Gear.size() != (size_t)GearSlotCount)
    {
        // Profile page unavailable: fall back to the summary's item list (no slots / icons).
        ImFont* body = Theme::Fonts.Body;
        ImFont* small = Theme::Fonts.Small;
        dl->AddText(small, small->FontSize, o, Col(Colors.Warning), c.GearError.empty() ? "Gear layout unavailable." : c.GearError.c_str());
        const float col_w = (size.x - S(16.0f)) * 0.5f;
        for (size_t i = 0; i < c.Equipment.size(); i++)
        {
            const float x = o.x + (i % 2) * (col_w + S(16.0f));
            const float y = o.y + small->FontSize + S(14.0f) + (i / 2) * (body->FontSize + S(10.0f));
            const std::string name = Widgets::Ellipsize(body, c.Equipment[i].second.c_str(), col_w);
            dl->AddText(body, body->FontSize, ImFloor(ImVec2(x, y)), Col(Colors.Text), name.c_str());
        }
        ImGui::Dummy(size);
        return;
    }

    const float icon = S(36.0f);
    const float row = (size.y - icon) / 7.0f; // 8 rows spread over the height
    const float row_step = ImClamp(row, icon + S(4.0f), icon + S(14.0f));
    const float center_w = ImClamp(size.x * 0.26f, S(168.0f), S(220.0f));
    const float col_gap = S(14.0f);
    const float col_w = (size.x - center_w - col_gap * 2.0f) * 0.5f;
    const float rows_h = icon + row_step * 7.0f;

    for (int i = 0; i < 8; i++)
        DrawGearSlot(c.Gear[i], i, ImVec2(o.x, o.y + i * row_step), col_w, false);
    for (int i = 0; i < 8; i++)
        DrawGearSlot(c.Gear[8 + i], 8 + i, ImVec2(o.x + col_w + center_w + col_gap * 2.0f, o.y + i * row_step), col_w, true);
    DrawToon(c, ImVec2(o.x + col_w + col_gap, o.y), ImVec2(center_w, rows_h));

    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy(ImVec2(size.x, rows_h));
}

void App::DrawCharacterStats(const Character& c, float width)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* body = Theme::Fonts.Body;
    ImFont* small = Theme::Fonts.Small;
    const float tile_gap = S(12.0f);
    const ImVec2 tile((width - tile_gap * 2.0f) / 3.0f, S(66.0f));
    StatTile(tile, "ACHIEVEMENTS", c.AchievementPoints, ICON_SHIELD);
    ImGui::SameLine(0.0f, tile_gap);
    StatTile(tile, "HONORABLE KILLS", c.HonorableKills, ICON_USERS);
    ImGui::SameLine(0.0f, tile_gap);
    StatTile(tile, "EQUIPPED ITEMS", std::to_string(c.EquipmentCount), ICON_PUZZLE);

    ImGui::Dummy(ImVec2(0.0f, S(4.0f)));
    const ImVec2 colp = ImGui::GetCursorScreenPos();
    const float half = (width - tile_gap) * 0.5f;
    auto list = [&](ImVec2 at, float cw, const char* title, const std::vector<std::pair<std::string, std::string>>& items, const char* empty)
    {
        Widgets::SpacedText(dl, small, at, Col(Colors.TextFaint), title, S(1.2f));
        float y = at.y + small->FontSize + S(10.0f);
        if (items.empty())
            dl->AddText(body, body->FontSize, ImFloor(ImVec2(at.x, y)), Col(Colors.TextFaint), empty);
        for (const auto& it : items)
        {
            dl->AddText(body, body->FontSize, ImFloor(ImVec2(at.x, y)), Col(Colors.Text), it.first.c_str());
            if (!it.second.empty())
            {
                const float vw = body->CalcTextSizeA(body->FontSize, FLT_MAX, 0.0f, it.second.c_str()).x;
                dl->AddText(body, body->FontSize, ImFloor(ImVec2(at.x + cw - vw, y)), Col(Colors.AccentA), it.second.c_str());
            }
            y += body->FontSize + S(8.0f);
        }
    };
    list(colp, half, "PROFESSIONS", c.Professions, "None");
    list(ImVec2(colp.x + half + tile_gap, colp.y), half, "SPECS", c.Talents, "Unknown");
}

// Name, level/race/class, faction and guild, plus the Gear / Stats switch.
void App::DrawCharacterHeader(const Character& c, float width)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec4 class_col = ClassColor(c.Class);
    const float av = S(52.0f);

    const IconCache::Icon* portrait = m_Icons.Get(PortraitUrl(c));
    dl->AddRectFilled(origin, origin + ImVec2(av, av), Col(class_col, 0.16f), S(12.0f));
    if (portrait && portrait->Texture)
        dl->AddImageRounded(portrait->Texture, origin + ImVec2(2, 2), origin + ImVec2(av - 2, av - 2), ImVec2(0, 0), portrait->Uv1,
                            Col(ImVec4(1, 1, 1, 1)), S(10.0f));
    else
        Widgets::Icon(dl, ICON_SHIELD, origin + ImVec2(av, av) * 0.5f, Col(class_col), true);
    dl->AddRect(origin, origin + ImVec2(av, av), Col(class_col, 0.6f), S(12.0f), 0, S(1.5f));
    dl->AddCircleFilled(origin + ImVec2(av - S(3.0f), av - S(3.0f)), S(6.0f), Col(Colors.Card), 16);
    Widgets::StatusDot(dl, origin + ImVec2(av - S(3.0f), av - S(3.0f)), S(4.0f), c.Online ? Colors.Success : Colors.TextFaint, c.Online);

    ImFont* display = Theme::Fonts.Display;
    ImFont* body = Theme::Fonts.Body;
    ImFont* small = Theme::Fonts.Small;
    const float tx = origin.x + av + S(14.0f);
    dl->AddText(display, display->FontSize, ImFloor(ImVec2(tx, origin.y - S(4.0f))), Col(class_col), c.Name.c_str());
    char sub[256];
    snprintf(sub, sizeof(sub), "Level %s %s %s  \xc2\xb7  %s", c.Level.c_str(), c.Race.c_str(), c.Class.c_str(), c.Realm.c_str());
    dl->AddText(body, body->FontSize, ImFloor(ImVec2(tx, origin.y + display->FontSize - S(4.0f))), Col(Colors.TextDim), sub);
    const ImVec4 faction_col = c.Faction == "Horde" ? Colors.Danger : c.Faction == "Alliance" ? ImVec4(0.36f, 0.56f, 0.96f, 1.0f) : Colors.TextDim;
    std::string line = c.Faction;
    if (!c.Guild.empty())
        line += (line.empty() ? "" : "   \xc2\xb7   ") + std::string("<") + c.Guild + ">";
    line += std::string(line.empty() ? "" : "   \xc2\xb7   ") + (c.Online ? "Online" : "Offline");
    dl->AddText(small, small->FontSize, ImFloor(ImVec2(tx, origin.y + display->FontSize + body->FontSize - S(1.0f))), Col(faction_col), line.c_str());

    // Gear / Stats segmented switch (top right)
    static const char* tabs[] = { "Gear", "Stats" };
    const float tab_w = S(72.0f), tab_h = S(30.0f), pad = S(3.0f);
    const ImVec2 box(origin.x + width - tab_w * 2.0f - pad * 2.0f, origin.y + S(4.0f));
    dl->AddRectFilled(box, box + ImVec2(tab_w * 2.0f + pad * 2.0f, tab_h + pad * 2.0f), Col(Colors.CardHi), S(10.0f));
    for (int i = 0; i < 2; i++)
    {
        const ImVec2 p0(box.x + pad + i * tab_w, box.y + pad);
        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##tab", ImVec2(tab_w, tab_h)))
            m_ArmoryTab = i;
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool selected = m_ArmoryTab == i;
        if (selected)
            Widgets::GradientRect(dl, p0, p0 + ImVec2(tab_w, tab_h), Col(Colors.AccentA), Col(Colors.AccentB), S(8.0f));
        else if (hovered)
            dl->AddRectFilled(p0, p0 + ImVec2(tab_w, tab_h), Col(Colors.Border, 0.6f), S(8.0f));
        ImFont* bold = Theme::Fonts.Bold;
        const ImVec2 ts = bold->CalcTextSizeA(bold->FontSize, FLT_MAX, 0.0f, tabs[i]);
        const bool light_accent = (0.2126f * Colors.AccentA.x + 0.7152f * Colors.AccentA.y + 0.0722f * Colors.AccentA.z) > 0.55f;
        const ImU32 text_col = selected ? (light_accent ? Col(ImVec4(0.03f, 0.07f, 0.12f, 1.0f)) : Col(ImVec4(1, 1, 1, 1))) : Col(Colors.TextDim);
        dl->AddText(bold, bold->FontSize, ImFloor(p0 + (ImVec2(tab_w, tab_h) - ts) * 0.5f), text_col, tabs[i]);
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, av));
}

void App::DrawArmory()
{
    const float w = ImGui::GetContentRegionAvail().x;
    const float gap = S(14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

    static std::vector<const char*> realm_names;
    realm_names.clear();
    for (int i = 0; i < Game::RealmCount; i++)
        realm_names.push_back(Game::Realms[i].Name);

    // --- Search bar ---
    Widgets::BeginCard("##armory_search", ImVec2(w, S(14.0f) * 2.0f + ImGui::GetFrameHeight()), S(14.0f, 14.0f));
    {
        const float frame = ImGui::GetFrameHeight();
        const float realm_w = S(150.0f), search_w = S(120.0f);
        ImGui::SetNextItemWidth(realm_w);
        ImGui::Combo("##armory_realm", &m_ArmoryRealm, realm_names.data(), (int)realm_names.size());
        ImGui::SameLine(0.0f, S(8.0f));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - search_w - S(8.0f));
        const bool entered = ImGui::InputTextWithHint("##armory_name", "Search a character on the armory", m_ArmoryName, sizeof(m_ArmoryName),
                                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine(0.0f, S(8.0f));
        const bool busy = m_Armory.Loading();
        if ((Widgets::PlayButton(busy ? "..." : "Search", ImVec2(search_w, frame), busy) || entered) && !busy)
            StartArmorySearch(m_ArmoryName, m_ArmoryRealm);
    }
    Widgets::EndCard();

    // --- Result (fills the rest of the page) ---
    const float result_h = ImMax(S(380.0f), ImGui::GetContentRegionAvail().y);
    Widgets::BeginCard("##armory_result", ImVec2(w, result_h), S(18.0f, 16.0f));
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const Armory::State state = m_Armory.GetState();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 inner = ImGui::GetContentRegionAvail();

        if (state == Armory::State::Idle)
        {
            Widgets::Icon(dl, ICON_SHIELD, origin + ImVec2(inner.x * 0.5f, inner.y * 0.30f), Col(Colors.TextFaint), true);
            const char* hint = "Pick a realm, type a character name, and press Search.";
            const ImVec2 ts = ImGui::CalcTextSize(hint);
            dl->AddText(ImFloor(origin + ImVec2((inner.x - ts.x) * 0.5f, inner.y * 0.30f + S(24.0f))), Col(Colors.TextDim), hint);

            // Recent searches as centered chips.
            std::vector<std::string>& history = m_Settings.ArmoryHistory;
            if (!history.empty())
            {
                ImFont* small = Theme::Fonts.Small;
                const char* label = "RECENT SEARCHES";
                const ImVec2 ls = Widgets::SpacedTextSize(small, label, S(1.2f));
                float y = origin.y + inner.y * 0.30f + S(64.0f);
                Widgets::SpacedText(dl, small, ImVec2(origin.x + (inner.x - ls.x) * 0.5f, y), Col(Colors.TextFaint), label, S(1.2f));
                y += small->FontSize + S(10.0f);

                std::vector<std::string> labels;
                std::vector<float> widths;
                for (const std::string& entry : history)
                {
                    const size_t slash = entry.find('/');
                    labels.push_back(slash == std::string::npos ? entry : entry.substr(0, slash) + " \xc2\xb7 " + entry.substr(slash + 1));
                    widths.push_back(small->CalcTextSizeA(small->FontSize, FLT_MAX, 0.0f, labels.back().c_str()).x + S(24.0f));
                }
                // Wrap into rows that fit, each row centered.
                const float max_w = inner.x * 0.85f, chip_gap = S(8.0f), chip_h = S(28.0f);
                int clicked = -1;
                for (size_t first = 0; first < labels.size(); )
                {
                    size_t last = first;
                    float row_w = widths[first];
                    while (last + 1 < labels.size() && row_w + chip_gap + widths[last + 1] <= max_w)
                        row_w += chip_gap + widths[++last];
                    float x = origin.x + (inner.x - row_w) * 0.5f;
                    for (size_t i = first; i <= last; i++)
                    {
                        ImGui::SetCursorScreenPos(ImVec2(x, y));
                        ImGui::PushID((int)i);
                        if (ImGui::InvisibleButton("##chip", ImVec2(widths[i], chip_h)))
                            clicked = (int)i;
                        const bool hov = ImGui::IsItemHovered();
                        ImGui::PopID();
                        const ImVec2 p0(x, y), p1(x + widths[i], y + chip_h);
                        dl->AddRectFilled(p0, p1, Col(hov ? Colors.CardHi : Colors.Bg, hov ? 1.0f : 0.6f), S(14.0f));
                        dl->AddRect(p0, p1, Col(hov ? Colors.AccentA : Colors.Border, hov ? 0.7f : 1.0f), S(14.0f));
                        dl->AddText(small, small->FontSize, ImFloor(ImVec2(x + S(12.0f), y + (chip_h - small->FontSize) * 0.5f)),
                                    Col(hov ? Colors.Text : Colors.TextDim), labels[i].c_str());
                        x += widths[i] + chip_gap;
                    }
                    y += chip_h + chip_gap;
                    first = last + 1;
                }
                if (clicked >= 0)
                {
                    const std::string entry = history[clicked];
                    const size_t slash = entry.find('/');
                    StartArmorySearch(entry.substr(0, slash), slash == std::string::npos ? 0 : Game::FindRealm(entry.substr(slash + 1)));
                }
            }
        }
        else if (state == Armory::State::Loading)
        {
            Widgets::Spinner(dl, origin + ImVec2(inner.x * 0.5f, inner.y * 0.42f), S(26.0f), S(3.0f), m_Time);
            const std::string q = "Looking up " + m_Armory.Query();
            const ImVec2 ts = ImGui::CalcTextSize(q.c_str());
            dl->AddText(ImFloor(origin + ImVec2((inner.x - ts.x) * 0.5f, inner.y * 0.42f + S(40.0f))), Col(Colors.TextDim), q.c_str());
        }
        else if (state == Armory::State::Error)
        {
            const Character c = m_Armory.Result();
            const std::string msg = c.Error.empty() ? "Character not found." : c.Error;
            Widgets::Icon(dl, ICON_WARNING, origin + ImVec2(inner.x * 0.5f, inner.y * 0.38f), Col(Colors.Warning), true);
            const ImVec2 ts = ImGui::CalcTextSize(msg.c_str());
            dl->AddText(ImFloor(origin + ImVec2((inner.x - ts.x) * 0.5f, inner.y * 0.38f + S(22.0f))), Col(Colors.TextDim), msg.c_str());
            const char* hint2 = "Check the spelling and realm, then try again.";
            const ImVec2 ts2 = ImGui::CalcTextSize(hint2);
            dl->AddText(ImFloor(origin + ImVec2((inner.x - ts2.x) * 0.5f, inner.y * 0.38f + S(42.0f))), Col(Colors.TextFaint), hint2);
        }
        else // Done
        {
            const Character c = m_Armory.Result();
            DrawCharacterHeader(c, inner.x);
            ImGui::Dummy(ImVec2(0.0f, S(2.0f)));
            const ImVec2 body = ImGui::GetContentRegionAvail();
            if (m_ArmoryTab == 0)
                DrawGear(c, body);
            else
                DrawCharacterStats(c, inner.x);
        }
    }
    Widgets::EndCard();
    ImGui::PopStyleVar();
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
