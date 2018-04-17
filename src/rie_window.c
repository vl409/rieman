
/*
 * Copyright (C) 2017 Vladimir Homutov
 */

/*
 * This file is part of Rieman.
 *
 * Rieman is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rieman is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 */

#include "rieman.h"
#include "rie_xcb.h"

#include <math.h>

static void rie_window_cleanup_winlist(void *windows, size_t nitems);
static int rie_window_get_icon(rie_xcb_t *xcb, rie_gfx_t *gc,
    rie_window_t *window);
static void rie_window_free_icons(void *data, size_t nitems);
static int rie_window_center_resize(rie_t *pager, rie_window_t *win,
    rie_rect_t bb);
static int rie_windows_tile_awesome_fair(rie_t *pager, int desk,
    rie_tile_fair_orientation_e orientation);

static char *rie_window_missing_name = "-";


int
rie_window_init_list(rie_array_t *windows, size_t len)
{
    return rie_array_init(windows, len, sizeof(rie_window_t),
                          rie_window_cleanup_winlist);
}


static void
rie_window_cleanup_winlist(void *windows, size_t nitems)
{
    int           i;
    rie_window_t *win;

    win = (rie_window_t *) windows;

    for (i = 0; i < nitems; i++) {

        if (win[i].name && win[i].name != rie_window_missing_name) {
            free(win[i].name);
        }

        if (win[i].title && win[i].title != rie_window_missing_name) {
            free(win[i].title);
        }

        if (win[i].icons) {
            rie_array_wipe(win[i].icons);
            free(win[i].icons);
            win[i].icons = NULL;
        }
    }
}


rie_window_t *
rie_window_lookup(rie_t *pager, uint32_t winid)
{
    int            i;
    rie_window_t  *win;

    win = (rie_window_t *) pager->windows.data;

    for (i = 0; i < pager->windows.nitems; i++) {

        if (win[i].winid == winid) {
            return &win[i];
        }
    }

    return NULL;
}


int
rie_window_update_geometry(rie_t *pager)
{
    int            i, rc;
    rie_rect_t    *vp;
    rie_window_t  *win;
    xcb_window_t  *root;

    win = pager->windows.data;

    for (i = 0; i < pager->windows.nitems; i++) {

        vp = rie_array_get(&pager->viewports, win[i].desktop, rie_rect_t);
        root = rie_array_get(&pager->virtual_roots, win[i].desktop,
                             xcb_window_t);

        rc = rie_xcb_get_window_geometry(pager->xcb, &win[i].winid, root,
                                         &win[i].box, vp);
        if (rc != RIE_OK) {
            return rc;
        }
    }

    return RIE_OK;
}


int
rie_window_query(rie_t *pager, rie_window_t *window, uint32_t winid)
{
    int    rc;
    char  *textres;

    rie_xcb_t     *xcb;
    rie_rect_t    *vp, box, bb, frame;
    xcb_window_t  *root;

    xcb = pager->xcb;

    rc = rie_xcb_property_get(xcb, winid, RIE_NET_WM_DESKTOP,
                              XCB_ATOM_CARDINAL, &window->desktop);
    if (rc != RIE_OK) {
        return rc;
    }

    vp = rie_array_get(&pager->viewports, window->desktop, rie_rect_t);
    root = rie_array_get(&pager->virtual_roots, window->desktop, xcb_window_t);

    rc = rie_xcb_get_window_geometry(xcb, &winid, root, &window->box, vp);
    if (rc != RIE_OK) {
        return rc;
    }

    rc = rie_xcb_get_window_frame(pager->xcb, winid, &window->frame);
    if (rc != RIE_OK) {
        return rc;
    }

    window->winid = winid;

    rc = rie_xcb_property_get_utftext(xcb, winid, RIE_WM_NAME, &textres);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;

    } else  if (rc != RIE_OK) {
        textres = rie_window_missing_name; /* property is unset */
    }

    if (window->title && window->title != rie_window_missing_name) {
        free(window->title);
    }

    window->title = textres;

    rc = rie_xcb_property_get_utftext(xcb, winid, RIE_WM_CLASS, &textres);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;

    } else if (rc != RIE_OK) {
        textres = rie_window_missing_name; /* property is unset */
    }

    if (window->name && window->name != rie_window_missing_name) {
        free(window->name);
    }

    window->name = textres;

    rc = rie_window_get_icon(xcb, pager->gfx, window);
    if (rc != RIE_OK) {
        return rc;
    }

    rc = rie_xcb_get_window_type(pager->xcb, window, winid);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;

    } else if (rc == RIE_NOTFOUND) {
        window->types = 0;
    }

    /* we want to receive events about this window changes */
    (void) rie_xcb_update_event_mask(xcb, winid, SubstructureNotifyMask
                                                 | StructureNotifyMask
                                                 | PropertyChangeMask);

    /* result is ignored, as window may not exist */

    if (window->tile_adjust) {

        window->tile_adjust = 0;

        box = window->box;
        bb = window->tile_box;
        frame = window->frame;

        box.x = (abs(bb.w - (box.w + frame.x + frame.w)) / 2) + bb.x;
        box.y = (abs(bb.h - (box.h + frame.y + frame.h)) / 2) + bb.y;


        return rie_xcb_moveresize_window(pager->xcb, winid,
                                         box.x, box.y, box.w, box.h);
    }

    return RIE_OK;
}


void
rie_window_update_pager_focus(rie_t *pager)
{
    int  i, mx, my;

    rie_window_t  *win, *last;

    pager->fwindow = NULL;

    mx = pager->m_x;
    my = pager->m_y;

    win = (rie_window_t *) pager->windows.data;
    last = NULL;

    for (i = 0; i < pager->windows.nitems; i++) {

        if (rie_gfx_xy_inside_rect(mx, my, &win[i].sbox)
            && !(win[i].state & RIE_WIN_STATE_HIDDEN))
        {

            win[i].m_in = 1;
            pager->fwindow = &win[i];

            /* stacking order: only top-level window must be selected */
            if (last) {
                last->m_in = 0;
            }
            last = &win[i];

        } else {
            win[i].m_in = 0;
        }
    }
}


static int
rie_window_get_icon(rie_xcb_t *xcb, rie_gfx_t *gc, rie_window_t *window)
{
    int        rc, i;
    uint32_t  *data, *last;

    rie_array_t   res, *icons;
    rie_image_t  *img;

    memset(&res, 0, sizeof(rie_array_t));

    rc = rie_xcb_property_get_array(xcb, window->winid, RIE_NET_WM_ICON,
                                    XCB_ATOM_CARDINAL, &res);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;

    } else if (rc == RIE_NOTFOUND) {
        /* ignore missing icons */
        return RIE_OK;
    }

    data = res.data;
    last = ((uint32_t *) res.data) + res.nitems;

    /* calculate number of icons in array */
    for (i = 0; data < (last - 2); i++) {
        data += 2 + data[0] * data[1]; /* width, height, pixels[] */
    }

    icons = malloc(sizeof(rie_array_t));
    if (icons == NULL) {
        rie_log_error0(errno, "malloc");
        rie_array_wipe(&res);
        return RIE_ERROR;
    }

    /* new per-window icons array */
    if (rie_array_init(icons, i, sizeof(rie_image_t), rie_window_free_icons)
        != RIE_OK)
    {
        return RIE_ERROR;
    }

    data = res.data;
    img = icons->data;

    for (i = 0; i < icons->nitems; i++) {
        img[i].box.w = data[0];
        img[i].box.h = data[1];

        img[i].tx = rie_gfx_surface_from_icon(gc, &data[2], data[0], data[1]);
        if (img[i].tx == NULL) {
            goto failed;
        }

        data += 2 + data[0] * data[1];
    }

    rie_array_wipe(&res);

    /* replace old array with a new one, deallocating old */
    if (window->icons) {
        rie_array_wipe(window->icons);
        free(window->icons);
    }

    window->icons = icons;

    return RIE_OK;

failed:

    rie_array_wipe(&res);
    rie_array_wipe(icons);
    free(icons);

    return RIE_OK;
}


static void
rie_window_free_icons(void *data, size_t nitems)
{
    rie_image_t *img = data;

    int  i;

    for (i = 0; i < nitems; i++) {
        if (img[i].tx) {
            rie_gfx_surface_free(img[i].tx);
            img[i].tx = NULL;
        }
    }
}


static int
rie_window_center_resize(rie_t *pager, rie_window_t *win, rie_rect_t bb)
{
    int         rc;
    rie_rect_t  frame;

    frame = win->frame;

    bb.x += frame.x;
    bb.y += frame.y;
    bb.w -= (frame.x + frame.w);
    bb.h -= (frame.y + frame.h);

    rc = rie_xcb_moveresize_window(pager->xcb, win->winid,
                                   bb.x, bb.y, bb.w, bb.h);
    if (rc != RIE_OK) {
        return rc;
    }

    /*
     * request for window resize is sent...
     * ConfigureNotify event comes...
     * rie_window_query() is invoked...
     * and uses new coordinates to center self...
     */

    win->tile_adjust = 1;
    win->tile_box = bb;

    return RIE_OK;
}


int
rie_windows_tile(rie_t *pager, int desk)
{
    int  rc;

    switch (pager->current_tile_mode) {
    case RIE_TILE_MODE_FAIR_EAST:
        rc = rie_windows_tile_awesome_fair(pager, desk, RIE_TILE_FAIR_EAST);
        break;

    case RIE_TILE_MODE_FAIR_WEST:
        rc = rie_windows_tile_awesome_fair(pager, desk, RIE_TILE_FAIR_WEST);
        break;

    default:
        pager->current_tile_mode = 0;
        return rie_windows_tile(pager, desk);
    }

    pager->current_tile_mode++;

    return rc;
}


static int
rie_windows_tile_awesome_fair(rie_t *pager, int desk,
    rie_tile_fair_orientation_e orientation)
{
    int  i, k, n, nclients;

    uint32_t        row, rows, lrows, col, cols, lcols;
    rie_rect_t     *workarea, wa, g;
    rie_window_t   *win, *c;
    rie_desktop_t  *deskp;

    workarea = rie_array_get(&pager->workareas, desk, rie_rect_t);
    deskp = rie_array_get(&pager->desktops, desk, rie_desktop_t);

    nclients = deskp->nnormal;

    wa = *workarea;

    if (orientation == RIE_TILE_FAIR_EAST) {
        rie_swap(wa.w, wa.h, uint32_t);
        rie_swap(wa.x, wa.y, int);
    }

    rows = cols = 0;

    if (nclients  == 2) {
        rows = 1;
        cols = 2;

    } else {
        rows = ceil(sqrt(nclients));
        cols = ceil((double)nclients / rows);
    }

    win = pager->windows.data;
    for (i = 0, n = 0; i < pager->windows.nitems; i++) {
        if (win[i].dead
            || win[i].state & RIE_WIN_STATE_SKIP_PAGER
            || win[i].types & RIE_WINDOW_TYPE_DESKTOP
            || win[i].types & RIE_WINDOW_TYPE_DOCK
            || win[i].desktop != desk
            || win[i].state & RIE_WIN_STATE_HIDDEN)
        {
            continue;
        }

        c = &win[i];

        n++;
        k = n - 1;

        row = col = 0;
        g.x = g.y = g.w = g.h = 0;

        row = k % rows;
        col = floor((double) k / rows);

        lrows = lcols = 0;

        if (k >= rows * cols - rows) {
            lrows = nclients - (rows * cols - rows);
            lcols = cols;

        } else {
            lrows = rows;
            lcols = cols;
        }

        if (row == lrows - 1) {
            g.h = wa.h - ceil((double) wa.h / lrows) * row;
            g.y = wa.h - g.h;

        } else {
            g.h = ceil((double) wa.h / lrows);
            g.y = g.h * row;
        }

        if (col == lcols - 1) {
            g.w = wa.w - ceil((double) wa.w / lcols) * col;
            g.x = wa.w - g.w;

        } else {
            g.w = ceil((double) wa.w / lcols);
            g.x = g.w * col;
        }

        g.h -= c->frame.w * 2;
        g.w -= c->frame.h * 2;

        g.y += wa.y;
        g.x += wa.x;

        if (orientation == RIE_TILE_FAIR_EAST) {
            rie_swap(g.w, g.h, uint32_t);
            rie_swap(g.x, g.y, int);
        }

        rie_window_center_resize(pager, &win[i], g);
    }

    return RIE_OK;
}
