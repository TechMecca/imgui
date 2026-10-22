#pragma once

#include "imgui.h"

#include <string>

// Custom-drawn widgets and effects used by both screens.
// All sizes are in pixels already scaled for DPI (use Theme::S()).
namespace Widgets
{
    // --- Animation --------------------------------------------------------
    float Smooth(ImGuiID id, float target, float speed = 12.0f); // eases a per-id value toward 'target'
    float Smooth(const char* str_id, float target, float speed = 12.0f);
    float EaseOutCubic(float t);

    // --- Draw-list helpers ------------------------------------------------
    void   ShadeGradient(ImDrawList* dl, int vtx_start, ImVec2 p0, ImVec2 p1, ImU32 col0, ImU32 col1);
    void   GradientRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 left, ImU32 right, float rounding = 0.0f, ImDrawFlags flags = 0);
    ImVec2 SpacedTextSize(ImFont* font, const char* text, float spacing);
    void   SpacedText(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text, float spacing);
    void   GradientText(ImDrawList* dl, ImFont* font, ImVec2 pos, const char* text, ImU32 left, ImU32 right, float spacing = 0.0f);
    void   RadialGlow(ImDrawList* dl, ImVec2 center, float radius, ImU32 inner, ImU32 outer);
    void   Logo(ImDrawList* dl, ImVec2 center, float radius, float rotation, float alpha = 1.0f); // snowflake crest
    void   Spinner(ImDrawList* dl, ImVec2 center, float radius, float thickness, float time, float alpha = 1.0f);
    void   Ring(ImDrawList* dl, ImVec2 center, float radius, float thickness, float fraction, ImU32 col);
    void   CheckMark(ImDrawList* dl, ImVec2 center, float size, float thickness, float t, ImU32 col);
    void   CrossMark(ImDrawList* dl, ImVec2 center, float size, float thickness, float t, ImU32 col);
    void   Icon(ImDrawList* dl, const char* glyph, ImVec2 center, ImU32 col, bool large = false);
    void   StatusDot(ImDrawList* dl, ImVec2 center, float radius, const ImVec4& col, bool pulse);
    void   Snow(const char* str_id, ImDrawList* dl, ImVec2 min, ImVec2 max, float alpha, int flakes = 70);
    void   Aurora(ImDrawList* dl, ImVec2 min, ImVec2 max, float time, float alpha);
    void   Mountains(ImDrawList* dl, ImVec2 min, ImVec2 max, unsigned int seed, float peak_height, ImU32 top, ImU32 bottom, ImU32 ridge);
    void   MaskCorners(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, ImU32 outside); // paints outside rounded corners
    std::string Ellipsize(ImFont* font, const char* text, float max_width); // "C:\Games\...\World of Warcraft"

    // --- Widgets ----------------------------------------------------------
    bool WindowButton(const char* id, bool is_close, ImVec2 size);     // title-bar minimize / close
    bool NavItem(const char* icon, const char* label, bool selected, float width);
    bool Toggle(const char* id, bool* value);
    bool PrimaryButton(const char* label, ImVec2 size);                // accent gradient
    bool PlayButton(const char* label, ImVec2 size, bool busy);        // the big launch button
    bool IconButton(const char* id, const char* icon, ImVec2 size, const char* tooltip = nullptr);
    bool SecondaryButton(const char* label, ImVec2 size, const ImVec4* hover_tint = nullptr);
    bool Swatch(const char* id, const ImVec4& a, const ImVec4& b, bool selected, float radius);
    void ProgressBar(float fraction, ImVec2 size);
    void Pill(const char* label, const ImVec4& dot_col, bool pulse);  // small rounded status chip
    void SectionLabel(const char* text);                               // small, spaced, faint caps
    void TextColored(ImFont* font, const ImVec4& col, const char* text);
    void AreaChart(const char* id, const float* values, int count, ImVec2 size, float v_min, float v_max, bool grid = true);

    bool BeginCard(const char* id, ImVec2 size, ImVec2 padding = ImVec2(-1, -1));
    void EndCard();
    void CardHeader(const char* icon, const char* title, const char* subtitle = nullptr);
}
