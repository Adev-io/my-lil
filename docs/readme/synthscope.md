# My lil Synthscope

A structure generator for VCV Rack 2. You write short rules in EisenScript; they grow a 3D structure that is drawn on the
panel screen. Audio and CV bend it, a clock walks it object by object and turns each one into CV, and pieces from the
Algo-Art Cockpit show up as the backdrop and as the skin of the objects. By A_Development.

It starts with the *spiral-tower* preset, so it shows something the moment you add it. Eight presets are built in; right-click
for the menu, or load your own `.es` file and keep editing it while the structure follows every save.

EisenScript is the small language that Structure Synth made known. This module is a clean-room reimplementation written
from how the language behaves. Structure Synth (Mikael Hvidtfeldt Christensen) is GPL v2, so none of its code and none of
its example scripts are used here, and the 8 presets are original.

## The screen

The structure spins on its own (**YAW**). Top left: the script name and how many objects are shown out of how many were
built ("capped" means the script grows faster than it can draw and was cut off), then the seed and the art in use. Bottom left:
the names of the script's slider defines, as `A=twist B=shrink`. If the script has a mistake, that line is replaced by the
error in pink with its line number, and the last good structure stays on screen.

With CLOCK patched, the object the clock is on is outlined in lime.

## Knobs

| Knob | What it does |
|---|---|
| **YAW** | Spin speed, centre = stopped, up to 60 degrees per second each way. |
| **PITCH** | Tilt, up to 90 degrees each way. |
| **ZOOM** | Camera zoom, from 0.25x to 4x. |
| **GROW** | How many objects are shown, as a share of all of them, in the order they were built. Turn it up and the structure grows. |
| **A B C D** | The script's first four slider defines, in order (see below). Hover a knob and its tooltip shows the define's name. A knob with no define in the script does nothing. |
| **PULSE** | How much the audio at **AUDIO** swells the objects. |
| **ART** | Opacity of the Algo-Art Cockpit piece behind the structure. |
| **SKIN** | How much the objects take their colour from the art instead of from the script. |
| **RESEED** | Button. Picks a new random seed, so rules with several versions of the same name choose differently. |

Loading a preset or a file moves A to D to that script's slider defaults. Loading a patch does not: the patch's own knob
positions stand.

## Inputs

| Input | What it does |
|---|---|
| **YAW**, **PITCH** CV | Adds to the knob. 5 V sweeps the knob's whole travel from its centre. |
| **ZOOM**, **GROW**, **A** to **D** CV | Adds to the knob. 10 V sweeps the whole travel, so +-5 V moves it by half. |
| **RESEED** | A trigger does what the button does. |
| **CLOCK** | Each trigger steps to the next object (only the ones GROW shows), then wraps round to the first. |
| **RESET** | Goes back to before the first object, so the next CLOCK lands on the first. It also restarts the art loop from its start. |
| **AUDIO** | Any audio. It makes the objects swell (see PULSE). |

## Outputs

These follow the object the clock is on.

| Output | What it gives |
|---|---|
| **X** | -5 to 5 V: where the object's centre sits left to right across the whole structure. |
| **Y** | -5 to 5 V: the same, bottom to top. |
| **Z** | -5 to 5 V: the same, near to far. |
| **HUE** | 0 to 10 V: the object's hue (a full colour wheel is 10 V). |
| **SIZE** | 0 to 10 V: 10 V for the largest object, 1.25 V less for every halving in size. |
| **GATE** | 10 V while the CLOCK input is high, once the clock has stepped to an object. 0 V after RESET. |

Before the first CLOCK trigger, the outputs show the first object. If the script builds nothing, X, Y, Z, HUE and SIZE hold
their last value.

## Right-click menu

- **Presets**: bloom, coral, cube-lattice, fern, orbit-rings, spiral-tower, sponge, stairs.
- **Load .es file (live reload on save)**: pick a script. The file is read again twice a second, and when it changes, the
  structure rebuilds. Edit it in any editor and save.
- **Stop watching**: appears while a file is loaded. The script stays, the file is no longer read.
- **Art**: choose one of the pieces in the inbox, or None. Each entry shows its name and author.
- **Follow cockpit**: when on, a new piece sent from the cockpit is shown as it arrives.
- **Open inbox folder**: opens the folder the cockpit writes to, and makes it if it is not there yet.

The script text, its file path, the seed and the chosen art are saved with the patch, so a patch opens with the same
structure even if the file or preset has changed since. Re-initialising the module loads *spiral-tower* again.

## EisenScript

A script is a list of rules plus a few lines that start things off.

```
// a comment, and /* a block comment */
#define twist 12 (float:0-40)     // a slider: knob A, 0 to 40, starts at 12
set maxdepth 300
set background #101018

{ color #5ac8e6 } tower           // start: call the rule "tower" in blue

rule tower {
  { s 1.4 0.1 1.4 } box           // draw a flat box
  { y 0.9 ry twist s 0.97 h 3 } tower   // then call tower again, moved, turned, smaller, hue + 3
}
```

**Rules.** `rule name { ... }` holds actions. An action is a primitive to draw, or a call to another rule, optionally
with transforms in braces in front of it: `{ y 1 } box`. Lines outside any rule are the start. Write the same name more
than once and each time the rule runs, one definition is picked at random, by weight.

```
rule lattice w 3 { ... }      // weight (or w): 3 chances against the others' share
rule lattice w 1 { ... }
rule grow md 8 > ending { ... }   // maxdepth (or md) 8: after 8 uses in a chain, run "ending" instead
```

`maxdepth N` on its own just stops that rule after N uses in a chain. The rule chain is built one generation at a time, so
the structure grows evenly in every direction.

**Loops.** `N * { transforms } thing` repeats the thing N times, each time moved on by one more step of the transforms. The
first copy gets the transform once, the second twice, and so on, so `1 * { x 1 }` is the same as `{ x 1 }`. Loops can be
chained, and they multiply: `8 * { x 1.4 } 8 * { y 1.4 } box` is an 8 x 8 grid.

**Transforms.** They act about the centre of the unit cube. Angles are in degrees.

| Transform | Meaning |
|---|---|
| `x` `y` `z` | Move. |
| `rx` `ry` `rz` | Rotate about that axis. |
| `s v` or `s a b c` | Scale: one number for all axes, or three. |
| `fx` `fy` `fz` | Mirror across that axis (no number). |
| `m` and 9 numbers | A 3 x 3 matrix, row by row. |
| `h` or `hue` | Add to the hue, in degrees. |
| `sat` | Multiply saturation. |
| `b` or `brightness` | Multiply brightness. |
| `a` or `alpha` | Multiply opacity. |
| `color name` | Set the colour: a name (red, coral, lime, ...), `#rgb` or `#rrggbb`. `color random` picks from the colour pool. |
| `blend colour strength` | Mix towards a colour, strength 0 to 1. |

Numbers may be fractions (`1/3`). Transforms in one pair of braces apply left to right.

**Primitives.** `box`, `sphere`, `dot` (a single bright point), `line` (a line through the middle of the cube along x),
`grid` (a wireframe box), `cylinder` and `mesh` (both drawn as boxes here), and `triangle[x,y,z;x,y,z;x,y,z]`. A tag after
`::` (as in `box::floor`) is ignored.

**Settings.** `set key value` goes on its own line.

| Key | Meaning |
|---|---|
| `maxdepth` | Generations to grow (default 1000). |
| `maxobjects` | Stop after this many objects. Never more than 20000. |
| `minsize`, `maxsize` | Leave out objects whose size is outside this range. |
| `seed` | Start from this seed when the script is loaded (`seed initial` leaves it random). |
| `background` | Screen background colour. |
| `colorpool` | Where `color random` draws from: `randomhue`, `randomrgb`, `greyscale` or `list:red,#ff3d9a,coral`. |

Every other key, including all the camera and raytracer ones, is read and ignored, so scripts written for other tools load.
The camera here is the module's own (YAW, PITCH, ZOOM).

**Defines.** `#define name value` swaps `name` for `value` wherever it appears as a word, and defines can use other
defines. A define with a range becomes a slider:

```
#define twist 12 (float:0-40)
#define levels 3 (int:1-4)
```

The first four sliders in the script are bound to knobs A to D, in order. `int` sliders snap to whole numbers. Use a slider
name anywhere a number goes (and `-twist` for its negative). Sliders beyond the fourth stay at their default.

**Mistakes.** A parse error shows on the screen in pink with its line number, and the last good structure keeps running.
Fix the file and save.

## Algo-Art Cockpit

The cockpit's **Send to Synthscope** button turns the captured mp4 loop of a piece into a 32-frame sheet (`sheet.png`, 8 x 4
cells) plus `meta.json`, in `~/.my-lil-synthscope/inbox/<slug>/`. To use another folder, set the `SYNTHSCOPE_INBOX`
environment variable before starting Rack.

The module looks in the inbox once a second. With **Follow cockpit** on, it switches to each new piece sent while Rack is
running. Pieces that were already there when you started stay in the **Art** menu until you pick them. The piece plays as
a loop behind the structure (**ART**) and colours the objects (**SKIN**). RESET starts the loop over.

## Saving

The script, its file path, the seed, the chosen art and the Follow cockpit switch are saved with the patch.

## Building

```
pwsh -File tools/mk.ps1 test    # the core checks
pwsh -File tools/mk.ps1         # the plugin
pwsh -File tools/mk.ps1 dist    # the .vcvplugin
```

On Windows this runs make in MSYS2 mingw64 against the Rack SDK at `C:\dev\Rack-SDK` (set `RACK_DIR` in `tools/mk.ps1` if
yours is elsewhere). The panel and `src/layout/synthscope.hpp` come from `py -3.11 tools/panel/synthscope.py`. The core
checks live in `tests/synthscope/`, the presets in `res/synthscope/`.

## License

MIT. Ari Klopfer / A_Development.
