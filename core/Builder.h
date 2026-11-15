// Builder.h: grows a parsed Script into a Scene of primitives. No Rack dependency.
// Breadth-first by generation: global maxdepth = generations, maxobjects caps the output.
// Same-name rules are picked by weight with the seeded Rng; a rule's own maxdepth counts how often that
// definition already sits in the call chain, and past it the retire rule runs in its place.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Eisen.h"
#include "Rng.h"
#include <algorithm>
#include <utility>

namespace ss {

struct Obj
{
    int kind = P_BOX;
    Mat mat;
    Hsv hsv;
    Rgb rgb;
    int gen = 0;
    int tri = -1;   // P_TRIANGLE: index into Scene::tris
    Vec3 centre() const { return mat.apply (Vec3 (0.5f, 0.5f, 0.5f)); }
};

struct Scene
{
    std::vector<Obj> objs;
    std::vector<std::vector<Vec3>> tris;
    Vec3 lo, hi;
    float maxSize = 1.f;
    bool hasBackground = false;
    Rgb background;
    bool truncated = false;   // the pending queue hit its cap: the script grows faster than it draws
};

namespace eisen {

constexpr int MAX_OBJECTS = 20000;
constexpr size_t MAX_PENDING = 400000;
constexpr long MAX_CALLS = 2000000;

struct State
{
    Mat m;
    Hsv c;
    std::vector<std::pair<int, int>> depth;   // (rule definition key, times entered)
};

struct Builder
{
    const Script& sc;
    const float* sl;
    Rng rng;
    Scene out;
    int maxObjects, gen = 0;
    long calls = 0;
    struct Item { int name; State st; };
    std::vector<Item> next;

    Builder (const Script& s, uint32_t seed, const float* sliders) : sc (s), sl (sliders), rng (seed)
    {
        maxObjects = std::max (1, std::min (MAX_OBJECTS, (int) s.set.maxobjects.eval (sliders)));
    }

    bool full() const { return (int) out.objs.size() >= maxObjects; }

    Hsv randomColor (float alpha)
    {
        Hsv c;
        switch (sc.set.pool)
        {
            case POOL_RGB: { const float r = rng.uniform(), g = rng.uniform(), b = rng.uniform(); c = fromRgb (r, g, b); break; }
            case POOL_GREY: { const float g = rng.uniform(); c = fromRgb (g, g, g); break; }
            case POOL_LIST:
                if (! sc.set.poolList.empty()) { c = sc.set.poolList[std::min (sc.set.poolList.size() - 1, (size_t) (rng.uniform() * sc.set.poolList.size()))]; break; }
                // fall through
            default: c.h = rng.uniform() * 360.f; c.s = 1.f; c.v = 1.f; break;
        }
        c.a = alpha;
        return c;
    }

    void applyOps (const std::vector<Op>& ops, State& s)
    {
        for (const Op& o : ops)
        {
            const float v = o.n[0].eval (sl);
            switch (o.kind)
            {
                case OP_X: s.m = s.m * Mat::translate (v, 0, 0); break;
                case OP_Y: s.m = s.m * Mat::translate (0, v, 0); break;
                case OP_Z: s.m = s.m * Mat::translate (0, 0, v); break;
                case OP_RX: s.m = s.m * Mat::rot (0, v); break;
                case OP_RY: s.m = s.m * Mat::rot (1, v); break;
                case OP_RZ: s.m = s.m * Mat::rot (2, v); break;
                case OP_S: s.m = s.m * (o.count == 3 ? Mat::scale (v, o.n[1].eval (sl), o.n[2].eval (sl)) : Mat::scale (v, v, v)); break;
                case OP_FX: s.m = s.m * Mat::mirror (0); break;
                case OP_FY: s.m = s.m * Mat::mirror (1); break;
                case OP_FZ: s.m = s.m * Mat::mirror (2); break;
                case OP_M: { float l[9]; for (int i = 0; i < 9; ++i) l[i] = o.n[i].eval (sl); s.m = s.m * Mat::aboutCentre (l); break; }
                case OP_HUE: s.c.h = wrapHue (s.c.h + v); break;
                case OP_SAT: s.c.s = clamp01 (s.c.s * v); break;
                case OP_BRI: s.c.v = clamp01 (s.c.v * v); break;
                case OP_ALPHA: s.c.a = clamp01 (s.c.a * v); break;
                case OP_COLOR: { const float a = s.c.a; s.c = o.color; s.c.a = a; break; }
                case OP_RANDOM: s.c = randomColor (s.c.a); break;
                case OP_BLEND: s.c = blend (s.c, o.color, clamp01 (v)); break;
            }
        }
    }

    bool inSize (const Mat& m) const
    {
        const float z = m.size();
        return z >= sc.set.minsize && z <= sc.set.maxsize;
    }

    void emit (const Action& a, const State& s)
    {
        if (full() || ! inSize (s.m)) return;
        Obj o;
        o.kind = a.prim;
        o.mat = s.m;
        o.hsv = s.c;
        o.rgb = toRgb (s.c);
        o.gen = gen;
        if (a.prim == P_TRIANGLE) { o.tri = (int) out.tris.size(); out.tris.push_back (sc.tris[a.tri]); }
        out.objs.push_back (o);
    }

    // loops multiply out: level k runs its ops 1..N times on top of the levels before it
    void loops (const Action& a, size_t level, const State& s)
    {
        if (full()) return;
        if (level == a.loops.size())
        {
            if (a.prim != P_NONE) emit (a, s);
            else if (inSize (s.m))
            {
                if (next.size() < MAX_PENDING) next.push_back ({a.rule, s});
                else out.truncated = true;
            }
            return;
        }
        const Loop& lp = a.loops[level];
        const int n = std::max (0, (int) std::lround (lp.count.eval (sl)));
        State c = s;
        for (int k = 0; k < n && ! full(); ++k)
        {
            applyOps (lp.ops, c);
            loops (a, level + 1, c);
        }
    }

    void call (int name, const State& st, int retireDepth = 0)
    {
        const auto& defs = sc.rules[name];
        size_t ix = 0;
        if (defs.size() > 1)
        {
            float total = 0.f;
            for (const Rule& r : defs) total += std::max (0.f, r.weight.eval (sl));
            float pick = rng.uniform() * total;
            for (ix = 0; ix + 1 < defs.size(); ++ix)
            {
                pick -= std::max (0.f, defs[ix].weight.eval (sl));
                if (pick < 0.f) break;
            }
        }
        const Rule& r = defs[ix];
        State s = st;
        const int md = (int) std::lround (r.maxdepth.eval (sl));
        if (r.maxdepth.eval (sl) >= 0.f)
        {
            const int key = (name << 12) | (int) ix;
            auto it = std::find_if (s.depth.begin(), s.depth.end(), [key] (const std::pair<int, int>& p) { return p.first == key; });
            const int count = it == s.depth.end() ? 0 : it->second;
            if (count >= md)
            {
                if (r.retire >= 0 && retireDepth < 8)
                {
                    if (it != s.depth.end()) s.depth.erase (it);
                    call (r.retire, s, retireDepth + 1);
                }
                return;
            }
            if (it == s.depth.end()) s.depth.push_back ({key, 1});
            else ++it->second;
        }
        for (const Action& a : r.body) loops (a, 0, s);
    }

    void run()
    {
        const int maxDepth = std::max (0, (int) std::lround (sc.set.maxdepth.eval (sl)));
        State root;
        for (const Action& a : sc.top) loops (a, 0, root);
        std::vector<Item> cur;
        while (! next.empty() && gen < maxDepth && ! full())
        {
            cur.swap (next);
            next.clear();
            ++gen;
            for (const Item& it : cur)
            {
                if (full()) break;
                if (++calls > MAX_CALLS) { out.truncated = true; next.clear(); break; }   // ponytail: flat budget keeps a runaway script under ~0.5 s
                call (it.name, it.st);
            }
        }
        finish();
    }

    void finish()
    {
        out.hasBackground = sc.set.hasBackground;
        out.background = toRgb (sc.set.background);
        if (out.objs.empty()) { out.lo = Vec3 (0, 0, 0); out.hi = Vec3 (1, 1, 1); return; }
        Vec3 lo (1e30f, 1e30f, 1e30f), hi (-1e30f, -1e30f, -1e30f);
        float mx = 0.f;
        for (const Obj& o : out.objs)
        {
            for (int c = 0; c < 8; ++c)
            {
                const Vec3 p = o.mat.apply (Vec3 ((float) (c & 1), (float) ((c >> 1) & 1), (float) ((c >> 2) & 1)));
                lo = Vec3 (std::min (lo.x, p.x), std::min (lo.y, p.y), std::min (lo.z, p.z));
                hi = Vec3 (std::max (hi.x, p.x), std::max (hi.y, p.y), std::max (hi.z, p.z));
            }
            mx = std::max (mx, o.mat.size());
        }
        out.lo = lo;
        out.hi = hi;
        out.maxSize = mx > 0.f ? mx : 1.f;
    }
};

}   // namespace eisen

// sliders: one value per Script::sliders entry, already in lo..hi (may be null when there are none)
inline Scene build (const Script& sc, uint32_t seed, const float* sliders)
{
    std::vector<float> fallback;
    if (! sliders && ! sc.sliders.empty())
    {
        for (const Slider& s : sc.sliders) fallback.push_back (s.def);
        sliders = fallback.data();
    }
    eisen::Builder b (sc, seed, sliders);
    b.run();
    return b.out;
}

}   // namespace ss
