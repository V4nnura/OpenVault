#include "plib/gnw/intrface.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

namespace fallout {

// Maximum number of timed messages.
static constexpr int timed_msgs = 5;

static int create_pull_down(char** stringList, int stringListLength, int x, int y, int foregroundColor, int backgroundColor, Rect* rect);
static void win_debug_delete(int btn, int keyCode);
static int find_first_letter(int ch, char** stringList, int stringListLength);
static int process_pull_down(int win, Rect* rect, char** items, int itemsLength, int foregroundColor, int backgroundColor, MenuBar* menuBar, int pulldownIndex);
static void tm_watch_msgs();
static void tm_kill_msg();
static void tm_kill_out_of_order(int queueIndex);
static void tm_click_response(int btn);
static bool tm_index_active(int queueIndex);

// 0x53A268
static int wd = -1;

// 0x53A270
static bool tm_watch_active = false;

// 0x6B06D0
static struct {
    int taken;
    int y;
} tm_location[timed_msgs];

// 0x6B06F8
static struct {
    int created;
    int id;
    int location;
} tm_queue[timed_msgs];

// 0x6B0734
static int tm_text_y;

// 0x6B0738
static int tm_text_x;

// 0x6B073C
static int tm_h;

// 0x6B0740
static unsigned int tm_persistence;

// 0x6B0744
static int tm_kill;

// 0x6B0748
static int scr_center_x;

// 0x6B074C
static int tm_add;

// 0x4C6AA0
int win_list_select(const char* title, char** fileList, int fileListLength, SelectFunc* callback, int x, int y, int color)
{
    return win_list_select_at(title, fileList, fileListLength, callback, x, y, color, 0);
}

// 0x4C6AEC
int win_list_select_at(const char* title, char** items, int itemsLength, SelectFunc* callback, int x, int y, int color, int start)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    int listViewWidth = win_width_needed(items, itemsLength);
    int windowWidth = listViewWidth + 16;

    int titleWidth = text_width(title);
    if (titleWidth > windowWidth) {
        windowWidth = titleWidth;
        listViewWidth = titleWidth - 16;
    }

    windowWidth += 20;

    int win;
    int windowHeight;
    int listViewCapacity = 10;
    for (int heightMultiplier = 13; heightMultiplier > 8; heightMultiplier--) {
        windowHeight = heightMultiplier * text_height() + 22;
        win = win_add(x, y, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
        if (win != -1) {
            break;
        }
        listViewCapacity--;
    }

    if (win == -1) {
        return -1;
    }

    Window* window = GNW_find(win);
    Rect* windowRect = &(window->rect);
    unsigned char* windowBuffer = window->buffer;

    draw_box(windowBuffer,
        windowWidth,
        0,
        0,
        windowWidth - 1,
        windowHeight - 1,
        colorTable[0]);
    draw_shaded_box(windowBuffer,
        windowWidth,
        1,
        1,
        windowWidth - 2,
        windowHeight - 2,
        colorTable[GNW_wcolor[1]],
        colorTable[GNW_wcolor[2]]);

    buf_fill(windowBuffer + windowWidth * 5 + 5,
        windowWidth - 11,
        text_height() + 3,
        windowWidth,
        colorTable[GNW_wcolor[0]]);

    text_to_buf(windowBuffer + windowWidth / 2 + 8 * windowWidth - text_width(title) / 2,
        title,
        windowWidth,
        windowWidth,
        colorTable[GNW_wcolor[3]]);

    draw_shaded_box(windowBuffer,
        windowWidth,
        5,
        5,
        windowWidth - 6,
        text_height() + 8,
        colorTable[GNW_wcolor[2]],
        colorTable[GNW_wcolor[1]]);

    int listViewX = 8;
    int listViewY = text_height() + 16;
    unsigned char* listViewBuffer = windowBuffer + windowWidth * listViewY + listViewX;
    int listViewMaxY = listViewCapacity * text_height() + listViewY;

    buf_fill(listViewBuffer + windowWidth * (-2) + (-3),
        listViewWidth + listViewX - 2,
        listViewCapacity * text_height() + 2,
        windowWidth,
        colorTable[GNW_wcolor[0]]);

    int scrollOffset = start;
    if (start < 0 || start >= itemsLength) {
        scrollOffset = 0;
    }

    // Relative to `scrollOffset`.
    int selectedItemIndex;
    if (itemsLength - scrollOffset < listViewCapacity) {
        int newScrollOffset = itemsLength - listViewCapacity;
        if (newScrollOffset < 0) {
            newScrollOffset = 0;
        }
        int oldScrollOffset = scrollOffset;
        scrollOffset = newScrollOffset;
        selectedItemIndex = oldScrollOffset - newScrollOffset;
    } else {
        selectedItemIndex = 0;
    }

    win_text(win,
        items + start,
        itemsLength < listViewCapacity ? itemsLength : listViewCapacity,
        listViewWidth,
        listViewX,
        listViewY,
        color | 0x2000000);

    lighten_buf(listViewBuffer + windowWidth * selectedItemIndex * text_height(),
        listViewWidth,
        text_height(),
        windowWidth);

    draw_shaded_box(windowBuffer,
        windowWidth,
        5,
        listViewY - 3,
        listViewWidth + 10,
        listViewMaxY,
        colorTable[GNW_wcolor[2]],
        colorTable[GNW_wcolor[1]]);

    win_register_text_button(win,
        windowWidth - 25,
        listViewY - 3,
        -1,
        -1,
        KEY_ARROW_UP,
        -1,
        "\x18",
        0);

    win_register_text_button(win,
        windowWidth - 25,
        listViewMaxY - text_height() - 5,
        -1,
        -1,
        KEY_ARROW_DOWN,
        -1,
        "\x19",
        0);

    win_register_text_button(win,
        windowWidth / 2 - 32,
        windowHeight - 8 - text_height() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Done",
        0);

    int scrollbarX = windowWidth - 21;
    int scrollbarY = listViewY + text_height() + 7;
    int scrollbarKnobSize = 14;
    int scrollbarHeight = listViewMaxY - scrollbarY;
    unsigned char* scrollbarBuffer = windowBuffer + windowWidth * scrollbarY + scrollbarX;

    buf_fill(scrollbarBuffer,
        scrollbarKnobSize + 1,
        scrollbarHeight - text_height() - 8,
        windowWidth,
        colorTable[GNW_wcolor[0]]);

    win_register_button(win,
        scrollbarX,
        scrollbarY,
        scrollbarKnobSize + 1,
        scrollbarHeight - text_height() - 8,
        -1,
        -1,
        2048,
        -1,
        NULL,
        NULL,
        NULL,
        0);

    draw_shaded_box(windowBuffer,
        windowWidth,
        windowWidth - 22,
        scrollbarY - 1,
        scrollbarX + scrollbarKnobSize + 1,
        listViewMaxY - text_height() - 9,
        colorTable[GNW_wcolor[2]],
        colorTable[GNW_wcolor[1]]);
    draw_shaded_box(windowBuffer,
        windowWidth,
        scrollbarX,
        scrollbarY,
        scrollbarX + scrollbarKnobSize,
        scrollbarY + scrollbarKnobSize,
        colorTable[GNW_wcolor[1]],
        colorTable[GNW_wcolor[2]]);

    lighten_buf(scrollbarBuffer, scrollbarKnobSize, scrollbarKnobSize, windowWidth);

    for (int index = 0; index < listViewCapacity; index++) {
        win_register_button(win,
            listViewX,
            listViewY + index * text_height(),
            listViewWidth,
            text_height(),
            512 + index,
            -1,
            1024 + index,
            -1,
            NULL,
            NULL,
            NULL,
            0);
    }

    win_register_button(win,
        0,
        0,
        windowWidth,
        text_height() + 8,
        -1,
        -1,
        -1,
        -1,
        NULL,
        NULL,
        NULL,
        BUTTON_DRAG_HANDLE);

    win_draw(win);

    int absoluteSelectedItemIndex = -1;

    // Relative to `scrollOffset`.
    int previousSelectedItemIndex = -1;
    while (1) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();
        int mouseX, mouseY;
        mouse_get_position(&mouseX, &mouseY);

        if (keyCode == KEY_RETURN || (keyCode >= 1024 && keyCode < listViewCapacity + 1024)) {
            if (selectedItemIndex != -1) {
                absoluteSelectedItemIndex = scrollOffset + selectedItemIndex;
                if (absoluteSelectedItemIndex < itemsLength) {
                    if (callback == NULL) {
                        break;
                    }

                    callback(items, absoluteSelectedItemIndex);
                }
                absoluteSelectedItemIndex = -1;
            }
        } else if (keyCode == 2048) {
            if (window->rect.uly + scrollbarY > mouseY) {
                keyCode = KEY_PAGE_UP;
            } else if (window->rect.uly + scrollbarKnobSize + scrollbarY < mouseY) {
                keyCode = KEY_PAGE_DOWN;
            }
        }

        if (keyCode == KEY_ESCAPE) {
            break;
        }

        if (keyCode >= 512 && keyCode < listViewCapacity + 512) {
            int itemIndex = keyCode - 512;
            if (itemIndex != selectedItemIndex && itemIndex < itemsLength) {
                previousSelectedItemIndex = selectedItemIndex;
                selectedItemIndex = itemIndex;
                keyCode = -3;
            } else {
                continue;
            }
        } else {
            switch (keyCode) {
            case KEY_HOME:
                if (scrollOffset > 0) {
                    keyCode = -4;
                    scrollOffset = 0;
                }
                break;
            case KEY_ARROW_UP:
                if (selectedItemIndex > 0) {
                    keyCode = -3;
                    previousSelectedItemIndex = selectedItemIndex;
                    selectedItemIndex -= 1;
                } else {
                    if (scrollOffset > 0) {
                        keyCode = -4;
                        scrollOffset -= 1;
                    }
                }
                break;
            case KEY_PAGE_UP:
                if (scrollOffset > 0) {
                    scrollOffset -= listViewCapacity;
                    if (scrollOffset < 0) {
                        scrollOffset = 0;
                    }
                    keyCode = -4;
                }
                break;
            case KEY_END:
                if (scrollOffset < itemsLength - listViewCapacity) {
                    keyCode = -4;
                    scrollOffset = itemsLength - listViewCapacity;
                }
                break;
            case KEY_ARROW_DOWN:
                if (selectedItemIndex < listViewCapacity - 1 && selectedItemIndex < itemsLength - 1) {
                    keyCode = -3;
                    previousSelectedItemIndex = selectedItemIndex;
                    selectedItemIndex += 1;
                } else {
                    if (scrollOffset + listViewCapacity < itemsLength) {
                        keyCode = -4;
                        scrollOffset += 1;
                    }
                }
                break;
            case KEY_PAGE_DOWN:
                if (scrollOffset < itemsLength - listViewCapacity) {
                    scrollOffset += listViewCapacity;
                    if (scrollOffset > itemsLength - listViewCapacity) {
                        scrollOffset = itemsLength - listViewCapacity;
                    }
                    keyCode = -4;
                }
                break;
            default:
                if (itemsLength > listViewCapacity) {
                    if ((keyCode >= 'a' && keyCode <= 'z')
                        || (keyCode >= 'A' && keyCode <= 'Z')) {
                        int found = find_first_letter(keyCode, items, itemsLength);
                        if (found != -1) {
                            scrollOffset = found;
                            if (scrollOffset > itemsLength - listViewCapacity) {
                                scrollOffset = itemsLength - listViewCapacity;
                            }
                            keyCode = -4;
                            selectedItemIndex = found - scrollOffset;
                        }
                    }
                }
                break;
            }
        }

        if (keyCode == -4) {
            buf_fill(listViewBuffer,
                listViewWidth,
                listViewMaxY - listViewY,
                windowWidth,
                colorTable[GNW_wcolor[0]]);

            win_text(win,
                items + scrollOffset,
                itemsLength < listViewCapacity ? itemsLength : listViewCapacity,
                listViewWidth,
                listViewX,
                listViewY,
                color | 0x2000000);

            lighten_buf(listViewBuffer + windowWidth * selectedItemIndex * text_height(),
                listViewWidth,
                text_height(),
                windowWidth);

            if (itemsLength > listViewCapacity) {
                buf_fill(windowBuffer + windowWidth * scrollbarY + scrollbarX,
                    scrollbarKnobSize + 1,
                    scrollbarKnobSize + 1,
                    windowWidth,
                    colorTable[GNW_wcolor[0]]);

                scrollbarY = (scrollOffset * (listViewMaxY - listViewY - 2 * text_height() - 16 - scrollbarKnobSize - 1)) / (itemsLength - listViewCapacity)
                    + listViewY + text_height() + 7;

                draw_shaded_box(windowBuffer,
                    windowWidth,
                    scrollbarX,
                    scrollbarY,
                    scrollbarX + scrollbarKnobSize,
                    scrollbarY + scrollbarKnobSize,
                    colorTable[GNW_wcolor[1]],
                    colorTable[GNW_wcolor[2]]);

                lighten_buf(windowBuffer + windowWidth * scrollbarY + scrollbarX,
                    scrollbarKnobSize,
                    scrollbarKnobSize,
                    windowWidth);

                GNW_win_refresh(window, windowRect, NULL);
            }
        } else if (keyCode == -3) {
            Rect itemRect;
            itemRect.ulx = windowRect->ulx + listViewX;
            itemRect.lrx = itemRect.ulx + listViewWidth;

            if (previousSelectedItemIndex != -1) {
                itemRect.uly = windowRect->uly + listViewY + previousSelectedItemIndex * text_height();
                itemRect.lry = itemRect.uly + text_height();

                buf_fill(listViewBuffer + windowWidth * previousSelectedItemIndex * text_height(),
                    listViewWidth,
                    text_height(),
                    windowWidth,
                    colorTable[GNW_wcolor[0]]);

                int textColor;
                if ((color & 0xFF00) != 0) {
                    int colorIndex = (color & 0xFF) - 1;
                    textColor = (color & ~0xFFFF) | colorTable[GNW_wcolor[colorIndex]];
                } else {
                    textColor = color;
                }

                text_to_buf(listViewBuffer + windowWidth * previousSelectedItemIndex * text_height(),
                    items[scrollOffset + previousSelectedItemIndex],
                    windowWidth,
                    windowWidth,
                    textColor);

                GNW_win_refresh(window, &itemRect, NULL);
            }

            if (selectedItemIndex != -1) {
                itemRect.uly = windowRect->uly + listViewY + selectedItemIndex * text_height();
                itemRect.lry = itemRect.uly + text_height();

                lighten_buf(listViewBuffer + windowWidth * selectedItemIndex * text_height(),
                    listViewWidth,
                    text_height(),
                    windowWidth);

                GNW_win_refresh(window, &itemRect, NULL);
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    win_delete(win);

    return absoluteSelectedItemIndex;
}

// 0x4C7858
int win_get_str(char* dest, int length, const char* title, int x, int y)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    int titleWidth = text_width(title) + 12;
    if (titleWidth < text_max() * length) {
        titleWidth = text_max() * length;
    }

    int windowWidth = titleWidth + 16;
    if (windowWidth < 160) {
        windowWidth = 160;
    }

    int windowHeight = 5 * text_height() + 16;

    int win = win_add(x, y, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    win_border(win);

    unsigned char* windowBuffer = win_get_buf(win);

    buf_fill(windowBuffer + windowWidth * (text_height() + 14) + 14,
        windowWidth - 28,
        text_height() + 2,
        windowWidth,
        colorTable[GNW_wcolor[0]]);
    text_to_buf(windowBuffer + windowWidth * 8 + 8, title, windowWidth, windowWidth, colorTable[GNW_wcolor[4]]);

    draw_shaded_box(windowBuffer,
        windowWidth,
        14,
        text_height() + 14,
        windowWidth - 14,
        2 * text_height() + 16,
        colorTable[GNW_wcolor[2]],
        colorTable[GNW_wcolor[1]]);

    win_register_text_button(win,
        windowWidth / 2 - 72,
        windowHeight - 8 - text_height() - 6,
        -1,
        -1,
        -1,
        KEY_RETURN,
        "Done",
        0);

    win_register_text_button(win,
        windowWidth / 2 + 8,
        windowHeight - 8 - text_height() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Cancel",
        0);

    win_draw(win);

    int rc = win_input_str(win,
        dest,
        length,
        16,
        text_height() + 16,
        colorTable[GNW_wcolor[3]],
        colorTable[GNW_wcolor[0]]);

    win_delete(win);

    return rc;
}

// 0x4C7E78
int win_msg(const char* string, int x, int y, int color)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    int windowHeight = 3 * text_height() + 16;

    int windowWidth = text_width(string) + 16;
    if (windowWidth < 80) {
        windowWidth = 80;
    }

    windowWidth += 16;

    int win = win_add(x, y, windowWidth, windowHeight, 256, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    win_border(win);

    Window* window = GNW_find(win);
    unsigned char* windowBuffer = window->buffer;

    int textColor;
    if ((color & 0xFF00) != 0) {
        int colorIndex = (color & 0xFF) - 1;
        textColor = (color & ~0xFFFF) | colorTable[GNW_wcolor[colorIndex]];
    } else {
        textColor = color;
    }

    text_to_buf(windowBuffer + windowWidth * 8 + 16, string, windowWidth, windowWidth, textColor);

    win_register_text_button(win,
        windowWidth / 2 - 32,
        windowHeight - 8 - text_height() - 6,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        "Done",
        0);

    win_draw(win);

    while (get_input() != KEY_ESCAPE) {
        sharedFpsLimiter.mark();
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    win_delete(win);

    return 0;
}

// 0x4C7FA4
int win_pull_down(char** items, int itemsLength, int x, int y, int color)
{
    if (!GNW_win_init_flag) {
        return -1;
    }

    Rect rect;
    int win = create_pull_down(items, itemsLength, x, y, color, colorTable[GNW_wcolor[0]], &rect);
    if (win == -1) {
        return -1;
    }

    return process_pull_down(win, &rect, items, itemsLength, color, colorTable[GNW_wcolor[0]], NULL, -1);
}

// 0x4C8014
static int create_pull_down(char** stringList, int stringListLength, int x, int y, int foregroundColor, int backgroundColor, Rect* rect)
{
    int windowHeight = stringListLength * text_height() + 16;
    int windowWidth = win_width_needed(stringList, stringListLength) + 4;
    if (windowHeight < 2 || windowWidth < 2) {
        return -1;
    }

    int win = win_add(x, y, windowWidth, windowHeight, backgroundColor, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    win_text(win, stringList, stringListLength, windowWidth - 4, 2, 8, foregroundColor);
    win_box(win, 0, 0, windowWidth - 1, windowHeight - 1, colorTable[0]);
    win_box(win, 1, 1, windowWidth - 2, windowHeight - 2, foregroundColor);
    win_draw(win);
    win_get_rect(win, rect);

    return win;
}

// 0x4C80E4
static int process_pull_down(int win, Rect* rect, char** items, int itemsLength, int foregroundColor, int backgroundColor, MenuBar* menuBar, int pulldownIndex)
{
    if (menuBar != NULL) {
        unsigned char* parentWindowBuffer = GNW_find(menuBar->win)->buffer;
        MenuPulldown* pulldown = &(menuBar->pulldowns[pulldownIndex]);

        int x = pulldown->rect.ulx;
        int y = pulldown->rect.uly;
        int width = pulldown->rect.lrx - x + 1;
        int height = pulldown->rect.lry - y + 1;

        int color1 = menuBar->foregroundColor;
        if ((color1 & 0xFF00) != 0) {
            int colorIndex = (color1 & 0xFF) - 1;
            color1 = (color1 & ~0xFFFF) | colorTable[GNW_wcolor[colorIndex]];
        }

        int color2 = menuBar->backgroundColor;
        if ((color2 & 0xFF00) != 0) {
            int colorIndex = (color2 & 0xFF) - 1;
            color2 = (color2 & ~0xFFFF) | colorTable[GNW_wcolor[colorIndex]];
        }

        swap_color_buf(parentWindowBuffer + width * y + x,
            width,
            height,
            win_width(menuBar->win),
            color1,
            color2);
        win_draw_rect(menuBar->win, &(pulldown->rect));
    }

    unsigned char* windowBuffer = GNW_find(win)->buffer;
    int width = rectGetWidth(rect);
    int height = rectGetHeight(rect);

    int focusedIndex = -1;
    int rc;
    int mx1, my1;
    int mx2, my2;
    int input;

    mouse_get_position(&mx1, &my1);

    while (1) {
        sharedFpsLimiter.mark();

        input = get_input();
        if (input != -1) {
            break;
        }

        mouse_get_position(&mx2, &my2);

        if (mx2 < mx1 - 4
            || mx2 > mx1 + 4
            || my2 < my1 - 4
            || my2 > my1 + 4) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    while (1) {
        sharedFpsLimiter.mark();

        mouse_get_position(&mx2, &my2);

        if (input == -2) {
            if (menuBar != NULL) {
                if (mouse_click_in(menuBar->rect.ulx, menuBar->rect.uly, menuBar->rect.lrx, menuBar->rect.lry)) {
                    int index;
                    for (index = 0; index < menuBar->pulldownsLength; index++) {
                        MenuPulldown* pulldown = &(menuBar->pulldowns[index]);
                        if (mouse_click_in(pulldown->rect.ulx, pulldown->rect.uly, pulldown->rect.lrx, pulldown->rect.lry)) {
                            break;
                        }
                    }

                    if (index < menuBar->pulldownsLength && index != pulldownIndex) {
                        rc = -2 - index;
                        break;
                    }
                }
            }
        }

        if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_UP) != 0
            || ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0
                && (mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) == 0)) {
            if (mouse_click_in(rect->ulx, rect->uly + 8, rect->lrx, rect->lry - 9)) {
                rc = focusedIndex;
            } else {
                rc = -1;
            }
            break;
        }

        bool done = false;
        switch (input) {
        case KEY_ESCAPE:
            rc = -1;
            done = true;
            break;
        case KEY_RETURN:
            rc = focusedIndex;
            done = true;
            break;
        case KEY_ARROW_LEFT:
            if (menuBar != NULL && pulldownIndex > 0) {
                rc = -2 - (pulldownIndex - 1);
                done = true;
            }
            break;
        case KEY_ARROW_RIGHT:
            if (menuBar != NULL && pulldownIndex < menuBar->pulldownsLength - 1) {
                rc = -2 - (pulldownIndex + 1);
                done = true;
            }
            break;
        case KEY_ARROW_UP:
            while (focusedIndex > 0) {
                focusedIndex--;

                if (items[focusedIndex][0] != '\0') {
                    break;
                }
            }
            input = -3;
            break;
        case KEY_ARROW_DOWN:
            while (focusedIndex < itemsLength - 1) {
                focusedIndex++;

                if (items[focusedIndex][0] != '\0') {
                    break;
                }
            }
            input = -3;
            break;
        default:
            if (mx2 != mx1 || my2 != my1) {
                if (mouse_click_in(rect->ulx, rect->uly + 8, rect->lrx, rect->lry - 9)) {
                    input = (my2 - rect->uly - 8) / text_height();
                    if (input != -1) {
                        focusedIndex = items[input][0] != '\0' ? input : -1;
                        input = -3;
                    }
                }

                mx1 = mx2;
                my1 = my2;
            }
            break;
        }

        if (done) {
            break;
        }

        if (input == -3) {
            win_fill(win, 2, 8, width - 4, height - 16, backgroundColor);
            win_text(win, items, itemsLength, width - 4, 2, 8, foregroundColor);

            if (focusedIndex != -1) {
                lighten_buf(windowBuffer + (focusedIndex * text_height() + 8) * width + 2,
                    width - 4,
                    text_height(),
                    width);
            }

            win_draw(win);
        }

        input = get_input();

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (menuBar != NULL) {
        unsigned char* parentWindowBuffer = GNW_find(menuBar->win)->buffer;
        MenuPulldown* pulldown = &(menuBar->pulldowns[pulldownIndex]);

        int x = pulldown->rect.ulx;
        int y = pulldown->rect.uly;
        int width = pulldown->rect.lrx - x + 1;
        int height = pulldown->rect.lry - y + 1;

        int color1 = menuBar->foregroundColor;
        if ((color1 & 0xFF00) != 0) {
            int colorIndex = (color1 & 0xFF) - 1;
            color1 = (color1 & ~0xFFFF) | colorTable[GNW_wcolor[colorIndex]];
        }

        int color2 = menuBar->backgroundColor;
        if ((color2 & 0xFF00) != 0) {
            int colorIndex = (color2 & 0xFF) - 1;
            color2 = (color2 & ~0xFFFF) | colorTable[GNW_wcolor[colorIndex]];
        }

        swap_color_buf(parentWindowBuffer + width * y + x,
            width,
            height,
            win_width(menuBar->win),
            color1,
            color2);
        win_draw_rect(menuBar->win, &(pulldown->rect));

        renderPresent();
    }

    win_delete(win);

    return rc;
}

// 0x4C86EC
int win_debug(char* string)
{
    // 0x6B0750
    static int curry;

    // 0x6B0754
    static int currx;

    if (!GNW_win_init_flag) {
        return -1;
    }

    // CE: Debug window metrics were designed for default DOS-style font (0).
    // We don't expect caller to properly set one, so without manually forcing
    // it debug window might contain mixed fonts.
    int oldFont = text_curr();
    text_font(0);

    int lineHeight = text_height();

    if (wd == -1) {
        wd = win_add(80, 80, 300, 192, 256, WINDOW_MOVE_ON_TOP);
        if (wd == -1) {
            return -1;
        }

        win_border(wd);

        Window* window = GNW_find(wd);
        unsigned char* windowBuffer = window->buffer;

        win_fill(wd, 8, 8, 284, lineHeight, 0x100 | 1);

        win_print(wd,
            "Debug",
            0,
            (300 - text_width("Debug")) / 2,
            8,
            0x2000000 | 0x100 | 4);

        draw_shaded_box(windowBuffer,
            300,
            8,
            8,
            291,
            lineHeight + 8,
            colorTable[GNW_wcolor[2]],
            colorTable[GNW_wcolor[1]]);

        win_fill(wd, 9, 26, 282, 135, 0x100 | 1);

        draw_shaded_box(windowBuffer,
            300,
            8,
            25,
            291,
            lineHeight + 145,
            colorTable[GNW_wcolor[2]],
            colorTable[GNW_wcolor[1]]);

        currx = 9;
        curry = 26;

        int btn = win_register_text_button(wd,
            (300 - text_width("Close")) / 2,
            192 - 8 - lineHeight - 6,
            -1,
            -1,
            -1,
            -1,
            "Close",
            0);
        win_register_button_func(btn, NULL, NULL, NULL, win_debug_delete);

        win_register_button(wd,
            8,
            8,
            284,
            lineHeight,
            -1,
            -1,
            -1,
            -1,
            NULL,
            NULL,
            NULL,
            BUTTON_DRAG_HANDLE);
    }

    char temp[2];
    temp[1] = '\0';

    char* pch = string;
    while (*pch != '\0') {
        int characterWidth = text_char_width(*pch & 0xFF);
        if (*pch == '\n' || currx + characterWidth > 291) {
            currx = 9;
            curry += lineHeight;
        }

        while (160 - curry < lineHeight) {
            Window* window = GNW_find(wd);
            unsigned char* windowBuffer = window->buffer;
            buf_to_buf(windowBuffer + lineHeight * 300 + 300 * 26 + 9,
                282,
                134 - lineHeight - 1,
                300,
                windowBuffer + 300 * 26 + 9,
                300);
            curry -= lineHeight;
            win_fill(wd, 9, curry, 282, lineHeight, 0x100 | 1);
        }

        if (*pch != '\n') {
            temp[0] = *pch;
            win_print(wd, temp, 0, currx, curry, 0x2000000 | 0x100 | 4);
            currx += characterWidth + text_spacing();
        }

        pch++;
    }

    win_draw(wd);

    // CE: Restore font.
    text_font(oldFont);

    return 0;
}

// 0x4C8A3C
static void win_debug_delete(int btn, int keyCode)
{
    win_delete(wd);
    wd = -1;
}

// 0x4C8A54
int win_register_menu_bar(int win, int x, int y, int width, int height, int foregroundColor, int backgroundColor)
{
    Window* window = GNW_find(win);

    if (!GNW_win_init_flag) {
        return -1;
    }

    if (window == NULL) {
        return -1;
    }

    if (window->menuBar != NULL) {
        return -1;
    }

    int right = x + width;
    if (right > window->width) {
        return -1;
    }

    int bottom = y + height;
    if (bottom > window->height) {
        return -1;
    }

    MenuBar* menuBar = window->menuBar = (MenuBar*)mem_malloc(sizeof(MenuBar));
    if (menuBar == NULL) {
        return -1;
    }

    menuBar->win = win;
    menuBar->rect.ulx = x;
    menuBar->rect.uly = y;
    menuBar->rect.lrx = right - 1;
    menuBar->rect.lry = bottom - 1;
    menuBar->pulldownsLength = 0;
    menuBar->foregroundColor = foregroundColor;
    menuBar->backgroundColor = backgroundColor;

    win_fill(win, x, y, width, height, backgroundColor);
    win_box(win, x, y, right - 1, bottom - 1, foregroundColor);

    return 0;
}

// 0x4C8B48
int win_register_menu_pulldown(int win, int x, char* title, int keyCode, int itemsLength, char** items, int foregroundColor, int backgroundColor)
{
    Window* window = GNW_find(win);

    if (!GNW_win_init_flag) {
        return -1;
    }

    if (window == NULL) {
        return -1;
    }

    MenuBar* menuBar = window->menuBar;
    if (menuBar == NULL) {
        return -1;
    }

    if (window->menuBar->pulldownsLength == 15) {
        return -1;
    }

    int titleX = menuBar->rect.ulx + x;
    int titleY = (menuBar->rect.uly + menuBar->rect.lry - text_height()) / 2;
    int btn = win_register_button(win,
        titleX,
        titleY,
        text_width(title),
        text_height(),
        -1,
        -1,
        keyCode,
        -1,
        NULL,
        NULL,
        NULL,
        0);
    if (btn == -1) {
        return -1;
    }

    win_print(win, title, 0, titleX, titleY, window->menuBar->foregroundColor | 0x2000000);

    MenuPulldown* pulldown = &(window->menuBar->pulldowns[window->menuBar->pulldownsLength]);
    pulldown->rect.ulx = titleX;
    pulldown->rect.uly = titleY;
    pulldown->rect.lrx = text_width(title) + titleX - 1;
    pulldown->rect.lry = text_height() + titleY - 1;
    pulldown->keyCode = keyCode;
    pulldown->itemsLength = itemsLength;
    pulldown->items = items;
    pulldown->foregroundColor = foregroundColor;
    pulldown->backgroundColor = backgroundColor;

    window->menuBar->pulldownsLength++;

    return 0;
}

// 0x4C8CB0
void win_delete_menu_bar(int win)
{
    Window* window = GNW_find(win);

    if (!GNW_win_init_flag) {
        return;
    }

    if (window == NULL) {
        return;
    }

    if (window->menuBar == NULL) {
        return;
    }

    win_fill(win,
        window->menuBar->rect.ulx,
        window->menuBar->rect.uly,
        rectGetWidth(&(window->menuBar->rect)),
        rectGetHeight(&(window->menuBar->rect)),
        window->color);

    mem_free(window->menuBar);
    window->menuBar = NULL;
}

// 0x4C8D10
int GNW_process_menu(MenuBar* menuBar, int pulldownIndex)
{
    // 0x53A26C
    static MenuBar* curr_menu = NULL;

    if (curr_menu != NULL) {
        return -1;
    }

    curr_menu = menuBar;

    int keyCode;
    Rect rect;
    do {
        MenuPulldown* pulldown = &(menuBar->pulldowns[pulldownIndex]);
        int win = create_pull_down(pulldown->items,
            pulldown->itemsLength,
            pulldown->rect.ulx,
            menuBar->rect.lry + 1,
            pulldown->foregroundColor,
            pulldown->backgroundColor,
            &rect);
        if (win == -1) {
            curr_menu = NULL;
            return -1;
        }

        keyCode = process_pull_down(win, &rect, pulldown->items, pulldown->itemsLength, pulldown->foregroundColor, pulldown->backgroundColor, menuBar, pulldownIndex);
        if (keyCode < -1) {
            pulldownIndex = -2 - keyCode;
        }
    } while (keyCode < -1);

    if (keyCode != -1) {
        flush_input_buffer();
        GNW_add_input_buffer(keyCode);
        keyCode = menuBar->pulldowns[pulldownIndex].keyCode;
    }

    curr_menu = NULL;

    return keyCode;
}

// 0x4C8DD0
static int find_first_letter(int ch, char** stringList, int stringListLength)
{
    if (ch >= 'A' && ch <= 'Z') {
        ch += ' ';
    }

    for (int index = 0; index < stringListLength; index++) {
        char* string = stringList[index];
        if (string[0] == ch || string[0] == ch - ' ') {
            return index;
        }
    }

    return -1;
}

// 0x4C8E10
int win_width_needed(char** fileNameList, int fileNameListLength)
{
    int maxWidth = 0;

    for (int index = 0; index < fileNameListLength; index++) {
        int width = text_width(fileNameList[index]);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }

    return maxWidth;
}

// 0x4C8E3C
int win_input_str(int win, char* dest, int maxLength, int x, int y, int textColor, int backgroundColor)
{
    Window* window = GNW_find(win);
    unsigned char* buffer = window->buffer + window->width * y + x;

    size_t cursorPos = strlen(dest);
    dest[cursorPos] = '_';
    dest[cursorPos + 1] = '\0';

    int lineHeight = text_height();
    int stringWidth = text_width(dest);
    buf_fill(buffer, stringWidth, lineHeight, window->width, backgroundColor);
    text_to_buf(buffer, dest, stringWidth, window->width, textColor);

    Rect dirtyRect;
    dirtyRect.ulx = window->rect.ulx + x;
    dirtyRect.uly = window->rect.uly + y;
    dirtyRect.lrx = dirtyRect.ulx + stringWidth;
    dirtyRect.lry = dirtyRect.uly + lineHeight;
    GNW_win_refresh(window, &dirtyRect, NULL);

    // NOTE: This loop is slightly different compared to other input handling
    // loops. Cursor position is managed inside an incrementing loop. Cursor is
    // decremented in the loop body when key is not handled.
    bool isFirstKey = true;
    for (; cursorPos <= maxLength; cursorPos++) {
        sharedFpsLimiter.mark();

        int keyCode = get_input();
        if (keyCode != -1) {
            if (keyCode == KEY_ESCAPE) {
                dest[cursorPos] = '\0';
                return -1;
            }

            switch (keyCode) {
            case KEY_BACKSPACE:
                if (cursorPos > 0) {
                    stringWidth = text_width(dest);

                    if (isFirstKey) {
                        buf_fill(buffer, stringWidth, lineHeight, window->width, backgroundColor);

                        dirtyRect.ulx = window->rect.ulx + x;
                        dirtyRect.uly = window->rect.uly + y;
                        dirtyRect.lrx = dirtyRect.ulx + stringWidth;
                        dirtyRect.lry = dirtyRect.uly + lineHeight;
                        GNW_win_refresh(window, &dirtyRect, NULL);

                        dest[0] = '_';
                        dest[1] = '\0';
                        cursorPos = 1;
                    } else {
                        dest[cursorPos] = ' ';
                        dest[cursorPos - 1] = '_';
                    }

                    buf_fill(buffer, stringWidth, lineHeight, window->width, backgroundColor);
                    text_to_buf(buffer, dest, stringWidth, window->width, textColor);

                    dirtyRect.ulx = window->rect.ulx + x;
                    dirtyRect.uly = window->rect.uly + y;
                    dirtyRect.lrx = dirtyRect.ulx + stringWidth;
                    dirtyRect.lry = dirtyRect.uly + lineHeight;
                    GNW_win_refresh(window, &dirtyRect, NULL);

                    dest[cursorPos] = '\0';
                    cursorPos -= 2;

                    isFirstKey = false;
                } else {
                    cursorPos--;
                }
                break;
            case KEY_RETURN:
                break;
            default:
                if (cursorPos == maxLength) {
                    cursorPos = maxLength - 1;
                } else {
                    if (keyCode > 0 && keyCode < 256) {
                        dest[cursorPos] = keyCode;
                        dest[cursorPos + 1] = '_';
                        dest[cursorPos + 2] = '\0';

                        int stringWidth = text_width(dest);
                        buf_fill(buffer, stringWidth, lineHeight, window->width, backgroundColor);
                        text_to_buf(buffer, dest, stringWidth, window->width, textColor);

                        dirtyRect.ulx = window->rect.ulx + x;
                        dirtyRect.uly = window->rect.uly + y;
                        dirtyRect.lrx = dirtyRect.ulx + stringWidth;
                        dirtyRect.lry = dirtyRect.uly + lineHeight;
                        GNW_win_refresh(window, &dirtyRect, NULL);

                        isFirstKey = false;
                    } else {
                        cursorPos--;
                    }
                }
                break;
            }
        } else {
            cursorPos--;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    dest[cursorPos] = '\0';

    return 0;
}

// Calculates max length of string needed to represent value1 or value2.
//
// 0x4C941C
size_t calc_max_field_chars_wcursor(int value1, int value2)
{
    char* str = (char*)mem_malloc(17);
    if (str == NULL) {
        return -1;
    }

    snprintf(str, 17, "%d", value1);
    size_t len1 = strlen(str);

    snprintf(str, 17, "%d", value2);
    size_t len2 = strlen(str);

    mem_free(str);

    return std::max(len1, len2) + 1;
}

// 0x4C97CC
void GNW_intr_init()
{
    tm_persistence = 3000;
    tm_add = 0;
    tm_kill = -1;
    scr_center_x = scr_size.lrx / 2;

    if (scr_size.lry >= 479) {
        tm_text_y = 16;
        tm_text_x = 16;
    } else {
        tm_text_y = 10;
        tm_text_x = 10;
    }

    tm_h = 2 * tm_text_y + text_height();

    int spacing = scr_size.lry >> 3;
    int top = scr_size.lry >> 2;

    for (int index = 0; index < 5; index++) {
        tm_location[index].y = spacing * index + top;
        tm_location[index].taken = false;
    }
}

// 0x4C987C
void win_timed_msg_defaults(unsigned int persistence)
{
    tm_persistence = persistence;
}

// 0x4C9884
void GNW_intr_exit()
{
    remove_bk_process(tm_watch_msgs);
    while (tm_kill != -1) {
        tm_kill_msg();
    }
}

// 0x4C9A48
static void tm_watch_msgs()
{
    if (tm_watch_active) {
        return;
    }

    tm_watch_active = true;
    while (tm_kill != -1) {
        if (elapsed_time(tm_queue[tm_kill].created) < tm_persistence) {
            break;
        }

        tm_kill_msg();
    }
    tm_watch_active = false;
}

// 0x4C9A9C
static void tm_kill_msg()
{
    if (tm_kill != -1) {
        win_delete(tm_queue[tm_kill].id);
        tm_location[tm_queue[tm_kill].location].taken = 0;

        tm_kill++;
        if (tm_kill == timed_msgs) {
            tm_kill = 0;
        }

        if (tm_kill == tm_add) {
            tm_add = 0;
            tm_kill = -1;
            remove_bk_process(tm_watch_msgs);
        }
    }
}

// 0x4C9B20
static void tm_kill_out_of_order(int queueIndex)
{
    if (tm_kill == -1) {
        return;
    }

    if (!tm_index_active(queueIndex)) {
        return;
    }

    win_delete(tm_queue[queueIndex].id);

    tm_location[tm_queue[queueIndex].location].taken = false;

    int copy_to = queueIndex;
    while (copy_to != tm_kill) {
        int copy_from = copy_to - 1;
        if (copy_from < 0) {
            copy_from = 4;
        }

        tm_queue[copy_to] = tm_queue[copy_from];
        copy_to = copy_from;
    };

    tm_kill++;
    if (tm_kill == timed_msgs) {
        tm_kill = 0;
    }

    if (tm_add == tm_kill) {
        tm_add = 0;
        tm_kill = -1;
        remove_bk_process(tm_watch_msgs);
    }
}

// 0x4C9C04
static void tm_click_response(int btn)
{
    if (tm_kill == -1) {
        return;
    }

    int win = win_button_winID(btn);
    int queueIndex = tm_kill;
    while (win != tm_queue[queueIndex].id) {
        queueIndex++;
        if (queueIndex == timed_msgs) {
            queueIndex = 0;
        }

        if (queueIndex == tm_kill || !tm_index_active(queueIndex))
            return;
    }

    tm_kill_out_of_order(queueIndex);
}

// 0x4C9C48
static bool tm_index_active(int queueIndex)
{
    if (tm_kill == tm_add) {
        return true;
    }

    if (tm_kill < tm_add) {
        return queueIndex >= tm_kill && queueIndex < tm_add;
    }

    return queueIndex < tm_add || queueIndex >= tm_kill;
}

} // namespace fallout
