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
 * When non-zero, print_text / print_text_centered / print_text_fmt_int
 * are no-ops.  Set during the right-eye render_hud() call so that text
 * is not added to sTextLabels a second time (it was already added in the
 * left-eye pass and kept alive there).  Clear before render_text_labels()
 * so the right-eye pass can still render those saved labels.
 */
extern int gSBSSkipTextAccumulation;

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

/**
 * Emit a display-list command that sets the interpreter's internal SBS HUD
 * eye variable at DL-execute time.  This is necessary because gSBSHudEye is
 * reset to 0 before exec_display_list() is called, so the interpreter can
 * never read the correct value from the C global directly.
 *
 * Opcode 0x4A == G_SBS_HUD_EYE (lus_gbi.h).  The eye value occupies bits
 * 7-0 of word 0, sign-extended to int8_t by the handler.
 *
 * eye: -1 = left eye, 0 = normal/reset, +1 = right eye
 */
#define gSBSHudSetEye(pkt, eye) \
    do { \
        Gfx *_sbsEyeG = (Gfx *)(pkt); \
        _sbsEyeG->words.w0 = (u32)(0x4Au << 24) | ((u32)((eye) & 0xFF)); \
        _sbsEyeG->words.w1 = 0; \
    } while (0)

#ifdef __cplusplus
}

// C++ API — call from PortEnhancements_Init() and the menu bar
void StereoRendering_Init();
void StereoRendering_DrawMenu();

#endif
