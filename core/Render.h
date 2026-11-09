// Render.h: draws a Scene into an RGBA buffer with a z-buffer, on the CPU. No Rack dependency.
// One image upload per frame instead of one NanoVG fill per face (20k boxes would be ~60k GPU calls).
// Box faces cull by their true outward normal (cofactor matrix, so mirrors work), flat-shaded by one light.
// Spheres are shaded discs, grid draws box edges, line and dot are strokes; cylinder and mesh draw as boxes.
// ponytail: alpha < 1 blends but does not write depth, so overlapping glass is order-dependent. Sort if it matters.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Art.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ss {

struct Px { float x, y, z; };   // screen x, y in pixels (y down); z larger = nearer

struct Raster
{
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    std::vector<float> z;

    void resize (int W, int H) { w = W; h = H; rgba.assign ((size_t) W * H * 4, 0); z.assign ((size_t) W * H, -1e30f); }

    void put (int x, int y, float zz, const Rgb& c, float a, bool ztest = true)
    {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        const size_t i = (size_t) y * w + x;
        if (ztest && zz <= z[i]) return;
        uint8_t* p = &rgba[i * 4];
        auto mix = [a] (uint8_t d, float s) { return (uint8_t) std::lround (255.f * clamp01 (d / 255.f + a * (s - d / 255.f))); };
        p[0] = mix (p[0], c.r); p[1] = mix (p[1], c.g); p[2] = mix (p[2], c.b); p[3] = 255;
        if (a >= 0.99f && ztest) z[i] = zz;
    }

    void tri (const Px& a, const Px& b, const Px& c, const Rgb& col, float alpha)
    {
        const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::fabs (area) < 1e-6f) return;
        const int x0 = std::max (0, (int) std::floor (std::min (a.x, std::min (b.x, c.x))));
        const int x1 = std::min (w - 1, (int) std::ceil (std::max (a.x, std::max (b.x, c.x))));
        const int y0 = std::max (0, (int) std::floor (std::min (a.y, std::min (b.y, c.y))));
        const int y1 = std::min (h - 1, (int) std::ceil (std::max (a.y, std::max (b.y, c.y))));
        const float inv = 1.f / area;
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
            {
                const float px = x + 0.5f, py = y + 0.5f;
                const float w0 = ((c.x - b.x) * (py - b.y) - (c.y - b.y) * (px - b.x)) * inv;
                const float w1 = ((a.x - c.x) * (py - c.y) - (a.y - c.y) * (px - c.x)) * inv;
                const float w2 = 1.f - w0 - w1;
                if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
                put (x, y, w0 * a.z + w1 * b.z + w2 * c.z, col, alpha);
            }
    }

    // shaded ball: per-pixel normal from the disc, lit by l (view space)
    void ball (const Px& c, float r, float rz, const Rgb& col, float alpha, const Vec3& l, float gain)
    {
        if (r < 0.5f) { put ((int) c.x, (int) c.y, c.z, col, alpha); return; }
        for (int y = std::max (0, (int) (c.y - r)); y <= std::min (h - 1, (int) (c.y + r)); ++y)
            for (int x = std::max (0, (int) (c.x - r)); x <= std::min (w - 1, (int) (c.x + r)); ++x)
            {
                const float dx = (x + 0.5f - c.x) / r, dy = (y + 0.5f - c.y) / r, d2 = dx * dx + dy * dy;
                if (d2 > 1.f) continue;
                const float nz = std::sqrt (1.f - d2);
                const float s = gain * (0.25f + 0.75f * std::max (0.f, dx * l.x - dy * l.y + nz * l.z));
                Rgb o; o.r = clamp01 (col.r * s); o.g = clamp01 (col.g * s); o.b = clamp01 (col.b * s);
                put (x, y, c.z + nz * rz, o, alpha);
            }
    }

    void line (const Px& a, const Px& b, const Rgb& col, float alpha, bool ztest = true)
    {
        const int n = std::max (1, (int) std::ceil (std::max (std::fabs (b.x - a.x), std::fabs (b.y - a.y))));
        if (n > 4 * (w + h)) return;   // a wildly off-screen segment is not worth walking
        for (int i = 0; i <= n; ++i)
        {
            const float t = (float) i / n;
            put ((int) (a.x + t * (b.x - a.x)), (int) (a.y + t * (b.y - a.y)), a.z + t * (b.z - a.z) + 1e-3f, col, alpha, ztest);
        }
    }
};

struct View { float yaw = 0.f, pitch = 0.f, zoom = 1.f; };   // degrees, degrees, x

struct RenderOpts
{
    int visible = -1;                 // draw the first N objects (build order); < 0: all
    float pulse = 0.f;                // 0..1: audio swells each object
    const float* audio = nullptr;     // ring of audio samples, -1..1
    int audioN = 0, audioHead = 0;
    const ArtSheet* art = nullptr;    // backdrop + skin source, may be null
    int artFrame = 0;
    float artMix = 0.f;               // backdrop opacity
    float skin = 0.f;                 // 0: script colours, 1: art colours
    int highlight = -1;               // sequencer's object, outlined in lime
};

namespace render {

// raster pixel -> 0..1 inside the art frame, cover-fit (the frame fills the screen, cropped at the long side)
inline void coverUv (const ArtSheet& a, int W, int H, float x, float y, float& u, float& v)
{
    const float ca = (float) (a.w / a.cols) / (float) (a.h / a.rows), ra = (float) W / H;
    u = x / W; v = y / H;
    if (ra > ca) v = 0.5f + (v - 0.5f) * ca / ra;
    else u = 0.5f + (u - 0.5f) * ra / ca;
}

inline Rgb lerp (const Rgb& a, const Rgb& b, float k)
{
    Rgb o; o.r = a.r + k * (b.r - a.r); o.g = a.g + k * (b.g - a.g); o.b = a.b + k * (b.b - a.b);
    return o;
}

}   // namespace render

inline void renderScene (const Scene& sc, const View& view, const RenderOpts& o, Raster& r, const Rgb& fallbackBg)
{
    using namespace render;
    const Rgb bg = sc.hasBackground ? sc.background : fallbackBg;
    const bool art = o.art && o.art->ok();
    for (int y = 0; y < r.h; ++y)
        for (int x = 0; x < r.w; ++x)
        {
            Rgb c = bg;
            if (art && o.artMix > 0.f)
            {
                float u, v;
                coverUv (*o.art, r.w, r.h, x + 0.5f, y + 0.5f, u, v);
                c = lerp (bg, o.art->sample (o.artFrame, u, v), clamp01 (o.artMix));
            }
            uint8_t* p = &r.rgba[((size_t) y * r.w + x) * 4];
            p[0] = (uint8_t) std::lround (255.f * c.r); p[1] = (uint8_t) std::lround (255.f * c.g); p[2] = (uint8_t) std::lround (255.f * c.b); p[3] = 255;
        }
    std::fill (r.z.begin(), r.z.end(), -1e30f);
    if (sc.objs.empty()) return;

    // view: centre the scene, fit its bounding sphere to the screen height, rotate, light from upper left
    const Vec3 mid = (sc.lo + sc.hi) * 0.5f;
    const float rad = std::max (1e-6f, (sc.hi - sc.lo).length() * 0.5f);
    const Mat ry = Mat::rot (1, view.yaw), rx = Mat::rot (0, view.pitch);
    const Mat rot = rx * ry;   // linear parts only are used below
    auto rotate = [&] (const Vec3& p) { return rot.applyDir (p); };
    const float D = 3.f, F = 0.45f * r.h * view.zoom;
    const Vec3 cam (0.f, 0.f, D);
    Vec3 l (-0.35f, 0.55f, 1.f);
    l = l * (1.f / l.length());
    auto toView = [&] (const Vec3& p) { return rotate ((p - mid) * (1.f / rad)); };
    auto project = [&] (const Vec3& q) {
        const float k = D / std::max (0.2f, D - q.z);
        return Px {r.w * 0.5f + q.x * k * F, r.h * 0.5f - q.y * k * F, q.z};
    };

    const int n = o.visible < 0 ? (int) sc.objs.size() : std::min ((int) sc.objs.size(), o.visible);
    for (int i = 0; i < n; ++i)
    {
        const Obj& ob = sc.objs[i];
        float swell = 1.f;
        if (o.audio && o.audioN > 0 && o.pulse > 0.f) swell = 1.f + o.pulse * std::fabs (o.audio[(o.audioHead + i) % o.audioN]);
        const Vec3 c = ob.centre();
        auto world = [&] (const Vec3& local) { return c + (ob.mat.apply (local) - c) * swell; };

        const Vec3 qc = toView (c);
        const Px pc = project (qc);
        Rgb col = ob.rgb;
        if (art && o.skin > 0.f)
        {
            float u, v;
            coverUv (*o.art, r.w, r.h, pc.x, pc.y, u, v);
            col = lerp (col, o.art->sample (o.artFrame, u, v), clamp01 (o.skin));
        }
        const float gain = 1.f + 0.5f * (swell - 1.f), alpha = clamp01 (ob.hsv.a);
        const float sizeView = ob.mat.size() * swell / rad;

        switch (ob.kind)
        {
            case P_SPHERE:
                r.ball (pc, 0.5f * sizeView * F * D / std::max (0.2f, D - qc.z), 0.5f * sizeView, col, alpha, l, gain);
                break;
            case P_DOT:
                r.ball (pc, 1.2f, 0.f, col, alpha, l, 1.3f);
                break;
            case P_LINE:
                r.line (project (toView (world (Vec3 (0, 0.5f, 0.5f)))), project (toView (world (Vec3 (1, 0.5f, 0.5f)))), col, alpha);
                break;
            case P_TRIANGLE:
            {
                const auto& t = sc.tris[ob.tri];
                const Vec3 a = toView (world (t[0])), b = toView (world (t[1])), d = toView (world (t[2]));
                Vec3 nrm = (b - a).cross (d - a);
                const float len = nrm.length();
                if (len < 1e-12f) break;
                const float s = gain * (0.25f + 0.75f * std::fabs (nrm.dot (l) / len));
                Rgb sh; sh.r = clamp01 (col.r * s); sh.g = clamp01 (col.g * s); sh.b = clamp01 (col.b * s);
                r.tri (project (a), project (b), project (d), sh, alpha);
                break;
            }
            default:   // box, grid, cylinder, mesh
            {
                Vec3 q[8];
                Px p[8];
                for (int k = 0; k < 8; ++k)
                {
                    q[k] = toView (world (Vec3 ((float) (k & 1), (float) ((k >> 1) & 1), (float) ((k >> 2) & 1))));
                    p[k] = project (q[k]);
                }
                if (ob.kind == P_GRID)
                {
                    for (int k = 0; k < 8; ++k)
                        for (int bit = 0; bit < 3; ++bit)
                            if (! (k & (1 << bit))) r.line (p[k], p[k | (1 << bit)], col, alpha);
                    break;
                }
                // outward normals: cofactor(M) * axis, sign-corrected for mirrors
                const float (&m)[3][3] = ob.mat.m;
                const float cof[3][3] = {
                    {m[1][1] * m[2][2] - m[1][2] * m[2][1], -(m[1][0] * m[2][2] - m[1][2] * m[2][0]), m[1][0] * m[2][1] - m[1][1] * m[2][0]},
                    {-(m[0][1] * m[2][2] - m[0][2] * m[2][1]), m[0][0] * m[2][2] - m[0][2] * m[2][0], -(m[0][0] * m[2][1] - m[0][1] * m[2][0])},
                    {m[0][1] * m[1][2] - m[0][2] * m[1][1], -(m[0][0] * m[1][2] - m[0][2] * m[1][0]), m[0][0] * m[1][1] - m[0][1] * m[1][0]}};
                const float sgn = ob.mat.det() >= 0.f ? 1.f : -1.f;
                for (int axis = 0; axis < 3; ++axis)
                    for (int side = 0; side < 2; ++side)
                    {
                        const float dir = (side ? 1.f : -1.f) * sgn;
                        Vec3 nrm = rotate (Vec3 (cof[0][axis] * dir, cof[1][axis] * dir, cof[2][axis] * dir));
                        const float len = nrm.length();
                        if (len < 1e-12f) continue;
                        nrm = nrm * (1.f / len);
                        const int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3, base = side << axis;
                        const int k0 = base, k1 = base | (1 << a1), k2 = base | (1 << a1) | (1 << a2), k3 = base | (1 << a2);
                        const Vec3 fc = (q[k0] + q[k2]) * 0.5f;
                        if (nrm.dot (cam - fc) <= 0.f) continue;   // faces away from the camera
                        const float s = gain * (0.25f + 0.75f * std::max (0.f, nrm.dot (l)));
                        Rgb sh; sh.r = clamp01 (col.r * s); sh.g = clamp01 (col.g * s); sh.b = clamp01 (col.b * s);
                        r.tri (p[k0], p[k1], p[k2], sh, alpha);
                        r.tri (p[k0], p[k2], p[k3], sh, alpha);
                    }
            }
        }
    }

    if (o.highlight >= 0 && o.highlight < n)
    {
        const Obj& ob = sc.objs[o.highlight];
        Rgb lime; lime.r = 0xc8 / 255.f; lime.g = 1.f; lime.b = 0x3a / 255.f;
        Px p[8];
        for (int k = 0; k < 8; ++k) p[k] = project (toView (ob.mat.apply (Vec3 ((float) (k & 1), (float) ((k >> 1) & 1), (float) ((k >> 2) & 1)))));
        for (int k = 0; k < 8; ++k)
            for (int bit = 0; bit < 3; ++bit)
                if (! (k & (1 << bit))) r.line (p[k], p[k | (1 << bit)], lime, 1.f, false);
    }
}

}   // namespace ss
