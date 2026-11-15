// Art.h: the cockpit's frame sheet (one loop as cols x rows cells) and the sequencer's voltage mapping.
// No Rack dependency. The sheet's pixels come in as RGBA from the caller (stb_image in the plugin).
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Builder.h"
#include <cstdint>
#include <vector>

namespace ss {

struct ArtSheet
{
    int w = 0, h = 0, frames = 0, cols = 1, rows = 1;
    double period = 1.0;          // seconds per loop
    std::vector<uint8_t> rgba;    // w * h * 4

    bool ok() const { return w > 0 && h > 0 && frames > 0 && cols > 0 && rows > 0 && rgba.size() == (size_t) w * h * 4; }

    int frameAt (double phase) const
    {
        phase -= std::floor (phase);
        return std::min (frames - 1, std::max (0, (int) (phase * frames)));
    }

    // u, v in 0..1 across one frame (v down), nearest pixel
    Rgb sample (int frame, float u, float v) const
    {
        Rgb o;
        if (! ok()) return o;
        const int cw = w / cols, ch = h / rows, f = std::min (frames - 1, std::max (0, frame));
        const int px = (f % cols) * cw + std::min (cw - 1, std::max (0, (int) (u * cw)));
        const int py = (f / cols) * ch + std::min (ch - 1, std::max (0, (int) (v * ch)));
        const uint8_t* p = &rgba[((size_t) py * w + px) * 4];
        o.r = p[0] / 255.f; o.g = p[1] / 255.f; o.b = p[2] / 255.f;
        return o;
    }
};

struct Volts { float x = 0, y = 0, z = 0, hue = 0, size = 0; };

// X/Y/Z: the object's centre across the scene bounds as -5..+5 V. HUE: 0..360 as 0..10 V.
// SIZE: 10 V for the largest object, 1.25 V less per halving, floored at 0.
inline Volts walk (const Obj& o, const Scene& sc)
{
    Volts v;
    const Vec3 c = o.centre();
    auto map = [] (float x, float lo, float hi) { return hi - lo > 1e-6f ? -5.f + 10.f * (x - lo) / (hi - lo) : 0.f; };
    v.x = map (c.x, sc.lo.x, sc.hi.x);
    v.y = map (c.y, sc.lo.y, sc.hi.y);
    v.z = map (c.z, sc.lo.z, sc.hi.z);
    v.hue = 10.f * wrapHue (o.hsv.h) / 360.f;
    const float s = o.mat.size();
    v.size = s > 0.f ? 10.f * clamp01 (1.f + std::log2 (s / sc.maxSize) / 8.f) : 0.f;
    return v;
}

}   // namespace ss
