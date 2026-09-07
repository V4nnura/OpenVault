#include "game/gmouse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "game/actions.h"
#include "game/art.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/map.h"
#include "game/object.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/scripts.h"
#include "game/skilldex.h"
#include "game/tile.h"
#include "game/tweaks.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/kb.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

typedef enum ScrollableDirections {
    SCROLLABLE_W = 0x01,
    SCROLLABLE_E = 0x02,
    SCROLLABLE_N = 0x04,
    SCROLLABLE_S = 0x08,
} ScrollableDirections;

static int gmouse_3d_init();
static int gmouse_3d_reset();
static void gmouse_3d_exit();
static int gmouse_3d_lock_frames();
static void gmouse_3d_unlock_frames();
static int gmouse_3d_set_flat_fid(int fid, Rect* rect);
static int gmouse_3d_reset_flat_fid(Rect* rect);
static int gmouse_3d_move_to(int x, int y, int elevation, Rect* a4);
static int gmouse_check_scrolling(int x, int y, int cursor);
static int gmouse_3d_determine_auto_mode(int mouseX, int mouseY, int elevation);
static void gmouse_tooltip_update(Object* target, int mouseX, int mouseY);

// 0x505258
static bool gmouse_initialized = false;

// 0x50525C
static int gmouse_enabled = 0;

// 0x505260
static int gmouse_mapper_mode = 0;

// 0x505264
static int gmouse_click_to_scroll = 0;

// 0x505268
static int gmouse_scrolling_enabled = 1;

// 0x50526C
static int gmouse_current_cursor = MOUSE_CURSOR_NONE;

// 0x505270
static CacheEntry* gmouse_current_cursor_key = INVALID_CACHE_ENTRY;

// 0x505274
static int gmouse_cursor_nums[MOUSE_CURSOR_TYPE_COUNT] = {
    266,
    267,
    268,
    269,
    270,
    271,
    272,
    273,
    274,
    275,
    276,
    277,
    330,
    331,
    329,
    328,
    332,
    334,
    333,
    335,
    279,
    280,
    281,
    293,
    310,
    278,
    295,
};

// 0x5052E0
static bool gmouse_3d_initialized = false;

// 0x5052E4
static bool gmouse_3d_hover_test = false;

// 0x5052E8
static unsigned int gmouse_3d_last_move_time = 0;

// actmenu.frm
// 0x5052EC
static Art* gmouse_3d_menu_frame = NULL;

// 0x5052F0
static CacheEntry* gmouse_3d_menu_frame_key = INVALID_CACHE_ENTRY;

// 0x5052F4
static int gmouse_3d_menu_frame_width = 0;

// 0x5052F8
static int gmouse_3d_menu_frame_height = 0;

// 0x5052FC
static int gmouse_3d_menu_frame_size = 0;

// 0x505300
static int gmouse_3d_menu_frame_hot_x = 0;

// 0x505304
static int gmouse_3d_menu_frame_hot_y = 0;

// 0x505308
static unsigned char* gmouse_3d_menu_frame_data = NULL;

// actpick.frm
// 0x50530C
static Art* gmouse_3d_pick_frame = NULL;

// 0x505310
static CacheEntry* gmouse_3d_pick_frame_key = INVALID_CACHE_ENTRY;

// 0x505314
static int gmouse_3d_pick_frame_width = 0;

// 0x505318
static int gmouse_3d_pick_frame_height = 0;

// 0x50531C
static int gmouse_3d_pick_frame_size = 0;

// 0x505320
static int gmouse_3d_pick_frame_hot_x = 0;

// 0x505324
static int gmouse_3d_pick_frame_hot_y = 0;

// 0x505328
static unsigned char* gmouse_3d_pick_frame_data = NULL;

// acttohit.frm
// 0x50532C
static Art* gmouse_3d_to_hit_frame = NULL;

// 0x505330
static CacheEntry* gmouse_3d_to_hit_frame_key = INVALID_CACHE_ENTRY;

// 0x505334
static int gmouse_3d_to_hit_frame_width = 0;

// 0x505338
static int gmouse_3d_to_hit_frame_height = 0;

// 0x50533C
static int gmouse_3d_to_hit_frame_size = 0;

// 0x505340
static unsigned char* gmouse_3d_to_hit_frame_data = NULL;

// blank.frm
// 0x505344
static Art* gmouse_3d_hex_base_frame = NULL;

// 0x505348
static CacheEntry* gmouse_3d_hex_base_frame_key = INVALID_CACHE_ENTRY;

// 0x50534C
static int gmouse_3d_hex_base_frame_width = 0;

// 0x505350
static int gmouse_3d_hex_base_frame_height = 0;

// 0x505354
static int gmouse_3d_hex_base_frame_size = 0;

// 0x505358
static unsigned char* gmouse_3d_hex_base_frame_data = NULL;

// msef000.frm
// 0x50535C
static Art* gmouse_3d_hex_frame = NULL;

// 0x505360
static CacheEntry* gmouse_3d_hex_frame_key = INVALID_CACHE_ENTRY;

// 0x505364
static int gmouse_3d_hex_frame_width = 0;

// 0x505368
static int gmouse_3d_hex_frame_height = 0;

// 0x50536C
static int gmouse_3d_hex_frame_size = 0;

// 0x505370
static unsigned char* gmouse_3d_hex_frame_data = NULL;

// 0x505374
static unsigned char gmouse_3d_menu_available_actions = 0;

// 0x505378
static unsigned char* gmouse_3d_menu_actions_start = NULL;

// 0x50537C
static unsigned char gmouse_3d_menu_current_action_index = 0;

// 0x50537E
static short gmouse_3d_action_nums[GAME_MOUSE_ACTION_MENU_ITEM_COUNT] = {
    253, // Cancel
    255, // Drop
    257, // Inventory
    259, // Look
    261, // Rotate
    263, // Talk
    265, // Use/Get
    302, // Unload
    304, // Skill
};

// 0x505390
static int gmouse_3d_modes_enabled = 1;

// 0x505394
static int gmouse_3d_current_mode = GAME_MOUSE_MODE_MOVE;

// 0x505398
static int gmouse_3d_mode_nums[GAME_MOUSE_MODE_COUNT] = {
    249,
    250,
    251,
    293,
    293,
    293,
    293,
    293,
    293,
    293,
    293,
};

// 0x5053C4
static int gmouse_skill_table[GAME_MOUSE_MODE_SKILL_COUNT] = {
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_SCIENCE,
    SKILL_REPAIR,
};

// 0x5053E0
static int gmouse_wait_cursor_frame = 0;

// 0x5053E4
static unsigned int gmouse_wait_cursor_time = 0;

// 0x5053E8
static int gmouse_bk_last_cursor = -1;

// 0x5053EC
static bool gmouse_3d_item_highlight = true;

// 0x5053F0
static Object* outlined_object = NULL;

// 0x5053F4
bool gmouse_clicked_on_edge = false;

// Object tooltip window
static int gmouse_tooltip_win = -1;
static bool gmouse_tooltip_visible = false;
static Object* gmouse_tooltip_object = NULL;

// Accessibility highlight state
static bool accessibility_highlight_active = false;

// Maximum width for tooltip (characters can have long names)
static const int GMOUSE_TOOLTIP_MAX_WIDTH = 200;
static const int GMOUSE_TOOLTIP_PADDING = 4;

// 0x5951E0
static int gmouse_3d_menu_frame_actions[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];

// 0x595204
static int gmouse_3d_last_mouse_x;

// 0x595208
static int gmouse_3d_last_mouse_y;

// 0x59520C
Object* obj_mouse;

// 0x595210
Object* obj_mouse_flat;

// 0x442EA0
int gmouse_init()
{
    if (gmouse_initialized) {
        return -1;
    }

    if (gmouse_3d_init() != 0) {
        return -1;
    }

    gmouse_initialized = true;
    gmouse_enabled = 1;

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    return 0;
}

// 0x442ED8
int gmouse_reset()
{
    if (!gmouse_initialized) {
        return -1;
    }

    // NOTE: Uninline.
    if (gmouse_3d_reset() != 0) {
        return -1;
    }

    // NOTE: Uninline.
    gmouse_enable();

    gmouse_scrolling_enabled = 1;
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
    gmouse_wait_cursor_frame = 0;
    gmouse_wait_cursor_time = 0;
    gmouse_clicked_on_edge = 0;

    return 0;
}

// 0x442FA8
void gmouse_exit()
{
    if (!gmouse_initialized) {
        return;
    }

    mouse_hide();

    mouse_set_shape(NULL, 0, 0, 0, 0, 0, 0);

    // NOTE: Uninline.
    gmouse_3d_exit();

    if (gmouse_current_cursor_key != INVALID_CACHE_ENTRY) {
        art_ptr_unlock(gmouse_current_cursor_key);
    }
    gmouse_current_cursor_key = INVALID_CACHE_ENTRY;

    gmouse_enabled = 0;
    gmouse_initialized = false;
    gmouse_current_cursor = -1;
}

// 0x443044
void gmouse_enable()
{
    if (!gmouse_enabled) {
        gmouse_current_cursor = -1;
        gmouse_set_cursor(MOUSE_CURSOR_NONE);
        gmouse_scrolling_enabled = 1;
        gmouse_enabled = 1;
        gmouse_bk_last_cursor = -1;
    }
}

// 0x44307C
void gmouse_disable(int a1)
{
    if (gmouse_enabled) {
        gmouse_set_cursor(MOUSE_CURSOR_NONE);
        gmouse_enabled = 0;

        if (a1 & 1) {
            gmouse_scrolling_enabled = 1;
        } else {
            gmouse_scrolling_enabled = 0;
        }
    }
}

// 0x4430B4
int gmouse_is_enabled()
{
    return gmouse_enabled;
}

// 0x4430BC
void gmouse_enable_scrolling()
{
    gmouse_scrolling_enabled = 1;
}

// 0x4430C8
void gmouse_disable_scrolling()
{
    gmouse_scrolling_enabled = 0;
}

// 0x4430D4
int gmouse_scrolling_is_enabled()
{
    return gmouse_scrolling_enabled;
}

// 0x4430DC
void gmouse_set_click_to_scroll(int a1)
{
    if (a1 != gmouse_click_to_scroll) {
        gmouse_click_to_scroll = a1;
        gmouse_clicked_on_edge = 0;
    }
}

// 0x4430F4
int gmouse_get_click_to_scroll()
{
    return gmouse_click_to_scroll;
}

// 0x44313C
int gmouse_is_scrolling()
{
    int is_scrolling = 0;

    if (gmouse_scrolling_enabled) {
        int x;
        int y;
        mouse_get_position(&x, &y);
        if (x == scr_size.ulx || x == scr_size.lrx || y == scr_size.uly || y == scr_size.lry) {
            switch (gmouse_current_cursor) {
            case MOUSE_CURSOR_SCROLL_NW:
            case MOUSE_CURSOR_SCROLL_N:
            case MOUSE_CURSOR_SCROLL_NE:
            case MOUSE_CURSOR_SCROLL_E:
            case MOUSE_CURSOR_SCROLL_SE:
            case MOUSE_CURSOR_SCROLL_S:
            case MOUSE_CURSOR_SCROLL_SW:
            case MOUSE_CURSOR_SCROLL_W:
            case MOUSE_CURSOR_SCROLL_NW_INVALID:
            case MOUSE_CURSOR_SCROLL_N_INVALID:
            case MOUSE_CURSOR_SCROLL_NE_INVALID:
            case MOUSE_CURSOR_SCROLL_E_INVALID:
            case MOUSE_CURSOR_SCROLL_SE_INVALID:
            case MOUSE_CURSOR_SCROLL_S_INVALID:
            case MOUSE_CURSOR_SCROLL_SW_INVALID:
            case MOUSE_CURSOR_SCROLL_W_INVALID:
                is_scrolling = 1;
                break;
            default:
                return is_scrolling;
            }
        }
    }

    return is_scrolling;
}

void hide_roof_on_hover(int mouseX, int mouseY)
{
    // Hover-hide roof tracking
    static int gmouse_hover_roof_x = -1;
    static int gmouse_hover_roof_y = -1;
    static int gmouse_hover_roof_elev = -1;

    int roofX;
    int roofY;
    square_xy_roof(mouseX, mouseY, map_elevation, &roofX, &roofY);

    // Check bounds
    if (roofX >= 0 && roofX < 100 && roofY >= 0 && roofY < 100) {
        int currentSquare = square[map_elevation]->field_0[roofX + 100 * roofY];
        int roofData = (currentSquare >> 16) & 0xFFFF;
        int roofTileId = roofData & 0xFFF;
        int roofFlags = (roofData >> 12) & 0xF;
        int emptyRoofTileId = 1;

        int emptyRoofArtId = art_id(OBJ_TYPE_TILE, emptyRoofTileId, 0, 0, 0);
        bool hasRoof = art_id(OBJ_TYPE_TILE, roofTileId, 0, 0, 0) != emptyRoofArtId;
        bool isHidden = (roofFlags & 0x01) != 0;

        // Check if player is under a roof (to avoid conflicts with player-hide system)
        int playerRoofX, playerRoofY, playerElev;
        bool playerUnderRoof = obj_get_player_roof_info(&playerRoofX, &playerRoofY, &playerElev);

        if (hasRoof) {
            bool isNewPosition = roofX != gmouse_hover_roof_x || roofY != gmouse_hover_roof_y || map_elevation != gmouse_hover_roof_elev;

            if (isNewPosition) {
                if (isHidden) {
                    // Roof is already hidden - could be our hover-hide or player-inside-hide
                    if (playerUnderRoof && map_elevation == playerElev) {
                        // Player is under a roof on same elevation - assume this is player-hidden
                        // Restore our tracked roof if it's on a different elevation
                        if (gmouse_hover_roof_x != -1 && gmouse_hover_roof_elev != playerElev) {
                            tile_fill_roof(gmouse_hover_roof_x, gmouse_hover_roof_y, gmouse_hover_roof_elev, true);
                            tile_refresh_display();
                        }
                        gmouse_hover_roof_x = -1;
                        gmouse_hover_roof_y = -1;
                        gmouse_hover_roof_elev = -1;
                    } else if (gmouse_hover_roof_x != -1 && map_elevation == gmouse_hover_roof_elev) {
                        // No player conflict, same elevation as our tracked roof
                        // This tile was hidden by our flood-fill - just update tracking position
                        gmouse_hover_roof_x = roofX;
                        gmouse_hover_roof_y = roofY;
                    } else if (gmouse_hover_roof_x != -1) {
                        // Tracking a roof on different elevation - restore it
                        tile_fill_roof(gmouse_hover_roof_x, gmouse_hover_roof_y, gmouse_hover_roof_elev, true);
                        gmouse_hover_roof_x = -1;
                        gmouse_hover_roof_y = -1;
                        gmouse_hover_roof_elev = -1;
                        tile_refresh_display();
                    }
                    // else: not tracking anything and roof is hidden (by player), ignore it
                } else {
                    // Roof is visible - we need to hide it
                    // First restore any previously hidden roof
                    if (gmouse_hover_roof_x != -1) {
                        tile_fill_roof(gmouse_hover_roof_x, gmouse_hover_roof_y, gmouse_hover_roof_elev, true);
                    }

                    // Hide the new roof
                    tile_fill_roof(roofX, roofY, map_elevation, false);
                    gmouse_hover_roof_x = roofX;
                    gmouse_hover_roof_y = roofY;
                    gmouse_hover_roof_elev = map_elevation;
                    tile_refresh_display();
                }
            }
            // else: same position as we're already tracking, do nothing
        } else {
            // No roof at this position - restore any previously hidden roof
            if (gmouse_hover_roof_x != -1) {
                tile_fill_roof(gmouse_hover_roof_x, gmouse_hover_roof_y, gmouse_hover_roof_elev, true);
                gmouse_hover_roof_x = -1;
                gmouse_hover_roof_y = -1;
                gmouse_hover_roof_elev = -1;
                tile_refresh_display();
            }
        }
    }
}

// 0x443274
void gmouse_bk_process()
{
    // 0x595214
    static Object* last_object;

    // 0x5053F8
    static int last_tile = -1;

    if (!gmouse_initialized) {
        return;
    }

    // Check accessibility highlight key state
    gmouse_accessibility_highlight_check();

    int mouseX;
    int mouseY;

    if (gmouse_current_cursor >= FIRST_GAME_MOUSE_ANIMATED_CURSOR) {
        mouse_info();

        // NOTE: Uninline.
        if (gmouse_scrolling_is_enabled()) {
            mouse_get_position(&mouseX, &mouseY);
            int oldMouseCursor = gmouse_current_cursor;

            if (gmouse_check_scrolling(mouseX, mouseY, gmouse_current_cursor) == 0) {
                switch (oldMouseCursor) {
                case MOUSE_CURSOR_SCROLL_NW:
                case MOUSE_CURSOR_SCROLL_N:
                case MOUSE_CURSOR_SCROLL_NE:
                case MOUSE_CURSOR_SCROLL_E:
                case MOUSE_CURSOR_SCROLL_SE:
                case MOUSE_CURSOR_SCROLL_S:
                case MOUSE_CURSOR_SCROLL_SW:
                case MOUSE_CURSOR_SCROLL_W:
                case MOUSE_CURSOR_SCROLL_NW_INVALID:
                case MOUSE_CURSOR_SCROLL_N_INVALID:
                case MOUSE_CURSOR_SCROLL_NE_INVALID:
                case MOUSE_CURSOR_SCROLL_E_INVALID:
                case MOUSE_CURSOR_SCROLL_SE_INVALID:
                case MOUSE_CURSOR_SCROLL_S_INVALID:
                case MOUSE_CURSOR_SCROLL_SW_INVALID:
                case MOUSE_CURSOR_SCROLL_W_INVALID:
                    break;
                default:
                    gmouse_bk_last_cursor = oldMouseCursor;
                    break;
                }
                return;
            }

            if (gmouse_bk_last_cursor != -1) {
                gmouse_set_cursor(gmouse_bk_last_cursor);
                gmouse_bk_last_cursor = -1;
                return;
            }
        }

        gmouse_set_cursor(gmouse_current_cursor);
        return;
    }

    if (!gmouse_enabled) {
        // NOTE: Uninline.
        if (gmouse_scrolling_is_enabled()) {
            mouse_get_position(&mouseX, &mouseY);
            int oldMouseCursor = gmouse_current_cursor;

            if (gmouse_check_scrolling(mouseX, mouseY, gmouse_current_cursor) == 0) {
                switch (oldMouseCursor) {
                case MOUSE_CURSOR_SCROLL_NW:
                case MOUSE_CURSOR_SCROLL_N:
                case MOUSE_CURSOR_SCROLL_NE:
                case MOUSE_CURSOR_SCROLL_E:
                case MOUSE_CURSOR_SCROLL_SE:
                case MOUSE_CURSOR_SCROLL_S:
                case MOUSE_CURSOR_SCROLL_SW:
                case MOUSE_CURSOR_SCROLL_W:
                case MOUSE_CURSOR_SCROLL_NW_INVALID:
                case MOUSE_CURSOR_SCROLL_N_INVALID:
                case MOUSE_CURSOR_SCROLL_NE_INVALID:
                case MOUSE_CURSOR_SCROLL_E_INVALID:
                case MOUSE_CURSOR_SCROLL_SE_INVALID:
                case MOUSE_CURSOR_SCROLL_S_INVALID:
                case MOUSE_CURSOR_SCROLL_SW_INVALID:
                case MOUSE_CURSOR_SCROLL_W_INVALID:
                    break;
                default:
                    gmouse_bk_last_cursor = oldMouseCursor;
                    break;
                }

                return;
            }

            if (gmouse_bk_last_cursor != -1) {
                gmouse_set_cursor(gmouse_bk_last_cursor);
                gmouse_bk_last_cursor = -1;
            }
        }

        return;
    }

    mouse_get_position(&mouseX, &mouseY);

    int oldMouseCursor = gmouse_current_cursor;
    if (gmouse_check_scrolling(mouseX, mouseY, MOUSE_CURSOR_NONE) == 0) {
        switch (oldMouseCursor) {
        case MOUSE_CURSOR_SCROLL_NW:
        case MOUSE_CURSOR_SCROLL_N:
        case MOUSE_CURSOR_SCROLL_NE:
        case MOUSE_CURSOR_SCROLL_E:
        case MOUSE_CURSOR_SCROLL_SE:
        case MOUSE_CURSOR_SCROLL_S:
        case MOUSE_CURSOR_SCROLL_SW:
        case MOUSE_CURSOR_SCROLL_W:
        case MOUSE_CURSOR_SCROLL_NW_INVALID:
        case MOUSE_CURSOR_SCROLL_N_INVALID:
        case MOUSE_CURSOR_SCROLL_NE_INVALID:
        case MOUSE_CURSOR_SCROLL_E_INVALID:
        case MOUSE_CURSOR_SCROLL_SE_INVALID:
        case MOUSE_CURSOR_SCROLL_S_INVALID:
        case MOUSE_CURSOR_SCROLL_SW_INVALID:
        case MOUSE_CURSOR_SCROLL_W_INVALID:
            break;
        default:
            gmouse_bk_last_cursor = oldMouseCursor;
            break;
        }
        return;
    }

    if (gmouse_bk_last_cursor != -1) {
        gmouse_set_cursor(gmouse_bk_last_cursor);
        gmouse_bk_last_cursor = -1;
    }

    if (win_get_top_win(mouseX, mouseY) != display_win) {
        if (gmouse_current_cursor == MOUSE_CURSOR_NONE) {
            gmouse_3d_off();
            gmouse_set_cursor(MOUSE_CURSOR_ARROW);

            if (gmouse_3d_current_mode >= 2 && !isInCombat()) {
                gmouse_3d_set_mode(GAME_MOUSE_MODE_MOVE);
            }
        }
        return;
    }

    // NOTE: Strange set of conditions and jumps. Not sure about this one.
    switch (gmouse_current_cursor) {
    case MOUSE_CURSOR_NONE:
    case MOUSE_CURSOR_ARROW:
    case MOUSE_CURSOR_SMALL_ARROW_UP:
    case MOUSE_CURSOR_SMALL_ARROW_DOWN:
    case MOUSE_CURSOR_CROSSHAIR:
    case MOUSE_CURSOR_USE_CROSSHAIR:
        gmouse_set_cursor(MOUSE_CURSOR_NONE);

        if (!gmouse_3d_is_on()) {
            gmouse_3d_on();
        }

        break;
    }

    Rect r1;
    if (gmouse_3d_move_to(mouseX, mouseY, map_elevation, &r1) == 0) {
        tile_refresh_rect(&r1, map_elevation);
    }

    if (!gmouse_3d_is_on() || gmouse_mapper_mode != 0) {
        return;
    }

    unsigned int v3 = get_bk_time();
    if (mouseX == gmouse_3d_last_mouse_x && mouseY == gmouse_3d_last_mouse_y) {
        if (gmouse_3d_hover_test || elapsed_tocks(v3, gmouse_3d_last_move_time) < 250) {
            return;
        }

        if (gmouse_3d_current_mode != GAME_MOUSE_MODE_MOVE) {
            if (gmouse_3d_current_mode == GAME_MOUSE_MODE_ARROW) {
                gmouse_3d_last_move_time = v3;
                gmouse_3d_hover_test = true;

                Object* target = object_under_mouse(-1, true, map_elevation);
                if (target != NULL) {
                    int primaryAction = -1;

                    switch (FID_TYPE(target->fid)) {
                    case OBJ_TYPE_ITEM:
                        primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        if (gmouse_3d_item_highlight) {
                            Rect tmp;
                            if (obj_outline_object(target, OUTLINE_TYPE_ITEM, &tmp) == 0) {
                                tile_refresh_rect(&tmp, map_elevation);
                                outlined_object = target;
                            }
                        }
                        break;
                    case OBJ_TYPE_CRITTER:
                        if (target == obj_dude) {
                            primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_ROTATE;
                        } else {
                            if (obj_action_can_talk_to(target)) {
                                if (isInCombat()) {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                                } else {
                                    primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_TALK;
                                }
                            } else {
                                primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                            }
                        }
                        break;
                    case OBJ_TYPE_SCENERY:
                        if (!proto_action_can_use(target->pid)) {
                            primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                        } else {
                            primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                        }
                        break;
                    case OBJ_TYPE_WALL:
                        primaryAction = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                        break;
                    }

                    if (primaryAction != -1) {
                        if (gmouse_3d_build_pick_frame(mouseX, mouseY, primaryAction, scr_size.lrx - scr_size.ulx + 1, scr_size.lry - scr_size.uly - 99) == 0) {
                            Rect tmp;
                            int fid = art_id(OBJ_TYPE_INTERFACE, 282, 0, 0, 0);
                            // NOTE: Uninline.
                            if (gmouse_3d_set_flat_fid(fid, &tmp) == 0) {
                                tile_refresh_rect(&tmp, map_elevation);
                            }
                        }
                    }

                    if (target != last_object) {
                        last_object = target;
                        obj_look_at(obj_dude, last_object);
                    }

                    // Update tooltip for the hovered object
                    gmouse_tooltip_update(target, mouseX, mouseY);

                    // Debug: print target's script name
                    if (target->sid != -1) {
                        Script* script;
                        if (scr_ptr(target->sid, &script) != -1) {
                            char scriptName[16];
                            if (scr_list_str(script->scr_script_idx, scriptName, sizeof(scriptName)) != -1) {
                                printf("Target script: %s\n", scriptName);
                            }
                        }
                    }
                } else {
                    // No object under cursor, hide tooltip
                    gmouse_tooltip_update(NULL, mouseX, mouseY);
                }
            } else if (gmouse_3d_current_mode == GAME_MOUSE_MODE_CROSSHAIR) {
                Object* pointedObject = object_under_mouse(OBJ_TYPE_CRITTER, false, map_elevation);
                if (pointedObject != NULL) {
                    int color;
                    int accuracy;
                    char formattedAccuracy[8];
                    if (combat_to_hit(pointedObject, &accuracy)) {
                        snprintf(formattedAccuracy, sizeof(formattedAccuracy), "%d%%", accuracy);
                        color = colorTable[32767];
                    } else {
                        snprintf(formattedAccuracy, sizeof(formattedAccuracy), " %c ", 'X');
                        color = colorTable[31744];
                    }

                    if (gmouse_3d_build_to_hit_frame(formattedAccuracy, color) == 0) {
                        Rect tmp;
                        int fid = art_id(OBJ_TYPE_INTERFACE, 284, 0, 0, 0);
                        // NOTE: Uninline.
                        if (gmouse_3d_set_flat_fid(fid, &tmp) == 0) {
                            tile_refresh_rect(&tmp, map_elevation);
                        }
                    }

                    if (last_object != pointedObject) {
                        last_object = pointedObject;
                    }
                } else {
                    Rect tmp;
                    if (gmouse_3d_reset_flat_fid(&tmp) == 0) {
                        tile_refresh_rect(&tmp, map_elevation);
                    }
                }

                gmouse_3d_last_move_time = v3;
                gmouse_3d_hover_test = true;
            }
            return;
        }

        char formattedActionPoints[8];
        int color;
        int distance = make_path(obj_dude, obj_dude->tile, obj_mouse_flat->tile, NULL, 1);
        if (distance != 0) {
            if (!isInCombat()) {
                formattedActionPoints[0] = '\0';
                color = colorTable[31744];
            } else {
                int actionPointsMax = critter_compute_ap_from_distance(obj_dude, distance);
                int actionPointsRequired = std::max(0, actionPointsMax - combat_free_move);

                if (actionPointsRequired <= obj_dude->data.critter.combat.ap) {
                    snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%d", actionPointsRequired);
                    color = colorTable[32767];
                } else {
                    snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%c", 'X');
                    color = colorTable[31744];
                }
            }
        } else {
            snprintf(formattedActionPoints, sizeof(formattedActionPoints), "%c", 'X');
            color = colorTable[31744];
        }

        if (gmouse_3d_build_hex_frame(formattedActionPoints, color) == 0) {
            Rect tmp;
            obj_bound(obj_mouse_flat, &tmp);
            tile_refresh_rect(&tmp, 0);
        }

        gmouse_3d_last_move_time = v3;
        gmouse_3d_hover_test = true;
        last_tile = obj_mouse_flat->tile;
        return;
    }

    gmouse_3d_last_move_time = v3;
    gmouse_3d_hover_test = false;
    gmouse_3d_last_mouse_x = mouseX;
    gmouse_3d_last_mouse_y = mouseY;

    // Hide tooltip when mouse moves
    gmouse_tooltip_update(NULL, mouseX, mouseY);

    // Auto-switch between MOVE, ARROW, and CROSSHAIR modes based on what's under cursor
    if (!gmouse_mapper_mode) {
        if (tweaks_auto_mouse_mode()) {
            // Include CROSSHAIR in auto-switchable modes when in combat
            bool isAutoSwitchableMode = gmouse_3d_current_mode == GAME_MOUSE_MODE_MOVE
                || gmouse_3d_current_mode == GAME_MOUSE_MODE_ARROW
                || (isInCombat() && gmouse_3d_current_mode == GAME_MOUSE_MODE_CROSSHAIR);

            if (isAutoSwitchableMode) {
                int autoMode = gmouse_3d_determine_auto_mode(mouseX, mouseY, map_elevation);
                if (autoMode != gmouse_3d_current_mode) {
                    gmouse_3d_set_mode(autoMode);
                }
            }
        }
        gmouse_3d_reset_fid();
    }

    // Hover-hide roof: hide roofs when mouse hovers over them
    if (tweaks_hover_hide_roof() && tile_roof_visible()) {
        hide_roof_on_hover(mouseX, mouseY);
    }

    int v34 = 0;

    Rect r2;
    Rect r26;
    if (gmouse_3d_reset_flat_fid(&r2) == 0) {
        v34 |= 1;
    }

    if (outlined_object != NULL) {
        if (obj_remove_outline(outlined_object, &r26) == 0) {
            v34 |= 2;
        }
        outlined_object = NULL;
    }

    switch (v34) {
    case 3:
        rect_min_bound(&r2, &r26, &r2);
        // FALLTHROUGH
    case 1:
        tile_refresh_rect(&r2, map_elevation);
        break;
    case 2:
        tile_refresh_rect(&r26, map_elevation);
        break;
    }
}

// 0x443AA0
void gmouse_handle_event(int mouseX, int mouseY, int mouseState)
{
    if (!gmouse_initialized) {
        return;
    }

    if (gmouse_current_cursor >= MOUSE_CURSOR_WAIT_PLANET) {
        return;
    }

    if (!gmouse_enabled) {
        return;
    }

    if (gmouse_clicked_on_edge) {
        if (gmouse_get_click_to_scroll()) {
            return;
        }
    }

    if (!mouse_click_in(0, 0, scr_size.lrx - scr_size.ulx, scr_size.lry - scr_size.uly - 100)) {
        return;
    }

    // CE: Make sure we cannot go outside of the map.
    if (!tile_point_inside_bound(mouseX, mouseY)) {
        return;
    }

    if ((mouseState & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
        if ((mouseState & MOUSE_EVENT_RIGHT_BUTTON_REPEAT) == 0) {
            if (gmouse_3d_is_on()) {
                gmouse_3d_toggle_mode();
            }
        }
        return;
    }

    if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
        if (gmouse_3d_current_mode == GAME_MOUSE_MODE_MOVE) {
            int actionPoints;
            if (isInCombat()) {
                actionPoints = combat_free_move + obj_dude->data.critter.combat.ap;
            } else {
                actionPoints = -1;
            }

            bool running;
            configGetBool(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_KEY, &running);

            if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
                if (running) {
                    dude_move(actionPoints);
                    return;
                }
            } else {
                if (!running) {
                    dude_move(actionPoints);
                    return;
                }
            }

            dude_run(actionPoints);
            return;
        }

        if (gmouse_3d_current_mode == GAME_MOUSE_MODE_ARROW) {
            Object* target = object_under_mouse(-1, true, map_elevation);
            if (target != NULL) {
                switch (FID_TYPE(target->fid)) {
                case OBJ_TYPE_ITEM:
                    action_get_an_object(obj_dude, target);
                    break;
                case OBJ_TYPE_CRITTER:
                    if (target == obj_dude) {
                        if (FID_ANIM_TYPE(obj_dude->fid) == ANIM_STAND) {
                            Rect a1;
                            if (obj_inc_rotation(target, &a1) == 0) {
                                tile_refresh_rect(&a1, target->elevation);
                            }
                        }
                    } else {
                        if (obj_action_can_talk_to(target)) {
                            if (isInCombat()) {
                                if (obj_examine(obj_dude, target) == -1) {
                                    obj_look_at(obj_dude, target);
                                }
                            } else {
                                action_talk_to(obj_dude, target);
                            }
                        } else {
                            action_loot_container(obj_dude, target);
                        }
                    }
                    break;
                case OBJ_TYPE_SCENERY:
                    if (proto_action_can_use(target->pid)) {
                        action_use_an_object(obj_dude, target);
                    } else {
                        if (obj_examine(obj_dude, target) == -1) {
                            obj_look_at(obj_dude, target);
                        }
                    }
                    break;
                case OBJ_TYPE_WALL:
                    if (obj_examine(obj_dude, target) == -1) {
                        obj_look_at(obj_dude, target);
                    }
                    break;
                }
            }
            return;
        }

        if (gmouse_3d_current_mode == GAME_MOUSE_MODE_CROSSHAIR) {
            Object* target = object_under_mouse(OBJ_TYPE_CRITTER, false, map_elevation);
            if (target != NULL) {
                combat_attack_this(target);
                gmouse_3d_hover_test = true;
                gmouse_3d_last_mouse_y = mouseY;
                gmouse_3d_last_mouse_x = mouseX;
                gmouse_3d_last_move_time = get_time() - 250;
            }
            return;
        }

        if (gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_CROSSHAIR) {
            Object* target = object_under_mouse(-1, true, map_elevation);
            if (target != NULL) {
                Object* weapon;
                if (intface_get_current_item(&weapon) != -1) {
                    if (isInCombat()) {
                        int hitMode = intface_is_item_right_hand()
                            ? HIT_MODE_RIGHT_WEAPON_PRIMARY
                            : HIT_MODE_LEFT_WEAPON_PRIMARY;

                        int actionPointsRequired = item_mp_cost(obj_dude, hitMode, false);
                        if (actionPointsRequired <= obj_dude->data.critter.combat.ap) {
                            if (action_use_an_item_on_object(obj_dude, target, weapon) != -1) {
                                int actionPoints = obj_dude->data.critter.combat.ap;
                                if (actionPointsRequired > actionPoints) {
                                    obj_dude->data.critter.combat.ap = 0;
                                } else {
                                    obj_dude->data.critter.combat.ap -= actionPointsRequired;
                                }
                                intface_update_move_points(obj_dude->data.critter.combat.ap, combat_free_move);
                            }
                        }
                    } else {
                        action_use_an_item_on_object(obj_dude, target, weapon);
                    }
                }
            }
            gmouse_set_cursor(MOUSE_CURSOR_NONE);
            gmouse_3d_set_mode(GAME_MOUSE_MODE_MOVE);
            return;
        }

        if (gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_FIRST_AID
            || gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_DOCTOR
            || gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_LOCKPICK
            || gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_STEAL
            || gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_TRAPS
            || gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_SCIENCE
            || gmouse_3d_current_mode == GAME_MOUSE_MODE_USE_REPAIR) {
            Object* target = object_under_mouse(-1, 1, map_elevation);
            if (target == NULL || action_use_skill_on(obj_dude, target, gmouse_skill_table[gmouse_3d_current_mode - FIRST_GAME_MOUSE_MODE_SKILL]) != -1) {
                gmouse_set_cursor(MOUSE_CURSOR_NONE);
                gmouse_3d_set_mode(GAME_MOUSE_MODE_MOVE);
            }
            return;
        }
    }

    if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) == MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT && gmouse_3d_current_mode == GAME_MOUSE_MODE_ARROW) {
        Object* target = object_under_mouse(-1, true, map_elevation);
        if (target != NULL) {
            int actionMenuItemsCount = 0;
            int actionMenuItems[6];
            switch (FID_TYPE(target->fid)) {
            case OBJ_TYPE_ITEM:
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                if (item_get_type(target) == ITEM_TYPE_CONTAINER) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                }
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_CRITTER:
                if (target == obj_dude) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_ROTATE;
                } else {
                    if (obj_action_can_talk_to(target)) {
                        if (!isInCombat()) {
                            actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_TALK;
                        }
                    } else {
                        actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                    }
                }

                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_SCENERY:
                if (proto_action_can_use(target->pid)) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE;
                }

                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL;
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            case OBJ_TYPE_WALL:
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_LOOK;
                if (proto_action_can_use(target->pid)) {
                    actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY;
                }
                actionMenuItems[actionMenuItemsCount++] = GAME_MOUSE_ACTION_MENU_ITEM_CANCEL;
                break;
            }

            if (gmouse_3d_build_menu_frame(mouseX, mouseY, actionMenuItems, actionMenuItemsCount, scr_size.lrx - scr_size.ulx + 1, scr_size.lry - scr_size.uly - 99) == 0) {
                Rect v43;
                int fid = art_id(OBJ_TYPE_INTERFACE, 283, 0, 0, 0);
                // NOTE: Uninline.
                if (gmouse_3d_set_flat_fid(fid, &v43) == 0 && gmouse_3d_move_to(mouseX, mouseY, map_elevation, &v43) == 0) {
                    tile_refresh_rect(&v43, map_elevation);
                    map_disable_bk_processes();

                    int v33 = mouseY;
                    int actionIndex = 0;
                    while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
                        sharedFpsLimiter.mark();

                        get_input();

                        if (game_user_wants_to_quit != 0) {
                            actionMenuItems[actionIndex] = 0;
                        }

                        int v48;
                        int v47;
                        mouse_get_position(&v48, &v47);

                        if (abs(v47 - v33) > 10) {
                            if (v33 >= v47) {
                                actionIndex -= 1;
                            } else {
                                actionIndex += 1;
                            }

                            if (gmouse_3d_highlight_menu_frame(actionIndex) == 0) {
                                tile_refresh_rect(&v43, map_elevation);
                            }
                            v33 = v47;
                        }

                        renderPresent();
                        sharedFpsLimiter.throttle();
                    }

                    map_enable_bk_processes();

                    gmouse_3d_hover_test = false;
                    gmouse_3d_last_mouse_x = mouseX;
                    gmouse_3d_last_mouse_y = mouseY;
                    gmouse_3d_last_move_time = get_time();

                    mouse_set_position(mouseX, v33);

                    if (gmouse_3d_reset_flat_fid(&v43) == 0) {
                        tile_refresh_rect(&v43, map_elevation);
                    }

                    switch (actionMenuItems[actionIndex]) {
                    case GAME_MOUSE_ACTION_MENU_ITEM_INVENTORY:
                        use_inventory_on(target);
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_LOOK:
                        if (obj_examine(obj_dude, target) == -1) {
                            obj_look_at(obj_dude, target);
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_ROTATE:
                        if (obj_inc_rotation(target, &v43) == 0) {
                            tile_refresh_rect(&v43, target->elevation);
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_TALK:
                        action_talk_to(obj_dude, target);
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_USE:
                        switch (FID_TYPE(target->fid)) {
                        case OBJ_TYPE_SCENERY:
                            action_use_an_object(obj_dude, target);
                            break;
                        case OBJ_TYPE_CRITTER:
                            action_loot_container(obj_dude, target);
                            break;
                        default:
                            action_get_an_object(obj_dude, target);
                            break;
                        }
                        break;
                    case GAME_MOUSE_ACTION_MENU_ITEM_USE_SKILL:
                        if (1) {
                            int skill = -1;

                            int rc = skilldex_select();
                            switch (rc) {
                            case SKILLDEX_RC_SNEAK:
                                action_skill_use(SKILL_SNEAK);
                                break;
                            case SKILLDEX_RC_LOCKPICK:
                                skill = SKILL_LOCKPICK;
                                break;
                            case SKILLDEX_RC_STEAL:
                                skill = SKILL_STEAL;
                                break;
                            case SKILLDEX_RC_TRAPS:
                                skill = SKILL_TRAPS;
                                break;
                            case SKILLDEX_RC_FIRST_AID:
                                skill = SKILL_FIRST_AID;
                                break;
                            case SKILLDEX_RC_DOCTOR:
                                skill = SKILL_DOCTOR;
                                break;
                            case SKILLDEX_RC_SCIENCE:
                                skill = SKILL_SCIENCE;
                                break;
                            case SKILLDEX_RC_REPAIR:
                                skill = SKILL_REPAIR;
                                break;
                            }

                            if (skill != -1) {
                                action_use_skill_on(obj_dude, target, skill);
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
}

// 0x4442D4
int gmouse_set_cursor(int cursor)
{
    if (!gmouse_initialized) {
        return -1;
    }

    if (cursor != MOUSE_CURSOR_ARROW && cursor == gmouse_current_cursor && (gmouse_current_cursor < 25 || gmouse_current_cursor >= 27)) {
        return -1;
    }

    CacheEntry* mouseCursorFrmHandle;
    int fid = art_id(OBJ_TYPE_INTERFACE, gmouse_cursor_nums[cursor], 0, 0, 0);
    Art* mouseCursorFrm = art_ptr_lock(fid, &mouseCursorFrmHandle);
    if (mouseCursorFrm == NULL) {
        return -1;
    }

    bool shouldUpdate = true;
    int frame = 0;
    if (cursor >= FIRST_GAME_MOUSE_ANIMATED_CURSOR) {
        unsigned int tick = get_time();

        if (gmouse_3d_is_on()) {
            gmouse_3d_off();
        }

        unsigned int delay = 1000 / art_frame_fps(mouseCursorFrm);
        if (elapsed_tocks(tick, gmouse_wait_cursor_time) < delay) {
            shouldUpdate = false;
        } else {
            if (art_frame_max_frame(mouseCursorFrm) <= gmouse_wait_cursor_frame) {
                gmouse_wait_cursor_frame = 0;
            }

            frame = gmouse_wait_cursor_frame;
            gmouse_wait_cursor_time = tick;
            gmouse_wait_cursor_frame++;
        }
    }

    if (!shouldUpdate) {
        return -1;
    }

    int width = art_frame_width(mouseCursorFrm, frame, 0);
    int height = art_frame_length(mouseCursorFrm, frame, 0);

    int offsetX;
    int offsetY;
    art_frame_hot(mouseCursorFrm, frame, 0, &offsetX, &offsetY);

    unsigned char* mouseCursorFrmData = art_frame_data(mouseCursorFrm, frame, 0);
    if (mouse_set_shape(mouseCursorFrmData, width, height, width, offsetX, offsetY, 0) != 0) {
        return -1;
    }

    if (gmouse_current_cursor_key != INVALID_CACHE_ENTRY) {
        art_ptr_unlock(gmouse_current_cursor_key);
    }

    gmouse_current_cursor = cursor;
    gmouse_current_cursor_key = mouseCursorFrmHandle;

    return 0;
}

// 0x44447C
int gmouse_get_cursor()
{
    return gmouse_current_cursor;
}

// 0x444484
void gmouse_set_mapper_mode(int mode)
{
    gmouse_mapper_mode = mode;
}

// 0x44448C
void gmouse_3d_enable_modes()
{
    gmouse_3d_modes_enabled = 1;
}

// 0x444498
void gmouse_3d_disable_modes()
{
    gmouse_3d_modes_enabled = 0;
}

// 0x4444A4
int gmouse_3d_modes_are_enabled()
{
    return gmouse_3d_modes_enabled;
}

// 0x4444AC
void gmouse_3d_set_mode(int mode)
{
    if (!gmouse_initialized) {
        return;
    }

    if (!gmouse_3d_modes_enabled) {
        return;
    }

    if (mode == gmouse_3d_current_mode) {
        return;
    }

    int fid = art_id(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    gmouse_3d_set_fid(fid);

    fid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_mode_nums[mode], 0, 0, 0);

    Rect rect;
    // NOTE: Uninline.
    if (gmouse_3d_set_flat_fid(fid, &rect) == -1) {
        return;
    }

    int mouseX;
    int mouseY;
    mouse_get_position(&mouseX, &mouseY);

    Rect r2;
    if (gmouse_3d_move_to(mouseX, mouseY, map_elevation, &r2) == 0) {
        rect_min_bound(&rect, &r2, &rect);
    }

    int v5 = 0;
    if (gmouse_3d_current_mode == GAME_MOUSE_MODE_CROSSHAIR) {
        v5 = -1;
    }

    if (mode != 0) {
        if (mode == GAME_MOUSE_MODE_CROSSHAIR) {
            v5 = 1;
        }

        if (gmouse_3d_current_mode == 0) {
            if (obj_turn_off_outline(obj_mouse_flat, &r2) == 0) {
                rect_min_bound(&rect, &r2, &rect);
            }
        }
    } else {
        if (obj_turn_on_outline(obj_mouse_flat, &r2) == 0) {
            rect_min_bound(&rect, &r2, &rect);
        }
    }

    gmouse_3d_current_mode = mode;
    gmouse_3d_hover_test = false;
    gmouse_3d_last_move_time = get_time();

    tile_refresh_rect(&rect, map_elevation);

    switch (v5) {
    case 1:
        combat_outline_on();
        break;
    case -1:
        combat_outline_off();
        break;
    }
}

// 0x444600
int gmouse_3d_get_mode()
{
    return gmouse_3d_current_mode;
}

// 0x444608
void gmouse_3d_toggle_mode()
{
    int mode;

    if (tweaks_auto_mouse_mode()) {
        // With auto-switching between MOVE and ARROW, right-click now toggles
        // between the auto modes and CROSSHAIR (combat targeting)
        if (gmouse_3d_current_mode == GAME_MOUSE_MODE_MOVE || gmouse_3d_current_mode == GAME_MOUSE_MODE_ARROW) {
            // From auto mode: switch to CROSSHAIR in combat, otherwise no-op
            if (isInCombat()) {
                Object* item;
                if (intface_get_current_item(&item) == 0) {
                    // Only allow CROSSHAIR if holding a weapon
                    if (item != NULL && item_get_type(item) != ITEM_TYPE_WEAPON) {
                        return;
                    }
                }
                mode = GAME_MOUSE_MODE_CROSSHAIR;
            } else {
                // Outside combat, no toggle needed (auto-switching handles MOVE/ARROW)
                return;
            }
        } else if (gmouse_3d_current_mode == GAME_MOUSE_MODE_CROSSHAIR) {
            // From CROSSHAIR: return to auto-determined mode
            int mouseX;
            int mouseY;
            mouse_get_position(&mouseX, &mouseY);
            mode = gmouse_3d_determine_auto_mode(mouseX, mouseY, map_elevation);
        } else {
            // From other modes (skill modes, USE_CROSSHAIR): return to MOVE
            mode = GAME_MOUSE_MODE_MOVE;
        }
    } else {
        // Original toggle behavior: cycle through MOVE -> ARROW -> CROSSHAIR
        mode = (gmouse_3d_current_mode + 1) % 3;

        if (isInCombat()) {
            Object* item;
            if (intface_get_current_item(&item) == 0) {
                // Only allow CROSSHAIR if holding a weapon
                if (item != NULL && item_get_type(item) != ITEM_TYPE_WEAPON && mode == GAME_MOUSE_MODE_CROSSHAIR) {
                    mode = GAME_MOUSE_MODE_MOVE;
                }
            }
        } else {
            // Outside combat, no toggle needed (auto-switching handles MOVE/ARROW)
            if (mode == GAME_MOUSE_MODE_CROSSHAIR) {
                mode = GAME_MOUSE_MODE_MOVE;
            }
        }
    }

    gmouse_3d_set_mode(mode);
}

// 0x444664
void gmouse_3d_refresh()
{
    gmouse_3d_last_mouse_x = -1;
    gmouse_3d_last_mouse_y = -1;
    gmouse_3d_hover_test = false;
    gmouse_3d_last_move_time = 0;
    gmouse_bk_process();
}

// 0x444690
int gmouse_3d_set_fid(int fid)
{
    if (!gmouse_initialized) {
        return -1;
    }

    if (!art_exists(fid)) {
        return -1;
    }

    if (obj_mouse->fid == fid) {
        return -1;
    }

    if (!gmouse_mapper_mode) {
        return obj_change_fid(obj_mouse, fid, NULL);
    }

    int v1 = 0;

    Rect oldRect;
    if (obj_mouse->fid != -1) {
        obj_bound(obj_mouse, &oldRect);
        v1 |= 1;
    }

    int rc = -1;

    Rect rect;
    if (obj_change_fid(obj_mouse, fid, &rect) == 0) {
        rc = 0;
        v1 |= 2;
    }

    if (gmouse_3d_is_on()) {
        if (v1 == 1) {
            tile_refresh_rect(&oldRect, map_elevation);
        } else if (v1 == 2) {
            tile_refresh_rect(&rect, map_elevation);
        } else if (v1 == 3) {
            rect_min_bound(&oldRect, &rect, &oldRect);
            tile_refresh_rect(&oldRect, map_elevation);
        }
    }

    return rc;
}

// 0x444788
int gmouse_3d_get_fid()
{
    if (gmouse_initialized) {
        return obj_mouse->fid;
    }

    return -1;
}

// 0x4447A0
void gmouse_3d_reset_fid()
{
    int fid = art_id(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    gmouse_3d_set_fid(fid);
}

// 0x4447C0
void gmouse_3d_on()
{
    if (!gmouse_initialized) {
        return;
    }

    int v2 = 0;

    Rect rect1;
    if (obj_turn_on(obj_mouse, &rect1) == 0) {
        v2 |= 1;
    }

    Rect rect2;
    if (obj_turn_on(obj_mouse_flat, &rect2) == 0) {
        v2 |= 2;
    }

    Rect tmp;
    if (gmouse_3d_current_mode != GAME_MOUSE_MODE_MOVE) {
        if (obj_turn_off_outline(obj_mouse_flat, &tmp) == 0) {
            if ((v2 & 2) != 0) {
                rect_min_bound(&rect2, &tmp, &rect2);
            } else {
                memcpy(&rect2, &tmp, sizeof(rect2));
                v2 |= 2;
            }
        }
    }

    if (gmouse_3d_reset_flat_fid(&tmp) == 0) {
        if ((v2 & 2) != 0) {
            rect_min_bound(&rect2, &tmp, &rect2);
        } else {
            memcpy(&rect2, &tmp, sizeof(rect2));
            v2 |= 2;
        }
    }

    if (v2 != 0) {
        Rect* rect;
        switch (v2) {
        case 1:
            rect = &rect1;
            break;
        case 2:
            rect = &rect2;
            break;
        case 3:
            rect_min_bound(&rect1, &rect2, &rect1);
            rect = &rect1;
            break;
        default:
            assert(false && "Should be unreachable");
        }

        tile_refresh_rect(rect, map_elevation);
    }

    gmouse_3d_hover_test = false;
    gmouse_3d_last_move_time = get_time() - 250;
}

// 0x4448C8
void gmouse_3d_off()
{
    if (!gmouse_initialized) {
        return;
    }

    int v1 = 0;

    Rect rect1;
    if (obj_turn_off(obj_mouse, &rect1) == 0) {
        v1 |= 1;
    }

    Rect rect2;
    if (obj_turn_off(obj_mouse_flat, &rect2) == 0) {
        v1 |= 2;
    }

    if (v1 == 1) {
        tile_refresh_rect(&rect1, map_elevation);
    } else if (v1 == 2) {
        tile_refresh_rect(&rect2, map_elevation);
    } else if (v1 == 3) {
        rect_min_bound(&rect1, &rect2, &rect1);
        tile_refresh_rect(&rect1, map_elevation);
    }
}

// 0x444944
bool gmouse_3d_is_on()
{
    return (obj_mouse_flat->flags & OBJECT_HIDDEN) == 0;
}

// 0x444958
Object* object_under_mouse(int objectType, bool a2, int elevation)
{
    int mouseX;
    int mouseY;
    mouse_get_position(&mouseX, &mouseY);

    bool v13 = false;
    if (objectType == -1) {
        if (square_roof_intersect(mouseX, mouseY, elevation)) {
            if (obj_intersects_with(obj_egg, mouseX, mouseY) == 0) {
                v13 = true;
            }
        }
    }

    Object* v4 = NULL;
    if (!v13) {
        ObjectWithFlags* entries;
        int count = obj_create_intersect_list(mouseX, mouseY, elevation, objectType, &entries);
        for (int index = count - 1; index >= 0; index--) {
            ObjectWithFlags* ptr = &(entries[index]);
            if (a2 || obj_dude != ptr->object) {
                v4 = ptr->object;
                if ((ptr->flags & 0x01) != 0) {
                    if ((ptr->flags & 0x04) == 0) {
                        if (FID_TYPE(ptr->object->fid) != OBJ_TYPE_CRITTER || (ptr->object->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) == 0) {
                            break;
                        }
                    }
                }
            }
        }

        if (count != 0) {
            obj_delete_intersect_list(&entries);
        }
    }
    return v4;
}

// 0x444A34
int gmouse_3d_build_pick_frame(int x, int y, int menuItem, int width, int height)
{
    CacheEntry* menuItemFrmHandle;
    int menuItemFid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_action_nums[menuItem], 0, 0, 0);
    Art* menuItemFrm = art_ptr_lock(menuItemFid, &menuItemFrmHandle);
    if (menuItemFrm == NULL) {
        return -1;
    }

    CacheEntry* arrowFrmHandle;
    int arrowFid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_mode_nums[GAME_MOUSE_MODE_ARROW], 0, 0, 0);
    Art* arrowFrm = art_ptr_lock(arrowFid, &arrowFrmHandle);
    if (arrowFrm == NULL) {
        art_ptr_unlock(menuItemFrmHandle);
        // FIXME: Why this is success?
        return 0;
    }

    unsigned char* arrowFrmData = art_frame_data(arrowFrm, 0, 0);
    int arrowFrmWidth = art_frame_width(arrowFrm, 0, 0);
    int arrowFrmHeight = art_frame_length(arrowFrm, 0, 0);

    unsigned char* menuItemFrmData = art_frame_data(menuItemFrm, 0, 0);
    int menuItemFrmWidth = art_frame_width(menuItemFrm, 0, 0);
    int menuItemFrmHeight = art_frame_length(menuItemFrm, 0, 0);

    unsigned char* arrowFrmDest = gmouse_3d_pick_frame_data;
    unsigned char* menuItemFrmDest = gmouse_3d_pick_frame_data;

    gmouse_3d_pick_frame_hot_x = 0;
    gmouse_3d_pick_frame_hot_y = 0;

    gmouse_3d_pick_frame->xOffsets[0] = gmouse_3d_pick_frame_width / 2;
    gmouse_3d_pick_frame->yOffsets[0] = gmouse_3d_pick_frame_height - 1;

    int maxX = x + menuItemFrmWidth + arrowFrmWidth - 1;
    int maxY = y + menuItemFrmHeight - 1;
    int shiftY = maxY - height + 2;

    if (maxX < width) {
        menuItemFrmDest += arrowFrmWidth;
        if (maxY >= height) {
            gmouse_3d_pick_frame_hot_y = shiftY;
            gmouse_3d_pick_frame->yOffsets[0] -= shiftY;
            arrowFrmDest += gmouse_3d_pick_frame_width * shiftY;
        }
    } else {
        art_ptr_unlock(arrowFrmHandle);

        arrowFid = art_id(OBJ_TYPE_INTERFACE, 285, 0, 0, 0);
        arrowFrm = art_ptr_lock(arrowFid, &arrowFrmHandle);
        arrowFrmData = art_frame_data(arrowFrm, 0, 0);
        arrowFrmDest += menuItemFrmWidth;

        gmouse_3d_pick_frame->xOffsets[0] = -gmouse_3d_pick_frame->xOffsets[0];
        gmouse_3d_pick_frame_hot_x += menuItemFrmWidth + arrowFrmWidth;

        if (maxY >= height) {
            gmouse_3d_pick_frame_hot_y += shiftY;
            gmouse_3d_pick_frame->yOffsets[0] -= shiftY;

            arrowFrmDest += gmouse_3d_pick_frame_width * shiftY;
        }
    }

    memset(gmouse_3d_pick_frame_data, 0, gmouse_3d_pick_frame_size);

    buf_to_buf(arrowFrmData, arrowFrmWidth, arrowFrmHeight, arrowFrmWidth, arrowFrmDest, gmouse_3d_pick_frame_width);
    buf_to_buf(menuItemFrmData, menuItemFrmWidth, menuItemFrmHeight, menuItemFrmWidth, menuItemFrmDest, gmouse_3d_pick_frame_width);

    art_ptr_unlock(arrowFrmHandle);
    art_ptr_unlock(menuItemFrmHandle);

    return 0;
}

// 0x444C94
int gmouse_3d_pick_frame_hot(int* a1, int* a2)
{
    *a1 = gmouse_3d_pick_frame_hot_x;
    *a2 = gmouse_3d_pick_frame_hot_y;
    return 0;
}

// 0x444CA8
int gmouse_3d_build_menu_frame(int x, int y, const int* menuItems, int menuItemsLength, int width, int height)
{
    gmouse_3d_menu_actions_start = NULL;
    gmouse_3d_menu_current_action_index = 0;
    gmouse_3d_menu_available_actions = 0;

    if (menuItems == NULL) {
        return -1;
    }

    if (menuItemsLength == 0 || menuItemsLength >= GAME_MOUSE_ACTION_MENU_ITEM_COUNT) {
        return -1;
    }

    CacheEntry* menuItemFrmHandles[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];
    Art* menuItemFrms[GAME_MOUSE_ACTION_MENU_ITEM_COUNT];

    for (int index = 0; index < menuItemsLength; index++) {
        int frmId = gmouse_3d_action_nums[menuItems[index]] & 0xFFFF;
        if (index == 0) {
            frmId -= 1;
        }

        int fid = art_id(OBJ_TYPE_INTERFACE, frmId, 0, 0, 0);

        menuItemFrms[index] = art_ptr_lock(fid, &(menuItemFrmHandles[index]));
        if (menuItemFrms[index] == NULL) {
            while (--index >= 0) {
                art_ptr_unlock(menuItemFrmHandles[index]);
            }
            return -1;
        }
    }

    CacheEntry* arrowFrmHandle;
    int arrowFid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_mode_nums[GAME_MOUSE_MODE_ARROW], 0, 0, 0);
    Art* arrowFrm = art_ptr_lock(arrowFid, &arrowFrmHandle);
    if (arrowFrm == NULL) {
        for (int index = 0; index < menuItemsLength; index++) {
            art_ptr_unlock(menuItemFrmHandles[index]);
        }
        return -1;
    }

    unsigned char* arrowFrmData = art_frame_data(arrowFrm, 0, 0);
    int arrowFrmWidth = art_frame_width(arrowFrm, 0, 0);
    int arrowFrmHeight = art_frame_length(arrowFrm, 0, 0);

    int menuItemFrmWidth = art_frame_width(menuItemFrms[0], 0, 0);
    int menuItemFrmHeight = art_frame_length(menuItemFrms[0], 0, 0);

    gmouse_3d_menu_frame_hot_x = 0;
    gmouse_3d_menu_frame_hot_y = 0;

    gmouse_3d_menu_frame->xOffsets[0] = gmouse_3d_menu_frame_width / 2;
    gmouse_3d_menu_frame->yOffsets[0] = gmouse_3d_menu_frame_height - 1;

    int maxY = y + menuItemsLength * menuItemFrmHeight - 1;
    int shiftY = maxY - height + 2;
    unsigned char* arrowFrmDest = gmouse_3d_menu_frame_data;
    unsigned char* menuItemFrmDest = arrowFrmDest;

    if (x + arrowFrmWidth + menuItemFrmWidth - 1 < width) {
        menuItemFrmDest += arrowFrmWidth;
        if (height <= maxY) {
            gmouse_3d_menu_frame_hot_y += shiftY;
            arrowFrmDest += gmouse_3d_menu_frame_width * shiftY;
            gmouse_3d_menu_frame->yOffsets[0] -= shiftY;
        }
    } else {
        // Mirrored arrow (from left to right).
        art_ptr_unlock(arrowFrmHandle);

        arrowFid = art_id(OBJ_TYPE_INTERFACE, 285, 0, 0, 0);
        arrowFrm = art_ptr_lock(arrowFid, &arrowFrmHandle);
        arrowFrmData = art_frame_data(arrowFrm, 0, 0);
        arrowFrmDest += menuItemFrmWidth;

        gmouse_3d_menu_frame->xOffsets[0] = -gmouse_3d_menu_frame->xOffsets[0];
        gmouse_3d_menu_frame_hot_x += menuItemFrmWidth + arrowFrmWidth;

        if (maxY >= height) {
            gmouse_3d_menu_frame_hot_y += shiftY;
            gmouse_3d_menu_frame->yOffsets[0] -= shiftY;
            arrowFrmDest += gmouse_3d_menu_frame_width * shiftY;
        }
    }

    memset(gmouse_3d_menu_frame_data, 0, gmouse_3d_menu_frame_size);
    buf_to_buf(arrowFrmData, arrowFrmWidth, arrowFrmHeight, arrowFrmWidth, arrowFrmDest, gmouse_3d_pick_frame_width);

    memcpy(gmouse_3d_menu_frame_actions, menuItems, sizeof(*gmouse_3d_menu_frame_actions) * menuItemsLength);
    gmouse_3d_menu_available_actions = menuItemsLength;
    gmouse_3d_menu_actions_start = menuItemFrmDest;

    for (int index = 0; index < menuItemsLength; index++) {
        unsigned char* data = art_frame_data(menuItemFrms[index], 0, 0);
        buf_to_buf(data, menuItemFrmWidth, menuItemFrmHeight, menuItemFrmWidth, menuItemFrmDest, gmouse_3d_pick_frame_width);
        menuItemFrmDest += gmouse_3d_menu_frame_width * menuItemFrmHeight;
    }

    art_ptr_unlock(arrowFrmHandle);

    for (int index = 0; index < menuItemsLength; index++) {
        art_ptr_unlock(menuItemFrmHandles[index]);
    }

    Sound* sound = gsound_load_sound("iaccuxx1", NULL);
    if (sound != NULL) {
        gsound_play_sound(sound);
    }

    return 0;
}

// 0x4450A8
int gmouse_3d_menu_frame_hot(int* x, int* y)
{
    *x = gmouse_3d_menu_frame_hot_x;
    *y = gmouse_3d_menu_frame_hot_y;
    return 0;
}

// 0x4450BC
int gmouse_3d_highlight_menu_frame(int menuItemIndex)
{
    if (menuItemIndex < 0 || menuItemIndex >= gmouse_3d_menu_available_actions) {
        return -1;
    }

    CacheEntry* handle;
    int fid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_action_nums[gmouse_3d_menu_frame_actions[gmouse_3d_menu_current_action_index]], 0, 0, 0);
    Art* art = art_ptr_lock(fid, &handle);
    if (art == NULL) {
        return -1;
    }

    int width = art_frame_width(art, 0, 0);
    int height = art_frame_length(art, 0, 0);
    unsigned char* data = art_frame_data(art, 0, 0);
    buf_to_buf(data, width, height, width, gmouse_3d_menu_actions_start + gmouse_3d_menu_frame_width * height * gmouse_3d_menu_current_action_index, gmouse_3d_menu_frame_width);
    art_ptr_unlock(handle);

    fid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_action_nums[gmouse_3d_menu_frame_actions[menuItemIndex]] - 1, 0, 0, 0);
    art = art_ptr_lock(fid, &handle);
    if (art == NULL) {
        return -1;
    }

    data = art_frame_data(art, 0, 0);
    buf_to_buf(data, width, height, width, gmouse_3d_menu_actions_start + gmouse_3d_menu_frame_width * height * menuItemIndex, gmouse_3d_menu_frame_width);
    art_ptr_unlock(handle);

    gmouse_3d_menu_current_action_index = menuItemIndex;

    return 0;
}

// 0x445200
int gmouse_3d_build_to_hit_frame(const char* string, int color)
{
    CacheEntry* crosshairFrmHandle;
    int fid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_mode_nums[GAME_MOUSE_MODE_CROSSHAIR], 0, 0, 0);
    Art* crosshairFrm = art_ptr_lock(fid, &crosshairFrmHandle);
    if (crosshairFrm == NULL) {
        return -1;
    }

    memset(gmouse_3d_to_hit_frame_data, 0, gmouse_3d_to_hit_frame_size);

    int crosshairFrmWidth = art_frame_width(crosshairFrm, 0, 0);
    int crosshairFrmHeight = art_frame_length(crosshairFrm, 0, 0);
    unsigned char* crosshairFrmData = art_frame_data(crosshairFrm, 0, 0);
    buf_to_buf(crosshairFrmData,
        crosshairFrmWidth,
        crosshairFrmHeight,
        crosshairFrmWidth,
        gmouse_3d_to_hit_frame_data,
        gmouse_3d_to_hit_frame_width);

    int oldFont = text_curr();
    text_font(101);

    text_to_buf(gmouse_3d_to_hit_frame_data + gmouse_3d_to_hit_frame_width + crosshairFrmWidth + 1,
        string,
        gmouse_3d_to_hit_frame_width - crosshairFrmWidth,
        gmouse_3d_to_hit_frame_width,
        color);

    buf_outline(gmouse_3d_to_hit_frame_data + crosshairFrmWidth,
        gmouse_3d_to_hit_frame_width - crosshairFrmWidth,
        gmouse_3d_to_hit_frame_height,
        gmouse_3d_to_hit_frame_width,
        colorTable[0]);

    text_font(oldFont);

    art_ptr_unlock(crosshairFrmHandle);

    return 0;
}

// 0x445304
int gmouse_3d_build_hex_frame(const char* string, int color)
{
    memset(gmouse_3d_hex_frame_data, 0, gmouse_3d_hex_frame_width * gmouse_3d_hex_frame_height);

    if (*string == '\0') {
        return 0;
    }

    int oldFont = text_curr();
    text_font(101);

    int length = text_width(string);
    text_to_buf(gmouse_3d_hex_frame_data + gmouse_3d_hex_frame_width * (gmouse_3d_hex_frame_height - text_height()) / 2 + (gmouse_3d_hex_frame_width - length) / 2, string, gmouse_3d_hex_frame_width, gmouse_3d_hex_frame_width, color);

    buf_outline(gmouse_3d_hex_frame_data, gmouse_3d_hex_frame_width, gmouse_3d_hex_frame_height, gmouse_3d_hex_frame_width, colorTable[0]);

    text_font(oldFont);

    int fid = art_id(OBJ_TYPE_INTERFACE, 1, 0, 0, 0);
    gmouse_3d_set_fid(fid);

    return 0;
}

// 0x4453E0
void gmouse_3d_synch_item_highlight()
{
    bool itemHighlight;
    if (configGetBool(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_ITEM_HIGHLIGHT_KEY, &itemHighlight)) {
        gmouse_3d_item_highlight = itemHighlight;
    }
}

// 0x445410
static int gmouse_3d_init()
{
    int fid;

    if (gmouse_3d_initialized) {
        return -1;
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    if (obj_new(&obj_mouse, fid, -1) != 0) {
        return -1;
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 1, 0, 0, 0);
    if (obj_new(&obj_mouse_flat, fid, -1) != 0) {
        return -1;
    }

    if (obj_outline_object(obj_mouse_flat, OUTLINE_PALETTED | OUTLINE_TYPE_2, NULL) != 0) {
        return -1;
    }

    if (gmouse_3d_lock_frames() != 0) {
        return -1;
    }

    obj_mouse->flags |= OBJECT_LIGHT_THRU;
    obj_mouse->flags |= OBJECT_NO_SAVE;
    obj_mouse->flags |= OBJECT_NO_REMOVE;
    obj_mouse->flags |= OBJECT_SHOOT_THRU;
    obj_mouse->flags |= OBJECT_NO_BLOCK;

    obj_mouse_flat->flags |= OBJECT_NO_REMOVE;
    obj_mouse_flat->flags |= OBJECT_NO_SAVE;
    obj_mouse_flat->flags |= OBJECT_LIGHT_THRU;
    obj_mouse_flat->flags |= OBJECT_SHOOT_THRU;
    obj_mouse_flat->flags |= OBJECT_NO_BLOCK;

    obj_toggle_flat(obj_mouse_flat, NULL);

    int x;
    int y;
    mouse_get_position(&x, &y);

    Rect v9;
    gmouse_3d_move_to(x, y, map_elevation, &v9);

    gmouse_3d_initialized = true;

    gmouse_3d_synch_item_highlight();

    return 0;
}

// 0x44554C
static int gmouse_3d_reset()
{
    if (!gmouse_3d_initialized) {
        return -1;
    }

    // Hide any visible tooltip
    gmouse_tooltip_hide();

    // NOTE: Uninline.
    gmouse_3d_enable_modes();

    // NOTE: Uninline.
    gmouse_3d_reset_fid();

    gmouse_3d_set_mode(GAME_MOUSE_MODE_MOVE);
    gmouse_3d_on();

    gmouse_3d_last_mouse_x = -1;
    gmouse_3d_last_mouse_y = -1;
    gmouse_3d_hover_test = false;
    gmouse_3d_last_move_time = get_time();
    gmouse_3d_synch_item_highlight();

    return 0;
}

// 0x4455C0
static void gmouse_3d_exit()
{
    if (gmouse_3d_initialized) {
        gmouse_3d_unlock_frames();

        obj_mouse->flags &= ~OBJECT_NO_SAVE;
        obj_mouse_flat->flags &= ~OBJECT_NO_SAVE;

        obj_erase_object(obj_mouse, NULL);
        obj_erase_object(obj_mouse_flat, NULL);

        gmouse_3d_initialized = false;
    }
}

// 0x445604
static int gmouse_3d_lock_frames()
{
    int fid;

    // actmenu.frm - action menu
    fid = art_id(OBJ_TYPE_INTERFACE, 283, 0, 0, 0);
    gmouse_3d_menu_frame = art_ptr_lock(fid, &gmouse_3d_menu_frame_key);
    if (gmouse_3d_menu_frame == NULL) {
        goto err;
    }

    // actpick.frm - action pick
    fid = art_id(OBJ_TYPE_INTERFACE, 282, 0, 0, 0);
    gmouse_3d_pick_frame = art_ptr_lock(fid, &gmouse_3d_pick_frame_key);
    if (gmouse_3d_pick_frame == NULL) {
        goto err;
    }

    // acttohit.frm - action to hit
    fid = art_id(OBJ_TYPE_INTERFACE, 284, 0, 0, 0);
    gmouse_3d_to_hit_frame = art_ptr_lock(fid, &gmouse_3d_to_hit_frame_key);
    if (gmouse_3d_to_hit_frame == NULL) {
        goto err;
    }

    // blank.frm - used be mset000.frm for top of bouncing mouse cursor
    fid = art_id(OBJ_TYPE_INTERFACE, 0, 0, 0, 0);
    gmouse_3d_hex_base_frame = art_ptr_lock(fid, &gmouse_3d_hex_base_frame_key);
    if (gmouse_3d_hex_base_frame == NULL) {
        goto err;
    }

    // msef000.frm - hex mouse cursor
    fid = art_id(OBJ_TYPE_INTERFACE, 1, 0, 0, 0);
    gmouse_3d_hex_frame = art_ptr_lock(fid, &gmouse_3d_hex_frame_key);
    if (gmouse_3d_hex_frame == NULL) {
        goto err;
    }

    gmouse_3d_menu_frame_width = art_frame_width(gmouse_3d_menu_frame, 0, 0);
    gmouse_3d_menu_frame_height = art_frame_length(gmouse_3d_menu_frame, 0, 0);
    gmouse_3d_menu_frame_size = gmouse_3d_menu_frame_width * gmouse_3d_menu_frame_height;
    gmouse_3d_menu_frame_data = art_frame_data(gmouse_3d_menu_frame, 0, 0);

    gmouse_3d_pick_frame_width = art_frame_width(gmouse_3d_pick_frame, 0, 0);
    gmouse_3d_pick_frame_height = art_frame_length(gmouse_3d_pick_frame, 0, 0);
    gmouse_3d_pick_frame_size = gmouse_3d_pick_frame_width * gmouse_3d_pick_frame_height;
    gmouse_3d_pick_frame_data = art_frame_data(gmouse_3d_pick_frame, 0, 0);

    gmouse_3d_to_hit_frame_width = art_frame_width(gmouse_3d_to_hit_frame, 0, 0);
    gmouse_3d_to_hit_frame_height = art_frame_length(gmouse_3d_to_hit_frame, 0, 0);
    gmouse_3d_to_hit_frame_size = gmouse_3d_to_hit_frame_width * gmouse_3d_to_hit_frame_height;
    gmouse_3d_to_hit_frame_data = art_frame_data(gmouse_3d_to_hit_frame, 0, 0);

    gmouse_3d_hex_base_frame_width = art_frame_width(gmouse_3d_hex_base_frame, 0, 0);
    gmouse_3d_hex_base_frame_height = art_frame_length(gmouse_3d_hex_base_frame, 0, 0);
    gmouse_3d_hex_base_frame_size = gmouse_3d_hex_base_frame_width * gmouse_3d_hex_base_frame_height;
    gmouse_3d_hex_base_frame_data = art_frame_data(gmouse_3d_hex_base_frame, 0, 0);

    gmouse_3d_hex_frame_width = art_frame_width(gmouse_3d_hex_frame, 0, 0);
    gmouse_3d_hex_frame_height = art_frame_length(gmouse_3d_hex_frame, 0, 0);
    gmouse_3d_hex_frame_size = gmouse_3d_hex_frame_width * gmouse_3d_hex_frame_height;
    gmouse_3d_hex_frame_data = art_frame_data(gmouse_3d_hex_frame, 0, 0);

    return 0;

err:

    // NOTE: Original code is different. There is no call to this function.
    // Instead it either use deep nesting or bunch of goto's to unwind
    // locked frms from the point of failure.
    gmouse_3d_unlock_frames();

    return -1;
}

// 0x4458D0
static void gmouse_3d_unlock_frames()
{
    if (gmouse_3d_hex_base_frame_key != INVALID_CACHE_ENTRY) {
        art_ptr_unlock(gmouse_3d_hex_base_frame_key);
    }
    gmouse_3d_hex_base_frame = NULL;
    gmouse_3d_hex_base_frame_key = INVALID_CACHE_ENTRY;

    if (gmouse_3d_hex_frame_key != INVALID_CACHE_ENTRY) {
        art_ptr_unlock(gmouse_3d_hex_frame_key);
    }
    gmouse_3d_hex_frame = NULL;
    gmouse_3d_hex_frame_key = INVALID_CACHE_ENTRY;

    if (gmouse_3d_to_hit_frame_key != INVALID_CACHE_ENTRY) {
        art_ptr_unlock(gmouse_3d_to_hit_frame_key);
    }
    gmouse_3d_to_hit_frame = NULL;
    gmouse_3d_to_hit_frame_key = INVALID_CACHE_ENTRY;

    if (gmouse_3d_menu_frame_key != INVALID_CACHE_ENTRY) {
        art_ptr_unlock(gmouse_3d_menu_frame_key);
    }
    gmouse_3d_menu_frame = NULL;
    gmouse_3d_menu_frame_key = INVALID_CACHE_ENTRY;

    if (gmouse_3d_pick_frame_key != INVALID_CACHE_ENTRY) {
        art_ptr_unlock(gmouse_3d_pick_frame_key);
    }

    gmouse_3d_pick_frame = NULL;
    gmouse_3d_pick_frame_key = INVALID_CACHE_ENTRY;

    gmouse_3d_pick_frame_data = NULL;
    gmouse_3d_pick_frame_width = 0;
    gmouse_3d_pick_frame_height = 0;
    gmouse_3d_pick_frame_size = 0;
}

// 0x4459A8
static int gmouse_3d_set_flat_fid(int fid, Rect* rect)
{
    if (obj_change_fid(obj_mouse_flat, fid, rect) == 0) {
        return 0;
    }

    return -1;
}

// 0x4459CC
static int gmouse_3d_reset_flat_fid(Rect* rect)
{
    int fid = art_id(OBJ_TYPE_INTERFACE, gmouse_3d_mode_nums[gmouse_3d_current_mode], 0, 0, 0);
    if (obj_mouse_flat->fid == fid) {
        return -1;
    }

    // NOTE: Uninline.
    return gmouse_3d_set_flat_fid(fid, rect);
}

// 0x445A20
static int gmouse_3d_move_to(int x, int y, int elevation, Rect* a4)
{
    if (gmouse_mapper_mode == 0) {
        if (gmouse_3d_current_mode != GAME_MOUSE_MODE_MOVE) {
            int offsetX = 0;
            int offsetY = 0;
            CacheEntry* hexCursorFrmHandle;
            Art* hexCursorFrm = art_ptr_lock(obj_mouse_flat->fid, &hexCursorFrmHandle);
            if (hexCursorFrm != NULL) {
                art_frame_offset(hexCursorFrm, 0, &offsetX, &offsetY);

                int frameOffsetX;
                int frameOffsetY;
                art_frame_hot(hexCursorFrm, 0, 0, &frameOffsetX, &frameOffsetY);

                offsetX += frameOffsetX;
                offsetY += frameOffsetY;

                art_ptr_unlock(hexCursorFrmHandle);
            }

            obj_move(obj_mouse_flat, x + offsetX, y + offsetY, elevation, a4);
        } else {
            int tile = tile_num(x, y, 0);
            if (tile != -1) {
                int screenX;
                int screenY;

                bool v1 = false;
                Rect rect1;
                if (tile_coord(tile, &screenX, &screenY, 0) == 0) {
                    if (obj_move(obj_mouse, screenX + 16, screenY + 15, 0, &rect1) == 0) {
                        v1 = true;
                    }
                }

                Rect rect2;
                if (obj_move_to_tile(obj_mouse_flat, tile, elevation, &rect2) == 0) {
                    if (v1) {
                        rect_min_bound(&rect1, &rect2, &rect1);
                    } else {
                        rectCopy(&rect1, &rect2);
                    }

                    rectCopy(a4, &rect1);
                }
            }
        }
        return 0;
    }

    int tile;
    int x1 = 0;
    int y1 = 0;

    int fid = obj_mouse->fid;
    if (FID_TYPE(fid) == OBJ_TYPE_TILE) {
        int squareTile = square_num(x, y, elevation);
        if (squareTile == -1) {
            tile = HEX_GRID_WIDTH * (2 * (squareTile / SQUARE_GRID_WIDTH) + 1) + 2 * (squareTile % SQUARE_GRID_WIDTH) + 1;
            x1 = -8;
            y1 = 13;

            char* executable;
            config_get_string(&game_config, GAME_CONFIG_SYSTEM_KEY, GAME_CONFIG_EXECUTABLE_KEY, &executable);
            if (compat_stricmp(executable, "mapper") == 0) {
                if (tile_roof_visible()) {
                    if ((obj_dude->flags & OBJECT_HIDDEN) == 0) {
                        y1 = -83;
                    }
                }
            }
        } else {
            tile = -1;
        }
    } else {
        tile = tile_num(x, y, elevation);
    }

    if (tile != -1) {
        bool v1 = false;

        Rect rect1;
        Rect rect2;

        if (obj_move_to_tile(obj_mouse, tile, elevation, &rect1) == 0) {
            if (x1 != 0 || y1 != 0) {
                if (obj_offset(obj_mouse, x1, y1, &rect2) == 0) {
                    rect_min_bound(&rect1, &rect2, &rect1);
                }
            }
            v1 = true;
        }

        if (gmouse_3d_current_mode != GAME_MOUSE_MODE_MOVE) {
            int offsetX = 0;
            int offsetY = 0;
            CacheEntry* hexCursorFrmHandle;
            Art* hexCursorFrm = art_ptr_lock(obj_mouse_flat->fid, &hexCursorFrmHandle);
            if (hexCursorFrm != NULL) {
                art_frame_offset(hexCursorFrm, 0, &offsetX, &offsetY);

                int frameOffsetX;
                int frameOffsetY;
                art_frame_hot(hexCursorFrm, 0, 0, &frameOffsetX, &frameOffsetY);

                offsetX += frameOffsetX;
                offsetY += frameOffsetY;

                art_ptr_unlock(hexCursorFrmHandle);
            }

            if (obj_move(obj_mouse_flat, x + offsetX, y + offsetY, elevation, &rect2) == 0) {
                if (v1) {
                    rect_min_bound(&rect1, &rect2, &rect1);
                } else {
                    rectCopy(&rect1, &rect2);
                    v1 = true;
                }
            }
        } else {
            if (obj_move_to_tile(obj_mouse_flat, tile, elevation, &rect2) == 0) {
                if (v1) {
                    rect_min_bound(&rect1, &rect2, &rect1);
                } else {
                    rectCopy(&rect1, &rect2);
                    v1 = true;
                }
            }
        }

        if (v1) {
            rectCopy(a4, &rect1);
        }
    }

    return 0;
}

// 0x445EB8
static int gmouse_check_scrolling(int x, int y, int cursor)
{
    if (!gmouse_scrolling_enabled) {
        return -1;
    }

    int flags = 0;
    const int edge_margin = 8;

    if (x <= scr_size.ulx + edge_margin) {
        flags |= SCROLLABLE_W;
    }

    if (x >= scr_size.lrx - edge_margin) {
        flags |= SCROLLABLE_E;
    }

    if (y <= scr_size.uly + edge_margin) {
        flags |= SCROLLABLE_N;
    }

    if (y >= scr_size.lry - edge_margin) {
        flags |= SCROLLABLE_S;
    }

    int dx = 0;
    int dy = 0;

    switch (flags) {
    case SCROLLABLE_W:
        dx = -1;
        cursor = MOUSE_CURSOR_SCROLL_W;
        break;
    case SCROLLABLE_E:
        dx = 1;
        cursor = MOUSE_CURSOR_SCROLL_E;
        break;
    case SCROLLABLE_N:
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_N;
        break;
    case SCROLLABLE_N | SCROLLABLE_W:
        dx = -1;
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_NW;
        break;
    case SCROLLABLE_N | SCROLLABLE_E:
        dx = 1;
        dy = -1;
        cursor = MOUSE_CURSOR_SCROLL_NE;
        break;
    case SCROLLABLE_S:
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_S;
        break;
    case SCROLLABLE_S | SCROLLABLE_W:
        dx = -1;
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_SW;
        break;
    case SCROLLABLE_S | SCROLLABLE_E:
        dx = 1;
        dy = 1;
        cursor = MOUSE_CURSOR_SCROLL_SE;
        break;
    }

    if (dx == 0 && dy == 0) {
        return -1;
    }

    int rc = map_scroll(dx, dy);
    switch (rc) {
    case -1:
        // Scrolling is blocked for whatever reason, upgrade cursor to
        // appropriate blocked version.
        cursor += 8;
        // FALLTHROUGH
    case 0:
        gmouse_set_cursor(cursor);
        break;
    }

    return 0;
}

// Determines the appropriate mouse mode based on what's under the cursor.
// Returns GAME_MOUSE_MODE_CROSSHAIR if in combat and hovering over a targetable critter,
// GAME_MOUSE_MODE_ARROW if an interactable object is under the cursor,
// GAME_MOUSE_MODE_MOVE if hovering over an empty walkable tile.
static int gmouse_3d_determine_auto_mode(int mouseX, int mouseY, int elevation)
{
    // In combat, check if hovering over a targetable critter
    if (isInCombat()) {
        // Check if player has a weapon equipped
        Object* item;
        if (intface_get_current_item(&item) == 0) {
            if (item == nullptr || item_get_type(item) == ITEM_TYPE_WEAPON) {
                // Check for critters under cursor (excluding player)
                Object* critterTarget = object_under_mouse(OBJ_TYPE_CRITTER, false, elevation);
                if (critterTarget != nullptr && critter_is_active(critterTarget)) {
                    // Verify we can actually target this critter
                    int accuracy;
                    if (combat_to_hit(critterTarget, &accuracy)) {
                        return GAME_MOUSE_MODE_CROSSHAIR;
                    }
                }
            }
        }
    }

    // Check for interactable objects (items, scenery, critters for non-combat interaction)
    Object* target = object_under_mouse(-1, true, elevation);
    if (target != nullptr) {
        const auto fidType = FID_TYPE(target->fid);
        if (fidType != OBJ_TYPE_INTERFACE && fidType != OBJ_TYPE_WALL && !(exit_grid_pid(target->pid)))
            return GAME_MOUSE_MODE_ARROW;
    }
    return GAME_MOUSE_MODE_MOVE;
}

// Checks if a tooltip should be shown for the given object.
// Returns true if the object passes the same basic checks as obj_look_at.
// Note: target must be non-NULL (caller should check).
static bool gmouse_tooltip_should_show(Object* viewer, Object* target)
{
    if (critter_is_dead(viewer)) {
        return false;
    }

    if (FID_TYPE(target->fid) == OBJ_TYPE_TILE) {
        return false;
    }

    Proto* proto;
    if (proto_ptr(target->pid, &proto) == -1) {
        return false;
    }

    return true;
}

// Creates and shows the tooltip window at the given position.
static void gmouse_tooltip_show(const char* text, int mouseX, int mouseY)
{
    if (gmouse_tooltip_win != -1) {
        win_delete(gmouse_tooltip_win);
        gmouse_tooltip_win = -1;
    }

    int oldFont = text_curr();
    text_font(101);

    int textWidth = text_width(text);
    int textHeight = text_height();

    // Limit text width and calculate window dimensions
    if (textWidth > GMOUSE_TOOLTIP_MAX_WIDTH) {
        textWidth = GMOUSE_TOOLTIP_MAX_WIDTH;
    }

    int winWidth = textWidth + GMOUSE_TOOLTIP_PADDING * 2;
    int winHeight = textHeight + GMOUSE_TOOLTIP_PADDING * 2;

    // Position tooltip below and to the right of the cursor
    int winX = mouseX - 16;
    int winY = mouseY + 16;

    // Keep tooltip on screen
    if (winX + winWidth > scr_size.lrx) {
        winX = mouseX - winWidth - 4;
    }
    if (winY + winHeight > scr_size.lry - 100) { // Account for interface bar
        winY = mouseY - winHeight - 4;
    }
    if (winX < scr_size.ulx) {
        winX = scr_size.ulx;
    }
    if (winY < scr_size.uly) {
        winY = scr_size.uly;
    }

    gmouse_tooltip_win = win_add(winX, winY, winWidth, winHeight, colorTable[0], WINDOW_TRANSPARENT | WINDOW_MOVE_ON_TOP);
    if (gmouse_tooltip_win != -1) {
        unsigned char* buf = win_get_buf(gmouse_tooltip_win);
        if (buf != NULL) {
            // Fill background with a semi-dark color
            buf_fill(buf, winWidth, winHeight, winWidth, colorTable[0]);

            // Draw border
            draw_box(buf, winWidth, 0, 0, winWidth - 1, winHeight - 1, colorTable[8947]);

            // Draw text
            text_to_buf(buf + GMOUSE_TOOLTIP_PADDING * winWidth + GMOUSE_TOOLTIP_PADDING,
                text,
                winWidth - GMOUSE_TOOLTIP_PADDING * 2,
                winWidth,
                colorTable[32747]);

            win_draw(gmouse_tooltip_win);
        }
    }

    text_font(oldFont);
    gmouse_tooltip_visible = true;
}

// Hides and destroys the tooltip window.
void gmouse_tooltip_hide()
{
    if (gmouse_tooltip_win != -1) {
        win_delete(gmouse_tooltip_win);
        gmouse_tooltip_win = -1;
    }
    gmouse_tooltip_visible = false;
    gmouse_tooltip_object = NULL;
}

// Updates the tooltip based on the current hover state.
// Should be called from gmouse_bk_process when hovering in ARROW mode.
static void gmouse_tooltip_update(Object* target, int mouseX, int mouseY)
{
    if (!tweaks_object_tooltip()) {
        if (gmouse_tooltip_visible) {
            gmouse_tooltip_hide();
        }
        return;
    }

    if (target == NULL || !gmouse_tooltip_should_show(obj_dude, target)) {
        if (gmouse_tooltip_visible) {
            gmouse_tooltip_hide();
        }
        return;
    }

    // Only update if the target changed
    if (target != gmouse_tooltip_object) {
        gmouse_tooltip_object = target;
        const char* name = object_name(target);
        if (name != NULL && name[0] != '\0') {
            gmouse_tooltip_show(name, mouseX, mouseY);
        } else {
            gmouse_tooltip_hide();
        }
    }
}

// 0x445FD0
void gmouse_remove_item_outline(Object* object)
{
    if (outlined_object != NULL && outlined_object == object) {
        Rect rect;
        if (obj_remove_outline(object, &rect) == 0) {
            tile_refresh_rect(&rect, map_elevation);
        }
        outlined_object = NULL;
    }
}

void gameMouseRefreshImmediately()
{
    gmouse_bk_process();
    renderPresent();
}

// Applies accessibility outlines to all interactable objects on current elevation.
static void gmouse_accessibility_highlight_on()
{
    if (accessibility_highlight_active) {
        return;
    }

    Object* obj = obj_find_first_at(map_elevation);
    while (obj != NULL) {
        int objType = PID_TYPE(obj->pid);

        if (objType == OBJ_TYPE_CRITTER) {
            // Living NPCs (not dead, not player)
            if (obj != obj_dude && !critter_is_dead(obj)) {
                obj_outline_object(obj, OUTLINE_TYPE_FRIENDLY, NULL, true);
            }
        } else if (objType == OBJ_TYPE_ITEM) {
            // Ground items and containers
            Proto* proto;
            if (proto_ptr(obj->pid, &proto) == 0) {
                if (proto->item.type == ITEM_TYPE_CONTAINER && obj->data.inventory.length == 0) {
                    obj = obj_find_next_at();
                    continue;
                }
                obj_outline_object(obj, OUTLINE_TYPE_ITEM, NULL, true);
            }
        } else if (objType == OBJ_TYPE_SCENERY) {
            // Doors, stairs, ladders
            Proto* proto;
            if (proto_ptr(obj->pid, &proto) == 0) {
                int sceneryType = proto->scenery.type;
                if (sceneryType == SCENERY_TYPE_DOOR
                    || sceneryType == SCENERY_TYPE_STAIRS
                    || sceneryType == SCENERY_TYPE_LADDER_UP
                    || sceneryType == SCENERY_TYPE_LADDER_DOWN) {
                    obj_outline_object(obj, OUTLINE_TYPE_4, NULL, true);
                }
            }
        }

        obj = obj_find_next_at();
    }

    accessibility_highlight_active = true;
    tile_refresh_display();
}

// Removes accessibility outlines from all objects on current elevation.
static void gmouse_accessibility_highlight_off()
{
    if (!accessibility_highlight_active) {
        return;
    }

    Object* obj = obj_find_first_at(map_elevation);
    while (obj != NULL) {
        obj_remove_outline(obj, NULL);
        obj = obj_find_next_at();
    }

    accessibility_highlight_active = false;
    tile_refresh_display();
}

// Checks if the accessibility highlight key is held and toggles highlights.
void gmouse_accessibility_highlight_check()
{
    int highlightKey = tweaks_highlight_objects_key();
    if (highlightKey <= 0 || highlightKey >= SDL_NUM_SCANCODES) {
        return;
    }

    if (keys[highlightKey]) {
        gmouse_accessibility_highlight_on();
    } else {
        gmouse_accessibility_highlight_off();
    }
}

} // namespace fallout
