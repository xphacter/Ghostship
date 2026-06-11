# Side-by-Side Stereoscopic 3D for Ghostship

Renders the game world twice per frame — once for each eye — split left/right.
Compatible with 3D TVs (side-by-side mode), VR headsets via SteamVR Theatre, or
anaglyph glasses with a post-process shader.

---

## How it works

| Step | What changes |
|------|-------------|
| 1 | `render_game()` computes a perpendicular eye offset from `gLakituState.pos/focus` |
| 2 | Sets `gSBSEye = -1`, shifts camera left, calls `geo_process_root()` |
| 3 | Sets `gSBSEye =  1`, shifts camera right, calls `geo_process_root()` again |
| 4 | `geo_process_root()` detects `gSBSEye ≠ 0`, halves the N64 viewport & scissor |
| 5 | HUD / text / menus render once at full width (`gSBSEye = 0`) |

The camera shift is purely lateral (X/Z plane) — no convergence adjustment — which
gives a zero-parallax plane at the point the camera looks at.  This is "toe-in free"
SBS and avoids the vertical parallax artefacts that toe-in produces.

---

## Files

### New files (add to repo)

| File | Purpose |
|------|---------|
| `port/src/enhancements/StereoRendering.h` | C-compatible `gSBSEye` declaration |
| `port/src/enhancements/StereoRendering.cpp` | CVar registration + ImGui menu |

### Patches to apply

| File | Change |
|------|--------|
| `src/game/area.c` | Double `geo_process_root` loop in `render_game()` |
| `src/game/rendering_graph_node.c` | Halve viewport/scissor when `gSBSEye ≠ 0` |

The `.sbs.patch` files next to the source files contain the exact diffs.

---

## Applying the patch

### 1 — Copy the new files
```
port/src/enhancements/StereoRendering.h
port/src/enhancements/StereoRendering.cpp
```

### 2 — Edit `src/game/area.c`

Add at the top (with the other port includes):
```c
#include "port/src/enhancements/StereoRendering.h"
```

Inside `render_game()`, replace the single `geo_process_root(...)` call with the
SBS branch shown in `src/game/area.c.sbs.patch`.

### 3 — Edit `src/game/rendering_graph_node.c`

Add the include and replace the viewport/scissor block as shown in
`src/game/rendering_graph_node.c.sbs.patch`.

### 4 — Wire up the menu

In your `GameMenuBar.cpp` (or equivalent), call:
```cpp
#include "port/src/enhancements/StereoRendering.h"

// In your Graphics / Enhancements window:
StereoRendering_DrawMenu();
```

And at startup:
```cpp
StereoRendering_Init();
```

### 5 — Add to CMake (if enhancements are listed explicitly)

```cmake
port/src/enhancements/StereoRendering.cpp
```

---

## CVars

| CVar | Type | Default | Description |
|------|------|---------|-------------|
| `gEnhancement_Stereoscopic3D` | int | 0 | 0 = off, 1 = SBS enabled |
| `gEnhancement_EyeSeparation` | float | 30.0 | Camera offset in game units |

---

## Tuning

- **Eye Separation 0–50** — subtle, safe for most content
- **Eye Separation 50–100** — strong depth, may cause discomfort on screens closer than 2 m
- **Eye Separation > 100** — extreme, useful for large projection screens

The default of **30** puts the zero-parallax plane (screen depth) roughly at the
distance Lakitu orbits Mario, which feels natural.

---

## Known limitations / future work

- **Focus point offset** — for a physically correct SBS implementation, the *focus* 
  point should also be shifted slightly so both eyes converge on the same world point.
  Currently only `pos` is shifted, which works fine at moderate separations.

- **HUD depth** — the HUD renders at screen depth (no stereo offset). A future
  enhancement could render HUD elements at a fixed negative parallax so they appear
  to float in front of the screen.

- **Frame interpolation** — the FrameInterpolation system records one set of matrices
  per frame. With SBS enabled it records twice (once per eye). This should be harmless
  but has not been extensively tested; disable frame interpolation if you see glitches.

- **Warp transitions** — transition effects render at full width. They will appear on
  both eyes correctly because `gSBSEye` is reset to 0 before those calls.
