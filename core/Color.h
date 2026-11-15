// Color.h: HSV colour state, named colours and #rgb parsing for EisenScript. No Rack dependency.
// Hue in degrees 0..360, saturation, value and alpha 0..1.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace ss {

struct Rgb { float r = 1.f, g = 1.f, b = 1.f; };

struct Hsv
{
    float h = 0.f, s = 0.f, v = 1.f, a = 1.f;   // default white, like Structure Synth
};

inline float clamp01 (float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
inline float wrapHue (float h) { h = std::fmod (h, 360.f); return h < 0.f ? h + 360.f : h; }

inline Rgb toRgb (const Hsv& c)
{
    const float h = wrapHue (c.h) / 60.f, s = clamp01 (c.s), v = clamp01 (c.v);
    const int i = (int) std::floor (h) % 6;
    const float f = h - std::floor (h), p = v * (1 - s), q = v * (1 - s * f), u = v * (1 - s * (1 - f));
    Rgb o;
    switch (i)
    {
        case 0: o.r = v; o.g = u; o.b = p; break;
        case 1: o.r = q; o.g = v; o.b = p; break;
        case 2: o.r = p; o.g = v; o.b = u; break;
        case 3: o.r = p; o.g = q; o.b = v; break;
        case 4: o.r = u; o.g = p; o.b = v; break;
        default: o.r = v; o.g = p; o.b = q; break;
    }
    return o;
}

inline Hsv fromRgb (float r, float g, float b)
{
    Hsv c;
    const float mx = std::max (r, std::max (g, b)), mn = std::min (r, std::min (g, b)), d = mx - mn;
    c.v = mx;
    c.s = mx > 0.f ? d / mx : 0.f;
    if (d > 0.f)
    {
        if (mx == r) c.h = 60.f * std::fmod ((g - b) / d, 6.f);
        else if (mx == g) c.h = 60.f * ((b - r) / d + 2.f);
        else c.h = 60.f * ((r - g) / d + 4.f);
    }
    c.h = wrapHue (c.h);
    return c;
}

// Names, #rgb, #rrggbb. Alpha is left alone (the caller keeps its own).
inline bool parseColor (std::string s, Hsv& out)
{
    for (auto& ch : s) ch = (char) std::tolower ((unsigned char) ch);
    auto hex = [] (char ch) { return ch >= 'a' ? ch - 'a' + 10 : ch - '0'; };
    if (! s.empty() && s[0] == '#')
    {
        for (size_t i = 1; i < s.size(); ++i)
            if (! std::isxdigit ((unsigned char) s[i])) return false;
        float r, g, b;
        if (s.size() == 4) { r = hex (s[1]) / 15.f; g = hex (s[2]) / 15.f; b = hex (s[3]) / 15.f; }
        else if (s.size() == 7) { r = (hex (s[1]) * 16 + hex (s[2])) / 255.f; g = (hex (s[3]) * 16 + hex (s[4])) / 255.f; b = (hex (s[5]) * 16 + hex (s[6])) / 255.f; }
        else return false;
        const float a = out.a;
        out = fromRgb (r, g, b);
        out.a = a;
        return true;
    }
    // ponytail: the common SVG names only; add more when a script asks for one.
    static const struct { const char* name; unsigned rgb; } names[] = {
        {"white", 0xffffff}, {"black", 0x000000}, {"red", 0xff0000}, {"green", 0x008000}, {"lime", 0x00ff00},
        {"blue", 0x0000ff}, {"yellow", 0xffff00}, {"orange", 0xffa500}, {"purple", 0x800080}, {"cyan", 0x00ffff},
        {"aqua", 0x00ffff}, {"magenta", 0xff00ff}, {"fuchsia", 0xff00ff}, {"grey", 0x808080}, {"gray", 0x808080},
        {"silver", 0xc0c0c0}, {"maroon", 0x800000}, {"olive", 0x808000}, {"navy", 0x000080}, {"teal", 0x008080},
        {"pink", 0xffc0cb}, {"brown", 0xa52a2a}, {"gold", 0xffd700}, {"violet", 0xee82ee}, {"indigo", 0x4b0082},
        {"lightgrey", 0xd3d3d3}, {"darkgrey", 0xa9a9a9}, {"darkred", 0x8b0000}, {"darkblue", 0x00008b},
        {"darkgreen", 0x006400}, {"skyblue", 0x87ceeb}, {"salmon", 0xfa8072}, {"coral", 0xff7f50},
        {"turquoise", 0x40e0d0}, {"beige", 0xf5f5dc}, {"ivory", 0xfffff0}, {"crimson", 0xdc143c},
    };
    for (const auto& n : names)
        if (s == n.name)
        {
            const float a = out.a;
            out = fromRgb (((n.rgb >> 16) & 255) / 255.f, ((n.rgb >> 8) & 255) / 255.f, (n.rgb & 255) / 255.f);
            out.a = a;
            return true;
        }
    return false;
}

// Linear move in HSV toward `to` by k (so red -> blue passes yellow and green, as Structure Synth does).
inline Hsv blend (const Hsv& from, const Hsv& to, float k)
{
    Hsv c = from;
    c.h = wrapHue (from.h + k * (to.h - from.h));
    c.s = clamp01 (from.s + k * (to.s - from.s));
    c.v = clamp01 (from.v + k * (to.v - from.v));
    return c;
}

}   // namespace ss
