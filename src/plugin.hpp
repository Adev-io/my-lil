// plugin.hpp: the plugin's models and the house light colours.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include <rack.hpp>
#include <cmath>
using namespace rack;
extern Plugin* pluginInstance;
extern Model* modelArpeggio;
extern Model* modelRipple;
extern Model* modelFilter;
extern Model* modelSlider;
extern Model* modelMixer;
extern Model* modelAcidBass;
extern Model* modelAcidVoice;
extern Model* modelLooper;

// House light colours, the same as the panels (tools/panel/toolbox.py) and My lil Acid Bass.
struct LimeLight : GrayModuleLightWidget { LimeLight() { addBaseColor (nvgRGB (0xc8, 0xff, 0x3a)); } };
struct MagentaLight : GrayModuleLightWidget { MagentaLight() { addBaseColor (nvgRGB (0xff, 0x3d, 0x9a)); } };
// Two-state lights: channel 0 lime, channel 1 magenta.
struct LimeMagentaLight : GrayModuleLightWidget
{
    LimeMagentaLight()
    {
        addBaseColor (nvgRGB (0xc8, 0xff, 0x3a));
        addBaseColor (nvgRGB (0xff, 0x3d, 0x9a));
    }
};
// Arpeggio keys: channel 0 white (in the scale), 1 lime (A's note), 2 blue (B's note).
struct KeyLight : GrayModuleLightWidget
{
    KeyLight()
    {
        addBaseColor (nvgRGB (0xf2, 0xf2, 0xf4));
        addBaseColor (nvgRGB (0xc8, 0xff, 0x3a));
        addBaseColor (nvgRGB (0x5a, 0xc8, 0xe6));
    }
};

// ---- from My lil Acid Bass: the cutoff range and knob maps, the cutoff glow.
static const float cutLo = 60.f, cutHi = 8000.f;   // the cutoff range, exponential on the knob
inline float cutoffKnob (float hz) { return std::log (hz / cutLo) / std::log (cutHi / cutLo); }
inline float decayKnob (float sec) { return std::log (sec / 0.03f) / std::log (83.f); }   // inverse of acidmaps::envDecSec

// A lime halo behind a knob that breathes with a light's brightness: the filter envelope under CUTOFF, so the sweep you
// hear is the sweep you see. Draws on layer 1 (self-lit), behind the knob added after it.
struct GlowRing : TransparentWidget
{
    Module* module = nullptr;
    int lightId = 0;
    float radius = 0.f;   // px, the knob's radius

    void drawLayer (const DrawArgs& args, int layer) override
    {
        if (layer != 1 || ! module) return;
        const float b = module->lights[lightId].getBrightness();
        if (b <= 0.01f) return;
        NVGcontext* vg = args.vg;
        const Vec c = box.size.div (2.f);
        const float r = radius * (1.05f + 0.3f * b);
        NVGpaint p = nvgRadialGradient (vg, c.x, c.y, radius * 0.9f, r, nvgRGBAf (0.78f, 1.f, 0.23f, 0.9f * b), nvgRGBAf (0.78f, 1.f, 0.23f, 0.f));
        nvgBeginPath (vg); nvgCircle (vg, c.x, c.y, r); nvgFillPaint (vg, p); nvgFill (vg);
    }
};

inline GlowRing* glowRing (Vec center, Module* module, int lightId, float knobRadiusMm)
{
    GlowRing* g = new GlowRing;
    g->box.size = mm2px (Vec (knobRadiusMm * 2.8f, knobRadiusMm * 2.8f));
    g->box.pos = center.minus (g->box.size.div (2.f));
    g->module = module; g->lightId = lightId; g->radius = mm2px (knobRadiusMm);
    return g;
}
