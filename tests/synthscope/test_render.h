// test_render.h: the CPU rasterizer draws what the camera should see.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Render.h"
#include "check.h"
using namespace ss;

static const uint8_t* pix (const Raster& r, int x, int y) { return &r.rgba[((size_t) y * r.w + x) * 4]; }

inline void testRender()
{
    Raster r;
    r.resize (64, 64);
    const Rgb black;
    Rgb bg; bg.r = bg.g = bg.b = 0.f;
    View v;
    RenderOpts o;

    renderScene (build (parse ("box"), 1, nullptr), v, o, r, bg);
    const float front = 0.5f / (std::sqrt (3.f) * 0.5f);
    check (pix (r, 32, 32)[0] > 0, "a box covers the centre");
    check (near (r.z[32 * 64 + 32], front, 0.02), "the front face wins the depth test", r.z[32 * 64 + 32]);
    check (pix (r, 1, 1)[0] == 0, "the corner stays background");

    renderScene (build (parse ("{ fx } box"), 1, nullptr), v, o, r, bg);
    check (near (r.z[32 * 64 + 32], front, 0.02), "a mirrored box still shows its front", r.z[32 * 64 + 32]);

    renderScene (build (parse ("{ color red z 1 } box\n{ color blue } box\n"), 1, nullptr), v, o, r, bg);
    check (pix (r, 32, 32)[0] > 60 && pix (r, 32, 32)[2] < 10, "the nearer box hides the farther", pix (r, 32, 32)[0]);

    ArtSheet art;
    art.w = 2; art.h = 2; art.cols = 1; art.rows = 1; art.frames = 1;
    art.rgba = {255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255};
    o.art = &art;
    o.artMix = 1.f;
    renderScene (build (parse ("{ color white } box"), 1, nullptr), v, o, r, bg);
    check (pix (r, 1, 1)[0] == 255 && pix (r, 1, 1)[1] == 0, "backdrop fills the screen");
    check (pix (r, 32, 32)[1] > 60, "without skin the box keeps its colour", pix (r, 32, 32)[1]);
    o.skin = 1.f;
    renderScene (build (parse ("{ color white } box"), 1, nullptr), v, o, r, bg);
    check (pix (r, 32, 32)[0] > 60 && pix (r, 32, 32)[1] < 5, "skin paints the box with the art", pix (r, 32, 32)[1]);

    RenderOpts h;
    h.highlight = 0;
    renderScene (build (parse ("{ color blue } box"), 1, nullptr), v, h, r, bg);
    int lime = 0;
    for (int i = 0; i < 64 * 64; ++i) lime += r.rgba[i * 4] == 0xc8 && r.rgba[i * 4 + 1] == 255;
    check (lime > 20, "the sequencer's object is outlined", lime);

    RenderOpts none;
    none.visible = 0;
    renderScene (build (parse ("box"), 1, nullptr), v, none, r, bg);
    check (pix (r, 32, 32)[0] == 0, "GROW 0 draws nothing");

    v.yaw = 45.f; v.pitch = 30.f;
    renderScene (build (parse ("{ s 1 1 0 } box\nsphere\n{ x 2 } grid\n{ y 2 } line\ndot\ntriangle[0,0,0;1,0,0;0,1,0]\n"), 1, nullptr), v, RenderOpts(), r, bg);
    int lit = 0;
    for (int i = 0; i < 64 * 64; ++i) lit += r.rgba[i * 4] > 0;
    check (lit > 50, "every primitive kind renders without trouble", lit);
    (void) black;
}
