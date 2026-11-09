// Synthscope.hpp: the module. Three threads touch it:
//   worker - parses, builds, watches the script file and the cockpit inbox, decodes art (all allocation lives here)
//   audio  - steps the sequencer through a finished table of voltages it is handed lock-free, fills the audio ring
//   UI     - draws the latest scene (Synthscope.cpp), edits script/art requests under the mutex
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include "FaceTap.h"
#include "Render.h"
#include "plugin.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

struct ArtEntry { std::string slug, name, author, license; double sentAt = 0.0; };

// stb.cpp: PNG bytes -> RGBA into the sheet (keeps the sheet's frame layout)
bool decodePng (const std::vector<uint8_t>& bytes, ss::ArtSheet& out);

// SYNTHSCOPE_INBOX, else ~/.my-lil-synthscope/inbox (the cockpit's Send to Synthscope writes here)
std::string inboxDir();

struct Synthscope : Module
{
    enum ParamId { YAW_PARAM, PITCH_PARAM, ZOOM_PARAM, GROW_PARAM, A_PARAM, B_PARAM, C_PARAM, D_PARAM, PULSE_PARAM, ART_PARAM, SKIN_PARAM, RESEED_PARAM, NUM_PARAMS };
    enum InputId { YAW_INPUT, PITCH_INPUT, ZOOM_INPUT, GROW_INPUT, A_INPUT, B_INPUT, C_INPUT, D_INPUT, RESEED_INPUT, CLOCK_INPUT, RESET_INPUT, AUDIO_INPUT, NUM_INPUTS };
    enum OutputId { X_OUTPUT, Y_OUTPUT, Z_OUTPUT, HUE_OUTPUT, SIZE_OUTPUT, GATE_OUTPUT, NUM_OUTPUTS };
    enum LightId { RESEED_LIGHT, NUM_LIGHTS };
    static constexpr int AUDIO_N = 512;
    static constexpr const char* DEFAULT_PRESET = "spiral-tower";

    // ---- under mx: the script, the art request, and what the worker publishes for the UI ----
    std::mutex mx;
    std::string text, name, path, error;
    int version = 0;              // bumps on every new script text
    bool applyDefaults = false;   // a fresh load: knobs A-D jump to the script's slider defaults
    uint32_t seed = 1;
    std::shared_ptr<const ss::Scene> scene;
    std::vector<ss::Slider> sliders;
    std::shared_ptr<const ss::ArtSheet> art;
    std::string artSlug, artWant;
    bool follow = true;
    std::vector<ArtEntry> inbox;

    // ---- audio thread <-> others ----
    struct Seq { std::vector<ss::Volts> v; };
    std::atomic<Seq*> seqPending {nullptr}, seqRetired {nullptr};
    Seq* seq = nullptr;   // audio thread's own
    std::atomic<int> step {-1}, reseeds {0};
    std::atomic<float> artPhase {0.f}, artPeriod {0.f};
    float audio[AUDIO_N] = {};
    std::atomic<int> audioHead {0};
    FaceTap face;

    Synthscope();
    ~Synthscope() override;
    void process (const ProcessArgs& args) override;
    void onReset (const ResetEvent& e) override;
    json_t* dataToJson() override;
    void dataFromJson (json_t* root) override;

    // UI thread: a new script (preset or file); path "" = not watched
    void loadScript (const std::string& src, const std::string& scriptName, const std::string& scriptPath);
    void loadPreset (const std::string& stem);
    void chooseArt (const std::string& slug) { std::lock_guard<std::mutex> lk (mx); artWant = slug; }

    // 0..1 knob plus CV (+-5 V spans the whole knob)
    float knob (int p, int in) { return clamp (params[p].getValue() + inputs[in].getVoltage() / 10.f, 0.f, 1.f); }

private:
    dsp::SchmittTrigger clockT, resetT, reseedT;
    dsp::BooleanTrigger reseedBtn;
    double artTime = 0.0;
    int st = -1, decim = 0;
    std::thread worker;
    std::atomic<bool> quit {false};
    std::condition_variable wake;
    void work();
};
