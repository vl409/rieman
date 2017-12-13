
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

#include <cairo-xcb.h>

#define rie_gfx_set_source_rgba(cr, col, a) \
    cairo_set_source_rgba(cr, (col)->r, (col)->g, (col)->b, a);

#define CS(x) ((cairo_surface_t*) (x))


struct rie_gfx_s {
    cairo_t          *cr;
    cairo_surface_t  *surface;
};


rie_gfx_t *
rie_gfx_new(rie_xcb_t *xcb)
{
    rie_gfx_t  *gc;

    xcb_window_t       win;
    xcb_visualtype_t  *visual;
    xcb_connection_t  *c;

    cairo_status_t  cs;

    gc = malloc(sizeof(rie_gfx_t));
    if (gc == NULL) {
        rie_log_error0(errno, "malloc");
        return NULL;
    }

    visual = rie_xcb_root_visual(xcb);
    if (visual == NULL) {
        return NULL;
    }

    c = rie_xcb_get_connection(xcb);
    win = rie_xcb_get_window(xcb);

    gc->surface = cairo_xcb_surface_create(c,win, visual, 1, 1);

    cs = cairo_surface_status(gc->surface);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs),
                          "cairo_xcb_surface_create()");
        return NULL;
    }

    gc->cr = cairo_create(gc->surface);

    cs = cairo_status(gc->cr);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs), "cairo_create()");
        return NULL;
    }

    /* referenced by cairo context, no need to maintain separately */
    cairo_surface_destroy(gc->surface);

    return gc;
}


void
rie_gfx_delete(rie_gfx_t *gc)
{
    cairo_destroy(gc->cr);
    free(gc);
}


void
rie_gfx_render_start(rie_gfx_t *gc)
{
    cairo_push_group(gc->cr);
}

void
rie_gfx_render_done(rie_gfx_t *gc)
{
    cairo_pop_group_to_source(gc->cr);

    cairo_paint(gc->cr);
    cairo_surface_flush(gc->surface);
}

void
rie_gfx_resize(rie_gfx_t *gc, int w, int h)
{
    cairo_xcb_surface_set_size(gc->surface, w, h);
}

void
rie_gfx_surface_free(rie_surface_t *surface)
{
    cairo_surface_destroy((cairo_surface_t*)surface);
}


int
rie_gfx_render_patch(rie_gfx_t *gc, rie_texture_t *tspec, rie_rect_t *dst,
    rie_rect_t *src, rie_clip_t *clip)
{
    if (tspec->type == RIE_TX_TYPE_NONE) {
        return RIE_OK;
    }

    if (dst->w == 0 || dst->h == 0) {
        /* do not try to render invisible items */
        return RIE_OK;
    }

    /* root background */
    rie_rect_t  dest = *dst;

    if (src) {
        dest.x -= src->x;
        dest.y -= src->y;
    }

    cairo_save(gc->cr);

    while (clip) {
        cairo_rectangle(gc->cr, clip->box->x, clip->box->y,
                                clip->box->w, clip->box->h);
        cairo_clip(gc->cr);
        clip = clip->parent;
    }

    cairo_set_source_surface(gc->cr, CS(tspec->tx), dest.x, dest.y);

    cairo_rectangle(gc->cr, dst->x, dst->y, dest.w, dest.h);
    cairo_clip(gc->cr);

    cairo_paint_with_alpha(gc->cr, tspec->alpha);

    cairo_restore(gc->cr);

    return RIE_OK;
}


int
rie_gfx_render_texture(rie_gfx_t *gc, rie_texture_t *tspec, rie_rect_t *dst,
    rie_clip_t *clip)
{
    double  dx, dy;

    if (tspec->type == RIE_TX_TYPE_NONE) {
        return RIE_OK;
    }

    if (dst->w == 0 || dst->h == 0) {
        /* do not try to render invisible items */
        return RIE_OK;
    }

    cairo_save(gc->cr);

    while (clip) {
        cairo_rectangle(gc->cr, clip->box->x, clip->box->y,
                                clip->box->w, clip->box->h);
        cairo_clip(gc->cr);
        clip = clip->parent;
    }

    if (tspec->type == RIE_TX_TYPE_COLOR) {

        rie_gfx_set_source_rgba(gc->cr, &tspec->color, tspec->alpha);
        cairo_rectangle(gc->cr, dst->x, dst->y, dst->w, dst->h);
        cairo_fill(gc->cr);


    } else if (tspec->type == RIE_TX_TYPE_PATTERN) {
        cairo_translate(gc->cr, dst->x, dst->y);
        cairo_rectangle(gc->cr, 0, 0, dst->w, dst->h);

        cairo_set_source(gc->cr, (cairo_pattern_t *) tspec->pat);

        cairo_clip(gc->cr);
        cairo_paint_with_alpha(gc->cr, tspec->alpha);

    } else {

        /* RIE_TX_TYPE_TEXTURE */

        dx = cairo_image_surface_get_width(CS(tspec->tx));
        dy = cairo_image_surface_get_height(CS(tspec->tx));

        dx = ((double)dst->w)/dx;
        dy = ((double)dst->h)/dy;

        cairo_push_group(gc->cr);

        cairo_translate(gc->cr, dst->x, dst->y);
        cairo_rectangle(gc->cr, 0, 0, dst->w, dst->h);
        cairo_scale(gc->cr, dx, dy);
        cairo_set_source_surface(gc->cr, CS(tspec->tx), 0, 0);

        cairo_fill(gc->cr);

        cairo_pop_group_to_source(gc->cr);

        cairo_paint_with_alpha(gc->cr, tspec->alpha);
    }

    cairo_restore(gc->cr);

    return RIE_OK;
}


int
rie_gfx_draw_text(rie_gfx_t *gc, rie_fc_t *fc, char *text, rie_rect_t *box,
    rie_clip_t *clip)
{
    cairo_text_extents_t  te;

    cairo_save(gc->cr);

    while (clip) {
        cairo_rectangle(gc->cr, clip->box->x, clip->box->y,
                                clip->box->w, clip->box->h);
        cairo_clip(gc->cr);
        clip = clip->parent;
    }

    cairo_set_font_face(gc->cr, (cairo_font_face_t *) fc->font);
    cairo_set_font_size(gc->cr, fc->points);
    cairo_text_extents(gc->cr, text, &te);
    rie_gfx_set_source_rgba(gc->cr, &fc->color, fc->alpha);


    cairo_move_to(gc->cr, box->x - te.x_bearing, box->y - te.y_bearing);
    cairo_show_text(gc->cr, text);
    cairo_restore(gc->cr);

    return RIE_OK;
}


rie_rect_t
rie_gfx_text_bounding_box(rie_gfx_t *gc, rie_fc_t *fc, char *text)
{
    rie_rect_t            res;
    cairo_text_extents_t  bb;

    cairo_save(gc->cr);
    cairo_set_font_face(gc->cr, (cairo_font_face_t *) fc->font);
    cairo_set_font_size(gc->cr, fc->points);
    cairo_text_extents(gc->cr, text, &bb);
    cairo_restore(gc->cr);

    res.w = bb.width;
    res.h = bb.height;
    res.x = bb.x_bearing;
    res.y = bb.y_bearing;

    return res;
}


static void
rie_gfx_pre_multiply_alpha(uint32_t *pixels, size_t size)
{
    int       i;
    double    alpha;
    uint32_t  a, r, g, b;

    for (i = 0; i < size; i++) {

        a = (pixels[i] & 0xFF000000) >> 24;
        r = (pixels[i] & 0x00FF0000) >> 16;
        g = (pixels[i] & 0x0000FF00) >> 8;
        b = (pixels[i] & 0x000000FF);

        alpha = ((double) a) / 255;

        r *= alpha;
        g *= alpha;
        b *= alpha;

        pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}


rie_surface_t *
rie_gfx_surface_from_icon(rie_gfx_t *gc, void *data, int w, int h)
{
    uint32_t         *pixels;
    cairo_status_t    cs;
    cairo_surface_t  *surface;

    /*
     * NET_WM_ICON:
     *
     * This is an array of 32bit packed CARDINAL ARGB with high byte being A,
     * low byte being B. The first two cardinals are width, height. Data is in
     * rows, left to right and top to bottom.
     */
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cs = cairo_surface_status(surface);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs),
                           "cairo_image_surface_create()");
        return NULL;
    }

    pixels = (uint32_t *) cairo_image_surface_get_data(surface);

    memcpy(pixels, data, (w * h) * sizeof(uint32_t));

    /* cairo expects pre-multiplied alpha */
    rie_gfx_pre_multiply_alpha(pixels, (w * h));

    cairo_surface_mark_dirty(surface);

    return (rie_surface_t *) surface;
}


rie_surface_t *
rie_gfx_surface_from_zpixmap(rie_gfx_t *gc, uint32_t *data, int w, int h)
{
    uint32_t         *pixels;
    cairo_status_t    cs;
    cairo_surface_t  *surface;

    int x,y;

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,w, h);

    cs = cairo_surface_status(surface);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs),
                           "cairo_image_surface_create()");
        return NULL;
    }

    pixels = (uint32_t *) cairo_image_surface_get_data(surface);

    /* convert 24-bit RGB to 32-bit ARGB */
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error TODO: extend to 32 with alpha properly on BE
#else
            pixels[y * w + x] = data[y * w + x] | 0xFF000000;
#endif
        }
    }

    cairo_surface_mark_dirty(surface);

    return (rie_surface_t *) surface;
}


rie_surface_t *
rie_gfx_surface_from_png(char *fname, int *w, int *h)
{
    cairo_status_t    cs;
    cairo_surface_t  *surface;

    surface = cairo_image_surface_create_from_png(fname);

    cs = cairo_surface_status(surface);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error(cairo_status_to_string(cs),
                          "cairo_image_surface_create_from_png('%s')", fname);
        return NULL;
    }

    if (w) {
        *w = cairo_image_surface_get_width(surface);
    }

    if (h) {
        *h = cairo_image_surface_get_height(surface);
    }

    return (rie_surface_t *) surface;
}


rie_surface_t *
rie_gfx_surface_from_clip(rie_surface_t *surface, int x, int y, int w, int h)
{
    cairo_status_t    cs;
    cairo_surface_t  *tx;

    tx = cairo_surface_create_for_rectangle(CS(surface), x, y, w, h);

    cs = cairo_surface_status(tx);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs),
                           "cairo_surface_create_for_rectangle()");
        return NULL;
    }

    return (rie_surface_t *) tx;
}


rie_pattern_t *
rie_gfx_pattern_from_surface(rie_surface_t *surface)
{
    cairo_status_t    cs;
    cairo_pattern_t *pat;

    pat = cairo_pattern_create_for_surface(CS(surface));

    cs = cairo_pattern_status(pat);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs),
                           "cairo_pattern_create_for_surface()");
        return NULL;
    }

    cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);

    return (rie_pattern_t *) pat;
}


void
rie_gfx_pattern_free(rie_pattern_t *pat)
{
    cairo_pattern_destroy((cairo_pattern_t *)pat);
}
