#define IMGUI_DEFINE_MATH_OPERATORS
#include "widgets.h"

#include "imgui_internal.h"
#include "theme.h"

#include <float.h>
#include <math.h>
#include <unordered_map>

using Theme::Col;
using Theme::Colors;
using Theme::Mix;
using Theme::S;

namespace Widgets
{
    static const float kPi = 3.14159265358979f;

    // A distinct id for each animated property of a widget.
    static ImGuiID SubId(ImGuiID id, int channel)
    {
        return ImHashData(&channel, sizeof(channel), id);
    }

    //-------------------------------------------------------------------------
    // Animation
    //-------------------------------------------------------------------------

    float Smooth(ImGuiID id, float target, float speed)
    {
        static std::unordered_map<ImGuiID, float> values;
        auto it = values.find(id);
        if (it == values.end() || !Theme::Animations)
        {
            values[id] = target;
            return target;
        }
        float& v = it->second;
        const float dt = ImMin(ImGui::GetIO().DeltaTime, 0.05f);
        v += (target - v) * (1.0f - expf(-speed * dt));
        if (fabsf(target - v) < 0.0005f)
            v = target;
        return v;
    }

    float Smooth(const char* str_id, float target, float speed)
    {
        return Smooth(ImGui::GetID(str_id), target, speed);
    }

    float EaseOutCubic(float t)
    {
        t = ImClamp(t, 0.0f, 1.0f);
        const float u = 1.0f - t;
        return 1.0f - u * u * u;
    }

    //-------------------------------------------------------------------------
    // Draw-list helpers
    //-------------------------------------------------------------------------

    // Recolors vertices [vtx_start, end) with a linear gradient from p0 to p1.
    // Unlike ImGui::ShadeVertsLinearColorGradientKeepAlpha this also interpolates alpha.
    void ShadeGradient(ImDrawList* dl, int vtx_start, ImVec2 p0, ImVec2 p1, ImU32 col0, ImU32 col1)
    {
        const ImVec2 extent = p1 - p0;
        const float inv_len2 = 1.0f / ImMax(ImLengthSqr(extent), 1e-6f);
        const ImVec4 c0 = ImGui::ColorConvertU32ToFloat4(col0);
        const ImVec4 c1 = ImGui::ColorConvertU32ToFloat4(col1);
        ImDrawVert* const end = dl->VtxBuffer.Data + dl->VtxBuffer.Size;
        for (ImDrawVert* v = dl->VtxBuffer.Data + vtx_start; v < end; ++v)
        {
            const float t = ImClamp(ImDot(v->pos - p0, extent) * inv_len2, 0.0f, 1.0f);
            ImVec4 c = Mix(c0, c1, t);
            c.w *= ((v->col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
            v->col = ImGui::ColorConvertFloat4ToU32(c);
        }
    }

    void GradientRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 left, ImU32 right, float rounding, ImDrawFlags flags)
    {
        const int vtx = dl->VtxBuffer.Size;
        dl->AddRectFilled(min, max, IM_COL32_WHITE, rounding, flags);
        ShadeGradient(dl, vtx, min, ImVec2(max.x, min.y), left, right);
    }

    ImVec2 SpacedTextSize(ImFont* font, const char* text, float spacing)
    {
        float width = 0.0f;
        int chars = 0;
        for (const char* s = text; *s; )
        {
            unsigned int c = 0;
            const int len = ImTextCharFromUtf8(&c, s, NULL);
            if (len <= 0)
                break;
            width += font->GetCharAdvance((ImWchar)c) + spacing;
            s += len;
            chars++;
        }
        if (chars > 0)
            width -= spacing;
        return ImVec2(width, font->FontSize);
    }

    void SpacedText(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const char* text, float spacing)
    {
        pos = ImFloor(pos);
        for (const char* s = text; *s; )
        {
            unsigned int c = 0;
            const int len = ImTextCharFromUtf8(&c, s, NULL);
            if (len <= 0)
                break;
            dl->AddText(font, font->FontSize, ImVec2(floorf(pos.x), pos.y), col, s, s + len);
            pos.x += font->GetCharAdvance((ImWchar)c) + spacing;
            s += len;
        }
    }

    void GradientText(ImDrawList* dl, ImFont* font, ImVec2 pos, const char* text, ImU32 left, ImU32 right, float spacing)
    {
        const ImVec2 size = SpacedTextSize(font, text, spacing);
        const int vtx = dl->VtxBuffer.Size;
        SpacedText(dl, font, pos, IM_COL32_WHITE, text, spacing);
        ShadeGradient(dl, vtx, pos, ImVec2(pos.x + size.x, pos.y), left, right);
    }

    // Soft circular glow: a triangle fan with a mid ring for a smoother falloff.
    void RadialGlow(ImDrawList* dl, ImVec2 center, float radius, ImU32 inner, ImU32 outer)
    {
        const int segments = 48;
        const ImVec4 ci = ImGui::ColorConvertU32ToFloat4(inner);
        const ImVec4 co = ImGui::ColorConvertU32ToFloat4(outer);
        const ImU32 mid = ImGui::ColorConvertFloat4ToU32(Mix(ci, co, 0.65f));
        const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();

        dl->PrimReserve(segments * 9, 1 + segments * 2);
        const unsigned int base = dl->_VtxCurrentIdx;
        dl->PrimWriteVtx(center, uv, inner);
        for (int i = 0; i < segments; i++)
        {
            const float a = (float)i / segments * 2.0f * kPi;
            const ImVec2 dir(cosf(a), sinf(a));
            dl->PrimWriteVtx(center + dir * (radius * 0.4f), uv, mid);
            dl->PrimWriteVtx(center + dir * radius, uv, outer);
        }
        for (int i = 0; i < segments; i++)
        {
            const unsigned int m0 = base + 1 + i * 2, o0 = m0 + 1;
            const unsigned int m1 = base + 1 + ((i + 1) % segments) * 2, o1 = m1 + 1;
            dl->PrimWriteIdx((ImDrawIdx)base); dl->PrimWriteIdx((ImDrawIdx)m0); dl->PrimWriteIdx((ImDrawIdx)m1);
            dl->PrimWriteIdx((ImDrawIdx)m0);   dl->PrimWriteIdx((ImDrawIdx)o0); dl->PrimWriteIdx((ImDrawIdx)o1);
            dl->PrimWriteIdx((ImDrawIdx)m0);   dl->PrimWriteIdx((ImDrawIdx)o1); dl->PrimWriteIdx((ImDrawIdx)m1);
        }
    }

    // Hexagon ring with a four-point star in the middle.
    void Logo(ImDrawList* dl, ImVec2 center, float radius, float rotation, float alpha)
    {
        const ImU32 ca = Col(Colors.AccentA, alpha);
        const ImU32 cb = Col(Colors.AccentB, alpha);
        const ImVec2 diag(radius, radius);

        ImVec2 outer[6], inner[6];
        for (int i = 0; i < 6; i++)
        {
            const float a = rotation + i * kPi / 3.0f - kPi / 2.0f;
            outer[i] = center + ImVec2(cosf(a), sinf(a)) * radius;
            inner[i] = center + ImVec2(cosf(a), sinf(a)) * (radius * 0.70f);
        }
        int vtx = dl->VtxBuffer.Size;
        dl->AddConvexPolyFilled(outer, 6, IM_COL32_WHITE);
        ShadeGradient(dl, vtx, center - diag, center + diag, ca, cb);
        dl->AddConvexPolyFilled(inner, 6, Col(Colors.Bg, alpha));

        // Star: four kites around the center, rotating the other way.
        vtx = dl->VtxBuffer.Size;
        const float star_rot = -rotation * 2.0f;
        const float tip = radius * 0.50f, waist = radius * 0.15f;
        for (int i = 0; i < 4; i++)
        {
            const float a = star_rot + i * kPi * 0.5f;
            const ImVec2 kite[4] =
            {
                center,
                center + ImVec2(cosf(a - kPi * 0.25f), sinf(a - kPi * 0.25f)) * waist,
                center + ImVec2(cosf(a), sinf(a)) * tip,
                center + ImVec2(cosf(a + kPi * 0.25f), sinf(a + kPi * 0.25f)) * waist,
            };
            dl->AddConvexPolyFilled(kite, 4, IM_COL32_WHITE);
        }
        ShadeGradient(dl, vtx, center + ImVec2(-tip, tip), center + ImVec2(tip, -tip), cb, ca);
    }

    // Rotating arc with a fading tail.
    void Spinner(ImDrawList* dl, ImVec2 center, float radius, float thickness, float time, float alpha)
    {
        dl->AddCircle(center, radius, Col(Colors.Border, 0.7f * alpha), 64, thickness);

        const float start = time * 4.2f;
        const float length = kPi * (0.75f + 0.35f * sinf(time * 1.6f));
        const int segments = 40;
        for (int i = 0; i < segments; i++)
        {
            const float t0 = (float)i / segments, t1 = (float)(i + 1) / segments;
            const ImVec4 c = Mix(Colors.AccentA, Colors.AccentB, t1);
            dl->PathArcTo(center, radius, start + length * t0, start + length * t1, 3);
            dl->PathStroke(Col(c, alpha * t1), 0, thickness);
        }
        const float head = start + length;
        dl->AddCircleFilled(center + ImVec2(cosf(head), sinf(head)) * radius, thickness * 0.5f, Col(Colors.AccentB, alpha), 12);
    }

    void Ring(ImDrawList* dl, ImVec2 center, float radius, float thickness, float fraction, ImU32 col)
    {
        if (fraction <= 0.0f)
            return;
        const float a0 = -kPi * 0.5f;
        dl->PathArcTo(center, radius, a0, a0 + 2.0f * kPi * ImMin(fraction, 1.0f), 64);
        dl->PathStroke(col, 0, thickness);
    }

    // Draws the first 't' (0..1) of a polyline, so marks can "write themselves".
    static void PartialPolyline(ImDrawList* dl, const ImVec2* pts, int count, float t, ImU32 col, float thickness)
    {
        float total = 0.0f;
        for (int i = 1; i < count; i++)
            total += ImSqrt(ImLengthSqr(pts[i] - pts[i - 1]));
        float remaining = total * ImClamp(t, 0.0f, 1.0f);
        if (remaining <= 0.0f)
            return;

        dl->PathLineTo(pts[0]);
        ImVec2 last = pts[0];
        for (int i = 1; i < count && remaining > 0.0f; i++)
        {
            const float seg = ImSqrt(ImLengthSqr(pts[i] - pts[i - 1]));
            const float take = ImMin(seg, remaining);
            last = pts[i - 1] + (pts[i] - pts[i - 1]) * (take / ImMax(seg, 1e-6f));
            dl->PathLineTo(last);
            remaining -= take;
        }
        dl->PathStroke(col, 0, thickness);
        dl->AddCircleFilled(pts[0], thickness * 0.5f, col, 12);
        dl->AddCircleFilled(last, thickness * 0.5f, col, 12);
    }

    void CheckMark(ImDrawList* dl, ImVec2 center, float size, float thickness, float t, ImU32 col)
    {
        const ImVec2 pts[3] =
        {
            center + ImVec2(-0.36f, 0.02f) * size,
            center + ImVec2(-0.10f, 0.28f) * size,
            center + ImVec2(0.38f, -0.24f) * size,
        };
        PartialPolyline(dl, pts, 3, t, col, thickness);
    }

    void CrossMark(ImDrawList* dl, ImVec2 center, float size, float thickness, float t, ImU32 col)
    {
        const float e = size * 0.28f;
        const ImVec2 a[2] = { center + ImVec2(-e, -e), center + ImVec2(e, e) };
        const ImVec2 b[2] = { center + ImVec2(e, -e), center + ImVec2(-e, e) };
        PartialPolyline(dl, a, 2, t * 2.0f, col, thickness);
        PartialPolyline(dl, b, 2, t * 2.0f - 1.0f, col, thickness);
    }

    void Icon(ImDrawList* dl, const char* glyph, ImVec2 center, ImU32 col, bool large)
    {
        ImFont* font = large ? Theme::Fonts.IconsLarge : Theme::Fonts.Icons;
        if (!font)
        {
            // No icon font on this system: draw a small diamond so layouts still read well.
            const float r = S(large ? 4.5f : 3.5f);
            dl->AddQuadFilled(center + ImVec2(0, -r), center + ImVec2(r, 0), center + ImVec2(0, r), center + ImVec2(-r, 0), col);
            return;
        }
        const ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, glyph);
        dl->AddText(font, font->FontSize, ImFloor(center - size * 0.5f), col, glyph);
    }

    void StatusDot(ImDrawList* dl, ImVec2 center, float radius, const ImVec4& col, bool pulse)
    {
        if (pulse && Theme::Animations)
        {
            const float p = fmodf((float)ImGui::GetTime() * 0.8f, 1.0f);
            dl->AddCircleFilled(center, radius + radius * 2.2f * p, Col(col, 0.35f * (1.0f - p)), 24);
        }
        dl->AddCircleFilled(center, radius, Col(col), 16);
    }

    // Drifting points joined by faint lines; points near the mouse link to it.
    void Particles(ImDrawList* dl, ImVec2 min, ImVec2 max, float alpha)
    {
        struct Particle { float x, y, vx, vy, r; };
        const int kCount = 46;
        static Particle particles[kCount];
        static bool initialized = false;
        if (!initialized)
        {
            unsigned int seed = 0x2545F491u;
            auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f; };
            for (Particle& p : particles)
                p = { rnd(), rnd(), (rnd() - 0.5f) * 0.035f, (rnd() - 0.5f) * 0.035f, 0.8f + rnd() * 1.2f };
            initialized = true;
        }

        const ImVec2 size = max - min;
        const float dt = Theme::Animations ? ImMin(ImGui::GetIO().DeltaTime, 0.05f) : 0.0f;
        for (Particle& p : particles)
        {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            if (p.x < -0.05f) p.x += 1.1f; else if (p.x > 1.05f) p.x -= 1.1f;
            if (p.y < -0.05f) p.y += 1.1f; else if (p.y > 1.05f) p.y -= 1.1f;
        }

        const float link = S(95.0f);
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool mouse_inside = ImGui::IsMousePosValid() && ImRect(min, max).Contains(mouse);
        ImVec2 pos[kCount];
        for (int i = 0; i < kCount; i++)
            pos[i] = min + ImVec2(particles[i].x * size.x, particles[i].y * size.y);

        for (int i = 0; i < kCount; i++)
        {
            const ImVec4 tint = Mix(Colors.AccentA, Colors.AccentB, ImClamp(particles[i].x, 0.0f, 1.0f));
            for (int j = i + 1; j < kCount; j++)
            {
                const float d = ImSqrt(ImLengthSqr(pos[i] - pos[j]));
                if (d < link)
                    dl->AddLine(pos[i], pos[j], Col(tint, alpha * 0.22f * (1.0f - d / link)), S(1.0f));
            }
            if (mouse_inside)
            {
                const float d = ImSqrt(ImLengthSqr(pos[i] - mouse));
                if (d < link * 1.4f)
                    dl->AddLine(pos[i], mouse, Col(tint, alpha * 0.45f * (1.0f - d / (link * 1.4f))), S(1.0f));
            }
            dl->AddCircleFilled(pos[i], S(particles[i].r), Col(tint, alpha * 0.75f), 8);
        }
    }

    //-------------------------------------------------------------------------
    // Widgets
    //-------------------------------------------------------------------------

    // Registers an item of 'size' at the cursor. Returns false if clipped.
    static bool AddItem(const char* str_id, ImVec2 size, ImRect* out_bb, ImGuiID* out_id)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;
        const ImGuiID id = window->GetID(str_id);
        const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
        ImGui::ItemSize(size);
        if (!ImGui::ItemAdd(bb, id))
            return false;
        *out_bb = bb;
        *out_id = id;
        return true;
    }

    bool WindowButton(const char* str_id, bool is_close, ImVec2 size)
    {
        ImRect bb;
        ImGuiID id;
        if (!AddItem(str_id, size, &bb, &id))
            return false;
        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        const float h = Smooth(id, hovered ? 1.0f : 0.0f, 18.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (h > 0.01f)
            dl->AddRectFilled(bb.Min, bb.Max, Col(is_close ? Colors.Danger : Colors.CardHi, h * (held ? 0.75f : 1.0f)), S(6.0f));

        const ImU32 fg = Col(Mix(Colors.TextDim, ImVec4(1, 1, 1, 1), h));
        const ImVec2 c = ImFloor(bb.GetCenter()) + ImVec2(0.5f, 0.5f);
        const float e = S(5.0f), th = S(1.3f);
        if (is_close)
        {
            dl->AddLine(c + ImVec2(-e, -e), c + ImVec2(e, e), fg, th);
            dl->AddLine(c + ImVec2(-e, e), c + ImVec2(e, -e), fg, th);
        }
        else
        {
            dl->AddLine(c + ImVec2(-e, 0), c + ImVec2(e, 0), fg, th);
        }
        return pressed;
    }

    bool NavItem(const char* icon, const char* label, bool selected, float width)
    {
        ImRect bb;
        ImGuiID id;
        if (!AddItem(label, ImVec2(width, S(40.0f)), &bb, &id))
            return false;
        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        const float sel = Smooth(SubId(id, 0), selected ? 1.0f : 0.0f, 14.0f);
        const float hov = Smooth(SubId(id, 1), hovered ? 1.0f : 0.0f, 16.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rounding = S(8.0f);
        if (hov > 0.01f)
            dl->AddRectFilled(bb.Min, bb.Max, Col(Colors.CardHi, hov * 0.7f), rounding);
        if (sel > 0.01f)
            GradientRect(dl, bb.Min, bb.Max, Col(Colors.AccentA, 0.24f * sel), Col(Colors.AccentA, 0.0f), rounding);

        const ImVec2 icon_center(bb.Min.x + S(22.0f), bb.GetCenter().y);
        Icon(dl, icon, icon_center, Col(Mix(Mix(Colors.TextDim, Colors.Text, hov), Colors.AccentA, sel)));

        ImFont* font = selected ? Theme::Fonts.Bold : Theme::Fonts.Body;
        const ImVec2 text_pos(bb.Min.x + S(44.0f) + S(2.0f) * sel, bb.GetCenter().y - font->FontSize * 0.5f);
        dl->AddText(font, font->FontSize, ImFloor(text_pos), Col(Mix(Colors.TextDim, Colors.Text, ImMax(sel, hov))), label);
        return pressed;
    }

    bool Toggle(const char* str_id, bool* value)
    {
        ImRect bb;
        ImGuiID id;
        const ImVec2 size(S(40.0f), S(22.0f));
        if (!AddItem(str_id, size, &bb, &id))
            return false;
        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed)
        {
            *value = !*value;
            ImGui::MarkItemEdited(id);
        }
        const float t = Smooth(SubId(id, 0), *value ? 1.0f : 0.0f, 14.0f);
        const float hov = Smooth(SubId(id, 1), hovered ? 1.0f : 0.0f, 16.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = size.y * 0.5f;
        dl->AddRectFilled(bb.Min, bb.Max, Col(Mix(Colors.Border, Mix(Colors.Border, Colors.TextFaint, 0.5f), hov)), r);
        if (t > 0.001f)
            GradientRect(dl, bb.Min, bb.Max, Col(Colors.AccentA, t), Col(Colors.AccentB, t), r);

        const float knob_r = r - S(3.0f);
        const ImVec2 knob(bb.Min.x + r + t * (size.x - 2.0f * r), bb.GetCenter().y);
        dl->AddCircleFilled(knob + ImVec2(0, S(1.0f)), knob_r + S(0.5f), Col(ImVec4(0, 0, 0, 0.35f)), 20);
        dl->AddCircleFilled(knob, knob_r * (held ? 0.88f : 1.0f), Col(ImVec4(1, 1, 1, 1)), 20);
        return pressed;
    }

    static bool ButtonBase(const char* label, ImVec2 size, ImRect* bb, ImGuiID* id, float* hov, bool* held)
    {
        if (!AddItem(label, size, bb, id))
            return false;
        bool hovered;
        const bool pressed = ImGui::ButtonBehavior(*bb, *id, &hovered, held);
        *hov = Smooth(*id, hovered ? 1.0f : 0.0f, 16.0f);
        return pressed;
    }

    static void ButtonLabel(ImDrawList* dl, const ImRect& bb, const char* label, ImU32 col)
    {
        ImFont* font = Theme::Fonts.Bold;
        const char* label_end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label, label_end);
        dl->AddText(font, font->FontSize, ImFloor(bb.GetCenter() - ts * 0.5f), col, label, label_end);
    }

    bool PrimaryButton(const char* label, ImVec2 size)
    {
        ImRect bb;
        ImGuiID id;
        float hov = 0.0f;
        bool held = false;
        const bool pressed = ButtonBase(label, size, &bb, &id, &hov, &held);
        if (bb.GetWidth() <= 0.0f)
            return false;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rounding = S(8.0f);
        for (int i = 1; i <= 3; i++) // glow
        {
            const float grow = S(2.0f) * i;
            dl->AddRectFilled(bb.Min - ImVec2(grow, grow), bb.Max + ImVec2(grow, grow), Col(Colors.AccentA, 0.05f + 0.05f * hov), rounding + grow);
        }
        GradientRect(dl, bb.Min, bb.Max, Col(Colors.AccentA), Col(Colors.AccentB), rounding);
        if (hov > 0.01f || held)
            dl->AddRectFilled(bb.Min, bb.Max, held ? Col(ImVec4(0, 0, 0, 0.15f)) : Col(ImVec4(1, 1, 1, 0.10f * hov)), rounding);
        ButtonLabel(dl, bb, label, Col(ImVec4(1, 1, 1, 1)));
        return pressed;
    }

    bool SecondaryButton(const char* label, ImVec2 size, const ImVec4* hover_tint)
    {
        ImRect bb;
        ImGuiID id;
        float hov = 0.0f;
        bool held = false;
        const bool pressed = ButtonBase(label, size, &bb, &id, &hov, &held);
        if (bb.GetWidth() <= 0.0f)
            return false;

        const ImVec4 tint = hover_tint ? *hover_tint : Colors.AccentA;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rounding = S(8.0f);
        dl->AddRectFilled(bb.Min, bb.Max, Col(Mix(Colors.CardHi, tint, 0.12f * hov + (held ? 0.08f : 0.0f))), rounding);
        dl->AddRect(bb.Min, bb.Max, Col(Mix(Colors.Border, tint, 0.6f * hov)), rounding, 0, 1.0f);
        ButtonLabel(dl, bb, label, Col(Mix(Colors.Text, hover_tint ? tint : Colors.Text, hov)));
        return pressed;
    }

    bool Swatch(const char* str_id, const ImVec4& a, const ImVec4& b, bool selected, float radius)
    {
        ImRect bb;
        ImGuiID id;
        const float pad = S(4.0f);
        if (!AddItem(str_id, ImVec2((radius + pad) * 2.0f, (radius + pad) * 2.0f), &bb, &id))
            return false;
        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        const float sel = Smooth(SubId(id, 0), selected ? 1.0f : 0.0f, 14.0f);
        const float hov = Smooth(SubId(id, 1), hovered ? 1.0f : 0.0f, 16.0f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 c = bb.GetCenter();
        const float r = radius - S(1.5f) * (1.0f - hov);
        const int vtx = dl->VtxBuffer.Size;
        dl->AddCircleFilled(c, r, IM_COL32_WHITE, 32);
        ShadeGradient(dl, vtx, c - ImVec2(r, r), c + ImVec2(r, r), Col(a), Col(b));
        if (sel > 0.01f)
        {
            dl->AddCircle(c, radius + S(3.0f), Col(ImVec4(1, 1, 1, 0.9f), sel), 32, S(1.5f));
            CheckMark(dl, c, radius * 1.1f, S(2.0f), sel, Col(ImVec4(1, 1, 1, 1)));
        }
        if (hovered)
            ImGui::SetTooltip("%s", str_id);
        return pressed;
    }

    void ProgressBar(float fraction, ImVec2 size)
    {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1 = p0 + size;
        ImGui::Dummy(size);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = size.y * 0.5f;
        dl->AddRectFilled(p0, p1, Col(Colors.CardHi), r);

        fraction = ImClamp(fraction, 0.0f, 1.0f);
        if (fraction <= 0.0f)
            return;
        const ImVec2 f1(p0.x + ImMax(size.y, size.x * fraction), p1.y);
        GradientRect(dl, p0, f1, Col(Colors.AccentA), Col(Colors.AccentB), r);

        // Shimmer sweeping across the filled part.
        if (Theme::Animations)
        {
            const float w = f1.x - p0.x;
            const float x = p0.x - S(60.0f) + fmodf((float)ImGui::GetTime() * 0.9f, 1.3f) / 1.3f * (w + S(120.0f));
            dl->PushClipRect(p0, f1, true);
            GradientRect(dl, ImVec2(x - S(40.0f), p0.y), ImVec2(x, p1.y), Col(ImVec4(1, 1, 1, 0.0f)), Col(ImVec4(1, 1, 1, 0.45f)));
            GradientRect(dl, ImVec2(x, p0.y), ImVec2(x + S(40.0f), p1.y), Col(ImVec4(1, 1, 1, 0.45f)), Col(ImVec4(1, 1, 1, 0.0f)));
            dl->PopClipRect();
        }
        RadialGlow(dl, ImVec2(f1.x - r, (p0.y + p1.y) * 0.5f), S(16.0f), Col(Colors.AccentB, 0.55f), Col(Colors.AccentB, 0.0f));
    }

    void Pill(const char* label, const ImVec4& dot_col, bool pulse)
    {
        ImFont* font = Theme::Fonts.Small;
        const ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label);
        const ImVec2 size(ts.x + S(30.0f), S(26.0f));
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::Dummy(size);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p0 + size, Col(Colors.Card), size.y * 0.5f);
        dl->AddRect(p0, p0 + size, Col(Colors.Border), size.y * 0.5f);
        StatusDot(dl, ImVec2(p0.x + S(13.0f), p0.y + size.y * 0.5f), S(3.5f), dot_col, pulse);
        dl->AddText(font, font->FontSize, ImFloor(ImVec2(p0.x + S(22.0f), p0.y + (size.y - ts.y) * 0.5f)), Col(Colors.TextDim), label);
    }

    void SectionLabel(const char* text)
    {
        ImFont* font = Theme::Fonts.Small;
        const ImVec2 size = SpacedTextSize(font, text, S(1.5f));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        SpacedText(ImGui::GetWindowDrawList(), font, p, Col(Colors.TextFaint), text, S(1.5f));
        ImGui::Dummy(size);
    }

    void TextColored(ImFont* font, const ImVec4& col, const char* text)
    {
        ImGui::PushFont(font);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void AreaChart(const char* str_id, const float* values, int count, ImVec2 size, float v_min, float v_max, bool grid)
    {
        if (count < 2)
            return;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1 = p0 + size;
        ImGui::InvisibleButton(str_id, size);
        const bool hovered = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (grid)
            for (int i = 0; i <= 3; i++)
            {
                const float y = floorf(p0.y + size.y * i / 3.0f) + 0.5f;
                dl->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), Col(Colors.Border, 0.6f));
            }

        ImVector<ImVec2> pts;
        pts.resize(count);
        for (int i = 0; i < count; i++)
        {
            const float t = ImSaturate((values[i] - v_min) / (v_max - v_min));
            pts[i] = ImVec2(p0.x + size.x * i / (count - 1), p1.y - S(2.0f) - (size.y - S(4.0f)) * t);
        }

        // Fill under the line: one quad per segment, fading to transparent at the bottom.
        const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
        const ImU32 top = Col(Colors.AccentA, 0.30f), bottom = Col(Colors.AccentA, 0.0f);
        dl->PrimReserve((count - 1) * 6, (count - 1) * 4);
        for (int i = 0; i < count - 1; i++)
        {
            const unsigned int idx = dl->_VtxCurrentIdx;
            dl->PrimWriteIdx((ImDrawIdx)idx); dl->PrimWriteIdx((ImDrawIdx)(idx + 1)); dl->PrimWriteIdx((ImDrawIdx)(idx + 2));
            dl->PrimWriteIdx((ImDrawIdx)idx); dl->PrimWriteIdx((ImDrawIdx)(idx + 2)); dl->PrimWriteIdx((ImDrawIdx)(idx + 3));
            dl->PrimWriteVtx(pts[i], uv, top);
            dl->PrimWriteVtx(pts[i + 1], uv, top);
            dl->PrimWriteVtx(ImVec2(pts[i + 1].x, p1.y), uv, bottom);
            dl->PrimWriteVtx(ImVec2(pts[i].x, p1.y), uv, bottom);
        }

        const int vtx = dl->VtxBuffer.Size;
        dl->AddPolyline(pts.Data, count, IM_COL32_WHITE, 0, S(2.0f));
        ShadeGradient(dl, vtx, p0, ImVec2(p1.x, p0.y), Col(Colors.AccentA), Col(Colors.AccentB));

        if (hovered)
        {
            const float mx = ImGui::GetIO().MousePos.x;
            const int i = ImClamp((int)((mx - p0.x) / size.x * (count - 1) + 0.5f), 0, count - 1);
            dl->AddLine(ImVec2(pts[i].x, p0.y), ImVec2(pts[i].x, p1.y), Col(Colors.TextFaint, 0.6f));
            dl->AddCircleFilled(pts[i], S(5.0f), Col(Colors.Bg), 16);
            dl->AddCircle(pts[i], S(5.0f), Col(Mix(Colors.AccentA, Colors.AccentB, (float)i / (count - 1))), 16, S(2.0f));
            ImGui::SetTooltip("%.0f", values[i]);
        }
    }

    bool BeginCard(const char* id, ImVec2 size, ImVec2 padding)
    {
        if (padding.x < 0.0f)
            padding = S(18.0f, 16.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors.Card);
        ImGui::PushStyleColor(ImGuiCol_Border, Colors.Border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        const bool open = ImGui::BeginChild(id, size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, S(8.0f, 6.0f)); // popped in EndCard()

        // Faint highlight along the top edge.
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const ImRect r = window->Rect();
        const float y = r.Min.y + 0.5f;
        const float inset = S(14.0f);
        ImDrawList* dl = window->DrawList;
        GradientRect(dl, ImVec2(r.Min.x + inset, y), ImVec2(r.GetCenter().x, y + 1.0f), Col(ImVec4(1, 1, 1, 0.0f)), Col(ImVec4(1, 1, 1, 0.07f)));
        GradientRect(dl, ImVec2(r.GetCenter().x, y), ImVec2(r.Max.x - inset, y + 1.0f), Col(ImVec4(1, 1, 1, 0.07f)), Col(ImVec4(1, 1, 1, 0.0f)));
        return open;
    }

    void EndCard()
    {
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    void CardHeader(const char* icon, const char* title, const char* subtitle)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float box = S(34.0f);
        dl->AddRectFilled(p, p + ImVec2(box, box), Col(Colors.AccentA, 0.14f), S(9.0f));
        Icon(dl, icon, p + ImVec2(box, box) * 0.5f, Col(Colors.AccentA));

        ImFont* bold = Theme::Fonts.Bold;
        ImFont* small = Theme::Fonts.Small;
        const float x = p.x + box + S(12.0f);
        if (subtitle)
        {
            const float block = bold->FontSize + S(1.0f) + small->FontSize;
            const float y = p.y + (box - block) * 0.5f;
            dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(x, y)), Col(Colors.Text), title);
            dl->AddText(small, small->FontSize, ImFloor(ImVec2(x, y + bold->FontSize + S(1.0f))), Col(Colors.TextDim), subtitle);
        }
        else
        {
            dl->AddText(bold, bold->FontSize, ImFloor(ImVec2(x, p.y + (box - bold->FontSize) * 0.5f)), Col(Colors.Text), title);
        }
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, box));
    }
}
