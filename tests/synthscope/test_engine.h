// test_engine.h: the EisenScript engine, the art sheet and the voltage walk.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Art.h"
#include "check.h"
using namespace ss;
#include <dirent.h>
#include <fstream>
#include <sstream>

static Scene run (const std::string& src, uint32_t seed = 1, const float* sl = nullptr) { return build (parse (src), seed, sl); }

static int parseErrorLine (const std::string& src)
{
    try { parse (src); } catch (const ParseError& e) { return e.line; }
    return -1;
}

inline void testCore()
{
    const Vec3 c (0.5f, 0.5f, 0.5f);
    for (int axis = 0; axis < 3; ++axis)
    {
        const Vec3 p = Mat::rot (axis, 90.f).apply (c);
        check (near (p.x, 0.5, 1e-5) && near (p.y, 0.5, 1e-5) && near (p.z, 0.5, 1e-5), "rotation keeps the cube centre fixed");
    }
    const Vec3 q = (Mat::translate (1, 0, 0) * Mat::scale (0.5f, 0.5f, 0.5f)).apply (Vec3 (0, 0, 0));
    check (near (q.x, 1.25, 1e-5) && near (q.y, 0.25, 1e-5), "translate then scale about the centre", q.x);
    check (near (Mat::scale (2, 2, 2).size(), 2.0, 1e-5), "size is the linear scale");

    Hsv h;
    check (parseColor ("#f00", h) && near (h.h, 0) && near (h.s, 1) && near (h.v, 1), "#f00 is red");
    check (parseColor ("white", h) && near (h.s, 0) && near (h.v, 1), "white");
    check (! parseColor ("notacolour", h), "unknown name refused");
    Rng a (7), b (7);
    bool same = true;
    for (int i = 0; i < 100; ++i) same = same && a.next() == b.next();
    check (same, "rng: same seed, same sequence");
}

inline void testParse()
{
    // whole-word defines: r1 becomes box, r12 is left alone
    const Scene s = run ("#define r1 box\nrule r12 { sphere }\nr12\nr1\n");
    check (s.objs.size() == 2 && s.objs[0].kind == P_BOX && s.objs[1].kind == P_SPHERE, "defines replace whole words only", (double) s.objs.size());
    const Script sl = parse ("#define ang 30 (float:0-90)\n#define n 3 (int:1-10)\n{ rz ang } box\n");
    check (sl.sliders.size() == 2 && sl.sliders[0].name == "ang" && near (sl.sliders[0].def, 30) && near (sl.sliders[0].hi, 90) && sl.sliders[1].isInt, "slider defines parse");
    const float vals[2] = {0.f, 5.f};
    check (run ("#define ang 30 (float:0-90)\n#define n 3 (int:1-10)\nn * { x 1 } box\n", 1, vals).objs.size() == 5, "slider value reaches a loop count");
    check (run ("#define n 3 (int:1-10)\nn * { x 1 } box\n").objs.size() == 3, "slider default used without values");
    check (run ("#define shrink s 0.5\n{ shrink } box\n").objs[0].mat.size() < 0.51f, "multi-word define");

    const Script r = parse ("r\nrule r w 3 md 4 > q { box }\nrule q { }\n");
    check (near (r.rules[0][0].weight.v, 3) && near (r.rules[0][0].maxdepth.v, 4) && r.rules[0][0].retire == 1, "rule header: weight, maxdepth, retire");
    check (parse ("set raytracer::shadows false\nset translation [0 0 -20]\nbox\n").top.size() == 1, "unknown set keys ignored");
    check (near (run ("{ s 1/3 } box").objs[0].mat.size(), 1.0 / 3, 1e-5), "fractions as numbers");
    check (run ("/* a\nb */ box // c\n").objs.size() == 1, "comments");
    check (run ("box::shiny").objs.size() == 1, "tags ignored");
    check (run ("triangle[0,0,0;1,0,0;0,1,0]").objs[0].kind == P_TRIANGLE, "triangle primitive");

    check (parseErrorLine ("box\n\nrule r {\n  { q 1 } box\n}\n") == 4, "unknown transform: error on its line", parseErrorLine ("box\n\nrule r {\n  { q 1 } box\n}\n"));
    check (parseErrorLine ("\nfoo\n") == 2, "missing rule: error where it is called");
    check (parseErrorLine ("rule r { box\n") >= 1, "unclosed rule is an error");
}

inline void testBuild()
{
    const Scene one = run ("1 * { x 1 } box"), bare = run ("{ x 1 } box");
    check (near (one.objs[0].centre().x, 1.5) && near (bare.objs[0].centre().x, 1.5), "1 * {x 1} == {x 1}", one.objs[0].centre().x);
    const Scene three = run ("3 * { x 1 } box");
    check (three.objs.size() == 3 && near (three.objs[0].centre().x, 1.5) && near (three.objs[2].centre().x, 3.5), "3 * {x 1}: x = 1, 2, 3");
    check (run ("3 * { x 1 } 2 * { y 1 } box").objs.size() == 6, "nested loops multiply");

    int boxes = 0;
    const int N = 4000;
    for (int seed = 0; seed < N; ++seed) boxes += run ("r\nrule r w 3 { box }\nrule r w 1 { sphere }\n", (uint32_t) seed).objs[0].kind == P_BOX;
    check (std::abs (boxes / (double) N - 0.75) < 0.03, "weights 3:1 pick ~75 %", boxes / (double) N);

    const Scene md = run ("R\nrule R md 4 > Q { box { x 1 } R }\nrule Q { sphere }\n");
    int b = 0, sp = 0;
    for (auto& o : md.objs) { b += o.kind == P_BOX; sp += o.kind == P_SPHERE; }
    check (b == 4 && sp == 1, "rule maxdepth 4 then retire", b * 10 + sp);
    check (run ("R\nrule R md 3 { box { x 1 } R }\n").objs.size() == 3, "rule maxdepth without retire stops");
    check (run ("set maxdepth 10\nR\nrule R { box { x 1 } R }\n").objs.size() == 10, "global maxdepth = rule expansions deep", (double) run ("set maxdepth 10\nR\nrule R { box { x 1 } R }\n").objs.size());

    check (run ("set maxobjects 100\nR\nrule R { box { x 1 } R }\n").objs.size() == 100, "maxobjects caps the output");
    check (run ("set maxobjects 10\n1000 * { x 1 } box\n").objs.size() == 10, "maxobjects caps a loop");
    check (run ("set maxobjects 999999\n200 * { x 1 } 200 * { y 1 } box\n").objs.size() == 20000, "hard cap 20000");
    check (run ("set minsize 0.3\nR\nrule R { box { s 0.5 } R }\n").objs.size() == 2, "minsize stops shrinking branches");

    const char* rnd = "set colorpool randomhue\nr\nrule r { 50 * { x 1 color random } box  { y 1 } r2 }\nrule r2 { box }\nrule r2 { sphere }\n";
    const Scene a1 = run (rnd, 11), a2 = run (rnd, 11), b1 = run (rnd, 12);
    bool same = a1.objs.size() == a2.objs.size(), diff = false;
    for (size_t i = 0; same && i < a1.objs.size(); ++i) same = a1.objs[i].hsv.h == a2.objs[i].hsv.h && a1.objs[i].kind == a2.objs[i].kind;
    for (size_t i = 0; i < std::min (a1.objs.size(), b1.objs.size()); ++i) diff = diff || a1.objs[i].hsv.h != b1.objs[i].hsv.h;
    check (same, "same seed, same structure");
    check (diff, "different seed, different structure");

    const Scene menger = run ("R\nrule R md 3 > c { { s 1/3 x -1 y -1 } R { s 1/3 x 1 y -1 } R { s 1/3 x -1 y 1 } R { s 1/3 x 1 y 1 } R }\nrule c { box }\n");
    check (menger.objs.size() == 64 && near (menger.lo.x, 0, 1e-4) && near (menger.hi.x, 1, 1e-4), "4-way subdivision stays inside the unit cube", (double) menger.objs.size());
    const Scene runaway = run ("R\nrule R { R R }\n");
    check (runaway.objs.empty() && runaway.truncated, "a rule that never draws builds nothing, ends, and says so");

    const Scene col = run ("{ color #00f b 0.5 h 120 } box");
    check (near (col.objs[0].hsv.h, 0, 0.01) && near (col.objs[0].hsv.v, 0.5), "colour ops: set, brightness, hue wraps", col.objs[0].hsv.h);
}

inline void testArt()
{
    ArtSheet a;
    a.w = 4; a.h = 2; a.cols = 2; a.rows = 1; a.frames = 2;
    a.rgba.assign (4 * 2 * 4, 0);
    for (int y = 0; y < 2; ++y)
        for (int x = 2; x < 4; ++x) a.rgba[(y * 4 + x) * 4] = 255;   // frame 1 is red, frame 0 black
    check (a.ok(), "sheet ok");
    check (a.frameAt (0.0) == 0 && a.frameAt (0.999) == 1 && a.frameAt (1.25) == 0, "frameAt wraps the loop");
    check (near (a.sample (1, 0.5f, 0.5f).r, 1) && near (a.sample (0, 0.5f, 0.5f).r, 0), "sample reads its own frame's cell");
    check (near (a.sample (1, 2.f, -1.f).r, 1), "sample clamps inside the cell");

    const Scene s = run ("box\n{ x 9 } box\n{ h 180 } box\n");
    const Volts v0 = walk (s.objs[0], s), v1 = walk (s.objs[1], s), v2 = walk (s.objs[2], s);
    check (near (v0.x, -4.5, 1e-4) && near (v1.x, 4.5, 1e-4), "walk: x across the bounds", v0.x);
    check (near (v2.hue, 5.0, 1e-4), "walk: hue 180 is 5 V");
    check (near (v0.size, 10.0, 1e-4), "walk: largest object is 10 V");
}

inline void testPresets()
{
    DIR* d = opendir ("../res/synthscope");
    int n = 0;
    if (d)
    {
        while (dirent* e = readdir (d))
        {
            const std::string f = e->d_name;
            if (f.size() < 4 || f.substr (f.size() - 3) != ".es") continue;
            std::ifstream in ("../res/synthscope/" + f);
            std::stringstream ss;
            ss << in.rdbuf();
            size_t objs = 0;
            try { objs = build (parse (ss.str()), 1, nullptr).objs.size(); } catch (const ParseError& err) { std::printf ("     %s: %s\n", f.c_str(), err.what()); }
            check (objs > 0, ("preset builds: " + f).c_str(), (double) objs);
            ++n;
        }
        closedir (d);
    }
    check (n >= 8, "eight presets", n);
}
