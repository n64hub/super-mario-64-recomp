#include "patches.h"
#include "model_ids.h"
#include "object_constants.h"
#include "object_fields.h"
#include "surface_terrains.h"

#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "engine/surface_load.h"
#include "game/area.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/memory.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"

// Draw Distance (its Graphics option, src/game/config.cpp). The game hides an object while Mario is further from it
// than its drawing distance (oDrawingDistance, 4000 units for most): for most objects at the end of their update
// (cur_obj_update, behavior_script.c), for an object with collision in load_object_collision_model. Hidden, an object
// is still updated, but its animation doesn't advance (geo_process_object advances it as it draws it), and the game
// reads what it draws: a hidden object makes no sound (spawn_sound.c), and some behaviours read its flags.
//
// So the game decides every flag as it does, and an object it hid only for its distance is drawn by the renderer
// alone, out to the option's scale times that distance: interpolation.c's geo_process_node_and_siblings asks
// draw_distance_begin for every object the game hides, draws it with geo_process_object and calls draw_distance_end.
// Everything the draw writes into the object (its animation's frame, its camera-relative position, which places its
// sounds, its throw matrix, whatever its model's geo functions write, and the object it holds, which a model like
// Ukiki's moves) is restored after it, so the game's state is the same as at Original. The animation drawn advances in
// a copy of its own (sFarAnim), from the game's frame when the object was last drawn, so an object out there moves as
// it does nearby; back inside the game's distance the game's own frame takes over, from where it stopped.
//
// An object the game hid for another reason stays hidden: one in a room (its room decides), one held, one that hides
// itself with cur_obj_disable_rendering after loading its collision. A hidden object (GRAPH_RENDER_INVISIBLE) stays
// out of view (obj_is_in_view).
//
// A coin formation (a line, ring or arrow of coins, bhvCoinFormation) spawns its coins only once Mario is within 2000
// units of it and deletes them past 2100 (bhv_coin_formation_loop), so further out there is no coin to draw: on High a
// ring's lone middle coin showed from across a course and the ring around it didn't. draw_distance_coins draws the
// coins a formation would spawn as far as a lone coin is drawn, the option's scale times the formation's drawing
// distance (4000, a coin's too), with no object made: each where spawn_coin_in_formation and
// bhv_coin_formation_spawn_loop would put it (on the floor, or left out where theirs is deleted), the ones collected
// left out, from a scratch object (sFarCoin) the game never sees. interpolation.c tags each by its formation and index
// (draw_distance_tag), since the scratch object is the same for all. Inside 2000 the game's own coins take over in the
// same places. A level with rooms (sLevelsWithRooms) gets none, as its coins' rooms decide them. Goombas from their
// spawner still appear only where they did.

// The game's own functions and data this file uses that its headers leave out (surface_load.c's and
// rendering_graph_node.c's functions are static).
void transform_object_vertices(TerrainData **data, TerrainData *vertexData);
void load_object_surfaces(TerrainData **data, TerrainData *vertexData);
void geo_process_object(struct Object *node);
extern s8 sLevelsWithRooms[];
extern s16 sCoinArrowPositions[][2];

// bhvCoinFormation, by its address in the behaviors' segment (0x13), and the distance its coins spawn within.
#define BHV_COIN_FORMATION ((const BehaviorScript *) 0x130008EC)
#define COIN_FORMATION_SPAWN_DIST 2000.0f

// For each pool object that load_object_collision_model hid for its distance: its behaviour when it did (so a slot
// used again by another object doesn't count) and Mario's distance then. NULL when it showed it.
static const BehaviorScript *sCollisionHidden[OBJECT_POOL_CAPACITY];
static f32 sCollisionHiddenDist[OBJECT_POOL_CAPACITY];

// The animation each object drawn here was last drawn with, and the game frame (gAreaUpdateCounter) it was drawn in.
static struct AnimInfo sFarAnim[OBJECT_POOL_CAPACITY];
static u16 sFarAnimFrame[OBJECT_POOL_CAPACITY];

// The object being drawn and the one it holds, as the game left them.
static struct Object sSavedObj;
static struct Object sSavedHeld;
static struct Object *sHeld;

static s32 sScale;
static u32 sScaleFrame = 0xFFFFFFFF;

// The coin draw_distance_coins draws, and the id interpolation.c tags it by (0 while none is drawn).
static struct Object sFarCoin;
u32 draw_distance_tag = 0;

static s32 scale(void) {
    if (sScaleFrame != gGlobalTimer) {
        sScale = recomp_get_draw_distance_scale();
        sScaleFrame = gGlobalTimer;
    }
    return sScale;
}

static s32 pool_slot(struct Object *obj) {
    u32 offset = (u32) obj - (u32) gObjectPool;
    if (offset < OBJECT_POOL_CAPACITY * sizeof(struct Object)) {
        return offset / sizeof(struct Object);
    }
    return -1;
}

static void forget_hidden(struct Object *obj) {
    s32 i = pool_slot(obj);
    if (i >= 0) {
        sCollisionHidden[i] = NULL;
    }
}

// Whether the game hid obj only for its distance, and Mario is within the option's scale of that distance.
static s32 hidden_by_distance(struct Object *obj, s32 i) {
    f32 dist;

    if (!(obj->activeFlags & ACTIVE_FLAG_ACTIVE) || obj->oRoom != -1) {
        return FALSE;
    }
    if (obj->collisionData != NULL) {
        // load_object_collision_model's choice, if it was the last to hide the object.
        if (sCollisionHidden[i] != obj->behavior) {
            return FALSE;
        }
        dist = sCollisionHiddenDist[i];
    } else {
        // cur_obj_update's, the last thing in an update.
        if (!(obj->oFlags & OBJ_FLAG_COMPUTE_DIST_TO_MARIO) || (obj->oFlags & OBJ_FLAG_ACTIVE_FROM_AFAR)
            || obj->oHeldState != HELD_FREE) {
            return FALSE;
        }
        dist = obj->oDistanceToMario;
        if (!(dist > obj->oDrawingDistance)) {
            return FALSE;
        }
    }
    return dist < obj->oDrawingDistance * sScale;
}

// Called for an object the game doesn't draw: whether to draw it anyway. If so, the caller draws it with
// geo_process_object and then calls draw_distance_end.
s32 draw_distance_begin(struct Object *obj) {
    s32 i = pool_slot(obj);

    if (i < 0 || scale() <= 1 || !hidden_by_distance(obj, i)) {
        return FALSE;
    }

    bcopy(obj, &sSavedObj, sizeof(struct Object));
    sHeld = obj->prevObj;
    if (sHeld != NULL && sHeld != obj) {
        bcopy(sHeld, &sSavedHeld, sizeof(struct Object));
    } else {
        sHeld = NULL;
    }

    // The last frame's animation goes on if it was drawn here then and the game hasn't changed animation since.
    if ((u16) (gAreaUpdateCounter - sFarAnimFrame[i]) <= 1 && sFarAnim[i].curAnim != NULL
        && sFarAnim[i].curAnim == obj->header.gfx.animInfo.curAnim
        && sFarAnim[i].animID == obj->header.gfx.animInfo.animID) {
        obj->header.gfx.animInfo = sFarAnim[i];
    }
    return TRUE;
}

void draw_distance_end(struct Object *obj) {
    s32 i = pool_slot(obj);

    sFarAnim[i] = obj->header.gfx.animInfo;
    sFarAnimFrame[i] = gAreaUpdateCounter;
    bcopy(&sSavedObj, obj, sizeof(struct Object));
    if (sHeld != NULL) {
        bcopy(&sSavedHeld, sHeld, sizeof(struct Object));
    }
}

// Called for every object of the scene after the game's own drawing of it: for a coin formation that hasn't spawned its
// coins for Mario's distance, with Mario within the option's scale of the formation's drawing distance, draws the coins
// it would spawn.
void draw_distance_coins(struct Object *obj) {
    s32 slot = pool_slot(obj);
    s32 shape;
    s32 i;

    if (slot < 0 || obj->behavior != segmented_to_virtual(BHV_COIN_FORMATION)
        || !(obj->activeFlags & ACTIVE_FLAG_ACTIVE) || obj->oAction != COIN_FORMATION_ACT_SPAWN_COINS
        || obj->oDistanceToMario < COIN_FORMATION_SPAWN_DIST || scale() <= 1
        || !(obj->oDistanceToMario < obj->oDrawingDistance * sScale)
        || is_item_in_array(gCurrLevelNum, sLevelsWithRooms)) {
        return;
    }
    shape = obj->oBhvParams2ndByte & COIN_FORMATION_BP_FLAG_MASK;

    for (i = 0; i <= 7; i++) {
        s32 pos[3] = { 0, 0, 0 };
        s32 onGround = TRUE;
        s32 model = MODEL_YELLOW_COIN;
        struct Surface *floor;
        f32 floorHeight;

        // A line has 5 coins.
        if ((obj->oCoinCollectedFlags & (1 << i))
            || ((shape == COIN_FORMATION_BP_LINE_HORIZONTAL || shape == COIN_FORMATION_BP_LINE_VERTICAL) && i > 4)) {
            continue;
        }

        // spawn_coin_in_formation's place for coin i.
        switch (shape) {
            case COIN_FORMATION_BP_LINE_HORIZONTAL:
                pos[2] = 160 * (i - 2);
                break;
            case COIN_FORMATION_BP_LINE_VERTICAL:
                onGround = FALSE;
                pos[1] = 160 * i * 0.8;
                break;
            case COIN_FORMATION_BP_RING_HORIZONTAL:
                pos[0] = sins(i << 13) * 300.0f;
                pos[2] = coss(i << 13) * 300.0f;
                break;
            case COIN_FORMATION_BP_RING_VERTICAL:
                onGround = FALSE;
                pos[0] = coss(i << 13) * 200.0f;
                pos[1] = sins(i << 13) * 200.0f + 200.0f;
                break;
            case COIN_FORMATION_BP_ARROW:
                pos[0] = sCoinArrowPositions[i][0];
                pos[2] = sCoinArrowPositions[i][1];
                break;
        }
        if (obj->oBhvParams2ndByte & COIN_FORMATION_BP_FLAG_FLYING) {
            onGround = FALSE;
        }

        // spawn_object_relative's position, from the formation's own.
        sFarCoin = *obj;
        sFarCoin.oParentRelativePosX = (s16) pos[0];
        sFarCoin.oParentRelativePosY = (s16) pos[1];
        sFarCoin.oParentRelativePosZ = (s16) pos[2];
        obj_build_transform_from_pos_and_angle(&sFarCoin, O_PARENT_RELATIVE_POS_INDEX, O_FACE_ANGLE_INDEX);
        obj_translate_local(&sFarCoin, O_POS_INDEX, O_PARENT_RELATIVE_POS_INDEX);

        // bhv_coin_formation_spawn_loop's first frame: on the floor (or deleted), or without its shadow in the air.
        if (onGround) {
            sFarCoin.oPosY += 300.0f;
            floorHeight = find_floor(sFarCoin.oPosX, sFarCoin.oPosY, sFarCoin.oPosZ, &floor);
            if (sFarCoin.oPosY < floorHeight || floorHeight < FLOOR_LOWER_LIMIT_MISC) {
                continue;
            }
            sFarCoin.oPosY = floorHeight;
        } else {
            floorHeight = find_floor(sFarCoin.oPosX, sFarCoin.oPosY, sFarCoin.oPosZ, &floor);
            if (absf(sFarCoin.oPosY - floorHeight) > 250.0f) {
                model = MODEL_YELLOW_COIN_NO_SHADOW;
            }
        }
        if (gLoadedGraphNodes[model] == NULL) {
            continue;
        }

        sFarCoin.header.gfx.sharedChild = gLoadedGraphNodes[model];
        sFarCoin.header.gfx.node.flags = GRAPH_RENDER_ACTIVE | GRAPH_RENDER_BILLBOARD;
        sFarCoin.header.gfx.node.children = NULL;
        sFarCoin.header.gfx.throwMatrix = NULL;
        sFarCoin.header.gfx.animInfo.curAnim = NULL;
        sFarCoin.header.gfx.pos[0] = sFarCoin.oPosX;
        sFarCoin.header.gfx.pos[1] = sFarCoin.oPosY;
        sFarCoin.header.gfx.pos[2] = sFarCoin.oPosZ;
        vec3f_set(sFarCoin.header.gfx.scale, 1.0f, 1.0f, 1.0f);
        sFarCoin.prevObj = NULL;
        sFarCoin.oAnimState = gGlobalTimer;

        draw_distance_tag = ((u32) slot << 3 | i) + 1;
        geo_process_object(&sFarCoin);
        draw_distance_tag = 0;
    }
}

// The game's (surface_load.c), noting an object it hides for its distance.
RECOMP_PATCH void load_object_collision_model(void) {
    TerrainData vertexData[600];

    TerrainData *collisionData = gCurrentObject->collisionData;
    f32 marioDist = gCurrentObject->oDistanceToMario;
    f32 tangibleDist = gCurrentObject->oCollisionDistance;
    s32 i = pool_slot(gCurrentObject);

    // On an object's first frame, the distance is set to 19000.0f.
    // If the distance hasn't been updated, update it now.
    if (gCurrentObject->oDistanceToMario == 19000.0f) {
        marioDist = dist_between_objects(gCurrentObject, gMarioObject);
    }

    // If the object collision is supposed to be loaded more than the
    // drawing distance of 4000, extend the drawing range.
    if (gCurrentObject->oCollisionDistance > 4000.0f) {
        gCurrentObject->oDrawingDistance = gCurrentObject->oCollisionDistance;
    }

    // Update if no Time Stop, in range, and in the current room.
    if (!(gTimeStopState & TIME_STOP_ACTIVE) && marioDist < tangibleDist
        && !(gCurrentObject->activeFlags & ACTIVE_FLAG_IN_DIFFERENT_ROOM)) {
        collisionData++;
        transform_object_vertices(&collisionData, vertexData);

        // TERRAIN_LOAD_CONTINUE acts as an "end" to the terrain data.
        while (*collisionData != TERRAIN_LOAD_CONTINUE) {
            load_object_surfaces(&collisionData, vertexData);
        }
    }

    if (marioDist < gCurrentObject->oDrawingDistance) {
        gCurrentObject->header.gfx.node.flags |= GRAPH_RENDER_ACTIVE;
        if (i >= 0) {
            sCollisionHidden[i] = NULL;
        }
    } else {
        gCurrentObject->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;
        if (i >= 0) {
            sCollisionHidden[i] = gCurrentObject->behavior;
            sCollisionHiddenDist[i] = marioDist;
        }
    }
}

// The game's (object_helpers.c): an object that hides itself after loading its collision stays hidden.
RECOMP_PATCH void cur_obj_disable_rendering_and_become_intangible(struct Object *obj) {
    obj->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;
    obj->oIntangibleTimer = -1;
    forget_hidden(obj);
}

RECOMP_PATCH void cur_obj_disable_rendering(void) {
    gCurrentObject->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;
    forget_hidden(gCurrentObject);
}
