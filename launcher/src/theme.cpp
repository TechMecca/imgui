#include "theme.h"

#include <math.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Theme
{
    FontSet Fonts;
    Palette Colors;
    bool    Animations        = true;
    bool    BackgroundEffects = true;

    const AccentPreset AccentPresets[] =
    {
        { "Warmane", ImVec4(0.91f, 0.74f, 0.40f, 1.00f), ImVec4(0.74f, 0.52f, 0.24f, 1.00f) },
        { "Frost",   ImVec4(0.27f, 0.62f, 1.00f, 1.00f), ImVec4(0.55f, 0.93f, 1.00f, 1.00f) },
        { "Arcane",  ImVec4(0.55f, 0.40f, 1.00f, 1.00f), ImVec4(0.31f, 0.78f, 1.00f, 1.00f) },
        { "Ember",   ImVec4(1.00f, 0.45f, 0.22f, 1.00f), ImVec4(1.00f, 0.77f, 0.30f, 1.00f) },
        { "Scourge", ImVec4(0.20f, 0.85f, 0.55f, 1.00f), ImVec4(0.70f, 1.00f, 0.38f, 1.00f) },
        { "Blood",   ImVec4(0.93f, 0.22f, 0.36f, 1.00f), ImVec4(1.00f, 0.52f, 0.38f, 1.00f) },
    };
    const int AccentPresetCount = IM_ARRAYSIZE(AccentPresets);

    static float g_DpiScale = 1.0f;
    static int   g_Accent   = 0;

    float DpiScale() { return g_DpiScale; }
    int   Accent()   { return g_Accent; }

    ImU32 Col(const ImVec4& c, float alpha)
    {
        return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * alpha));
    }

    ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
    {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
    }

    ImVec4 WithAlpha(const ImVec4& c, float alpha)
    {
        return ImVec4(c.x, c.y, c.z, alpha);
    }

    static ImVec4 Hex(unsigned int rgb, float alpha = 1.0f)
    {
        return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
    }

    //-------------------------------------------------------------------------
    // Fonts
    //-------------------------------------------------------------------------

    // Returns the first font from 'files' found in %WINDIR%\Fonts, or null.
    static ImFont* AddSystemFont(const char* const* files, int count, float size, const ImWchar* ranges, bool merge = false)
    {
        char windows_dir[MAX_PATH] = {};
        if (::GetWindowsDirectoryA(windows_dir, MAX_PATH) == 0)
            return nullptr;

        for (int i = 0; i < count; i++)
        {
            char path[MAX_PATH * 2];
            snprintf(path, sizeof(path), "%s\\Fonts\\%s", windows_dir, files[i]);
            if (::GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
                continue;

            ImFontConfig cfg;
            cfg.MergeMode = merge;
            cfg.OversampleH = 3;
            cfg.OversampleV = 2;
            if (ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, &cfg, ranges))
                return font;
        }
        return nullptr;
    }

    static void LoadFonts()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        Fonts = FontSet();

        // Latin + Latin-1 + general punctuation (bullets, dashes, ellipsis)
        static const ImWchar text_ranges[] = { 0x0020, 0x00FF, 0x2013, 0x2026, 0 };
        // Only the icons we use, so the atlas stays small.
        static const ImWchar icon_ranges[] =
        {
            0xE707, 0xE707, 0xE713, 0xE713, 0xE716, 0xE716, 0xE71B, 0xE71B, 0xE721, 0xE721,
            0xE72C, 0xE72C, 0xE73E, 0xE73E, 0xE740, 0xE740, 0xE74D, 0xE74D, 0xE768, 0xE768,
            0xE774, 0xE774, 0xE783, 0xE783, 0xE790, 0xE790, 0xE7BA, 0xE7BA, 0xE7FC, 0xE7FC,
            0xE80F, 0xE80F, 0xE838, 0xE838, 0xE8B7, 0xE8B7, 0xE943, 0xE943, 0xE946, 0xE946,
            0xEA18, 0xEA18, 0xEA86, 0xEA86, 0,
        };

        static const char* const regular[]  = { "segoeui.ttf", "tahoma.ttf", "arial.ttf" };
        static const char* const semibold[] = { "seguisb.ttf", "segoeuib.ttf", "tahomabd.ttf", "arialbd.ttf" };
        static const char* const bold[]     = { "segoeuib.ttf", "tahomabd.ttf", "arialbd.ttf" };
        static const char* const icons[]    = { "SegoeIcons.ttf", "segmdl2.ttf" };

        const float s = g_DpiScale;
        auto px = [s](float size) { return floorf(size * s + 0.5f); };

        Fonts.Body    = AddSystemFont(regular,  IM_ARRAYSIZE(regular),  px(15.0f), text_ranges);
        Fonts.Bold    = AddSystemFont(semibold, IM_ARRAYSIZE(semibold), px(15.0f), text_ranges);
        Fonts.Small   = AddSystemFont(regular,  IM_ARRAYSIZE(regular),  px(13.0f), text_ranges);
        Fonts.Heading = AddSystemFont(semibold, IM_ARRAYSIZE(semibold), px(22.0f), text_ranges);
        Fonts.Display = AddSystemFont(bold,     IM_ARRAYSIZE(bold),     px(30.0f), text_ranges);
        Fonts.Icons      = AddSystemFont(icons, IM_ARRAYSIZE(icons), px(15.0f), icon_ranges);
        Fonts.IconsLarge = AddSystemFont(icons, IM_ARRAYSIZE(icons), px(20.0f), icon_ranges);

        // No system fonts at all (unusual): fall back to the built-in font.
        if (!Fonts.Body)
        {
            ImFontConfig cfg;
            cfg.SizePixels = px(13.0f);
            Fonts.Body = io.Fonts->AddFontDefault(&cfg);
        }
        if (!Fonts.Bold)    Fonts.Bold    = Fonts.Body;
        if (!Fonts.Small)   Fonts.Small   = Fonts.Body;
        if (!Fonts.Heading) Fonts.Heading = Fonts.Bold;
        if (!Fonts.Display) Fonts.Display = Fonts.Heading;
        if (!Fonts.IconsLarge) Fonts.IconsLarge = Fonts.Icons;

        io.FontDefault = Fonts.Body;
        io.Fonts->Build();
    }

    //-------------------------------------------------------------------------
    // Style
    //-------------------------------------------------------------------------

    static void ApplyColors()
    {
        const AccentPreset& accent = AccentPresets[g_Accent];
        Colors.Bg        = Hex(0x0B0A07);
        Colors.Surface   = Hex(0x13110B);
        Colors.Card      = Hex(0x1A1710);
        Colors.CardHi    = Hex(0x231E15);
        Colors.Border    = Hex(0x352E1E);
        Colors.Text      = Hex(0xEDE6D6);
        Colors.TextDim   = Hex(0xA99F88);
        Colors.TextFaint = Hex(0x6E6553);
        Colors.AccentA   = accent.A;
        Colors.AccentB   = accent.B;
        Colors.Success   = Hex(0x2EE59D);
        Colors.Warning   = Hex(0xFFB547);
        Colors.Danger    = Hex(0xFF4D6A);

        const ImVec4 a = Colors.AccentA;
        ImVec4* c = ImGui::GetStyle().Colors;
        c[ImGuiCol_Text]                  = Colors.Text;
        c[ImGuiCol_TextDisabled]          = Colors.TextFaint;
        c[ImGuiCol_WindowBg]              = Colors.Bg;
        c[ImGuiCol_ChildBg]               = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg]               = Colors.CardHi;
        c[ImGuiCol_Border]                = Colors.Border;
        c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg]               = Colors.CardHi;
        c[ImGuiCol_FrameBgHovered]        = Mix(Colors.CardHi, a, 0.12f);
        c[ImGuiCol_FrameBgActive]         = Mix(Colors.CardHi, a, 0.22f);
        c[ImGuiCol_TitleBg]               = Colors.Bg;
        c[ImGuiCol_TitleBgActive]         = Colors.Bg;
        c[ImGuiCol_TitleBgCollapsed]      = Colors.Bg;
        c[ImGuiCol_MenuBarBg]             = Colors.Surface;
        c[ImGuiCol_ScrollbarBg]           = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab]         = Colors.Border;
        c[ImGuiCol_ScrollbarGrabHovered]  = Mix(Colors.Border, a, 0.4f);
        c[ImGuiCol_ScrollbarGrabActive]   = a;
        c[ImGuiCol_CheckMark]             = a;
        c[ImGuiCol_SliderGrab]            = a;
        c[ImGuiCol_SliderGrabActive]      = Colors.AccentB;
        c[ImGuiCol_Button]                = Colors.CardHi;
        c[ImGuiCol_ButtonHovered]         = Mix(Colors.CardHi, a, 0.18f);
        c[ImGuiCol_ButtonActive]          = Mix(Colors.CardHi, a, 0.30f);
        c[ImGuiCol_Header]                = WithAlpha(a, 0.22f);
        c[ImGuiCol_HeaderHovered]         = WithAlpha(a, 0.32f);
        c[ImGuiCol_HeaderActive]          = WithAlpha(a, 0.42f);
        c[ImGuiCol_Separator]             = Colors.Border;
        c[ImGuiCol_SeparatorHovered]      = a;
        c[ImGuiCol_SeparatorActive]       = a;
        c[ImGuiCol_ResizeGrip]            = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripHovered]     = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ResizeGripActive]      = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_Tab]                   = Colors.Card;
        c[ImGuiCol_TabHovered]            = Mix(Colors.Card, a, 0.3f);
        c[ImGuiCol_TabActive]             = Mix(Colors.Card, a, 0.2f);
        c[ImGuiCol_TabUnfocused]          = Colors.Card;
        c[ImGuiCol_TabUnfocusedActive]    = Colors.CardHi;
        c[ImGuiCol_PlotLines]             = a;
        c[ImGuiCol_PlotLinesHovered]      = Colors.AccentB;
        c[ImGuiCol_PlotHistogram]         = a;
        c[ImGuiCol_PlotHistogramHovered]  = Colors.AccentB;
        c[ImGuiCol_TableHeaderBg]         = Colors.CardHi;
        c[ImGuiCol_TableBorderStrong]     = Colors.Border;
        c[ImGuiCol_TableBorderLight]      = Mix(Colors.Border, Colors.Card, 0.5f);
        c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.02f);
        c[ImGuiCol_TextSelectedBg]        = WithAlpha(a, 0.35f);
        c[ImGuiCol_DragDropTarget]        = Colors.AccentB;
        c[ImGuiCol_NavHighlight]          = a;
        c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
        c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.5f);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.55f);
    }

    static void ApplyStyle()
    {
        ImGuiStyle& st = ImGui::GetStyle();
        st = ImGuiStyle();
        st.WindowPadding     = ImVec2(0, 0);
        st.WindowRounding    = 0.0f;
        st.WindowBorderSize  = 0.0f;
        st.ChildRounding     = 10.0f;
        st.ChildBorderSize   = 1.0f;
        st.PopupRounding     = 8.0f;
        st.PopupBorderSize   = 1.0f;
        st.FramePadding      = ImVec2(10, 7);
        st.FrameRounding     = 6.0f;
        st.FrameBorderSize   = 0.0f;
        st.ItemSpacing       = ImVec2(10, 8);
        st.ItemInnerSpacing  = ImVec2(8, 6);
        st.IndentSpacing     = 18.0f;
        st.ScrollbarSize     = 8.0f;
        st.ScrollbarRounding = 8.0f;
        st.GrabMinSize       = 12.0f;
        st.GrabRounding      = 6.0f;
        st.TabRounding       = 6.0f;
        st.SelectableTextAlign = ImVec2(0.0f, 0.5f);
        st.ScaleAllSizes(g_DpiScale);
        ApplyColors();
    }

    void Init(float dpi_scale)
    {
        g_DpiScale = dpi_scale;
        LoadFonts();
        ApplyStyle();
    }

    void SetDpiScale(float dpi_scale)
    {
        Init(dpi_scale);
    }

    void SetAccent(int preset_index)
    {
        if (preset_index < 0 || preset_index >= AccentPresetCount)
            return;
        g_Accent = preset_index;
        ApplyColors();
    }
}
