#include "plib/gnw/mouse.h"

#include "plib/color/color.h"
#include "plib/gnw/dxinput.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/touch.h"
#include "plib/gnw/vcr.h"

namespace fallout {

static void mouse_colorize();
static void mouse_anim();
static void mouse_clip();

// The default mouse cursor buffer.
//
// Initially it contains color codes, which will be replaced at startup
// according to loaded palette.
//
// Available color codes:
// - 0: transparent
// - 1: white
// - 15:  black
//
// 0x539D80
static unsigned char or_mask[MOUSE_DEFAULT_CURSOR_SIZE] = {
    // clang-format off
    1,  1,  1,  1,  1,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15,  1,  1, 0,
    1, 15, 15, 15, 15, 15,  1, 1,
    1, 15,  1,  1, 15, 15, 15, 1,
    1,  1,  1,  1,  1, 15, 15, 1,
    0,  0,  0,  0,  1,  1,  1, 1,
    // clang-format on
};

// 0x539DC0
static int mouse_idling = 0;

// 0x539DC4
static unsigned char* mouse_buf = NULL;

// 0x539DC8
static unsigned char* mouse_shape = NULL;

// 0x539DCC
static unsigned char* mouse_fptr = NULL;

// 0x539DD0
static double mouse_sensitivity = 1.0;

// 0x539DDC
static int last_buttons = 0;

// 0x671F18
static bool mouse_is_hidden;

// 0x671F1C
static int raw_x;

// 0x671F20
static int mouse_length;

// 0x671F24
static int raw_y;

// 0x671F28
static int raw_buttons;

// 0x671F2C
static int mouse_y;

// 0x671F30
static int mouse_x;

// 0x671F34
static bool mouse_disabled;

// 0x671F38
static int mouse_buttons;

// 0x671F10
static unsigned int mouse_speed;

// 0x671F3C
static int mouse_curr_frame;

// 0x671F40
static bool have_mouse;

// 0x671F44
static int mouse_full;

// 0x671F48
static int mouse_width;

// 0x671F4C
static int mouse_num_frames;

// 0x671F50
static int mouse_hoty;

// 0x671F54
static int mouse_hotx;

// 0x671F14
static unsigned int mouse_idle_start_time;

// 0x671F58
ScreenTransBlitFunc* mouse_blit_trans;

// 0x671F5C
ScreenBlitFunc* mouse_blit;

// 0x671F60
static char mouse_trans;

static int gMouseWheelX = 0;
static int gMouseWheelY = 0;

// 0x4B4780
int GNW_mouse_init()
{
    have_mouse = false;
    mouse_disabled = false;

    mouse_is_hidden = true;

    mouse_colorize();

    if (mouse_set_shape(NULL, 0, 0, 0, 0, 0, 0) == -1) {
        return -1;
    }

    if (!dxinput_acquire_mouse()) {
        return -1;
    }

    have_mouse = true;
    mouse_x = scr_size.lrx / 2;
    mouse_y = scr_size.lry / 2;
    raw_x = scr_size.lrx / 2;
    raw_y = scr_size.lry / 2;
    mouse_idle_start_time = get_time();

    return 0;
}

// 0x4B4818
void GNW_mouse_exit()
{
    dxinput_unacquire_mouse();

    if (mouse_buf != NULL) {
        mem_free(mouse_buf);
        mouse_buf = NULL;
    }

    if (mouse_fptr != NULL) {
        remove_bk_process(mouse_anim);
        mouse_fptr = NULL;
    }
}

// 0x4B485C
static void mouse_colorize()
{
    for (int index = 0; index < 64; index++) {
        switch (or_mask[index]) {
        case 0:
            or_mask[index] = colorTable[0];
            break;
        case 1:
            or_mask[index] = colorTable[8456];
            break;
        case 15:
            or_mask[index] = colorTable[32767];
            break;
        }
    }
}

// 0x4B48A4
void mouse_get_shape(unsigned char** buf, int* width, int* length, int* full, int* hotx, int* hoty, char* trans)
{
    *buf = mouse_shape;
    *width = mouse_width;
    *length = mouse_length;
    *full = mouse_full;
    *hotx = mouse_hotx;
    *hoty = mouse_hoty;
    *trans = mouse_trans;
}

// 0x4B48EC
int mouse_set_shape(unsigned char* buf, int width, int length, int full, int hotx, int hoty, char trans)
{
    Rect rect;
    unsigned char* cursor_frame;
    int hotx_delta;
    int hoty_delta;

    cursor_frame = buf;

    if (buf == NULL) {
        // NOTE: Original code looks tail recursion optimization.
        return mouse_set_shape(or_mask, MOUSE_DEFAULT_CURSOR_WIDTH, MOUSE_DEFAULT_CURSOR_HEIGHT, MOUSE_DEFAULT_CURSOR_WIDTH, 1, 1, colorTable[0]);
    }

    bool cursorWasHidden = mouse_is_hidden;
    if (!mouse_is_hidden && have_mouse) {
        mouse_is_hidden = true;
        mouse_get_rect(&rect);
        win_refresh_all(&rect);
    }

    if (width != mouse_width || length != mouse_length) {
        unsigned char* buf = (unsigned char*)mem_malloc(width * length);
        if (buf == NULL) {
            if (!cursorWasHidden) {
                mouse_show();
            }
            return -1;
        }

        if (mouse_buf != NULL) {
            mem_free(mouse_buf);
        }

        mouse_buf = buf;
    }

    mouse_width = width;
    mouse_length = length;
    mouse_full = full;
    mouse_shape = cursor_frame;
    mouse_trans = trans;

    if (mouse_fptr) {
        remove_bk_process(mouse_anim);
        mouse_fptr = NULL;
    }

    hotx_delta = mouse_hotx - hotx;
    mouse_hotx = hotx;

    mouse_x += hotx_delta;

    hoty_delta = mouse_hoty - hoty;
    mouse_hoty = hoty;

    mouse_y += hoty_delta;

    mouse_clip();

    if (!cursorWasHidden) {
        mouse_show();
    }

    raw_x = mouse_x;
    raw_y = mouse_y;

    return 0;
}

// 0x4B4A4C
int mouse_get_anim(unsigned char** frames, int* num_frames, int* width, int* length, int* hotx, int* hoty, char* trans, int* speed)
{
    if (mouse_fptr == NULL) {
        return -1;
    }

    *frames = mouse_fptr;
    *num_frames = mouse_num_frames;
    *width = mouse_width;
    *length = mouse_length;
    *hotx = mouse_hotx;
    *hoty = mouse_hoty;
    *trans = mouse_trans;
    *speed = mouse_speed;

    return 0;
}

// 0x4B4AAC
int mouse_set_anim_frames(unsigned char* frames, int num_frames, int start_frame, int width, int length, int hotx, int hoty, char trans, int speed)
{
    if (mouse_set_shape(frames + start_frame * width * length, width, length, width, hotx, hoty, trans) == -1) {
        return -1;
    }

    mouse_fptr = frames;
    mouse_num_frames = num_frames;
    mouse_curr_frame = start_frame;
    mouse_speed = speed;

    add_bk_process(mouse_anim);

    return 0;
}

// 0x4B4B10
static void mouse_anim()
{
    // 0x539DD8
    static unsigned int ticker = 0;

    if (elapsed_time(ticker) >= mouse_speed) {
        ticker = get_time();

        if (++mouse_curr_frame == mouse_num_frames) {
            mouse_curr_frame = 0;
        }

        mouse_shape = mouse_width * mouse_curr_frame * mouse_length + mouse_fptr;

        if (!mouse_is_hidden) {
            mouse_show();
        }
    }
}

// 0x4B4B88
void mouse_show()
{
    unsigned char* cursorData;
    int clipX;
    int clipWidth;
    int clipY;
    int clipHeight;
    int cursorDataIndex;

    cursorData = mouse_buf;
    if (have_mouse) {
        if (!mouse_blit_trans || !mouse_is_hidden) {
            win_get_mouse_buf(mouse_buf);
            cursorData = mouse_buf;
            cursorDataIndex = 0;

            for (int y = 0; y < mouse_length; y++) {
                for (int x = 0; x < mouse_width; x++) {
                    unsigned char pixel = mouse_shape[y * mouse_full + x];
                    if (pixel != mouse_trans) {
                        cursorData[cursorDataIndex] = pixel;
                    }
                    cursorDataIndex++;
                }
            }
        }

        if (mouse_x >= scr_size.ulx) {
            if (mouse_width + mouse_x - 1 <= scr_size.lrx) {
                clipWidth = mouse_width;
                clipX = 0;
            } else {
                clipX = 0;
                clipWidth = scr_size.lrx - mouse_x + 1;
            }
        } else {
            clipX = scr_size.ulx - mouse_x;
            clipWidth = mouse_width - (scr_size.ulx - mouse_x);
        }

        if (mouse_y >= scr_size.uly) {
            if (mouse_length + mouse_y - 1 <= scr_size.lry) {
                clipY = 0;
                clipHeight = mouse_length;
            } else {
                clipY = 0;
                clipHeight = scr_size.lry - mouse_y + 1;
            }
        } else {
            clipY = scr_size.uly - mouse_y;
            clipHeight = mouse_length - (scr_size.uly - mouse_y);
        }

        mouse_buf = cursorData;
        if (mouse_blit_trans && mouse_is_hidden) {
            mouse_blit_trans(mouse_shape, mouse_full, mouse_length, clipX, clipY, clipWidth, clipHeight, clipX + mouse_x, clipY + mouse_y, mouse_trans);
        } else {
            mouse_blit(mouse_buf, mouse_width, mouse_length, clipX, clipY, clipWidth, clipHeight, clipX + mouse_x, clipY + mouse_y);
        }

        cursorData = mouse_buf;
        mouse_is_hidden = false;
    }
    mouse_buf = cursorData;
}

// 0x4B4D70
void mouse_hide()
{
    Rect rect;

    if (have_mouse) {
        if (!mouse_is_hidden) {
            rect.ulx = mouse_x;
            rect.uly = mouse_y;
            rect.lrx = mouse_x + mouse_width - 1;
            rect.lry = mouse_y + mouse_length - 1;

            mouse_is_hidden = true;
            win_refresh_all(&rect);
        }
    }
}

// 0x4B4DD8
// New version of mouse_info for touch devices
void mouse_info()
{
    if (!have_mouse || mouse_is_hidden || mouse_disabled) {
        return;
    }

    // FIXME: Need to know a workaround between the Mouse not being locked
    // inside window in PC and the Touch not being broken on Mobile devices.

    Gesture gesture;
    if (touch_get_gesture(&gesture)) {
        static int prevx;
        static int prevy;
        // Track if current pan/drag started near cursor (for click+drag) vs far (just positioning)
        static bool pencil_dragging = false;

        switch (gesture.type) {
        case kUnrecognized:
            break;
        case kTap:
            if (gesture.numberOfTouches == 1) {
                // Get current cursor position
                int cursor_x, cursor_y;
                mouse_get_position(&cursor_x, &cursor_y);

                // Calculate distance from tap to cursor (for click radius check)
                int dx = gesture.x - cursor_x;
                int dy = gesture.y - cursor_y;
                int distance_sq = dx * dx + dy * dy;

                // Scale click radius based on screen resolution (base is 640x480)
                int screen_width = screenGetWidth();
                int radius = (40 * screen_width) / 640;
                int radius_sq = radius * radius;

                if (distance_sq < radius_sq) {
                    // Tap NEAR cursor = click at cursor position (no movement needed)
                    // This mimics mouse behavior: cursor is already there, just click
                    mouse_simulate_input(0, 0, MOUSE_STATE_LEFT_BUTTON_DOWN);
                } else {
                    // Tap FAR from cursor = move cursor only (no click)
                    // This allows positioning before clicking, like moving a mouse
                    mouse_simulate_input(dx, dy, 0);
                }
            } else if (gesture.numberOfTouches == 2) {
                // Two-finger tap = right-click at cursor position
                mouse_simulate_input(0, 0, MOUSE_STATE_RIGHT_BUTTON_DOWN);
            } else if (gesture.numberOfTouches == 3) {
                // Three-finger tap = left-click at cursor position
                mouse_simulate_input(0, 0, MOUSE_STATE_LEFT_BUTTON_DOWN);
            }
            break;
        case kLongPress:
        case kPan:
            if (gesture.state == kBegan) {
                prevx = gesture.x;
                prevy = gesture.y;
                // Check if pan/drag started near cursor (determines if it's a drag or reposition)
                if (gesture.numberOfTouches == 1) {
                    int cursor_x, cursor_y;
                    mouse_get_position(&cursor_x, &cursor_y);
                    int dx = gesture.x - cursor_x;
                    int dy = gesture.y - cursor_y;
                    int distance_sq = dx * dx + dy * dy;
                    int screen_width = screenGetWidth();
                    int radius = (40 * screen_width) / 640;
                    int radius_sq = radius * radius;

                    if (distance_sq < radius_sq) {
                        // Started ON/NEAR cursor = this is a drag operation
                        pencil_dragging = true;
                    } else {
                        // Started AWAY from cursor = just repositioning
                        pencil_dragging = false;
                        // Move cursor to touch position immediately
                        mouse_simulate_input(dx, dy, 0);
                    }
                }
            }

            if (gesture.type == kLongPress) {
                if (gesture.numberOfTouches == 1 && gesture.state == kBegan) {
                    // Single finger long-press = RIGHT-click for examine/context menu
                    // Use absolute positioning to cursor follows touch
                    int cursor_x, cursor_y;
                    mouse_get_position(&cursor_x, &cursor_y);
                    mouse_simulate_input(gesture.x - cursor_x, gesture.y - cursor_y, MOUSE_STATE_RIGHT_BUTTON_DOWN);
                } else if (gesture.numberOfTouches == 2 && (gesture.state == kBegan || gesture.state == kChanged)) {
                    // Two-finger long-press = LEFT-click + drag (for dragging operations)
                    mouse_simulate_input(gesture.x - prevx, gesture.y - prevy, MOUSE_STATE_LEFT_BUTTON_DOWN);
                }

                if (gesture.state == kEnded) {
                    mouse_simulate_input(0, 0, 0);
                }
            } else if (gesture.type == kPan) {
                if (gesture.state == kEnded && gesture.numberOfTouches == 1 && pencil_dragging) {
                    mouse_simulate_input(0, 0, 0);
                    pencil_dragging = false;
                }

                if (gesture.numberOfTouches == 1) {
                    // Use absolute positioning: cursor follows touch/pencil exactly
                    int cursor_x, cursor_y;
                    mouse_get_position(&cursor_x, &cursor_y);
                    int dx = gesture.x - cursor_x;
                    int dy = gesture.y - cursor_y;

                    if (pencil_dragging) {
                        // Dragging = cursor follows with left button held
                        mouse_simulate_input(dx, dy, MOUSE_STATE_LEFT_BUTTON_DOWN);
                    } else {
                        // Just repositioning = cursor follows, no button
                        mouse_simulate_input(dx, dy, 0);
                    }
                } else if (gesture.numberOfTouches == 2) {
                    gMouseWheelX = (prevx - gesture.x) / 2;
                    gMouseWheelY = (gesture.y - prevy) / 2;

                    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
                        mouse_buttons |= MOUSE_EVENT_WHEEL;
                        raw_buttons |= MOUSE_EVENT_WHEEL;
                    }
                }
            }

            prevx = gesture.x;
            prevy = gesture.y;
            break;
        }

        return;
    }

    int buttons = 0;

    MouseData mouseData;
    if (dxinput_get_mouse_state(&mouseData)) {
        if (mouseData.buttons[0] == 1) {
            buttons |= MOUSE_STATE_LEFT_BUTTON_DOWN;
        }

        if (mouseData.buttons[1] == 1) {
            buttons |= MOUSE_STATE_RIGHT_BUTTON_DOWN;
        }
    }

    if (vcr_state == VCR_STATE_PLAYING) {
        int x = mouseData.x;
        int y = mouseData.y;
        if (((vcr_terminate_flags & VCR_TERMINATE_ON_MOUSE_PRESS) != 0 && buttons != 0)
            || ((vcr_terminate_flags & VCR_TERMINATE_ON_MOUSE_MOVE) != 0 && (x != 0 || y != 0))) {
            vcr_terminated_condition = VCR_PLAYBACK_COMPLETION_REASON_TERMINATED;
            vcr_stop();
            return;
        }
        // In VCR playback mode, use delta-based input from recording
        mouse_simulate_input(0, 0, last_buttons);
    } else {
        // Use absolute positioning for perfect cursor sync with OS
        // This ensures the game cursor always matches the native cursor position
        int targetX = mouseData.absX - mouse_hotx;
        int targetY = mouseData.absY - mouse_hoty;

        // Calculate effective delta for VCR recording
        int deltaX = targetX - mouse_x;
        int deltaY = targetY - mouse_y;

        mouse_simulate_input(deltaX, deltaY, buttons);
    }

    // TODO: Move to `_mouse_simulate_input`.
    // TODO: Record wheel event in VCR.
    gMouseWheelX = mouseData.wheelX;
    gMouseWheelY = mouseData.wheelY;

    if (gMouseWheelX != 0 || gMouseWheelY != 0) {
        mouse_buttons |= MOUSE_EVENT_WHEEL;
        raw_buttons |= MOUSE_EVENT_WHEEL;
    }
}

// 0x4B4ECC
void mouse_simulate_input(int delta_x, int delta_y, int buttons)
{
    // 0x671F64
    static unsigned int right_time;

    // 0x671F68
    static unsigned int left_time;

    // 0x671F6C
    static int old;

    if (!have_mouse || mouse_is_hidden) {
        return;
    }

    if (delta_x || delta_y || buttons != last_buttons) {
        if (vcr_state == 0) {
            if (vcr_buffer_index == VCR_BUFFER_CAPACITY - 1) {
                vcr_dump_buffer();
            }

            VcrEntry* vcrEntry = &(vcr_buffer[vcr_buffer_index]);
            vcrEntry->type = VCR_ENTRY_TYPE_MOUSE_EVENT;
            vcrEntry->time = vcr_time;
            vcrEntry->counter = vcr_counter;
            vcrEntry->mouseEvent.dx = delta_x;
            vcrEntry->mouseEvent.dy = delta_y;
            vcrEntry->mouseEvent.buttons = buttons;

            vcr_buffer_index++;
        }
    } else {
        if (last_buttons == 0) {
            if (!mouse_idling) {
                mouse_idle_start_time = get_time();
                mouse_idling = 1;
            }

            last_buttons = 0;
            raw_buttons = 0;
            mouse_buttons = 0;

            return;
        }
    }

    mouse_idling = 0;
    last_buttons = buttons;
    old = mouse_buttons;
    mouse_buttons = 0;

    if ((old & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x01) != 0) {
            mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_REPEAT;

            if (elapsed_time(left_time) > BUTTON_REPEAT_TIME) {
                mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
                left_time = get_time();
            }
        } else {
            mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_UP;
        }
    } else {
        if ((buttons & 0x01) != 0) {
            mouse_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
            left_time = get_time();
        }
    }

    if ((old & MOUSE_EVENT_RIGHT_BUTTON_DOWN_REPEAT) != 0) {
        if ((buttons & 0x02) != 0) {
            mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_REPEAT;
            if (elapsed_time(right_time) > BUTTON_REPEAT_TIME) {
                mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
                right_time = get_time();
            }
        } else {
            mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_UP;
        }
    } else {
        if (buttons & 0x02) {
            mouse_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
            right_time = get_time();
        }
    }

    raw_buttons = mouse_buttons;

    if (delta_x != 0 || delta_y != 0) {
        Rect mouseRect;
        mouseRect.ulx = mouse_x;
        mouseRect.uly = mouse_y;
        mouseRect.lrx = mouse_width + mouse_x - 1;
        mouseRect.lry = mouse_length + mouse_y - 1;

        mouse_x += delta_x;
        mouse_y += delta_y;
        mouse_clip();

        win_refresh_all(&mouseRect);

        mouse_show();

        raw_x = mouse_x;
        raw_y = mouse_y;
    }
}

// 0x4B5154
bool mouse_in(int left, int top, int right, int bottom)
{
    if (!have_mouse) {
        return false;
    }

    return mouse_length + mouse_y > top
        && right >= mouse_x
        && mouse_width + mouse_x > left
        && bottom >= mouse_y;
}

// 0x4B51C0
bool mouse_click_in(int left, int top, int right, int bottom)
{
    if (!have_mouse) {
        return false;
    }

    return mouse_hoty + mouse_y >= top
        && mouse_hotx + mouse_x <= right
        && mouse_hotx + mouse_x >= left
        && mouse_hoty + mouse_y <= bottom;
}

// 0x4B522C
void mouse_get_rect(Rect* rect)
{
    rect->ulx = mouse_x;
    rect->uly = mouse_y;
    rect->lrx = mouse_width + mouse_x - 1;
    rect->lry = mouse_length + mouse_y - 1;
}

// 0x4B5268
void mouse_get_position(int* x, int* y)
{
    *x = mouse_hotx + mouse_x;
    *y = mouse_hoty + mouse_y;
}

// 0x4B528C
void mouse_set_position(int x, int y)
{
    mouse_x = x - mouse_hotx;
    mouse_y = y - mouse_hoty;
    raw_y = y - mouse_hoty;
    raw_x = x - mouse_hotx;
    mouse_clip();
}

// 0x4B52C0
static void mouse_clip()
{
    if (mouse_hotx + mouse_x < scr_size.ulx) {
        mouse_x = scr_size.ulx - mouse_hotx;
    } else if (mouse_hotx + mouse_x > scr_size.lrx) {
        mouse_x = scr_size.lrx - mouse_hotx;
    }

    if (mouse_hoty + mouse_y < scr_size.uly) {
        mouse_y = scr_size.uly - mouse_hoty;
    } else if (mouse_hoty + mouse_y > scr_size.lry) {
        mouse_y = scr_size.lry - mouse_hoty;
    }
}

// 0x4B5328
int mouse_get_buttons()
{
    return mouse_buttons;
}

// 0x4B5330
bool mouse_hidden()
{
    return mouse_is_hidden;
}

// 0x4B5338
void mouse_get_hotspot(int* hotx, int* hoty)
{
    *hotx = mouse_hotx;
    *hoty = mouse_hoty;
}

// 0x4B534C
void mouse_set_hotspot(int hotx, int hoty)
{
    bool mh;

    mh = mouse_is_hidden;
    if (!mouse_is_hidden) {
        mouse_hide();
    }

    mouse_x += mouse_hotx - hotx;
    mouse_y += mouse_hoty - hoty;
    mouse_hotx = hotx;
    mouse_hoty = hoty;

    if (mh) {
        mouse_show();
    }
}

// 0x4B53DC
bool mouse_query_exist()
{
    return have_mouse;
}

// 0x4B53E4
void mouse_get_raw_state(int* x, int* y, int* buttons)
{
    MouseData mouseData;
    if (!dxinput_get_mouse_state(&mouseData)) {
        mouseData.x = 0;
        mouseData.y = 0;
        mouseData.buttons[0] = (mouse_buttons & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0;
        mouseData.buttons[1] = (mouse_buttons & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0;
    }

    raw_buttons = 0;
    raw_x += mouseData.x;
    raw_y += mouseData.y;

    if (mouseData.buttons[0] != 0) {
        raw_buttons |= MOUSE_EVENT_LEFT_BUTTON_DOWN;
    }

    if (mouseData.buttons[1] != 0) {
        raw_buttons |= MOUSE_EVENT_RIGHT_BUTTON_DOWN;
    }

    *x = raw_x;
    *y = raw_y;
    *buttons = raw_buttons;
}

// 0x4B54A4
void mouse_disable()
{
    mouse_disabled = true;
}

// 0x4B54B0
void mouse_enable()
{
    mouse_disabled = false;
}

// 0x4B54BC
bool mouse_is_disabled()
{
    return mouse_disabled;
}

// 0x4B54C4
void mouse_set_sensitivity(double value)
{
    if (value >= 1.0 && value <= 2.5) {
        mouse_sensitivity = value;
    }
}

// 0x4B54F4
double mouse_get_sensitivity()
{
    return mouse_sensitivity;
}

// 0x4B54FC
unsigned int mouse_elapsed_time()
{
    if (mouse_idling) {
        if (have_mouse && !mouse_is_hidden && !mouse_disabled) {
            return elapsed_time(mouse_idle_start_time);
        }
        mouse_idling = false;
    }
    return 0;
}

// 0x4B553C
void mouse_reset_elapsed_time()
{
    if (mouse_idling) {
        mouse_idling = false;
    }
}

void mouseGetPositionInWindow(int win, int* x, int* y)
{
    mouse_get_position(x, y);

    Window* window = GNW_find(win);
    if (window != NULL) {
        *x -= window->rect.ulx;
        *y -= window->rect.uly;
    }
}

bool mouseHitTestInWindow(int win, int left, int top, int right, int bottom)
{
    Window* window = GNW_find(win);
    if (window != NULL) {
        left += window->rect.ulx;
        top += window->rect.uly;
        right += window->rect.ulx;
        bottom += window->rect.uly;
    }

    return mouse_click_in(left, top, right, bottom);
}

void mouseGetWheel(int* x, int* y)
{
    *x = gMouseWheelX;
    *y = gMouseWheelY;
}

void convertMouseWheelToArrowKey(int* keyCodePtr)
{
    if (*keyCodePtr == -1) {
        if ((mouse_get_buttons() & MOUSE_EVENT_WHEEL) != 0) {
            int wheelX;
            int wheelY;
            mouseGetWheel(&wheelX, &wheelY);

            if (wheelY > 0) {
                *keyCodePtr = KEY_ARROW_UP;
            } else if (wheelY < 0) {
                *keyCodePtr = KEY_ARROW_DOWN;
            }
        }
    }
}

} // namespace fallout
