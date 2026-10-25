#include "plib/gnw/rect.h"

#include <stdlib.h>

#include <algorithm>

#include "plib/gnw/memory.h"

namespace fallout {

// 0x539D58
static RectPtr rlist = NULL;

// 0x4B29B0
void GNW_rect_exit()
{
    while (rlist != NULL) {
        RectPtr temp = rlist->next;
        mem_free(rlist);
        rlist = temp;
    }
}

// 0x4B29D4
void rect_clip_list(RectPtr* pCur, Rect* bound)
{
    Rect clipRect;
    rectCopy(&clipRect, bound);

    // NOTE: Original code is slightly different.
    while (*pCur != NULL) {
        RectPtr rectListNode = *pCur;
        if (clipRect.lrx >= rectListNode->rect.ulx
            && clipRect.lry >= rectListNode->rect.uly
            && clipRect.ulx <= rectListNode->rect.lrx
            && clipRect.uly <= rectListNode->rect.lry) {
            Rect nodeRect;
            rectCopy(&nodeRect, &(rectListNode->rect));

            *pCur = rectListNode->next;

            rectListNode->next = rlist;
            rlist = rectListNode;

            if (nodeRect.uly < clipRect.uly) {
                RectPtr newRectListNode = rect_malloc();
                if (newRectListNode == NULL) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &nodeRect);
                newRectListNode->rect.lry = clipRect.uly - 1;
                newRectListNode->next = *pCur;

                *pCur = newRectListNode;
                pCur = &(newRectListNode->next);

                nodeRect.uly = clipRect.uly;
            }

            if (nodeRect.lry > clipRect.lry) {
                RectPtr newRectListNode = rect_malloc();
                if (newRectListNode == NULL) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &nodeRect);
                newRectListNode->rect.uly = clipRect.lry + 1;
                newRectListNode->next = *pCur;

                *pCur = newRectListNode;
                pCur = &(newRectListNode->next);

                nodeRect.lry = clipRect.lry;
            }

            if (nodeRect.ulx < clipRect.ulx) {
                RectPtr newRectListNode = rect_malloc();
                if (newRectListNode == NULL) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &nodeRect);
                newRectListNode->rect.lrx = clipRect.ulx - 1;
                newRectListNode->next = *pCur;

                *pCur = newRectListNode;
                pCur = &(newRectListNode->next);
            }

            if (nodeRect.lrx > clipRect.lrx) {
                RectPtr newRectListNode = rect_malloc();
                if (newRectListNode == NULL) {
                    return;
                }

                rectCopy(&(newRectListNode->rect), &nodeRect);
                newRectListNode->rect.ulx = clipRect.lrx + 1;
                newRectListNode->next = *pCur;

                *pCur = newRectListNode;
                pCur = &(newRectListNode->next);
            }
        } else {
            pCur = &(rectListNode->next);
        }
    }
}

// 0x4B2B5C
RectPtr rect_clip(Rect* b, Rect* t)
{
    RectPtr list = NULL;
    Rect clipped_t;

    if (rect_inside_bound(t, b, &clipped_t) == 0) {
        Rect clipped_b[4];

        clipped_b[0].ulx = b->ulx;
        clipped_b[0].uly = b->uly;
        clipped_b[0].lrx = b->lrx;
        clipped_b[0].lry = clipped_t.uly - 1;

        clipped_b[1].ulx = b->ulx;
        clipped_b[1].uly = clipped_t.uly;
        clipped_b[1].lrx = clipped_t.ulx - 1;
        clipped_b[1].lry = clipped_t.lry;

        clipped_b[2].ulx = clipped_t.lrx + 1;
        clipped_b[2].uly = clipped_t.uly;
        clipped_b[2].lrx = b->lrx;
        clipped_b[2].lry = clipped_t.lry;

        clipped_b[3].ulx = b->ulx;
        clipped_b[3].uly = clipped_t.lry + 1;
        clipped_b[3].lrx = b->lrx;
        clipped_b[3].lry = b->lry;

        RectPtr* next = &list;
        for (int k = 0; k < 4; k++) {
            if (clipped_b[k].lrx >= clipped_b[k].ulx && clipped_b[k].lry >= clipped_b[k].uly) {
                list = rect_malloc();
                *next = list;
                if (list == NULL) {
                    return NULL;
                }

                list->rect = clipped_b[k];
                list->next = NULL;

                next = &list;
            }
        }
    } else {
        list = rect_malloc();
        if (list != NULL) {
            list->rect.ulx = b->ulx;
            list->rect.uly = b->uly;
            list->rect.lrx = b->lrx;
            list->rect.lry = b->lry;
            list->next = NULL;
        }
    }

    return list;
}

// 0x4B2C68
RectPtr rect_malloc()
{
    RectPtr temp;

    if (rlist == NULL) {
        for (int i = 0; i < 10; i++) {
            temp = (RectPtr)mem_malloc(sizeof(*temp));
            if (temp == NULL) {
                break;
            }

            temp->next = rlist;
            rlist = temp;
        }
    }

    if (rlist == NULL) {
        return NULL;
    }

    temp = rlist;
    rlist = rlist->next;

    return temp;
}

// 0x4B2CB4
void rect_free(RectPtr ptr)
{
    ptr->next = rlist;
    rlist = ptr;
}

// Calculates a union of two source rectangles and places it into result
// rectangle.
//
// 0x4B2CC8
void rect_min_bound(const Rect* r1, const Rect* r2, Rect* min_bound)
{
    min_bound->ulx = std::min(r1->ulx, r2->ulx);
    min_bound->uly = std::min(r1->uly, r2->uly);
    min_bound->lrx = std::max(r1->lrx, r2->lrx);
    min_bound->lry = std::max(r1->lry, r2->lry);
}

// Calculates intersection of two source rectangles and places it into third
// rectangle and returns 0. If two source rectangles do not have intersection
// it returns -1 and resulting rectangle is a copy of r1.
//
// 0x4B2D18
int rect_inside_bound(const Rect* r1, const Rect* bound, Rect* r2)
{
    r2->ulx = r1->ulx;
    r2->uly = r1->uly;
    r2->lrx = r1->lrx;
    r2->lry = r1->lry;

    if (r1->ulx <= bound->lrx && bound->ulx <= r1->lrx && bound->lry >= r1->uly && bound->uly <= r1->lry) {
        if (bound->ulx > r1->ulx) {
            r2->ulx = bound->ulx;
        }

        if (bound->lrx < r1->lrx) {
            r2->lrx = bound->lrx;
        }

        if (bound->uly > r1->uly) {
            r2->uly = bound->uly;
        }

        if (bound->lry < r1->lry) {
            r2->lry = bound->lry;
        }

        return 0;
    }

    return -1;
}

} // namespace fallout
