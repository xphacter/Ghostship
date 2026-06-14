/**
 * StereoRendering.cpp
 * Side-by-side stereoscopic 3D enhancement for Ghostship.
 *
 * Registers CVars at startup. The menu is handled via AddWidget in
 * GhostshipMenuEnhancements.cpp. The camera-split logic lives in
 * area.c / rendering_graph_node.c.
 */

#include "StereoRendering.h"
#include <libultraship.h>
#include "port/ui/cvar_prefixes.h"

// ---------------------------------------------------------------------------
// Global definition (declared extern in StereoRendering.h)
// ---------------------------------------------------------------------------

extern "C" {
    int gSBSEye = 0;
    int gSBSHudEye = 0;
    int gSBSSkipTextAccumulation = 0;
}

// ---------------------------------------------------------------------------
// Initialisation – call once at startup from PortEnhancements_Init()
// ---------------------------------------------------------------------------

void StereoRendering_Init() {
    CVarRegisterInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0);
    CVarRegisterFloat(CVAR_ENHANCEMENT("EyeSeparation"), 30.0f);
    /* HUD mapping tuning -- live-adjustable via the Enhancements menu. */

}

// ---------------------------------------------------------------------------
// Not used – menu is handled by AddWidget in GhostshipMenuEnhancements.cpp
// ---------------------------------------------------------------------------

void StereoRendering_DrawMenu() {
}
