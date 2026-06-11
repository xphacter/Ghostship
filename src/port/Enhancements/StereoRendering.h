#pragma once

#include "port/ui/cvar_prefixes.h"

/**
 * StereoRendering.h
 * Side-by-side stereoscopic 3D support for Ghostship.
 *
 * Declares the gSBSEye global used by both C (rendering_graph_node.c, area.c)
 * and C++ (StereoRendering.cpp) translation units.
 *
 * CVar keys:
 *   CVAR_ENHANCEMENT("Stereoscopic3D")  – integer 0/1 toggle
 *   CVAR_ENHANCEMENT("EyeSeparation")   – float, world-units between eyes (default 30.0)
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Indicates which eye is currently being rendered.
 *  -1 = left eye
 *   0 = normal (SBS disabled, or HUD pass)
 *   1 = right eye
 *
 * Written by render_game() in area.c before each geo_process_root() call.
 * Read by geo_process_root() in rendering_graph_node.c to halve the viewport.
 */
extern int gSBSEye;

/**
 * Indicates which eye the HUD is currently being rendered into.
 *  -1 = left eye  (scissor 0..160)
 *   0 = normal    (SBS disabled)
 *   1 = right eye (scissor 160..320)
 *
 * Written by render_game() in area.c around each HUD render call.
 * Read by render_hud_tex_lut, render_hud_small_tex_lut (hud.c)
 * and render_textrect (print.c) to shift texture rect coordinates
 * into the correct eye half.
 */
extern int gSBSHudEye;

/**
 * Remaps an HUD x-coordinate for the current SBS eye half.
 *
 * In SBS mode each half is 160 px wide but the HUD was designed for
 * the full 320 px canvas.  We compress the x-position to fit inside
 * one half while keeping character/sprite spacing unchanged:
 *
 *   left  eye: x -> x/2         (fits 0..320 into 0..160)
 *   right eye: x -> x/2 + 160   (fits 0..320 into 160..320)
 *   no SBS:    x -> x            (identity)
 */
static inline int sbsHudBaseX(int x) {
    if (gSBSHudEye == 0) return x;
    return (x >> 1) + (gSBSHudEye == 1 ? 160 : 0);
}

#ifdef __cplusplus
}

// C++ API — call from PortEnhancements_Init() and the menu bar
void StereoRendering_Init();
void StereoRendering_DrawMenu();

#endif
