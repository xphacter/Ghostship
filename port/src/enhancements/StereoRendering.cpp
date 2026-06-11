/**
 * StereoRendering.cpp
 * Side-by-side stereoscopic 3D enhancement for Ghostship.
 *
 * Registers CVars and exposes the ImGui menu toggle.
 * The actual camera-split logic lives in area.c / rendering_graph_node.c.
 */

#include "StereoRendering.h"
#include <libultraship/libultraship.h>
#include "port/ui/cvar_prefixes.h"

// ---------------------------------------------------------------------------
// Global definition (declared extern in StereoRendering.h)
// ---------------------------------------------------------------------------

extern "C" {
    int gSBSEye = 0;
}

// ---------------------------------------------------------------------------
// Initialisation – call once at startup (e.g. from GameMenuBar or main init)
// ---------------------------------------------------------------------------

void StereoRendering_Init() {
    CVarRegisterInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0);
    CVarRegisterFloat(CVAR_ENHANCEMENT("EyeSeparation"), 30.0f);
}

// ---------------------------------------------------------------------------
// ImGui menu fragment – drop inside your Graphics / Enhancements window
// ---------------------------------------------------------------------------

void StereoRendering_DrawMenu() {
    bool sbsEnabled = CVarGetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0) != 0;
    if (ImGui::Checkbox("Side-by-Side 3D", &sbsEnabled)) {
        CVarSetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), sbsEnabled ? 1 : 0);
        LUS::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
    if (sbsEnabled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        float sep = CVarGetFloat(CVAR_ENHANCEMENT("EyeSeparation"), 30.0f);
        if (ImGui::SliderFloat("Eye Separation", &sep, 0.0f, 200.0f, "%.0f")) {
            CVarSetFloat(CVAR_ENHANCEMENT("EyeSeparation"), sep);
            LUS::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Distance between left and right eye cameras in game units.\n"
                "~30 is a good starting point. Increase for stronger depth effect.\n"
                "Use with a 3D TV, VR headset, or anaglyph glasses + shader.");
        }
    }
}
