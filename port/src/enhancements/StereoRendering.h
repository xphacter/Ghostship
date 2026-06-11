#pragma once

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

#ifdef __cplusplus
}
#endif
