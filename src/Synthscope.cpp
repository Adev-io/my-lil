// Synthscope.cpp: the module's threads, the screen, the panel widget and its menus.
// Author: Ari Klopfer / A_Development. MIT
#include "Synthscope.hpp"
#include "TalkingFace.hpp"
#include "layout.hpp"
#include <chrono>
#include <cstdlib>
#include <osdialog.h>

// rack has its own Scene and Slider, so those two stay qualified
using ss::ArtSheet; using ss::Obj; using ss::ParseError; using ss::Raster; using ss::RenderOpts; using ss::Rgb; using ss::Script; using ss::View; using ss::Volts;
using ss::build; using ss::parse; using ss::renderScene; using ss::walk;

// ---------------------------------------------------------------- files

static std::string readText (const std::string& p)
{
    try { std::vector<uint8_t> b = system::readFile (p); return std::string (b.begin(), b.end()); }
    catch (...) { return ""; }
}

std::string inboxDir()
{
    // ponytail: getenv is ANSI on Windows; a home folder with non-ASCII letters needs _wgetenv
    if (const char* e = std::getenv ("SYNTHSCOPE_INBOX")) if (*e) return e;
    const char* home = std::getenv ("USERPROFILE");
    if (! home || ! *home) home = std::getenv ("HOME");
    return system::join (home ? home : ".", ".my-lil-synthscope", "inbox");
}

static json_t* readJson (const std::string& p)
{
    const std::string s = readText (p);
    json_error_t err;
    return s.empty() ? nullptr : json_loadb (s.data(), s.size(), 0, &err);
}

static std::string jstr (json_t* o, const char* k) { json_t* v = json_object_get (o, k); return json_is_string (v) ? json_string_value (v) : ""; }
static double jnum (json_t* o, const char* k, double d) { json_t* v = json_object_get (o, k); return json_is_number (v) ? json_number_value (v) : d; }

static bool okSlug (const std::string& s)
{
    if (s.empty() || s.size() > 80 || s[0] == '.') return false;
    for (char c : s) if (! ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}

static std::vector<ArtEntry> scanInbox()
{
    std::vector<ArtEntry> out;
    const std::string dir = inboxDir();
    try
    {
        if (! system::isDirectory (dir)) return out;
        for (const std::string& d : system::getEntries (dir))
        {
            const std::string slug = system::getFilename (d);
            if (! okSlug (slug) || ! system::isDirectory (d)) continue;
            json_t* m = readJson (system::join (d, "meta.json"));
            if (! m) continue;   // a folder still being written has no meta yet
            ArtEntry e;
            e.slug = slug;
            e.name = jstr (m, "name");
            if (e.name.empty()) e.name = slug;
            e.author = jstr (m, "author");
            e.license = jstr (m, "license");
            e.sentAt = jnum (m, "sentAt", 0.0);
            json_decref (m);
            out.push_back (e);
        }
    }
    catch (...) {}
    std::sort (out.begin(), out.end(), [] (const ArtEntry& a, const ArtEntry& b) { return a.sentAt > b.sentAt; });
    return out;
}

static std::shared_ptr<ArtSheet> loadArt (const std::string& slug)
{
    if (! okSlug (slug)) return nullptr;
    const std::string d = system::join (inboxDir(), slug);
    json_t* m = readJson (system::join (d, "meta.json"));
    if (! m) return nullptr;
    auto a = std::make_shared<ArtSheet>();
    a->frames = (int) jnum (m, "frames", 32);
    a->cols = (int) jnum (m, "cols", 8);
    a->rows = (int) jnum (m, "rows", 4);
    a->period = jnum (m, "period", 1.0);
    json_decref (m);
    std::vector<uint8_t> png;
    try { png = system::readFile (system::join (d, "sheet.png")); } catch (...) { return nullptr; }
    if (a->frames < 1 || a->frames > a->cols * a->rows || a->period <= 0.0 || ! decodePng (png, *a)) return nullptr;
    return a;
}

// ---------------------------------------------------------------- module

Synthscope::Synthscope()
{
    config (NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam (YAW_PARAM, -1.f, 1.f, 0.2f, "Spin", " °/s", 0.f, 60.f);
    configParam (PITCH_PARAM, -1.f, 1.f, 0.25f, "Tilt", "°", 0.f, 90.f);
    configParam (ZOOM_PARAM, 0.f, 1.f, 0.5f, "Zoom", "x", 16.f, 0.25f);
    configParam (GROW_PARAM, 0.f, 1.f, 1.f, "Grow (objects shown, build order)", "%", 0.f, 100.f);
    const char* abcd[4] = {"A", "B", "C", "D"};
    for (int i = 0; i < 4; ++i) configParam (A_PARAM + i, 0.f, 1.f, 0.5f, abcd[i], "%", 0.f, 100.f);
    configParam (PULSE_PARAM, 0.f, 1.f, 0.3f, "Audio pulse", "%", 0.f, 100.f);
    configParam (ART_PARAM, 0.f, 1.f, 0.6f, "Art backdrop", "%", 0.f, 100.f);
    configParam (SKIN_PARAM, 0.f, 1.f, 0.f, "Art skin", "%", 0.f, 100.f);
    configButton (RESEED_PARAM, "Reseed");
    const char* cv[8] = {"Spin", "Tilt", "Zoom", "Grow", "A", "B", "C", "D"};
    for (int i = 0; i < 8; ++i) configInput (YAW_INPUT + i, std::string (cv[i]) + " CV");
    configInput (RESEED_INPUT, "Reseed trigger");
    configInput (CLOCK_INPUT, "Clock: next object");
    configInput (RESET_INPUT, "Reset: first object, art loop to its start");
    configInput (AUDIO_INPUT, "Audio");
    configOutput (X_OUTPUT, "Object X (-5..5 V across the structure)");
    configOutput (Y_OUTPUT, "Object Y (-5..5 V)");
    configOutput (Z_OUTPUT, "Object Z (-5..5 V)");
    configOutput (HUE_OUTPUT, "Object hue (0..10 V)");
    configOutput (SIZE_OUTPUT, "Object size (0..10 V, 10 = largest)");
    configOutput (GATE_OUTPUT, "Gate (follows the clock)");

    // The default preset is parsed here, before a patch's saved knobs land, so its slider defaults cannot overwrite them.
    text = readText (asset::plugin (pluginInstance, std::string ("res/synthscope/") + DEFAULT_PRESET + ".es"));
    name = DEFAULT_PRESET;
    try
    {
        const Script s = parse (text);
        for (size_t i = 0; i < s.sliders.size() && i < 4; ++i) params[A_PARAM + i].setValue ((s.sliders[i].def - s.sliders[i].lo) / (s.sliders[i].hi - s.sliders[i].lo));
    }
    catch (...) {}
    version = 1;
    seed = (uint32_t) std::chrono::steady_clock::now().time_since_epoch().count() | 1u;
    worker = std::thread ([this] { work(); });
}

Synthscope::~Synthscope()
{
    quit = true;
    wake.notify_all();
    if (worker.joinable()) worker.join();
    delete seq;
    delete seqPending.exchange (nullptr);
    delete seqRetired.exchange (nullptr);
}

void Synthscope::loadScript (const std::string& src, const std::string& scriptName, const std::string& scriptPath)
{
    {
        std::lock_guard<std::mutex> lk (mx);
        text = src;
        name = scriptName;
        path = scriptPath;
        applyDefaults = true;
        ++version;
    }
    wake.notify_one();
}

void Synthscope::loadPreset (const std::string& stem)
{
    loadScript (readText (asset::plugin (pluginInstance, "res/synthscope/" + stem + ".es")), stem, "");
}

void Synthscope::onReset (const ResetEvent& e)
{
    Module::onReset (e);
    loadPreset (DEFAULT_PRESET);
}

void Synthscope::process (const ProcessArgs& args)
{
    // take a new step table only once the worker has freed the one before last
    if (! seqRetired.load())
        if (Seq* n = seqPending.exchange (nullptr)) { seqRetired.store (seq); seq = n; }

    const int n = seq ? (int) seq->v.size() : 0;
    const int vis = n ? clamp ((int) std::ceil (knob (GROW_PARAM, GROW_INPUT) * n), 1, n) : 0;
    if (resetT.process (inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) { st = -1; artTime = 0.0; }
    if (clockT.process (inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f) && vis) st = (st + 1) % vis;
    if (st >= vis) st = vis ? st % vis : -1;   // GROW shrank under the walk
    if (vis)
    {
        const Volts& v = seq->v[std::max (0, st)];
        outputs[X_OUTPUT].setVoltage (v.x);
        outputs[Y_OUTPUT].setVoltage (v.y);
        outputs[Z_OUTPUT].setVoltage (v.z);
        outputs[HUE_OUTPUT].setVoltage (v.hue);
        outputs[SIZE_OUTPUT].setVoltage (v.size);
    }
    outputs[GATE_OUTPUT].setVoltage (vis && st >= 0 && clockT.isHigh() ? 10.f : 0.f);
    step.store (vis ? std::max (0, st) : -1);

    const bool pressed = params[RESEED_PARAM].getValue() > 0.f;
    if (reseedBtn.process (pressed) | reseedT.process (inputs[RESEED_INPUT].getVoltage(), 0.1f, 1.f)) ++reseeds;
    lights[RESEED_LIGHT].setBrightness (pressed ? 1.f : 0.f);

    const float s = inputs[AUDIO_INPUT].getVoltage() / 5.f;
    if (++decim >= 4)   // ~43 ms of audio across 512 slots at 48 kHz
    {
        decim = 0;
        const int h = audioHead.load();
        audio[h] = s;
        audioHead.store ((h + 1) % AUDIO_N);
    }
    face.push (s, args.sampleRate);

    const float period = artPeriod.load();
    if (period > 0.f)
    {
        artTime += args.sampleTime;
        if (artTime >= period) artTime = std::fmod (artTime, (double) period);
        artPhase.store ((float) (artTime / period));
    }
}

void Synthscope::work()
{
    Script live;
    bool haveLive = false, force = false;
    int builtVersion = -1, seenReseeds = reseeds.load();
    uint32_t builtSeed = 0;
    std::vector<float> builtSl;
    double nextFile = 0.0, nextInbox = 0.0, lastSent = -1.0;
    std::string loadedArt;

    while (! quit)
    {
        {
            std::unique_lock<std::mutex> lk (mx);
            wake.wait_for (lk, std::chrono::milliseconds (33));
        }
        if (quit) break;
        delete seqRetired.exchange (nullptr);
        const double now = system::getTime();

        std::string txt, pth, want;
        int ver;
        bool defaults, fol;
        uint32_t sd;
        {
            std::lock_guard<std::mutex> lk (mx);
            txt = text; pth = path; want = artWant; ver = version; defaults = applyDefaults; fol = follow; sd = seed;
            applyDefaults = false;
        }
        const int rs = reseeds.load();
        if (rs != seenReseeds)
        {
            seenReseeds = rs;
            sd = sd * 1664525u + (uint32_t) std::chrono::steady_clock::now().time_since_epoch().count() + 1013904223u;
            std::lock_guard<std::mutex> lk (mx);
            seed = sd;
        }

        // live reload: the file is re-read and compared, which also works where mtimes are coarse or paths are UTF-8
        if (! pth.empty() && now >= nextFile)
        {
            nextFile = now + 0.5;
            const std::string disk = readText (pth);
            if (! disk.empty() && disk != txt)
            {
                std::lock_guard<std::mutex> lk (mx);
                if (path == pth) { text = disk; ++version; }
                continue;   // picked up next tick
            }
        }

        if (ver != builtVersion)
        {
            builtVersion = ver;
            try
            {
                Script s = parse (txt);
                live = std::move (s);
                haveLive = force = true;
                if (defaults)
                {
                    for (size_t i = 0; i < live.sliders.size() && i < 4; ++i)
                        params[A_PARAM + i].setValue ((live.sliders[i].def - live.sliders[i].lo) / (live.sliders[i].hi - live.sliders[i].lo));
                    if (live.set.hasSeed) sd = live.set.seed;
                }
                std::lock_guard<std::mutex> lk (mx);
                error.clear();
                sliders = live.sliders;
                if (defaults && live.set.hasSeed) seed = sd;
            }
            catch (const std::exception& e)
            {
                std::lock_guard<std::mutex> lk (mx);
                error = e.what();   // the last good structure stays up
            }
        }

        if (haveLive)
        {
            std::vector<float> sl;
            for (size_t i = 0; i < live.sliders.size(); ++i)
            {
                const ss::Slider& d = live.sliders[i];
                const float k = i < 4 ? knob (A_PARAM + (int) i, A_INPUT + (int) i) : (d.def - d.lo) / (d.hi - d.lo);
                float v = d.lo + k * (d.hi - d.lo);
                if (d.isInt) v = std::round (v);
                sl.push_back (v);
            }
            if (force || sd != builtSeed || sl != builtSl)
            {
                auto sc = std::make_shared<ss::Scene> (build (live, sd, sl.empty() ? nullptr : sl.data()));
                Seq* q = new Seq;
                q->v.reserve (sc->objs.size());
                for (const Obj& o : sc->objs) q->v.push_back (walk (o, *sc));
                delete seqPending.exchange (q);   // one the audio thread never took is simply replaced
                {
                    std::lock_guard<std::mutex> lk (mx);
                    scene = sc;
                }
                builtSeed = sd;
                builtSl = sl;
                force = false;
            }
        }

        if (now >= nextInbox)
        {
            nextInbox = now + 1.0;
            std::vector<ArtEntry> list = scanInbox();
            const double newest = list.empty() ? 0.0 : list.front().sentAt;
            // only sends that arrive while the module runs are followed; older pieces wait in the Art menu
            bool followed = false;
            if (lastSent < 0.0) lastSent = newest;
            else if (fol && newest > lastSent)
            {
                lastSent = newest;
                want = list.front().slug;
                loadedArt.clear();   // the same slug sent again is a new sheet
                followed = true;
            }
            std::lock_guard<std::mutex> lk (mx);
            inbox = list;
            if (followed) artWant = want;
        }

        if (want != loadedArt)
        {
            loadedArt = want;
            std::shared_ptr<ArtSheet> a = want.empty() ? nullptr : loadArt (want);
            artPeriod.store (a ? (float) a->period : 0.f);
            std::lock_guard<std::mutex> lk (mx);
            art = a;
            artSlug = a ? want : "";
        }
    }
}

json_t* Synthscope::dataToJson()
{
    std::lock_guard<std::mutex> lk (mx);
    json_t* r = json_object();
    json_object_set_new (r, "script", json_stringn (text.data(), text.size()));
    json_object_set_new (r, "name", json_string (name.c_str()));
    json_object_set_new (r, "path", json_string (path.c_str()));
    json_object_set_new (r, "seed", json_integer (seed));
    json_object_set_new (r, "art", json_string (artWant.c_str()));
    json_object_set_new (r, "follow", json_boolean (follow));
    return r;
}

void Synthscope::dataFromJson (json_t* root)
{
    {
        std::lock_guard<std::mutex> lk (mx);
        if (json_t* s = json_object_get (root, "script")) if (json_is_string (s)) text.assign (json_string_value (s), json_string_length (s));
        name = jstr (root, "name");
        path = jstr (root, "path");
        if (json_t* s = json_object_get (root, "seed")) if (json_is_integer (s)) seed = (uint32_t) json_integer_value (s);
        artWant = jstr (root, "art");
        if (json_t* f = json_object_get (root, "follow")) follow = json_is_true (f);
        applyDefaults = false;   // the patch's own knob positions stand
        ++version;
    }
    wake.notify_one();
}

// ---------------------------------------------------------------- screen

struct ScopeScreen : Widget
{
    Synthscope* module = nullptr;
    Raster raster;
    int img = -1;
    float yaw = 30.f;
    double last = 0.0;
    std::shared_ptr<const ss::Scene> preview;   // the library browser has no module: a still of the default preset

    ~ScopeScreen() override
    {
        if (img >= 0 && APP && APP->window && APP->window->vg) nvgDeleteImage (APP->window->vg, img);
    }

    void onContextDestroy (const ContextDestroyEvent& e) override
    {
        if (img >= 0) nvgDeleteImage (e.vg, img);
        img = -1;
        Widget::onContextDestroy (e);
    }

    void drawLayer (const DrawArgs& a, int layer) override
    {
        if (layer == 1) drawScreen (a);
        Widget::drawLayer (a, layer);
    }

    void drawScreen (const DrawArgs& a)
    {
        const int W = std::max (16, (int) box.size.x), H = std::max (16, (int) box.size.y);   // 1 raster px per panel px at 100 %
        if (raster.w != W || raster.h != H)
        {
            raster.resize (W, H);
            if (img >= 0) nvgDeleteImage (a.vg, img);
            img = -1;
        }

        std::shared_ptr<const ss::Scene> sc;
        std::shared_ptr<const ArtSheet> art;
        std::string name, err, artName;
        std::vector<ss::Slider> sliders;
        uint32_t seed = 0;
        RenderOpts o;
        View v;
        const double now = system::getTime();
        const float dt = last > 0.0 ? (float) std::min (0.1, now - last) : 0.f;
        last = now;
        float spin = 0.2f, tilt = 0.25f, zoom = 0.5f;
        if (module)
        {
            {
                std::lock_guard<std::mutex> lk (module->mx);
                sc = module->scene; art = module->art; name = module->name; err = module->error; sliders = module->sliders; seed = module->seed;
                artName = module->artSlug;
            }
            spin = clamp (module->params[Synthscope::YAW_PARAM].getValue() + module->inputs[Synthscope::YAW_INPUT].getVoltage() / 5.f, -1.f, 1.f);
            tilt = clamp (module->params[Synthscope::PITCH_PARAM].getValue() + module->inputs[Synthscope::PITCH_INPUT].getVoltage() / 5.f, -1.f, 1.f);
            zoom = module->knob (Synthscope::ZOOM_PARAM, Synthscope::ZOOM_INPUT);
            const int n = sc ? (int) sc->objs.size() : 0;
            o.visible = n ? clamp ((int) std::ceil (module->knob (Synthscope::GROW_PARAM, Synthscope::GROW_INPUT) * n), 1, n) : 0;
            o.pulse = module->params[Synthscope::PULSE_PARAM].getValue();
            o.audio = module->audio;
            o.audioN = Synthscope::AUDIO_N;
            o.audioHead = module->audioHead.load();
            o.highlight = module->inputs[Synthscope::CLOCK_INPUT].isConnected() ? module->step.load() : -1;
            o.artMix = module->params[Synthscope::ART_PARAM].getValue();
            o.skin = module->params[Synthscope::SKIN_PARAM].getValue();
            if (art) { o.art = art.get(); o.artFrame = art->frameAt (module->artPhase.load()); }
        }
        else
        {
            if (! preview)
            {
                try { preview = std::make_shared<ss::Scene> (build (parse (readText (asset::plugin (pluginInstance, std::string ("res/synthscope/") + Synthscope::DEFAULT_PRESET + ".es"))), 1, nullptr)); }
                catch (...) { preview = std::make_shared<ss::Scene>(); }
            }
            sc = preview;
            name = Synthscope::DEFAULT_PRESET;
        }
        yaw = std::fmod (yaw + spin * 60.f * dt, 360.f);
        v.yaw = yaw;
        v.pitch = tilt * 90.f;
        v.zoom = 0.25f * std::pow (16.f, zoom);

        Rgb glass; glass.r = 0x0b / 255.f; glass.g = 0x0b / 255.f; glass.b = 0x0e / 255.f;
        renderScene (sc ? *sc : ss::Scene(), v, o, raster, glass);
        if (img < 0) img = nvgCreateImageRGBA (a.vg, raster.w, raster.h, 0, raster.rgba.data());
        else nvgUpdateImage (a.vg, img, raster.rgba.data());

        nvgSave (a.vg);
        nvgBeginPath (a.vg);
        nvgRoundedRect (a.vg, 0, 0, box.size.x, box.size.y, mm2px (1.5f));
        nvgFillPaint (a.vg, nvgImagePattern (a.vg, 0, 0, box.size.x, box.size.y, 0.f, img, 1.f));
        nvgFill (a.vg);

        std::shared_ptr<Font> font = APP->window->loadFont (asset::system ("res/fonts/ShareTechMono-Regular.ttf"));
        if (font && font->handle >= 0)
        {
            nvgFontFaceId (a.vg, font->handle);
            nvgFontSize (a.vg, 11.f);
            nvgTextAlign (a.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            auto label = [&] (float x, float y, const std::string& s, NVGcolor c) {
                nvgFillColor (a.vg, nvgRGBA (0, 0, 0, 170));
                nvgText (a.vg, x + 1, y + 1, s.c_str(), nullptr);
                nvgFillColor (a.vg, c);
                nvgText (a.vg, x, y, s.c_str(), nullptr);
            };
            const NVGcolor lime = nvgRGB (0xc8, 0xff, 0x3a), dim = nvgRGB (0x8d, 0x8d, 0x96), pink = nvgRGB (0xff, 0x3d, 0x9a);
            std::string top = name.empty() ? "script" : name;
            if (sc) top += "  ·  " + std::to_string (o.visible < 0 ? sc->objs.size() : (size_t) o.visible) + " / " + std::to_string (sc->objs.size()) + (sc->truncated ? " (capped)" : "");
            label (6, 5, top, lime);
            if (module) label (6, 18, "seed " + std::to_string (seed) + (artName.empty() ? "" : "   art: " + artName), dim);
            std::string abcd;
            for (size_t i = 0; i < sliders.size() && i < 4; ++i) abcd += std::string (1, (char) ('A' + i)) + "=" + sliders[i].name + "  ";
            nvgTextAlign (a.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
            if (! err.empty()) label (6, box.size.y - 5, err, pink);
            else if (! abcd.empty()) label (6, box.size.y - 5, abcd, dim);
        }
        nvgRestore (a.vg);
    }
};

// ---------------------------------------------------------------- panel

struct SynthscopeWidget : ModuleWidget
{
    SynthscopeWidget (Synthscope* module)
    {
        setModule (module);
        setPanel (createPanel (asset::plugin (pluginInstance, "res/Synthscope.svg")));
        namespace L = layout::synthscope;
        {
            ScopeScreen* s = createWidget<ScopeScreen> (mm2px (Vec (L::SCREEN[0], L::SCREEN[1])));
            s->box.size = mm2px (Vec (L::SCREEN[2], L::SCREEN[3]));
            s->module = module;
            addChild (s);
        }
        {
            TalkingFace* f = createWidget<TalkingFace> (mm2px (Vec (L::FACE[0] - L::FACE[2], L::FACE[1] - L::FACE[2])));
            f->box.size = mm2px (Vec (2 * L::FACE[2], 2 * L::FACE[2]));
            if (module) f->tap = &module->face;
            addChild (f);
        }
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild (createWidget<ScrewBlack> (Vec (RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild (createWidget<ScrewBlack> (Vec (box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto at = [] (const float* mm) { return mm2px (Vec (mm[0], mm[1])); };
        const float* knobs[11] = {L::YAW_PARAM, L::PITCH_PARAM, L::ZOOM_PARAM, L::GROW_PARAM, L::A_PARAM, L::B_PARAM, L::C_PARAM, L::D_PARAM, L::PULSE_PARAM, L::ART_PARAM, L::SKIN_PARAM};
        for (int i = 0; i < 11; ++i) addParam (createParamCentered<Rogan1PSWhite> (at (knobs[i]), module, Synthscope::YAW_PARAM + i));
        addParam (createLightParamCentered<VCVLightButton<MediumSimpleLight<LimeLight>>> (at (L::RESEED_PARAM), module, Synthscope::RESEED_PARAM, Synthscope::RESEED_LIGHT));
        const float* ins[12] = {L::YAW_INPUT, L::PITCH_INPUT, L::ZOOM_INPUT, L::GROW_INPUT, L::A_INPUT, L::B_INPUT, L::C_INPUT, L::D_INPUT, L::RESEED_INPUT, L::CLOCK_INPUT, L::RESET_INPUT, L::AUDIO_INPUT};
        for (int i = 0; i < 12; ++i) addInput (createInputCentered<PJ301MPort> (at (ins[i]), module, Synthscope::YAW_INPUT + i));
        const float* outs[6] = {L::X_OUTPUT, L::Y_OUTPUT, L::Z_OUTPUT, L::HUE_OUTPUT, L::SIZE_OUTPUT, L::GATE_OUTPUT};
        for (int i = 0; i < 6; ++i) addOutput (createOutputCentered<PJ301MPort> (at (outs[i]), module, Synthscope::X_OUTPUT + i));
    }

    // knob tooltips follow the script's slider names
    void step() override
    {
        ModuleWidget::step();
        Synthscope* m = dynamic_cast<Synthscope*> (module);
        if (! m) return;
        std::vector<ss::Slider> s;
        {
            std::lock_guard<std::mutex> lk (m->mx);
            s = m->sliders;
        }
        for (int i = 0; i < 4; ++i)
        {
            ParamQuantity* q = m->paramQuantities[Synthscope::A_PARAM + i];
            const std::string want = i < (int) s.size() ? std::string (1, (char) ('A' + i)) + ": " + s[i].name : std::string (1, (char) ('A' + i)) + " (no slider in this script)";
            if (q->name != want) q->name = want;
        }
    }

    void appendContextMenu (Menu* menu) override
    {
        Synthscope* m = dynamic_cast<Synthscope*> (module);
        if (! m) return;
        std::string name, path, artSlug;
        std::vector<ArtEntry> inbox;
        {
            std::lock_guard<std::mutex> lk (m->mx);
            name = m->name; path = m->path; artSlug = m->artWant; inbox = m->inbox;
        }
        menu->addChild (new MenuSeparator);
        menu->addChild (createMenuLabel ("Script: " + (name.empty() ? std::string ("(patch)") : name) + (path.empty() ? "" : "  (watching file)")));
        menu->addChild (createSubmenuItem ("Presets", "", [=] (Menu* sub) {
            std::vector<std::string> files;
            try { files = system::getEntries (asset::plugin (pluginInstance, "res/synthscope")); } catch (...) {}
            std::sort (files.begin(), files.end());
            for (const std::string& f : files)
            {
                if (system::getExtension (f) != ".es") continue;
                const std::string stem = system::getStem (f);
                sub->addChild (createMenuItem (stem, "", [=] { m->loadPreset (stem); }));
            }
        }));
        menu->addChild (createMenuItem ("Load .es file (live reload on save)...", "", [=] {
            osdialog_filters* filters = osdialog_filters_parse ("EisenScript:es;Text:txt");
            char* p = osdialog_file (OSDIALOG_OPEN, nullptr, nullptr, filters);
            osdialog_filters_free (filters);
            if (! p) return;
            const std::string file = p;
            std::free (p);
            m->loadScript (readText (file), system::getStem (file), file);
        }));
        if (! path.empty())
        {
            menu->addChild (createMenuItem ("Stop watching " + system::getFilename (path), "", [=] {
                std::lock_guard<std::mutex> lk (m->mx);
                m->path.clear();
            }));
        }

        menu->addChild (new MenuSeparator);
        menu->addChild (createMenuLabel ("Algo-Art Cockpit"));
        menu->addChild (createSubmenuItem ("Art", artSlug.empty() ? "none" : artSlug, [=] (Menu* sub) {
            sub->addChild (createCheckMenuItem ("None", "", [=] { return artSlug.empty(); }, [=] { m->chooseArt (""); }));
            if (inbox.empty()) sub->addChild (createMenuLabel ("(inbox empty: Send to Synthscope in the cockpit)"));
            for (const ArtEntry& e : inbox)
            {
                const std::string slug = e.slug;
                sub->addChild (createCheckMenuItem (e.name, e.author, [=] { return artSlug == slug; }, [=] { m->chooseArt (slug); }));
            }
        }));
        menu->addChild (createBoolMenuItem ("Follow cockpit (show each new send)", "",
            [=] { std::lock_guard<std::mutex> lk (m->mx); return m->follow; },
            [=] (bool b) { std::lock_guard<std::mutex> lk (m->mx); m->follow = b; }));
        menu->addChild (createMenuItem ("Open inbox folder", "", [=] {
            const std::string d = inboxDir();
            try { system::createDirectories (d); } catch (...) {}
            system::openDirectory (d);
        }));
    }
};

Model* modelSynthscope = createModel<Synthscope, SynthscopeWidget> ("Synthscope");
