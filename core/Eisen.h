// Eisen.h: EisenScript preprocessor, tokenizer and parser. No Rack dependency.
// Clean-room: written from the language's observable behaviour, not from Structure Synth's (GPL) source.
// parse() turns script text into a Script, or throws ParseError with the line it stopped on.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "Color.h"
#include "Math3.h"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ss {

struct ParseError : std::runtime_error
{
    int line;
    ParseError (int l, const std::string& m) : std::runtime_error ("line " + std::to_string (l) + ": " + m), line (l) {}
};

// A number in the script, or a slider define (s >= 0) the builder looks up, times k (for "-name").
struct Num
{
    float v = 0.f;
    int s = -1;
    float k = 1.f;
    Num() {}
    Num (float x) : v (x) {}
    float eval (const float* sl) const { return s < 0 || ! sl ? v : k * sl[s]; }
};

enum OpKind { OP_X, OP_Y, OP_Z, OP_RX, OP_RY, OP_RZ, OP_S, OP_FX, OP_FY, OP_FZ, OP_M, OP_HUE, OP_SAT, OP_BRI, OP_ALPHA, OP_COLOR, OP_RANDOM, OP_BLEND };
enum Prim { P_BOX, P_SPHERE, P_DOT, P_LINE, P_GRID, P_CYLINDER, P_MESH, P_TRIANGLE, P_NONE };

struct Op
{
    OpKind kind;
    Num n[9];
    int count = 1;    // OP_S: 1 or 3 numbers
    Hsv color;        // OP_COLOR, OP_BLEND
};

struct Loop { Num count = Num (1.f); std::vector<Op> ops; };

struct Action
{
    std::vector<Loop> loops;
    int prim = P_NONE;   // a primitive, or
    int rule = -1;       // an index into Script::names
    int tri = -1;        // P_TRIANGLE: index into Script::tris
    int line = 0;
};

struct Rule
{
    Num weight = Num (1.f);
    Num maxdepth = Num (-1.f);   // < 0: none
    int retire = -1;             // name index, or -1
    std::vector<Action> body;
};

struct Slider { std::string name; float def = 0, lo = 0, hi = 1; bool isInt = false; };

enum Pool { POOL_HUE, POOL_RGB, POOL_GREY, POOL_LIST };

struct Settings
{
    Num maxdepth = Num (1000.f);
    Num maxobjects = Num (20000.f);
    float minsize = 0.f, maxsize = 1e9f;
    bool hasSeed = false;
    uint32_t seed = 0;
    bool hasBackground = false;
    Hsv background;
    int pool = POOL_HUE;
    std::vector<Hsv> poolList;
};

struct Script
{
    std::vector<std::string> names;                 // rule names, by index
    std::vector<std::vector<Rule>> rules;           // same index: every definition of that name
    std::vector<Action> top;
    Settings set;
    std::vector<Slider> sliders;
    std::vector<std::vector<Vec3>> tris;            // triangle[...] corner lists (3 each)
};

namespace eisen {

struct Tok { std::string s; int line; };

inline std::string lower (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; }

inline bool toFloat (const std::string& w, float& out)
{
    if (w.empty()) return false;
    const size_t slash = w.find ('/');
    if (slash != std::string::npos)
    {
        float a, b;
        if (! toFloat (w.substr (0, slash), a) || ! toFloat (w.substr (slash + 1), b) || b == 0.f) return false;
        out = a / b;
        return true;
    }
    char* end = nullptr;
    const double d = std::strtod (w.c_str(), &end);
    if (end != w.c_str() + w.size()) return false;
    out = (float) d;
    return true;
}

// Comments out, newlines kept, so line numbers survive.
inline std::string stripComments (const std::string& src)
{
    std::string o;
    o.reserve (src.size());
    for (size_t i = 0; i < src.size(); ++i)
    {
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/')
        {
            while (i < src.size() && src[i] != '\n') ++i;
            if (i < src.size()) o += '\n';
        }
        else if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*')
        {
            i += 2;
            while (i < src.size() && ! (src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/')) { if (src[i] == '\n') o += '\n'; ++i; }
            ++i;
        }
        else o += src[i];
    }
    return o;
}

inline void splitWords (const std::string& line, int ln, std::vector<Tok>& out)
{
    size_t i = 0;
    while (i < line.size())
    {
        const char c = line[i];
        if (std::isspace ((unsigned char) c)) { ++i; continue; }
        if (std::string ("{}*>[],").find (c) != std::string::npos) { out.push_back ({std::string (1, c), ln}); ++i; continue; }
        std::string w;
        while (i < line.size() && ! std::isspace ((unsigned char) line[i]) && std::string ("{}*>[],").find (line[i]) == std::string::npos) w += line[i++];
        if (lower (w) == "triangle" && i < line.size() && line[i] == '[')   // triangle[x,y,z;x,y,z;x,y,z] is one word
            while (i < line.size()) { w += line[i]; if (line[i++] == ']') break; }
        out.push_back ({w, ln});
    }
}

struct Parser
{
    std::vector<Tok> t;
    size_t p = 0;
    Script sc;
    std::map<std::string, int> nameIx;
    std::vector<int> firstUse;   // line a name was first called on, by index

    int lastLine() const { return t.empty() ? 1 : t.back().line; }
    bool end() const { return p >= t.size(); }
    const std::string& peek (size_t k = 0) const { static const std::string none; return p + k < t.size() ? t[p + k].s : none; }
    int line() const { return p < t.size() ? t[p].line : lastLine(); }
    std::string next()
    {
        if (end()) throw ParseError (lastLine(), "unexpected end of script");
        return t[p++].s;
    }
    void expect (const char* s)
    {
        const int l = line();
        const std::string w = end() ? "end of script" : t[p].s;
        if (w != s) throw ParseError (l, std::string ("expected '") + s + "', got '" + w + "'");
        ++p;
    }

    bool isNum (const std::string& w) const { float f; return toFloat (w, f) || (! w.empty() && (w[0] == '$' || (w.size() > 1 && w[0] == '-' && w[1] == '$'))); }

    Num num()
    {
        const int l = line();
        const std::string w = next();
        Num n;
        if (! w.empty() && w[0] == '$') { n.s = std::atoi (w.c_str() + 1); return n; }
        if (w.size() > 1 && w[0] == '-' && w[1] == '$') { n.s = std::atoi (w.c_str() + 2); n.k = -1.f; return n; }
        if (! toFloat (w, n.v)) throw ParseError (l, "expected a number, got '" + w + "'");
        return n;
    }

    int nameIndex (const std::string& name, int l)
    {
        auto it = nameIx.find (name);
        if (it != nameIx.end()) return it->second;
        const int ix = (int) sc.names.size();
        nameIx[name] = ix;
        sc.names.push_back (name);
        sc.rules.emplace_back();
        firstUse.push_back (l);
        return ix;
    }

    Hsv colorWord (int l)
    {
        const std::string w = next();
        Hsv c;
        if (! parseColor (w, c)) throw ParseError (l, "unknown colour '" + w + "'");
        return c;
    }

    std::vector<Op> ops()
    {
        expect ("{");
        std::vector<Op> v;
        while (peek() != "}")
        {
            const int l = line();
            const std::string w = lower (next());
            Op o;
            if (w == "x") o.kind = OP_X;
            else if (w == "y") o.kind = OP_Y;
            else if (w == "z") o.kind = OP_Z;
            else if (w == "rx") o.kind = OP_RX;
            else if (w == "ry") o.kind = OP_RY;
            else if (w == "rz") o.kind = OP_RZ;
            else if (w == "fx") o.kind = OP_FX;
            else if (w == "fy") o.kind = OP_FY;
            else if (w == "fz") o.kind = OP_FZ;
            else if (w == "s") o.kind = OP_S;
            else if (w == "m") o.kind = OP_M;
            else if (w == "h" || w == "hue") o.kind = OP_HUE;
            else if (w == "sat") o.kind = OP_SAT;
            else if (w == "b" || w == "brightness") o.kind = OP_BRI;
            else if (w == "a" || w == "alpha") o.kind = OP_ALPHA;
            else if (w == "color")
            {
                if (lower (peek()) == "random") { next(); o.kind = OP_RANDOM; }
                else { o.kind = OP_COLOR; o.color = colorWord (l); }
                v.push_back (o);
                continue;
            }
            else if (w == "blend") { o.kind = OP_BLEND; o.color = colorWord (l); o.n[0] = num(); v.push_back (o); continue; }
            else throw ParseError (l, "unknown transformation '" + w + "'");

            if (o.kind == OP_FX || o.kind == OP_FY || o.kind == OP_FZ) { v.push_back (o); continue; }
            if (o.kind == OP_M) { for (int i = 0; i < 9; ++i) o.n[i] = num(); o.count = 9; v.push_back (o); continue; }
            o.n[0] = num();
            if (o.kind == OP_S && isNum (peek()) && isNum (peek (1))) { o.n[1] = num(); o.n[2] = num(); o.count = 3; }
            v.push_back (o);
        }
        expect ("}");
        return v;
    }

    Action action()
    {
        Action a;
        a.line = line();
        for (;;)
        {
            if (isNum (peek()) && peek (1) == "*")
            {
                Loop lp;
                lp.count = num();
                next();
                lp.ops = ops();
                a.loops.push_back (lp);
            }
            else if (peek() == "{") { Loop lp; lp.ops = ops(); a.loops.push_back (lp); }
            else break;
        }
        const int l = line();
        std::string w = next();
        if (w == "{" || w == "}" || w == "*" || w == ">" || w == "[" || w == "]" || w == "," || isNum (w))
            throw ParseError (l, "expected a rule or primitive name, got '" + w + "'");
        const size_t tag = w.find ("::");
        if (tag != std::string::npos) w = w.substr (0, tag);
        const std::string lw = lower (w);
        static const char* prims[] = {"box", "sphere", "dot", "line", "grid", "cylinder", "mesh"};
        for (int i = 0; i < 7; ++i)
            if (lw == prims[i]) { a.prim = i; return a; }
        if (lw.compare (0, 9, "triangle[") == 0)
        {
            std::vector<Vec3> pts;
            std::string body = w.substr (9, w.size() - 10), cur;
            float xyz[3];
            int k = 0;
            for (size_t i = 0; i <= body.size(); ++i)
            {
                const char c = i < body.size() ? body[i] : ';';
                if (c == ',' || c == ';')
                {
                    if (k > 2 || ! toFloat (cur, xyz[k])) throw ParseError (l, "bad triangle corners");
                    cur.clear();
                    ++k;
                    if (c == ';') { if (k != 3) throw ParseError (l, "bad triangle corners"); pts.push_back (Vec3 (xyz[0], xyz[1], xyz[2])); k = 0; }
                }
                else if (! std::isspace ((unsigned char) c)) cur += c;
            }
            if (pts.size() != 3) throw ParseError (l, "a triangle needs three corners");
            a.prim = P_TRIANGLE;
            a.tri = (int) sc.tris.size();
            sc.tris.push_back (pts);
            return a;
        }
        if (lw == "set" || lw == "rule") throw ParseError (l, "'" + w + "' is not allowed here");
        a.rule = nameIndex (w, l);
        return a;
    }

    void rule()
    {
        next();   // "rule"
        const int l = line();
        const std::string name = next();
        if (name == "{" || isNum (name)) throw ParseError (l, "rule needs a name");
        Rule r;
        while (peek() != "{")
        {
            const int ml = line();
            const std::string m = lower (next());
            if (m == "weight" || m == "w") r.weight = num();
            else if (m == "maxdepth" || m == "md")
            {
                r.maxdepth = num();
                if (peek() == ">") { next(); r.retire = nameIndex (next(), ml); }
            }
            else throw ParseError (ml, "unexpected '" + m + "' in rule header");
        }
        expect ("{");
        while (peek() != "}")
        {
            if (end()) throw ParseError (lastLine(), "rule '" + name + "' is missing its '}'");
            r.body.push_back (action());
        }
        expect ("}");
        sc.rules[nameIndex (name, l)].push_back (r);
    }

    void set()
    {
        const int l = line();
        next();   // "set"
        const std::string key = lower (next());
        std::vector<std::string> args;
        while (! end() && t[p].line == l) args.push_back (t[p++].s);
        auto arg = [&] () -> std::string { if (args.empty()) throw ParseError (l, "set " + key + " needs a value"); return args[0]; };
        auto number = [&] () { float f; if (! toFloat (arg(), f)) throw ParseError (l, "set " + key + " needs a number"); return f; };
        auto numOrSlider = [&] () { const size_t keep = p; t.insert (t.begin() + (long) p, Tok {arg(), l}); Num n = num(); p = keep; t.erase (t.begin() + (long) p); return n; };
        if (key == "maxdepth") sc.set.maxdepth = numOrSlider();
        else if (key == "maxobjects") sc.set.maxobjects = numOrSlider();
        else if (key == "minsize") sc.set.minsize = number();
        else if (key == "maxsize") sc.set.maxsize = number();
        else if (key == "seed") { if (lower (arg()) != "initial") { sc.set.hasSeed = true; sc.set.seed = (uint32_t) number(); } }
        else if (key == "background") { Hsv c; if (! parseColor (arg(), c)) throw ParseError (l, "unknown colour '" + arg() + "'"); sc.set.hasBackground = true; sc.set.background = c; }
        else if (key == "colorpool")
        {
            std::string all;
            for (auto& s : args) all += s;
            const std::string la = lower (all);
            if (la == "randomhue") sc.set.pool = POOL_HUE;
            else if (la == "randomrgb") sc.set.pool = POOL_RGB;
            else if (la == "greyscale" || la == "grayscale") sc.set.pool = POOL_GREY;
            else if (la.compare (0, 5, "list:") == 0)
            {
                sc.set.pool = POOL_LIST;
                sc.set.poolList.clear();
                std::string cur;
                for (size_t i = 5; i <= all.size(); ++i)
                    if (i == all.size() || all[i] == ',')
                    {
                        Hsv c;
                        if (! parseColor (cur, c)) throw ParseError (l, "unknown colour '" + cur + "' in colorpool");
                        sc.set.poolList.push_back (c);
                        cur.clear();
                    }
                    else cur += all[i];
            }
            // ponytail: image: pools fall back to random hue; the art bridge's SKIN knob is the image colouring here
        }
        // everything else (raytracer::*, translation, rotation, pivot, scale, perspective-angle, ...) is ignored
    }

    void run()
    {
        while (! end())
        {
            const std::string w = lower (peek());
            if (w == "set") set();
            else if (w == "rule") rule();
            else sc.top.push_back (action());
        }
        for (size_t i = 0; i < sc.names.size(); ++i)
            if (sc.rules[i].empty()) throw ParseError (firstUse[i], "no rule named '" + sc.names[i] + "'");
    }
};

}   // namespace eisen

// #define name value, and the slider form #define name value (float:lo-hi) / (int:lo-hi).
inline Script parse (const std::string& source)
{
    using namespace eisen;
    const std::string src = stripComments (source);
    std::map<std::string, std::vector<std::string>> defines;
    std::map<std::string, int> sliderIx;
    std::vector<Slider> sliders;
    std::vector<std::pair<std::string, int>> lines;
    {
        int ln = 1;
        size_t a = 0;
        for (size_t i = 0; i <= src.size(); ++i)
            if (i == src.size() || src[i] == '\n') { lines.push_back ({src.substr (a, i - a), ln++}); a = i + 1; }
    }
    for (auto& L : lines)
    {
        std::string s = L.first;
        const size_t f = s.find_first_not_of (" \t\r");
        if (f == std::string::npos || s.compare (f, 7, "#define") != 0) continue;
        std::vector<Tok> w;
        splitWords (s.substr (f + 7), L.second, w);
        if (w.empty()) throw ParseError (L.second, "#define needs a name");
        const std::string name = w[0].s;
        const size_t open = s.find ('(');
        if (open != std::string::npos && s.find (':', open) != std::string::npos)
        {
            const std::string spec = s.substr (open + 1, s.find (')', open) - open - 1);
            const size_t colon = spec.find (':');
            const std::string kind = lower (spec.substr (0, colon)), range = spec.substr (colon + 1);
            const size_t dash = range.find ('-', 1);   // skip a leading minus on lo
            Slider sl;
            sl.name = name;
            sl.isInt = kind == "int";
            if (w.size() < 2 || ! toFloat (w[1].s, sl.def) || dash == std::string::npos || ! toFloat (range.substr (0, dash), sl.lo) || ! toFloat (range.substr (dash + 1), sl.hi) || sl.hi <= sl.lo)
                throw ParseError (L.second, "bad slider define, expected: #define name value (float:lo-hi)");
            sliderIx[name] = (int) sliders.size();
            sliders.push_back (sl);
        }
        else
        {
            std::vector<std::string> body;
            for (size_t i = 1; i < w.size(); ++i) body.push_back (w[i].s);
            defines[name] = body;
        }
        L.first.clear();
    }

    Parser ps;
    for (auto& L : lines)
    {
        std::vector<Tok> raw;
        splitWords (L.first, L.second, raw);
        std::vector<Tok> out;
        // whole-token substitution; nested defines expand up to 16 deep
        std::function<void (const Tok&, int)> expand = [&] (const Tok& tk, int depth) {
            auto si = sliderIx.find (tk.s);
            if (si != sliderIx.end()) { out.push_back ({"$" + std::to_string (si->second), tk.line}); return; }
            if (tk.s.size() > 1 && tk.s[0] == '-' && sliderIx.count (tk.s.substr (1))) { out.push_back ({"-$" + std::to_string (sliderIx[tk.s.substr (1)]), tk.line}); return; }
            auto d = defines.find (tk.s);
            if (d == defines.end() || depth > 16) { out.push_back (tk); return; }
            for (auto& b : d->second) expand ({b, tk.line}, depth + 1);
        };
        for (auto& tk : raw) expand (tk, 0);
        ps.t.insert (ps.t.end(), out.begin(), out.end());
    }
    ps.run();
    ps.sc.sliders = sliders;
    return ps.sc;
}

}   // namespace ss
