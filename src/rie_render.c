
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
#include "rie_skin.h"

#include <math.h>
#include <stdio.h>

#define min(x,y) ((x) < (y)) ? (x) : (y);
#define rie_wscale(dw, ww, sw)  nearbyintf((float) ((int)(dw) * (ww)) / (sw))


typedef struct {
    uint32_t          nrows;
    uint32_t          ncols;
    uint32_t          col;
    uint32_t          row;
    uint32_t          w;
    rie_rect_t       *box;
    rie_clip_t       *lim;
    rie_grid_elem_t   borders[RIE_FRAME_LAST];
} rie_border_create_t;

static inline uint32_t   rie_nfold(uint32_t val, uint32_t div);
static inline rie_rect_t rie_box_center(rie_rect_t canvas, rie_rect_t box);
static inline rie_rect_t rie_box_scale(rie_rect_t box, float sx, float sy);
static inline rie_rect_t rie_box_fit(rie_rect_t canvas, rie_rect_t box);
static inline rie_rect_t rie_scale_to_desktop(rie_t *pager, int desk_num,
    rie_rect_t box);

static void rie_count_hidden_windows(rie_t *pager, int i);
static int rie_draw_desktops(rie_t *pager, int resize_win);
static int rie_draw_windows(rie_t *pager);
static int rie_set_pager_geometry(rie_t *pager, int resize_win, rie_rect_t *win);

static int rie_draw_pager_background(rie_t *pager, rie_rect_t win);
static int rie_draw_desktop(rie_t *pager, int i, int row, int col, int active);
static int rie_draw_desktop_border(rie_t *pager, rie_desktop_t *box, int i,
    int active);
static int rie_draw_active_desktop_borders(rie_t *pager, int i);
static int rie_draw_viewports(rie_t *pager, rie_desktop_t *desk, int i);
static rie_desktop_t *rie_set_desktop_geometry(rie_t *pager, int i,
    int row, int col);
static int rie_draw_window_border(rie_t *pager, rie_texture_t *tspec,
    rie_rect_t *box, rie_rect_t *dbox);
static int rie_draw_window(rie_t *pager, rie_window_t *win);
static int rie_render_icon(rie_t *pager, rie_image_t *image, rie_rect_t wbox,
    rie_clip_t *clip);


static int rie_get_desktop_label_height(rie_t *pager, uint32_t *th);
static int rie_draw_desktop_label(rie_t *pager, rie_rect_t desk, int i);
static int rie_draw_desktop_text(rie_t *pager, rie_rect_t desk, int i);


/* entry point of drawing activity */
int
rie_render(rie_t *pager, int resize_win)
{
    int  rc;

    rie_gfx_render_start(pager->gfx);

    rc = rie_draw_desktops(pager, resize_win);

    rie_gfx_render_done(pager->gfx);

    rie_xcb_flush(pager->xcb);

    return rc;
}


int
rie_desktop_by_coords(rie_t *pager, int x, int y)
{
    int  i;

    rie_desktop_t  *desk;

    desk = pager->desktops.data;

    for (i = 0; i < pager->desktops.nitems; i++) {
        if (rie_gfx_xy_inside_rect(x, y, &desk[i].dbox)) {
            return i;
        }
    }

    return -1;
}


int
rie_viewport_by_coords(rie_t *pager, int x, int y, int *new_x, int *new_y)
{
    int  i, j, k;

    rie_rect_t      vp;
    rie_border_t   *vb;
    rie_desktop_t  *desk;

    desk = pager->desktops.data;

    vp.w = pager->vp.w;
    vp.h = pager->vp.h;

    vb = rie_skin_border(pager->skin, RIE_BORDER_VIEWPORT);

    for (k = 0; k < pager->desktops.nitems; k++) {
        for (i = 0; i < pager->vp_rows; i++) {
            for (j = 0; j < pager->vp_cols; j++) {

                vp.x = vp.w * j + vb->w * (j + 1) + desk[k].dbox.x;
                vp.y = vp.h * i + vb->w * (i + 1) + desk[k].dbox.y;

                if (rie_gfx_xy_inside_rect(x, y, &vp)) {

                    /* real coordinates are returned */
                    *new_x = j * pager->desktop_geom.w / pager->vp_cols;
                    *new_y = i * pager->desktop_geom.h / pager->vp_rows;

                    return k;
                }
            }
        }
    }

    return -1;
}


static inline uint32_t
rie_nfold(uint32_t val, uint32_t div)
{
    uint32_t rest;

    rest = val % div;
    if (rest == 0) {
        return val / div;
    }

    return (val / div)  + 1;
}


static inline rie_rect_t
rie_box_center(rie_rect_t canvas, rie_rect_t box)
{
    rie_rect_t res;

    res = box;

    res.x = canvas.x + (float) canvas.w / 2 - (float) box.w / 2;
    res.y = canvas.y + (float) canvas.h / 2 - (float) box.h / 2;

    return res;
}


static inline rie_rect_t
rie_box_scale(rie_rect_t box, float sx, float sy)
{
    rie_rect_t res;

    res = box;

    res.w /= sx;
    res.h /= sy;
    res.x /= sx;
    res.y /= sy;

    return res;
}


static inline rie_rect_t
rie_box_fit(rie_rect_t canvas, rie_rect_t box)
{
    float sx, sy;

    if (box.w < canvas.w && box.h < canvas.h) {
        /* box fits into canvas, return itself */
        return box;
    }

    sx = (float) canvas.w / box.w;
    sy = (float) canvas.h / box.h;

    /* scale according to worst-fitting dimension */
    sx = sx < sy ? sx : sy;

    box.w *= sx;
    box.h *= sx;

    return box;
}


/* scale box with real coordinates into pager's desktop rectangle */
static rie_rect_t
rie_scale_to_desktop(rie_t *pager, int desk_num, rie_rect_t box)
{
    rie_rect_t      scaled, *area, dbox;
    rie_border_t   *vpborder;
    rie_desktop_t  *deskp, *desk;

    deskp = pager->desktops.data;
    desk = &deskp[desk_num];

    dbox = desk->dbox;

    vpborder = rie_skin_border(pager->skin, RIE_BORDER_VIEWPORT);

    /* viewport internal box  */
    if (pager->vp_rows > 1 || pager->vp_cols > 1) {
        dbox.w -= vpborder->w;
        dbox.h -= vpborder->w;
    }

    area = &pager->desktop_geom;

    scaled.w = rie_wscale(dbox.w, box.w, area->w);
    scaled.h = rie_wscale(dbox.h, box.h, area->h);

    scaled.x = rie_wscale(dbox.w, box.x, area->w);
    scaled.y = rie_wscale(dbox.h, box.y, area->h);

    /* absolute coordinates of scaled box on screen */
    scaled.x += dbox.x;
    scaled.y += dbox.y;

    return scaled;
}


static void
rie_count_hidden_windows(rie_t *pager, int i)
{
    int  j;

    rie_window_t   *win;
    rie_desktop_t  *desk;

    win = pager->windows.data;
    desk = pager->desktops.data;

    desk[i].nhidden = 0;
    desk[i].nnormal = 0;

    for (j = 0; j < pager->windows.nitems; j++) {

        if (win[j].desktop != i) {
            continue;
        }

        if (!(win[j].state & RIE_WIN_STATE_HIDDEN)) {
            desk[i].nnormal++;
            continue;
        }

        win[j].hidden_idx = desk[i].nhidden++;
    }
}


static int
rie_draw_desktops(rie_t *pager, int resize_win)
{
    int  i, row, col, wrap, m_desk;

    rie_rect_t  wbox;

    /* calculate single desktop size and thus window size */
    if (rie_set_pager_geometry(pager, resize_win, &wbox) != RIE_OK) {
        return RIE_ERROR;
    }

    /* desktop under the mouse pointer */
    m_desk = pager->m_in ? pager->selected_desktop : - 1;

    /* render window background */
    if (rie_draw_pager_background(pager, wbox) != RIE_OK) {
        return RIE_ERROR;
    }

    /* row or column where to wrap in order to create 2D grid */
    wrap = (pager->cfg->desktop.orientation == XCB_EWMH_WM_ORIENTATION_HORZ)
            ? pager->ncols
            : pager->nrows;

    /* draw all desktops in a 2D grid */
    for (i = 0, col = 0, row = 0; i < pager->desktops.nitems; i++, col++) {

        if (i % wrap == 0) {
            row++;
            col = 0;
        }

        rie_count_hidden_windows(pager, i);

        if (rie_draw_desktop(pager, i, row - 1, col, m_desk == i) != RIE_OK) {
            return RIE_ERROR;
        }
    }

    if (rie_draw_active_desktop_borders(pager, pager->current_desktop)
        != RIE_OK)
    {
        return RIE_ERROR;
    }

    if (m_desk != -1) {
        if (rie_draw_active_desktop_borders(pager, m_desk) != RIE_OK) {
            return RIE_ERROR;
        }
    }

    return rie_draw_windows(pager);
}


static int
rie_draw_windows(rie_t *pager)
{
    int  i, j, tmp;

    rie_window_t  *win;

    /* on top of desktops grid, display existing windows */
    win = pager->windows.data;

    for (i = 0; i < pager->windows.nitems; i++) {

        /* sticky windows have desktop number like 65k */
        if (win[i].desktop > pager->desktops.nitems) {
            tmp = win[i].desktop;

            /* display sticky window on all desktops */
            for (j = 0; j < pager->desktops.nitems; j++) {
                win[i].desktop = j;
                if (rie_draw_window(pager, &win[i]) != RIE_OK) {
                    win[i].desktop = tmp;
                    return RIE_ERROR;
                }
            }

            win[i].desktop = tmp;

        } else {

            /* display window just once on its desktop */
            if (rie_draw_window(pager, &win[i]) != RIE_OK) {
                return RIE_ERROR;
            }
        }
    }

    return RIE_OK;
}


static int
rie_set_pager_geometry(rie_t *pager, int resize_win, rie_rect_t *win)
{
    int32_t   x, y;
    uint32_t  cols, rows;

    rie_rect_t     *dbox, *pad, res, *workarea, *cell;
    rie_border_t   *border, *vpborder;

    /* shortcuts */
    cell = &pager->template.cell;
    dbox = &pager->template.dbox;
    pad = &pager->template.pad;

    dbox->w = pager->cfg->desktop.w;
    if (dbox->w == 0) {
        rie_log_error0(0, "zero desktop width specified in configuration");
        return RIE_ERROR;
    }

    if (pager->cfg->desktop.h) {
        dbox->h = pager->cfg->desktop.h;

    } else {
        /* automatically determine height using screen aspect ratio */
        dbox->h = (((float) (pager->desktop_geom.h) / pager->desktop_geom.w))
                  * pager->cfg->desktop.w;
    }

    res = rie_xcb_resolution(pager->xcb);

    pager->vp_rows = pager->desktop_geom.h / res.h;
    pager->vp_cols = pager->desktop_geom.w / res.w;

    if (pager->vp_rows > 1 || pager->vp_cols > 1) {
        pager->vp.h = dbox->h / pager->vp_rows;
        pager->vp.w = dbox->w / pager->vp_cols;

        vpborder = rie_skin_border(pager->skin, RIE_BORDER_VIEWPORT);

        dbox->w += vpborder->w * (pager->vp_cols + 1);
        dbox->h += vpborder->w * (pager->vp_rows + 1);
    }

    pad->w = dbox->w;
    if (rie_get_desktop_label_height(pager, &pad->h) != RIE_OK) {
        return RIE_ERROR;
    }

    cell->w = dbox->w;
    cell->h = dbox->h + pad->h;

    if (pager->cfg->desktop.orientation == XCB_EWMH_WM_ORIENTATION_HORZ) {

        if (pager->cfg->desktop.wrap) {
            cols = min(pager->cfg->desktop.wrap, pager->desktops.nitems);
            rows = rie_nfold(pager->desktops.nitems, cols);

        } else {
            cols = pager->desktops.nitems;
            rows = 1;
        }

    } else { /* XCB_EWMH_WM_ORIENTATION_VERT */

        if (pager->cfg->desktop.wrap) {
            rows = min(pager->cfg->desktop.wrap, pager->desktops.nitems);
            cols = rie_nfold(pager->desktops.nitems, rows);

        } else {
            rows = pager->desktops.nitems;
            cols = 1;
        }
    }

    pager->nrows = rows;
    pager->ncols = cols;

    border = rie_skin_border(pager->skin, RIE_BORDER_PAGER);

    /* full size of a pager window */
    win->w = cols * cell->w + (cols + 1) * border->w;
    win->h = rows * cell->h + (rows + 1) * border->w;

    if (resize_win) {

        x = 0;
        y = 0;

        /* workarea is used to avoid various DE's toolbars/docks */
        workarea = rie_array_get(&pager->workareas,
                                 pager->current_desktop, rie_rect_t);

        switch (pager->cfg->position) {
        case RIE_POS_TOPLEFT:
            break;
        case RIE_POS_TOPRIGHT:
            x = workarea->x + workarea->w - win->w;
            break;
        case RIE_POS_BOTTOMLEFT:
            y = workarea->y + workarea->h - win->h;
            break;
        case RIE_POS_BOTTOMRIGHT:
            x = workarea->x + workarea->w - win->w;
            y = workarea->y + workarea->h - win->h;
            break;
        }

        rie_xcb_configure_window(pager->xcb, x, y, win->w, win->h);
        rie_gfx_resize(pager->gfx, win->w, win->h);
    }

    /* get actual coordinates of pager window; */
    if (rie_xcb_get_window_geometry(pager->xcb, NULL, NULL, win, NULL)
        != RIE_OK)
    {
        return RIE_ERROR;
    }

    return RIE_OK;
}


static int
rie_draw_pager_background(rie_t *pager, rie_rect_t win)
{
    rie_rect_t      box;
    rie_texture_t  *tspec, root;

    box = win;
    box.x = 0;
    box.y = 0;

    tspec = rie_skin_texture(pager->skin, RIE_TX_BACKGROUND);

    if (tspec->img_is_root) {

        root = *tspec;

        root.tag = "root_bg";
        root.alpha = 1.0; /* root is never transparent, as nothing below */

        if (pager->root_bg.tx) {

            root.type = RIE_TX_TYPE_TEXTURE;
            root.tx = pager->root_bg.tx;

        } else {

            /* fallback to gray color if we failed to obtain root pixmap */

            root.type = RIE_TX_TYPE_COLOR;
            root.color.r = 0.5;
            root.color.g = 0.5;
            root.color.b = 0.5;
        }

        tspec = &root;
    }

    /* clip is not needed, full window covered */

    switch (tspec->type) {

    case RIE_TX_TYPE_TEXTURE:
        return rie_gfx_render_patch(pager->gfx, tspec, &box, &win, NULL);

    case RIE_TX_TYPE_COLOR:
        return rie_gfx_render_texture(pager->gfx, tspec, &box, NULL);

    default:
        /* should never happen, as RIE_TX_TYPE_PATTERN cannot be set for the
         * pager background in configuration, so only image/color
         */
        return RIE_ERROR;

    }

    /* unreachable */
    return RIE_OK;
}


static int
rie_create_borders(rie_border_create_t *args)
{
    /*  ┌──┬─────┬──┐
     *  │TL│ FT  │TR│
     *  ├──┼─────┼──┤
     *  │L │ box │ R│
     *  ├──┼─────┼──┤
     *  │BL│ FB  │BR│
     *  └──┴─────┴──┘
     */
    int               w, i, row, col, nrows, ncols;
    rie_rect_t       *box;
    rie_grid_elem_t  *borders;

    nrows = args->nrows;
    ncols = args->ncols;
    col = args->col;
    row = args->row;
    w = args->w;
    box = args->box;
    borders = args->borders;

    memset(borders, 0, sizeof(rie_grid_elem_t) * RIE_FRAME_LAST);

    for (i = 0; i < RIE_FRAME_LAST; i++) {

        switch (i) {

        case RIE_FRAME_TOPLEFT:
            if (row != 0) {
                continue;
            }

            borders[i].box.x = box->x - w;
            borders[i].box.y = box->y - w;
            borders[i].box.w = w;
            borders[i].box.h = w;

            if (col == 0 && row == 0) {
                borders[i].type = RIE_GRID_CORNER_TOPLEFT;

            } else {
                borders[i].type = RIE_GRID_MIDDLE_TOP;
            }
            break;

        case RIE_FRAME_TOP:
            if (row != 0) {
                continue;
            }

            borders[i].box.x = box->x;
            borders[i].box.y = box->y - w;
            borders[i].box.w = box->w;
            borders[i].box.h = w;

            borders[i].type = RIE_GRID_HORIZONTAL_TOP;
            break;

        case RIE_FRAME_TOPRIGHT:
            if (col != ncols - 1 || row != 0) {
                continue;
            }

            borders[i].box.x = box->x + box->w;
            borders[i].box.y = box->y - w;
            borders[i].box.w = w;
            borders[i].box.h = w;

            borders[i].type = RIE_GRID_CORNER_TOPRIGHT;
            break;

        case RIE_FRAME_BOTTOM:
            borders[i].box.x = box->x;
            borders[i].box.y = box->y + box->h;
            borders[i].box.w = box->w;
            borders[i].box.h = w;
            borders[i].type = RIE_GRID_HORIZONTAL_BOTTOM;
            break;

        case RIE_FRAME_LEFT:
            if (col != 0) {
                continue;
            }

            borders[i].box.x = box->x - w;
            borders[i].box.y = box->y;
            borders[i].box.w = w;
            borders[i].box.h = box->h;
            borders[i].type = RIE_GRID_VERTICAL_LEFT;
            break;

        case RIE_FRAME_RIGHT:
            borders[i].box.x = box->x + box->w;
            borders[i].box.y = box->y;
            borders[i].box.w = w;
            borders[i].box.h = box->h;
            borders[i].type = RIE_GRID_VERTICAL_RIGHT;
            break;

        case RIE_FRAME_BOTTOMLEFT:
            if (col != 0) {
                continue;
            }

            borders[i].box.x = box->x - w;
            borders[i].box.y = box->y + box->h;
            borders[i].box.w = w;
            borders[i].box.h = w;

            if (row == nrows - 1) {
                borders[i].type = RIE_GRID_CORNER_BOTTOMLEFT;

            } else {
                borders[i].type = RIE_GRID_MIDDLE_LEFT;
            }
            break;

        case RIE_FRAME_BOTTOMRIGHT:
            borders[i].box.x = box->x + box->w;
            borders[i].box.y = box->y + box->h;
            borders[i].box.w = w;
            borders[i].box.h = w;

            if (ncols - 1 == col && nrows - 1 == row) {
                borders[i].type = RIE_GRID_CORNER_BOTTOMRIGHT;

            } else if (nrows - 1 == row) {
                borders[i].type = RIE_GRID_MIDDLE_BOTTOM;

            } else if (ncols - 1 == col) {
                borders[i].type = RIE_GRID_MIDDLE_RIGHT;

            } else {
                borders[i].type = RIE_GRID_CROSS;
            }

            break;
        }
    }

    return RIE_OK;
}


static int
rie_draw_border(rie_t *pager, rie_border_t *border,
                rie_grid_elem_t borders[RIE_FRAME_LAST], rie_clip_t *clip)
{
    int  i, rc;

    rie_texture_t   tspec;

    if (border->type == RIE_TX_TYPE_NONE) {
        return RIE_OK;
    }

    for (i = 0; i < RIE_FRAME_LAST; i++) {
        if (borders[i].box.w == 0) {
            continue;
        }

        if (border->type == RIE_TX_TYPE_TEXTURE) {

            tspec.tx = border->tx;
            tspec.alpha = border->alpha;


            switch (borders[i].type) {

            case RIE_GRID_HORIZONTAL_TOP:
            case RIE_GRID_VERTICAL_LEFT:
            case RIE_GRID_HORIZONTAL_BOTTOM:
            case RIE_GRID_VERTICAL_RIGHT:

                tspec.type = RIE_TX_TYPE_PATTERN;
                tspec.tag = "border_pattern";
                tspec.pat = border->tiles[borders[i].type].pat;

                rc = rie_gfx_render_texture(pager->gfx, &tspec,
                                            &borders[i].box, clip);
                break;

            default:

                tspec.type = RIE_TX_TYPE_TEXTURE;
                tspec.tag = "border_image";
                tspec.tx = border->tiles[borders[i].type].surf;

                rc = rie_gfx_render_patch(pager->gfx, &tspec, &borders[i].box,
                                          NULL, clip);
            }

            if (rc != RIE_OK) {
                return RIE_ERROR;
            }

            continue;
        }

        tspec.type = RIE_TX_TYPE_COLOR;
        tspec.tag = "border_color";
        tspec.color = border->color;
        tspec.alpha = border->alpha;

        if (rie_gfx_render_texture(pager->gfx, &tspec, &borders[i].box, clip)
            != RIE_OK)
        {
            return RIE_ERROR;
        }
    }

    return RIE_OK;
}


static int
rie_draw_desktop_border(rie_t *pager, rie_desktop_t *desk, int i, int active)
{
    rie_border_t         *border;
    rie_border_create_t   bc;

    border = rie_skin_border(pager->skin, RIE_BORDER_PAGER);

    if (border->w == 0) {
        return RIE_OK;
    }

    bc.nrows = pager->nrows;
    bc.ncols = pager->ncols;
    bc.row = desk->lrow;
    bc.col = desk->lcol;
    bc.w = border->w;
    bc.box = &desk->cell;
    bc.lim = NULL;

    if (rie_create_borders(&bc) != RIE_OK) {
        return RIE_ERROR;
    }

    return rie_draw_border(pager, border, bc.borders, NULL);
}


static int
rie_draw_active_desktop_borders(rie_t *pager, int i)
{
    rie_border_t         *aborder, *border;
    rie_desktop_t        *desk;
    rie_border_create_t   bc;

    desk = pager->desktops.data;
    desk = &desk[i];

    border = rie_skin_border(pager->skin, RIE_BORDER_PAGER);
    if (border->w == 0) {
        return RIE_OK;
    }

    aborder = rie_skin_border(pager->skin, RIE_BORDER_DESKTOP_ACTIVE);

    bc.nrows = 1;
    bc.ncols = 1;
    bc.row = 0;
    bc.col = 0;
    bc.box = &desk->cell;

    /* use border of pager grid, as only available space is grid  */
    bc.w = border->w;

    bc.box = &desk->cell;
    bc.lim = NULL;

    if (rie_create_borders(&bc) != RIE_OK) {
        return RIE_ERROR;
    }

    return rie_draw_border(pager, aborder, bc.borders, NULL);
}


static int
rie_draw_viewports(rie_t *pager, rie_desktop_t *desk, int di)
{
    int                   i, j;
    rie_clip_t            vclip;
    rie_rect_t            vbox, *viewport, abox, cbox;
    rie_border_t         *dborder, *dborder_act;
    rie_texture_t        *tspec;
    rie_border_create_t   bc, cbc, abc;

    dborder = rie_skin_border(pager->skin, RIE_BORDER_VIEWPORT);

    if (pager->vp_rows <= 1 && pager->vp_cols <= 1) {
        return RIE_OK;
    }

    vbox = pager->vp;

    bc.nrows = pager->vp_rows;
    bc.ncols = pager->vp_cols;

    /* active (i.e. under mouse) viewport borders */
    abc.w = 0;

    /* current viewport borders */
    cbc = abc;

    viewport = rie_array_get(&pager->viewports, di, rie_rect_t);

    for (i = 0; i < pager->vp_rows; i++) {
        for (j = 0; j < pager->vp_cols; j++) {

            vbox = pager->vp;

            vbox.x = vbox.w * j + dborder->w * (j + 1) + desk->dbox.x;
            vbox.y = vbox.h * i + dborder->w * (i + 1) + desk->dbox.y;

            bc.row = i;
            bc.col = j;
            bc.w = dborder->w;
            bc.box = &vbox;
            bc.lim = NULL;

            if (rie_create_borders(&bc) != RIE_OK) {
                return RIE_ERROR;
            }

            /* current viewport on desktop we draw now */
            if (i == viewport->h && j == viewport->w) {

                /* viewport on current desktop is active */
                if (di == pager->current_desktop) {
                    tspec = rie_skin_texture(pager->skin,
                                             RIE_TX_VIEWPORT_ACTIVE);

                    cbc = bc;
                    cbc.row = 0;
                    cbc.col = 0;
                    cbc.nrows = 1;
                    cbc.ncols = 1;

                    cbox = vbox;
                    cbc.box = &cbox;

                } else {
                    /* current viewport of other desktop is normal */
                    tspec = rie_skin_texture(pager->skin,
                                             RIE_TX_VIEWPORT);
                }

            } else {
                /* non-current viewports are not shown as textures, they
                 * exist solely as a space in grid on top of desktop
                 */
                tspec = NULL;
            }

            /* viewport under mouse - display it as active */
            if (rie_gfx_xy_inside_rect(pager->m_x, pager->m_y, &vbox)) {

                abc = bc;
                abc.row = 0;
                abc.col = 0;
                abc.nrows = 1;
                abc.ncols = 1;

                abox = vbox;
                abc.box = &abox;

                tspec = rie_skin_texture(pager->skin, RIE_TX_VIEWPORT_ACTIVE);
            }

            if (tspec) {
                /* display this viewport with selected texture */

                vclip.box = &desk->dbox;
                vclip.parent = NULL;

                if (rie_gfx_render_texture(pager->gfx, tspec, &vbox , &vclip)
                    != RIE_OK)
                {
                    return RIE_ERROR;
                }
            }

            /* draws viewport grid cell */
            rie_draw_border(pager, dborder, bc.borders, NULL);
        }
    }

    /* active borders are drawn on top of normal grid */

    dborder_act = rie_skin_border(pager->skin, RIE_BORDER_VIEWPORT_ACTIVE);

    if (abc.w) {
        if (rie_create_borders(&abc) != RIE_OK) {
            return RIE_ERROR;
        }

        rie_draw_border(pager, dborder_act, abc.borders, NULL);
    }

    if (cbc.w) {
        if (rie_create_borders(&cbc) != RIE_OK) {
            return RIE_ERROR;
        }

        rie_draw_border(pager, dborder_act, cbc.borders, NULL);
    }

    return RIE_OK;
}


static int
rie_draw_desktop(rie_t *pager, int i, int row, int col, int active)
{
    rie_desktop_t  *desk;
    rie_texture_t  *tspec, root;

    desk = rie_set_desktop_geometry(pager, i, row, col);

    if (i == pager->current_desktop || active) {
        tspec = rie_skin_texture(pager->skin, RIE_TX_CURRENT_DESKTOP);

    } else {
        tspec = rie_skin_texture(pager->skin, RIE_TX_DESKTOP);
    }

    if (tspec->img_is_root) {

        /* we failed to get root image from X11, fallback to color */
        if (pager->root_bg.tx == NULL) {

            root = *tspec;

            root.type = RIE_TX_TYPE_COLOR;

            /* default color is gray */
            root.color.r = 0.5;
            root.color.g = 0.5;
            root.color.b = 0.5;
            root.alpha = 1.0;

            tspec = &root;

        } else {
            tspec->tx = pager->root_bg.tx;
        }
    }

    if (rie_gfx_render_texture(pager->gfx, tspec, &desk->dbox, NULL) != RIE_OK) {
        return RIE_ERROR;
    }

    if (pager->cfg->show_viewports) {
        if (rie_draw_viewports(pager, desk, i) != RIE_OK) {
            return RIE_ERROR;
        }
    }

    if (rie_draw_desktop_border(pager, desk, i, active) != RIE_OK) {
        return RIE_ERROR;
    }

    if (pager->cfg->show_pad) {
        rie_draw_desktop_label(pager, desk->pad, i);
    }

    if (pager->cfg->show_text) {
        rie_draw_desktop_text(pager, desk->dbox, i + 1);
    }

    return RIE_OK;
}


static rie_desktop_t *
rie_set_desktop_geometry(rie_t *pager, int i, int row, int col)
{
    uint32_t        lrow, lcol;

    rie_rect_t     *cell, *dbox, *pad;
    rie_border_t   *border;
    rie_desktop_t  *desk;

    /* shortcuts */
    cell = &pager->template.cell;
    dbox = &pager->template.dbox;
    pad = &pager->template.pad;
    desk = pager->desktops.data;

    /*
     * convert (row, col) - on-screen position inside grid into logical
     * position (lrow, lcol), depending on how desktops are layed out;
     *
     * on-screen grid is always starts topleft and goes to bottomright.
     */
    if (pager->cfg->desktop.orientation == XCB_EWMH_WM_ORIENTATION_HORZ) {

        switch (pager->cfg->desktop.corner) {
        case XCB_EWMH_WM_TOPLEFT:
            lrow = row;
            lcol = col;
            break;

        case XCB_EWMH_WM_TOPRIGHT:
            lrow = row;
            lcol = pager->ncols - col - 1;
            break;

        case XCB_EWMH_WM_BOTTOMRIGHT:
            lrow = rie_nfold(pager->desktops.nitems, pager->ncols) - row - 1;
            lcol = pager->ncols - col - 1;
            break;

        case XCB_EWMH_WM_BOTTOMLEFT:
            lrow = rie_nfold(pager->desktops.nitems, pager->ncols) - row - 1;
            lcol = col;
            break;

        default: /* makes compiler happy, unreachable */
            lcol = 0;
            lrow = 0;
        }

    } else { /* XCB_EWMH_WM_ORIENTATION_VERT */

        switch (pager->cfg->desktop.corner) {
        case XCB_EWMH_WM_TOPLEFT:
            lrow = col;
            lcol = row;
            break;

        case XCB_EWMH_WM_TOPRIGHT:
            lrow = col;
            lcol = rie_nfold(pager->desktops.nitems, pager->nrows) - row - 1;
            break;

        case XCB_EWMH_WM_BOTTOMRIGHT:
            lrow = pager->nrows - col - 1;
            lcol = rie_nfold(pager->desktops.nitems, pager->nrows) - row - 1;
            break;

        case XCB_EWMH_WM_BOTTOMLEFT:
            lrow = pager->nrows - col - 1;
            lcol = row;
            break;

        default: /* makes compiler happy, unreachable */
            lcol = 0;
            lrow = 0;
        }
    }

    desk[i].lrow = lrow;
    desk[i].lcol = lcol;

    border = rie_skin_border(pager->skin, RIE_BORDER_PAGER);

    /* initialize desktop, pad and bounding box rectangles for current desk */
    desk[i].cell.x = lcol * cell->w + border->w * (lcol + 1);
    desk[i].cell.y = lrow * cell->h + border->w * (lrow + 1);
    desk[i].cell.w = cell->w;
    desk[i].cell.h = cell->h;

    desk[i].dbox.x = desk[i].cell.x;
    desk[i].dbox.y = desk[i].cell.y;
    desk[i].dbox.w = dbox->w;
    desk[i].dbox.h = dbox->h;

    desk[i].pad.x = desk[i].dbox.x;
    desk[i].pad.y = desk[i].dbox.y + desk[i].dbox.h;
    desk[i].pad.w = pad->w;
    desk[i].pad.h = pad->h;

    if (pager->cfg->pad_position == RIE_PAD_POS_ABOVE) {
        desk[i].dbox.y = desk[i].cell.y + pad->h;
        desk[i].pad.y = desk[i].cell.y;
    }
    /* else: RIE_PAD_POS_BELOW, default */

    return &desk[i];
}


/* bounding box of an icon: x times smaller than surrounding window */
#define ICON_BB_SCALE  1.5

/* finds the icon best suitable to fit into the box */
rie_image_t *
rie_render_select_icon(rie_array_t *icons, rie_rect_t box)
{
    int     i, best, win_dim, icon_dim;
    size_t  n;

    rie_image_t  *img;

    img = icons->data;
    n = icons->nitems;

    float delta[n];

    /* area for icon inside current window: just part of it, not 100% */
    box = rie_box_scale(box, ICON_BB_SCALE, ICON_BB_SCALE);

    /* for icons of all sizes.... */
    for (i = 0; i < n; i++) {

        win_dim = box.w + box.h;
        icon_dim = img[i].box.w + img[i].box.h;

        /* difference between desired and real proportions of current
         * icon dimensions to available window dimensions
         */
        delta[i] = fabs(1 - (float) win_dim / icon_dim);
    }

    /* find the icon with the minimal difference:
     * it is the most suitable for available space
     */
    for (best = 0, i = 0; i < n; i++) {
        if (delta[i] < delta[best]) {
            best = i;
        }
    }

    return &img[best];
}


static int
rie_draw_window_border(rie_t *pager, rie_texture_t *tspec, rie_rect_t *box,
    rie_rect_t *dbox)
{
    rie_clip_t           wclip;
    rie_border_create_t  bc;

    if (tspec->border == NULL || tspec->border->w == 0) {
        return RIE_OK;
    }

    wclip.box = dbox;
    wclip.parent = NULL;

    bc.nrows = 1;
    bc.ncols = 1;
    bc.col = 0;
    bc.row = 0;
    bc.w = tspec->border->w;
    bc.box = box;
    bc.lim = &wclip;

    if (rie_create_borders(&bc) != RIE_OK) {
        return RIE_ERROR;
    }

    return rie_draw_border(pager, tspec->border, bc.borders, &wclip);
}


static int
rie_draw_window(rie_t *pager, rie_window_t *win)
{
    rie_rect_t      scaled, hidbox;
    rie_clip_t      wclip, iclip;
    rie_image_t    *icon;
    rie_texture_t  *tspec;
    rie_desktop_t  *deskp, *desk;

    if (win->dead) {
        return RIE_OK;
    }

    if (win->state & RIE_WIN_STATE_SKIP_PAGER) {
        return RIE_OK;
    }

    if (win->types & RIE_WINDOW_TYPE_DESKTOP
        || win->types & RIE_WINDOW_TYPE_DOCK)
    {
        return RIE_OK;
    }

    /* fluxbox workaround: fails to handle NET_WM_STATE_SKIP_PAGER properly */
    if (strcmp(win->name, RIEMAN_TITLE) == 0) {
        return RIE_OK;
    }

    deskp = pager->desktops.data;
    desk = &deskp[win->desktop];

    if (win->state & RIE_WIN_STATE_HIDDEN) {

        /* for hidden windows, draw their icons in a pad below desktop */

        if (!pager->cfg->show_pad || !pager->cfg->show_minitray) {
            return RIE_OK;
        }

        hidbox = desk->pad;
        hidbox.w = hidbox.h;    /* i.e. square */

        /* icons has the same size as text and vertically centered in pad */
        hidbox.h -= pager->cfg->pad_margin * 2;
        hidbox.y += pager->cfg->pad_margin;

        /* if list of icons doesn't fit - recalculate width */
        if (hidbox.w * desk->nhidden > desk->pad.w) {
            hidbox.w = ((float) desk->pad.w) / desk->nhidden;
        }

        /* position icons starting from the left */
        hidbox.x += win->hidden_idx * hidbox.w;

        /* we should fit into pad finally */
        wclip.box = &desk->pad;
        wclip.parent = NULL;

        /* save box to window, to find it by coordinates to click on it */
        win->hbox = hidbox;

        if (win->icons && win->icons->nitems) {

            icon = rie_render_select_icon(win->icons, hidbox);

            if (rie_render_icon(pager, icon, hidbox, &wclip) != RIE_OK) {
                return RIE_ERROR;
            }

        } else {

            tspec = rie_skin_texture(pager->skin, RIE_TX_MISSING_ICON);

            if (rie_gfx_render_texture(pager->gfx, tspec, &hidbox, &wclip)
                != RIE_OK)
            {
                return RIE_ERROR;
            }
        }

        return RIE_OK;
    }

    scaled = rie_scale_to_desktop(pager, win->desktop, win->box);

    win->sbox = scaled;

    if (win->focused || win->m_in) {
        tspec = rie_skin_texture(pager->skin, RIE_TX_WINDOW_FOCUSED);

    } else if (win->state & RIE_WIN_STATE_ATTENTION) {
        tspec = rie_skin_texture(pager->skin, RIE_TX_WINDOW_ATTENTION);

    } else {
        tspec = rie_skin_texture(pager->skin, RIE_TX_WINDOW);
    }

    /* window must be inside desktop */
    wclip.box = &desk->dbox;
    wclip.parent = NULL;

    if (rie_gfx_render_texture(pager->gfx, tspec, &scaled, &wclip) != RIE_OK) {
        return RIE_ERROR;
    }

    if (rie_draw_window_border(pager, tspec, &scaled, &desk->dbox) != RIE_OK) {
        return RIE_ERROR;
    }

    if (pager->cfg->show_window_icons) {
        if (win->icons && win->icons->nitems) {

            icon = rie_render_select_icon(win->icons, scaled);

            iclip.box = &scaled;
            iclip.parent = &wclip;

            if (rie_render_icon(pager, icon, scaled, &iclip) != RIE_OK) {
                return RIE_ERROR;
            }
        }
    }

    return RIE_OK;
}


static int
rie_render_icon(rie_t *pager, rie_image_t *image, rie_rect_t wbox,
    rie_clip_t *clip)
{
    rie_rect_t     ibox;
    rie_texture_t  tspec;

    memset(&tspec, 0, sizeof(rie_texture_t));

    /* icon bounding box is smaller than window box at ICON_BB_SCALE times */
    ibox = rie_box_scale(wbox, ICON_BB_SCALE, ICON_BB_SCALE);

    if (image->box.w < ibox.w && image->box.h < ibox.h) {
        /* original icon fits into the window, just center it */
        ibox = rie_box_center(wbox, image->box);

    } else {
        /* icon does not fit into window box, at least with 1 dimension */

        ibox = rie_box_fit(wbox, image->box);
        ibox = rie_box_center(wbox, ibox);
    }

    tspec.alpha = rie_skin_icon_alpha(pager->skin);
    tspec.tx = image->tx;
    tspec.tag = "icon";
    tspec.type = RIE_TX_TYPE_TEXTURE;

    return rie_gfx_render_texture(pager->gfx, &tspec, &ibox, clip);
}


static int
rie_get_desktop_label_height(rie_t *pager, uint32_t *th)
{
    rie_rect_t   box;
    rie_fc_t  *fc;

    if (pager->cfg->show_pad) {

        fc = rie_skin_font(pager->skin, RIE_FONT_DESKTOP_NAME);

        box = rie_gfx_text_bounding_box(pager->gfx, fc, "yM");

        *th = box.h + pager->cfg->pad_margin * 2;

    } else {
        *th = 0;
    }

    return RIE_OK;
}


static int
rie_draw_desktop_label(rie_t *pager, rie_rect_t pad, int i)
{
    char  *dname, **dnames;

    rie_fc_t       *fc;
    rie_clip_t      clip;
    rie_rect_t      label;
    rie_texture_t  *tspec;

    dnames = (char **) pager->desktop_names.data;

    /* not all desktops are named */
    dname = (i < pager->desktop_names.nitems) ? dnames[i] : "-";

    /* windows in pager under mouse display their name instead of desktop's */
    if (pager->fwindow && pager->fwindow->m_in && i == pager->fwindow->desktop)
    {
        dname = pager->fwindow->name;
        fc = rie_skin_font(pager->skin, RIE_FONT_WINDOW_NAME);

    } else {
        fc = rie_skin_font(pager->skin, RIE_FONT_DESKTOP_NAME);
    }

    tspec = rie_skin_texture(pager->skin, RIE_TX_DESKTOP_NAME_BG);

    if (rie_gfx_render_texture(pager->gfx, tspec, &pad, NULL) != RIE_OK) {
        return RIE_ERROR;
    }

    label = rie_gfx_text_bounding_box(pager->gfx, fc, dname);

    label = rie_box_center(pad, label);

    clip.box = &pad;
    clip.parent = NULL;

    return rie_gfx_draw_text(pager->gfx, fc, dname, &label, &clip);
}


static int
rie_draw_desktop_text(rie_t *pager, rie_rect_t desk, int i)
{
    char        *dname, **dnames;
    size_t       len;
    rie_fc_t    *fc;
    rie_rect_t   box;
    rie_clip_t   clip;

    fc = rie_skin_font(pager->skin, RIE_FONT_DESKTOP_NUMBER);

    clip.box = &desk;
    clip.parent = NULL;

    if (pager->cfg->desktop.content == RIE_DESKTOP_NUMBER) {
        len = (i == 0) ? 1 : ceil(log10(abs(i + 1)));

        char dnumber[len + 1];

        sprintf(dnumber, "%d", i);

        box = rie_gfx_text_bounding_box(pager->gfx, fc, dnumber);
        box = rie_box_center(desk, box);
        return rie_gfx_draw_text(pager->gfx, fc, dnumber, &box, &clip);

    } else {

        dnames = (char **) pager->desktop_names.data;

        /* not all desktops are named */
        dname = (i < pager->desktop_names.nitems) ? dnames[i] : "-";

        box = rie_gfx_text_bounding_box(pager->gfx, fc, dname);
        box = rie_box_center(desk, box);
        return rie_gfx_draw_text(pager->gfx, fc, dname, &box, &clip);
    }
}
