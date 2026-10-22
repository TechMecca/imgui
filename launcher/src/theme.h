#pragma once

#include "imgui.h"

// Icon glyphs (UTF-8) from "Segoe Fluent Icons" (Windows 11) / "Segoe MDL2 Assets" (Windows 10).
// Draw them with Widgets::Icon(), which skips them when neither font is installed.
#define ICON_HOME         "\xee\xa0\x8f" // U+E80F
#define ICON_PLAY         "\xee\x9d\xa8" // U+E768
#define ICON_GAME         "\xee\x9f\xbc" // U+E7FC
#define ICON_PUZZLE       "\xee\xaa\x86" // U+EA86
#define ICON_SETTINGS     "\xee\x9c\x93" // U+E713
#define ICON_INFO         "\xee\xa5\x86" // U+E946
#define ICON_SHIELD       "\xee\xa8\x98" // U+EA18
#define ICON_GLOBE        "\xee\x9d\xb4" // U+E774
#define ICON_CHECK        "\xee\x9c\xbe" // U+E73E
#define ICON_ERROR        "\xee\x9e\x83" // U+E783
#define ICON_WARNING      "\xee\x9e\xba" // U+E7BA
#define ICON_FOLDER       "\xee\xa2\xb7" // U+E8B7
#define ICON_FOLDER_OPEN  "\xee\xa0\xb8" // U+E838
#define ICON_REFRESH      "\xee\x9c\xac" // U+E72C
#define ICON_SEARCH       "\xee\x9c\xa1" // U+E721
#define ICON_DELETE       "\xee\x9d\x8d" // U+E74D
#define ICON_WINDOW       "\xee\x9d\x80" // U+E740
#define ICON_LINK         "\xee\x9c\x9b" // U+E71B
#define ICON_MAP          "\xee\x9c\x87" // U+E707
#define ICON_CODE         "\xee\xa5\x83" // U+E943
#define ICON_PALETTE      "\xee\x9e\x90" // U+E790
#define ICON_USERS        "\xee\x9c\x96" // U+E716

namespace Theme
{
    // Branding - change these to rename the launcher.
    constexpr const char* AppName    = "NORTHREND";
    constexpr const char* AppTagline = "LAUNCHER FOR WARMANE";
    constexpr const char* AppVersion = "1.0.0";

    // Window sizes in 96-DPI units (scaled automatically on high-DPI monitors).
    constexpr float ConnectWindowWidth  = 440.0f;
    constexpr float ConnectWindowHeight = 340.0f;
    constexpr float MainWindowWidth     = 1000.0f;
    constexpr float MainWindowHeight    = 640.0f;

    struct FontSet
    {
        ImFont* Body       = nullptr;
        ImFont* Bold       = nullptr;
        ImFont* Small      = nullptr;
        ImFont* Heading    = nullptr;
        ImFont* Display    = nullptr; // logo wordmark, banner titles
        ImFont* Icons      = nullptr; // null when the icon font is missing
        ImFont* IconsLarge = nullptr;
    };
    extern FontSet Fonts;

    struct Palette
    {
        ImVec4 Bg;        // window background
        ImVec4 Surface;   // main content panel
        ImVec4 Card;
        ImVec4 CardHi;    // hovered / raised elements
        ImVec4 Border;
        ImVec4 Text;
        ImVec4 TextDim;
        ImVec4 TextFaint;
        ImVec4 AccentA;   // accent gradient start
        ImVec4 AccentB;   // accent gradient end
        ImVec4 Success;
        ImVec4 Warning;
        ImVec4 Danger;
    };
    extern Palette Colors;

    struct AccentPreset { const char* Name; ImVec4 A, B; };
    extern const AccentPreset AccentPresets[];
    extern const int AccentPresetCount;

    // User toggles (Settings page)
    extern bool Animations;
    extern bool BackgroundEffects;

    void  Init(float dpi_scale);        // fonts + style; call once after ImGui::CreateContext()
    void  SetDpiScale(float dpi_scale); // rebuilds fonts + style; call between frames with the renderer's font texture released
    void  SetAccent(int preset_index);
    int   Accent();
    float DpiScale();

    inline float  S(float v)          { return v * DpiScale(); }
    inline ImVec2 S(float x, float y) { return ImVec2(x * DpiScale(), y * DpiScale()); }

    inline const char* BuildArch() { return sizeof(void*) == 8 ? "x64" : "x86"; }
#ifdef NDEBUG
    inline const char* BuildConfig() { return "Release"; }
#else
    inline const char* BuildConfig() { return "Debug"; }
#endif

    ImU32  Col(const ImVec4& c, float alpha = 1.0f); // includes the current style alpha, so fades apply
    ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t);
    ImVec4 WithAlpha(const ImVec4& c, float alpha);
}
