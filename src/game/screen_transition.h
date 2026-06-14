#ifndef SCREEN_TRANSITION_H
#define SCREEN_TRANSITION_H

#include <libultra/types.h>
#include <libultra/gbi.h>

#include "macros.h"
#include "types.h"

enum TextureTransitionID {
    TEX_TRANS_STAR,
    TEX_TRANS_CIRCLE,
    TEX_TRANS_MARIO,
    TEX_TRANS_BOWSER
};

enum TextureTransitionType {
    TRANS_TYPE_MIRROR,
    TRANS_TYPE_CLAMP
};

extern_s s32 render_screen_transition(s8 fadeTimer, s8 transType, u8 transTime, struct WarpTransitionData *transData);
extern_s Gfx *geo_cannon_circle_base(s32 callContext, struct GraphNode *node, UNUSED Mat4 mtx);

/* Timer state -- exported so area.c can save/restore for SBS double-pass rendering. */
extern u8  sTransitionColorFadeCount[4];
extern u16 sTransitionTextureFadeCount[2];

#endif // SCREEN_TRANSITION_H
