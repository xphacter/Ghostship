#include <libultra/types.h>

#include "prevent_bss_reordering.h"
#include "area.h"
#include "sm64.h"
#include "gfx_dimensions.h"
#include "behavior_data.h"
#include "game_init.h"
#include "object_list_processor.h"
#include "engine/surface_load.h"
#include "ingame_menu.h"
#include "screen_transition.h"
#include "mario.h"
#include "mario_actions_cutscene.h"
#include "print.h"
#include "hud.h"
#include "audio/external.h"
#include "area.h"
#include "rendering_graph_node.h"
#include "level_update.h"
#include "engine/geo_layout.h"
#include "save_file.h"
#include "level_table.h"
#include "dialog_ids.h"
#include "port/Enhancements/StereoRendering.h"

struct SpawnInfo gPlayerSpawnInfos[1];
struct GraphNode *D_8033A160[0x100];
struct Area gAreaData[8];

struct WarpTransition gWarpTransition;

s16 gCurrCourseNum;
s16 gCurrActNum;
s16 gCurrAreaIndex;
s16 gSavedCourseNum;
s16 gMenuOptSelectIndex;
s16 gSaveOptSelectIndex;
s8  gSBSActive = 0; /* 1 when SBS mode is enabled; updated each render_game() call */

struct SpawnInfo *gMarioSpawnInfo = &gPlayerSpawnInfos[0];
struct GraphNode **gLoadedGraphNodes = D_8033A160;
struct Area *gAreas = gAreaData;
struct Area *gCurrentArea = NULL;
struct CreditsEntry *gCurrCreditsEntry = NULL;
Vp *D_8032CE74 = NULL;
Vp *D_8032CE78 = NULL;
s16 gWarpTransDelay = 0;
u32 gFBSetColor = 0;
u32 gWarpTransFBSetColor = 0;
u8 gWarpTransRed = 0;
u8 gWarpTransGreen = 0;
u8 gWarpTransBlue = 0;
s16 gCurrSaveFileNum = 1;
s16 gCurrLevelNum = LEVEL_MIN;

/*
 * The following two tables are used in get_mario_spawn_type() to determine spawn type
 * from warp behavior.
 * When looping through sWarpBhvSpawnTable, if the behavior function in the table matches
 * the spawn behavior executed, the index of that behavior is used with sSpawnTypeFromWarpBhv
*/

const BehaviorScript *sWarpBhvSpawnTable[] = {
    bhvDoorWarp,                bhvStar,                   bhvExitPodiumWarp,          bhvWarp,
    bhvWarpPipe,                bhvFadingWarp,             bhvInstantActiveWarp,       bhvAirborneWarp,
    bhvHardAirKnockBackWarp,    bhvSpinAirborneCircleWarp, bhvDeathWarp,               bhvSpinAirborneWarp,
    bhvFlyingWarp,              bhvSwimmingWarp,           bhvPaintingStarCollectWarp, bhvPaintingDeathWarp,
    bhvAirborneStarCollectWarp, bhvAirborneDeathWarp,      bhvLaunchStarCollectWarp,   bhvLaunchDeathWarp,
};

u8 sSpawnTypeFromWarpBhv[] = {
    MARIO_SPAWN_DOOR_WARP,             MARIO_SPAWN_UNKNOWN_02,           MARIO_SPAWN_UNKNOWN_03,            MARIO_SPAWN_UNKNOWN_03,
    MARIO_SPAWN_UNKNOWN_03,            MARIO_SPAWN_TELEPORT,             MARIO_SPAWN_INSTANT_ACTIVE,        MARIO_SPAWN_AIRBORNE,
    MARIO_SPAWN_HARD_AIR_KNOCKBACK,    MARIO_SPAWN_SPIN_AIRBORNE_CIRCLE, MARIO_SPAWN_DEATH,                 MARIO_SPAWN_SPIN_AIRBORNE,
    MARIO_SPAWN_FLYING,                MARIO_SPAWN_SWIMMING,             MARIO_SPAWN_PAINTING_STAR_COLLECT, MARIO_SPAWN_PAINTING_DEATH,
    MARIO_SPAWN_AIRBORNE_STAR_COLLECT, MARIO_SPAWN_AIRBORNE_DEATH,       MARIO_SPAWN_LAUNCH_STAR_COLLECT,   MARIO_SPAWN_LAUNCH_DEATH,
};

Vp D_8032CF00 = { {
    { 640, 480, 511, 0 },
    { 640, 480, 511, 0 },
} };

#ifdef VERSION_EU
const char *gNoControllerMsg[] = {
    "NO CONTROLLER",
    "MANETTE DEBRANCHEE",
    "CONTROLLER FEHLT",
};
#endif

void override_viewport_and_clip(Vp *a, Vp *b, u8 c, u8 d, u8 e) {
    u16 sp6 = ((c >> 3) << 11) | ((d >> 3) << 6) | ((e >> 3) << 1) | 1;

    gFBSetColor = (sp6 << 16) | sp6;
    D_8032CE74 = a;
    D_8032CE78 = b;
}

void set_warp_transition_rgb(u8 red, u8 green, u8 blue) {
    u16 warpTransitionRGBA16 = ((red >> 3) << 11) | ((green >> 3) << 6) | ((blue >> 3) << 1) | 1;

    gWarpTransFBSetColor = (warpTransitionRGBA16 << 16) | warpTransitionRGBA16;
    gWarpTransRed = red;
    gWarpTransGreen = green;
    gWarpTransBlue = blue;
}

void print_intro_text(void) {
#ifdef VERSION_EU
    s32 language = eu_get_language();
#endif
    if ((gGlobalTimer & 31) < 20) {
        if (gControllerBits == 0) {
#ifdef VERSION_EU
            print_text_centered(SCREEN_WIDTH / 2, 20, gNoControllerMsg[language]);
#else
            print_text_centered(SCREEN_WIDTH / 2, 20, "NO CONTROLLER");
#endif
        } else {
#ifdef VERSION_EU
            print_text(20, 20, "START");
#else
            if (CVarGetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0)) {
                /* In SBS mode GFX_DIMENSIONS_FROM_LEFT_EDGE(60)≈7 maps off-screen
                 * after the world-pass correction.  Use a tunable CVar instead. */
                s32 sbsPressX = CVarGetInteger(CVAR_ENHANCEMENT("SBSPressStartX"), 60);
                print_text_centered(sbsPressX, 38, "PRESS");
                print_text_centered(sbsPressX, 20, "START");
            } else {
                print_text_centered(GFX_DIMENSIONS_FROM_LEFT_EDGE(60), 38, "PRESS");
                print_text_centered(GFX_DIMENSIONS_FROM_LEFT_EDGE(60), 20, "START");
            }
#endif
        }
    }
}

u32 get_mario_spawn_type(struct Object *o) {
    s32 i;
    const BehaviorScript *behavior = virtual_to_segmented(0x13, o->behavior);

    for (i = 0; i < 20; i++) {
        if (sWarpBhvSpawnTable[i] == behavior) {
            return sSpawnTypeFromWarpBhv[i];
        }
    }
    return 0;
}

struct ObjectWarpNode *area_get_warp_node(u8 id) {
    struct ObjectWarpNode *node = NULL;

    for (node = gCurrentArea->warpNodes; node != NULL; node = node->next) {
        if (node->node.id == id) {
            break;
        }
    }
    return node;
}

struct ObjectWarpNode *area_get_warp_node_from_params(struct Object *o) {
    u8 sp1F = (o->oBehParams & 0x00FF0000) >> 16;

    return area_get_warp_node(sp1F);
}

void load_obj_warp_nodes(void) {
    struct ObjectWarpNode *sp24;
    struct Object *sp20 = (struct Object *) gObjParentGraphNode.children;

    do {
        struct Object *sp1C = sp20;

        if (sp1C->activeFlags != ACTIVE_FLAG_DEACTIVATED && get_mario_spawn_type(sp1C) != 0) {
            sp24 = area_get_warp_node_from_params(sp1C);
            if (sp24 != NULL) {
                sp24->object = sp1C;
            }
        }
    } while ((sp20 = (struct Object *) sp20->header.gfx.node.next)
             != (struct Object *) gObjParentGraphNode.children);
}

void clear_areas(void) {
    s32 i;

    gCurrentArea = NULL;
    gWarpTransition.isActive = FALSE;
    gWarpTransition.pauseRendering = FALSE;
    gMarioSpawnInfo->areaIndex = -1;

    for (i = 0; i < 8; i++) {
        gAreaData[i].index = i;
        gAreaData[i].flags = 0;
        gAreaData[i].terrainType = 0;
        gAreaData[i].unk04 = NULL;
        gAreaData[i].terrainData = NULL;
        gAreaData[i].surfaceRooms = NULL;
        gAreaData[i].macroObjects = NULL;
        gAreaData[i].warpNodes = NULL;
        gAreaData[i].paintingWarpNodes = NULL;
        gAreaData[i].instantWarps = NULL;
        gAreaData[i].objectSpawnInfos = NULL;
        gAreaData[i].camera = NULL;
        gAreaData[i].unused = NULL;
        gAreaData[i].whirlpools[0] = NULL;
        gAreaData[i].whirlpools[1] = NULL;
        gAreaData[i].dialog[0] = DIALOG_NONE;
        gAreaData[i].dialog[1] = DIALOG_NONE;
        gAreaData[i].musicParam = 0;
        gAreaData[i].musicParam2 = 0;
    }
}

void clear_area_graph_nodes(void) {
    s32 i;

    if (gCurrentArea != NULL) {
        geo_call_global_function_nodes(&gCurrentArea->unk04->node, GEO_CONTEXT_AREA_UNLOAD);
        gCurrentArea = NULL;
        gWarpTransition.isActive = FALSE;
    }

    for (i = 0; i < 8; i++) {
        if (gAreaData[i].unk04 != NULL) {
            geo_call_global_function_nodes(&gAreaData[i].unk04->node, GEO_CONTEXT_AREA_INIT);
            gAreaData[i].unk04 = NULL;
        }
    }
}

void load_area(s32 index) {
    if (gCurrentArea == NULL && gAreaData[index].unk04 != NULL) {
        gCurrentArea = &gAreaData[index];
        gCurrAreaIndex = gCurrentArea->index;

        if (gCurrentArea->terrainData != NULL) {
            load_area_terrain(index, gCurrentArea->terrainData, gCurrentArea->surfaceRooms,
                              gCurrentArea->macroObjects);
        }

        if (gCurrentArea->objectSpawnInfos != NULL) {
            spawn_objects_from_info(0, gCurrentArea->objectSpawnInfos);
        }

        load_obj_warp_nodes();
        geo_call_global_function_nodes(&gCurrentArea->unk04->node, GEO_CONTEXT_AREA_LOAD);
    }
}

void unload_area(void) {
    if (gCurrentArea != NULL) {
        unload_objects_from_area(0, gCurrentArea->index);
        geo_call_global_function_nodes(&gCurrentArea->unk04->node, GEO_CONTEXT_AREA_UNLOAD);

        gCurrentArea->flags = 0;
        gCurrentArea = NULL;
        gWarpTransition.isActive = FALSE;
    }
}

void load_mario_area(void) {
    stop_sounds_in_continuous_banks();
    load_area(gMarioSpawnInfo->areaIndex);

    if (gCurrentArea->index == gMarioSpawnInfo->areaIndex) {
        gCurrentArea->flags |= 0x01;
        spawn_objects_from_info(0, gMarioSpawnInfo);
    }
}

void unload_mario_area(void) {
    if (gCurrentArea != NULL && (gCurrentArea->flags & 0x01)) {
        unload_objects_from_area(0, gMarioSpawnInfo->activeAreaIndex);

        gCurrentArea->flags &= ~0x01;
        if (gCurrentArea->flags == 0) {
            unload_area();
        }
    }
}

void change_area(s32 index) {
    s32 areaFlags = gCurrentArea->flags;

    if (gCurrAreaIndex != index) {
        unload_area();
        load_area(index);

        gCurrentArea->flags = areaFlags;
        gMarioObject->oActiveParticleFlags = 0;
    }

    if (areaFlags & 0x01) {
        gMarioObject->header.gfx.areaIndex = index, gMarioSpawnInfo->areaIndex = index;
    }
}

void area_update_objects(void) {
    gAreaUpdateCounter++;
    update_objects(0);
}

/*
 * Sets up the information needed to play a warp transition, including the
 * transition type, time in frames, and the RGB color that will fill the screen.
 */
void play_transition(s16 transType, s16 time, u8 red, u8 green, u8 blue) {
    gWarpTransition.isActive = TRUE;
    gWarpTransition.type = transType;
    gWarpTransition.time = time;
    gWarpTransition.pauseRendering = FALSE;

    // The lowest bit of transType determines if the transition is fading in or out.
    if (transType & 1) {
        set_warp_transition_rgb(red, green, blue);
    } else {
        red = gWarpTransRed, green = gWarpTransGreen, blue = gWarpTransBlue;
    }

    if (transType < 8) { // if transition is RGB
        gWarpTransition.data.red = red;
        gWarpTransition.data.green = green;
        gWarpTransition.data.blue = blue;
    } else { // if transition is textured
        gWarpTransition.data.red = red;
        gWarpTransition.data.green = green;
        gWarpTransition.data.blue = blue;

        // Both the start and end textured transition are always located in the middle of the screen.
        // If you really wanted to, you could place the start at one corner and the end at
        // the opposite corner. This will make the transition image look like it is moving
        // across the screen.
        gWarpTransition.data.startTexX = SCREEN_WIDTH / 2;
        gWarpTransition.data.startTexY = SCREEN_HEIGHT / 2;
        gWarpTransition.data.endTexX = SCREEN_WIDTH / 2;
        gWarpTransition.data.endTexY = SCREEN_HEIGHT / 2;

        gWarpTransition.data.texTimer = 0;

        if (transType & 1) { // Is the image fading in?
            gWarpTransition.data.startTexRadius = GFX_DIMENSIONS_FULL_RADIUS;
            if (transType >= 0x0F) {
                gWarpTransition.data.endTexRadius = 16;
            } else {
                gWarpTransition.data.endTexRadius = 0;
            }
        } else { // The image is fading out. (Reverses start & end circles)
            if (transType >= 0x0E) {
                gWarpTransition.data.startTexRadius = 16;
            } else {
                gWarpTransition.data.startTexRadius = 0;
            }
            gWarpTransition.data.endTexRadius = GFX_DIMENSIONS_FULL_RADIUS;
        }
    }
}

/*
 * Sets up the information needed to play a warp transition, including the
 * transition type, time in frames, and the RGB color that will fill the screen.
 * The transition will play only after a number of frames specified by 'delay'
 */
void play_transition_after_delay(s16 transType, s16 time, u8 red, u8 green, u8 blue, s16 delay) {
    gWarpTransDelay = delay; // Number of frames to delay playing the transition.
    play_transition(transType, time, red, green, blue);
}

void render_game(void) {
    /* Cache SBS state once per frame so behavior code (UPDATE phase) can read it
     * next frame without needing to call CVarGetInteger directly. */
    gSBSActive = (s8)(CVarGetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0) != 0);
    if (gCurrentArea != NULL && !gWarpTransition.pauseRendering) {
        if (CVarGetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0)) {
            /* Compute the camera's right vector (horizontal only).
             * right = forward × up with up = (0,1,0) simplifies to:
             *   right.x = -forward.z = -dz/len
             *   right.z =  forward.x =  dx/len  */
            f32 eyeSep = CVarGetFloat(CVAR_ENHANCEMENT("EyeSeparation"), 30.0f) * 0.5f;
            f32 dx = gLakituState.focus[0] - gLakituState.pos[0];
            f32 dz = gLakituState.focus[2] - gLakituState.pos[2];
            f32 len = sqrtf(dx * dx + dz * dz);
            f32 rx = 0.0f, rz = 0.0f;
            if (len > 0.001f) {
                rx = (-dz / len) * eyeSep;
                rz = ( dx / len) * eyeSep;
            }
            f32 savedX = gLakituState.pos[0];
            f32 savedZ = gLakituState.pos[2];

            /* Left eye */
            gSBSEye = -1;
            gLakituState.pos[0] = savedX - rx;
            gLakituState.pos[2] = savedZ - rz;
            geo_process_root(gCurrentArea->unk04, D_8032CE74, D_8032CE78, gFBSetColor);

            /* Right eye */
            gSBSEye = 1;
            gLakituState.pos[0] = savedX + rx;
            gLakituState.pos[2] = savedZ + rz;
            geo_process_root(gCurrentArea->unk04, D_8032CE74, D_8032CE78, 0);

            /* Restore and reset for HUD pass */
            gLakituState.pos[0] = savedX;
            gLakituState.pos[2] = savedZ;
            gSBSEye = 0;
        } else {
        geo_process_root(gCurrentArea->unk04, D_8032CE74, D_8032CE78, gFBSetColor);
        } /* end SBS branch */

        if (CVarGetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0) &&
            CVarGetInteger(CVAR_ENHANCEMENT("SBS_HideHUD"), 0)) {
            /* HUD suppressed — still must restore viewport/scissor and run
             * one-time-per-frame state-advancing calls (cutscene, text free). */
            gSBSHudEye = 0;
            gSPViewport(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(&D_8032CF00));
            gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                          SCREEN_HEIGHT - BORDER_HEIGHT);
            /* Must clear text labels and advance cutscene state exactly once.
             * Discard (not render) so PRESS START / HUD text queued this frame
             * doesn't get drawn — render_text_labels() would still draw the
             * queued glyphs, making the HUD-hide setting just blink the text
             * on its normal cadence instead of fully hiding it. */
            discard_text_labels();
            do_cutscene_handler();
            print_displaying_credits_entry();
            gMenuOptSelectIndex = render_menus_and_dialogs();
            if (gMenuOptSelectIndex != MENU_OPT_NONE) {
                gSaveOptSelectIndex = gMenuOptSelectIndex;
            }
        } else if (CVarGetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0)) {
            /* Render HUD, text and dialogs into both eye halves at screen depth.
             *
             * Key constraints:
             * - libultraship's GfxDrawRectangle bypasses gSPViewport for texture
             *   rectangles; only the scissor clips them.  We therefore shift the
             *   actual x-coordinates of each sprite/glyph via sbsHudBaseX() so
             *   they land inside the correct half.
             * - 3D menu geometry (shade, text triangles) uses a full-screen viewport
             *   combined with a per-eye ortho shift in create_dl_ortho_matrix()
             *   (gSBSHudEye drives the shift).  Half-screen viewports here would
             *   stack with that shift and double-compress the content.
             * - render_text_labels() frees its buffer after the first call, so we
             *   must NOT free on the left-eye pass (gSBSHudEye == -1).
             * - do_cutscene_handler() / print_displaying_credits_entry() must only
             *   run once per frame to avoid double-advancing game state.
             */

            /* Restore full-screen viewport coming out of the SBS world passes.
             * 3D menu elements use the ortho shift instead of a half-screen
             * viewport; texture-rect HUD elements bypass the viewport entirely. */
            gSPViewport(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(&D_8032CF00));

            /* --- Left eye HUD --- */
            /* Each eye pass is in its own compound block so that CALL_CANCELLABLE_EVENT's
             * local variable declarations (e.g. RenderHud_) don't collide. */
            {
                gSBSHudEye = -1;
                /* Embed eye state in the DL so the interpreter reads it at execute-time.
                 * gSBSHudEye is reset to 0 before exec_display_list() fires, so the
                 * interpreter cannot read the C global; G_SBS_HUD_EYE carries it instead. */
                gSBSHudSetEye(gDisplayListHead++, -1);
                gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                              0, BORDER_HEIGHT, SCREEN_WIDTH / 2, SCREEN_HEIGHT - BORDER_HEIGHT);
                CALL_CANCELLABLE_EVENT(RenderHud) { render_hud(); }

                gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                              0, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT);
                /* Left-eye text: renders labels but does NOT free them (gSBSHudEye == -1) */
                CALL_CANCELLABLE_EVENT(RenderTextLabels) { render_text_labels(); }
                /* State-advancing calls: run once only (left-eye pass) */
                do_cutscene_handler();
                print_displaying_credits_entry();

                gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                              0, BORDER_HEIGHT, SCREEN_WIDTH / 2, SCREEN_HEIGHT - BORDER_HEIGHT);
                gMenuOptSelectIndex = render_menus_and_dialogs();
                if (gMenuOptSelectIndex != MENU_OPT_NONE) {
                    gSaveOptSelectIndex = gMenuOptSelectIndex;
                }
            }

            /* --- Right eye HUD --- */
            {
                gSBSHudEye = 1;
                gSBSHudSetEye(gDisplayListHead++, 1);
                gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                              SCREEN_WIDTH / 2, BORDER_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - BORDER_HEIGHT);
                gSBSSkipTextAccumulation = 1; /* don't re-add labels already queued in left-eye pass */
                CALL_CANCELLABLE_EVENT(RenderHud) { render_hud(); }
                gSBSSkipTextAccumulation = 0;

                gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                              SCREEN_WIDTH / 2, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                /* Right-eye text: renders labels AND frees them (gSBSHudEye == 1) */
                CALL_CANCELLABLE_EVENT(RenderTextLabels) { render_text_labels(); }

                gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                              SCREEN_WIDTH / 2, BORDER_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - BORDER_HEIGHT);
                /* Right-eye pass is render-only: the left-eye pass already
                 * processed input and advanced game state this frame.
                 *
                 * Two specific hazards if we let the right-eye result land:
                 *  1. When unpausing, the left eye clears gMenuMode to NONE and
                 *     returns MENU_OPT_DEFAULT.  The right eye then sees
                 *     gMenuMode==NONE and returns MENU_OPT_NONE.  If that NONE
                 *     were written to gMenuOptSelectIndex, play_mode_paused()
                 *     would re-pause the game on the very next frame.
                 *  2. gDialogColorFadeTimer is incremented inside
                 *     render_menus_and_dialogs(); calling it twice per frame
                 *     makes palette fades run at double speed.
                 *
                 * Fix: snapshot both before the call and restore afterward,
                 * keeping only a non-NONE right-eye result (edge case guard). */
                {
                    /*
                     * Save ALL mutable dialog/menu state before the right-eye
                     * render-only call.  The left-eye pass already advanced
                     * every timer and text-position variable for this frame;
                     * the right-eye call must only DRAW, not advance.
                     *
                     * Variables not previously saved (root cause of the
                     * dialog crash and infinite-repeat bugs):
                     *   gDialogTextPos       -- character index in dialog string
                     *   gDialogLineNum       -- which line is being drawn
                     *   gLastDialogLineNum   -- line number from previous frame
                     *   gLastDialogPageStrPos-- string position of last page break
                     *   gLastDialogResponse  -- YES/NO response tracking
                     *   gMenuHoldKeyIndex    -- which key is held
                     *   gMenuHoldKeyTimer    -- hold-repeat timer
                     *   gDialogResponse      -- final response value
                     *   gDialogTextAlpha     -- text fade alpha
                     *   gDialogVariable      -- NPC variable (e.g. star count)
                     *   gCutsceneMsgXOffset  -- cutscene subtitle position
                     *   gCutsceneMsgYOffset
                     *
                     * Additional hazard (root cause of the SBS "can't pause" bug):
                     *   On the first frame of pausing, the left eye runs
                     *   DIALOG_STATE_OPENING (no button check) and the right eye
                     *   then runs DIALOG_STATE_VERTICAL where it detects the same
                     *   buttonPressed event that triggered the pause.  This causes
                     *   the right eye to return MENU_OPT_DEFAULT (continue/unpause),
                     *   which immediately unpauses the game on the next frame before
                     *   the player ever sees the pause screen.
                     *
                     *   Fix: zero gPlayer3Controller->buttonPressed for the right-eye
                     *   call so it is render-only.  The left eye already handled all
                     *   button input for this frame.  Also save/restore gMenuMode so
                     *   any right-eye button handler that sneaks through cannot leave
                     *   the mode in an inconsistent state.
                     */
                    /* Variables already declared in ingame_menu.h */
                    u16 savedFadeTimer       = gDialogColorFadeTimer;
                    s8  savedBoxState        = gDialogBoxState;
                    f32 savedBoxTimer        = gDialogBoxOpenTimer;
                    f32 savedBoxScale        = gDialogBoxScale;
                    s16 savedScrollOffset    = gDialogScrollOffsetY;
                    s32 savedDialogResponse  = gDialogResponse;
                    u16 savedTextAlpha       = gDialogTextAlpha;
                    s8  savedLastLineNum     = gLastDialogLineNum;
                    s32 savedDialogVariable  = gDialogVariable;
                    s16 savedCutsceneMsgX    = gCutsceneMsgXOffset;
                    s16 savedCutsceneMsgY    = gCutsceneMsgYOffset;
                    /* Variables only defined in ingame_menu.c -- forward-declare here */
                    extern s16 gDialogTextPos;
                    extern s8  gDialogLineNum;
                    extern s16 gLastDialogPageStrPos;
                    extern s8  gLastDialogResponse;
                    extern u8  gMenuHoldKeyIndex;
                    extern u8  gMenuHoldKeyTimer;
                    extern s16 gMenuMode;
                    s16 savedTextPos         = gDialogTextPos;
                    s8  savedLineNum         = gDialogLineNum;
                    s16 savedLastPageStrPos  = gLastDialogPageStrPos;
                    s8  savedLastResponse    = gLastDialogResponse;
                    u8  savedHoldKeyIndex    = gMenuHoldKeyIndex;
                    u8  savedHoldKeyTimer    = gMenuHoldKeyTimer;
                    /* Prevent right-eye from processing button inputs -- it is render-only.
                     * Also save gMenuMode; the button handler in render_pause_courses_and_castle
                     * sets it to MENU_MODE_NONE, which must not persist after right-eye. */
                    s16 savedMenuMode        = gMenuMode; /* gMenuMode extern declared above */
                    u16 savedButtonPressed   = gPlayer3Controller->buttonPressed;
                    gPlayer3Controller->buttonPressed = 0;

                    s16 rightIndex = render_menus_and_dialogs();

                    /* Restore all state so this frame's net advance = 1x (left eye only). */
                    gDialogColorFadeTimer = savedFadeTimer;
                    gDialogBoxState       = savedBoxState;
                    gDialogBoxOpenTimer   = savedBoxTimer;
                    gDialogBoxScale       = savedBoxScale;
                    gDialogScrollOffsetY  = savedScrollOffset;
                    gDialogResponse       = savedDialogResponse;
                    gDialogTextAlpha      = savedTextAlpha;
                    gLastDialogLineNum    = savedLastLineNum;
                    gDialogVariable       = savedDialogVariable;
                    gCutsceneMsgXOffset   = savedCutsceneMsgX;
                    gCutsceneMsgYOffset   = savedCutsceneMsgY;
                    gDialogTextPos        = savedTextPos;
                    gDialogLineNum        = savedLineNum;
                    gLastDialogPageStrPos = savedLastPageStrPos;
                    gLastDialogResponse   = savedLastResponse;
                    gMenuHoldKeyIndex     = savedHoldKeyIndex;
                    gMenuHoldKeyTimer     = savedHoldKeyTimer;
                    gMenuMode             = savedMenuMode;
                    gPlayer3Controller->buttonPressed = savedButtonPressed;

                    if (rightIndex != MENU_OPT_NONE) {
                        gMenuOptSelectIndex = rightIndex;
                        gSaveOptSelectIndex = rightIndex;
                    }
                }
            }

            /* Restore for warp transitions below */
            gSBSHudEye = 0;
            gSBSHudSetEye(gDisplayListHead++, 0); /* reset interpreter eye state */
            gSPViewport(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(&D_8032CF00));
            gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                          SCREEN_HEIGHT - BORDER_HEIGHT);
        } else {
            gSPViewport(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(&D_8032CF00));
            gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                          SCREEN_HEIGHT - BORDER_HEIGHT);
            CALL_CANCELLABLE_EVENT(RenderHud) { render_hud(); }

            gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            CALL_CANCELLABLE_EVENT(RenderTextLabels) { render_text_labels(); }
            do_cutscene_handler();
            print_displaying_credits_entry();

            gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                          SCREEN_HEIGHT - BORDER_HEIGHT);
            gMenuOptSelectIndex = render_menus_and_dialogs();
            if (gMenuOptSelectIndex != MENU_OPT_NONE) {
                gSaveOptSelectIndex = gMenuOptSelectIndex;
            }
        }

        if (D_8032CE78 != NULL) {
            make_viewport_clip_rect(D_8032CE78);
        } else {
            gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                          SCREEN_HEIGHT - BORDER_HEIGHT);
        }

        if (gWarpTransition.isActive) {
            if (gWarpTransDelay == 0) {
                s32 transitionDone;
                if (CVarGetInteger(CVAR_ENHANCEMENT("Stereoscopic3D"), 0)) {
                    /*
                     * SBS double-pass transition rendering.
                     *
                     * render_screen_transition() calls dl_proj_mtx_fullscreen which bakes
                     * a full-screen viewport.  screen_transition.c's sbs_transition_viewport()
                     * helper overrides that viewport with the appropriate half-screen one
                     * when gSBSEye != 0, so the full transition effect maps into each half.
                     *
                     * Both passes must render the same animation frame, so we save the
                     * timer state before the first call and restore it before the second.
                     * After the second call the timers sit at "advanced-once" state, correct.
                     */
                    u8  savedColorCount[4];
                    u16 savedTexCount[2];
                    memcpy(savedColorCount, sTransitionColorFadeCount, sizeof(savedColorCount));
                    memcpy(savedTexCount,   sTransitionTextureFadeCount, sizeof(savedTexCount));

                    /* Left eye */
                    gSBSEye = -1;
                    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                                  0, BORDER_HEIGHT, SCREEN_WIDTH / 2, SCREEN_HEIGHT - BORDER_HEIGHT);
                    transitionDone = render_screen_transition(0, gWarpTransition.type, gWarpTransition.time,
                                                              &gWarpTransition.data);

                    /* Restore timers so the right eye renders the same frame */
                    memcpy(sTransitionColorFadeCount,  savedColorCount, sizeof(savedColorCount));
                    memcpy(sTransitionTextureFadeCount, savedTexCount,   sizeof(savedTexCount));

                    /* Right eye */
                    gSBSEye = 1;
                    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                                  SCREEN_WIDTH / 2, BORDER_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - BORDER_HEIGHT);
                    render_screen_transition(0, gWarpTransition.type, gWarpTransition.time,
                                             &gWarpTransition.data);

                    /* Restore normal state */
                    gSBSEye = 0;
                    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE,
                                  0, BORDER_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT - BORDER_HEIGHT);
                } else {
                    transitionDone = render_screen_transition(0, gWarpTransition.type, gWarpTransition.time,
                                                              &gWarpTransition.data);
                }
                gWarpTransition.isActive = !transitionDone;
                if (!gWarpTransition.isActive) {
                    if (gWarpTransition.type & 1) {
                        gWarpTransition.pauseRendering = TRUE;
                    } else {
                        set_warp_transition_rgb(0, 0, 0);
                    }
                }
            } else {
                gWarpTransDelay--;
            }
        }
    } else {
        CALL_CANCELLABLE_EVENT(RenderTextLabels) {
            render_text_labels();
        }
        if (D_8032CE78 != NULL) {
            clear_viewport(D_8032CE78, gWarpTransFBSetColor);
        } else {
            clear_framebuffer(gWarpTransFBSetColor);
        }
    }

    D_8032CE74 = NULL;
    D_8032CE78 = NULL;
}
