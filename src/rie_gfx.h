
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

#ifndef __RIE_GFX_H__
#define __RIE_GFX_H__

#define rie_gfx_xy_inside_rect(dx, dy, box)                 \
    (((dx) > (box)->x) && ((dx) < (box)->x + (box)->w)     \
     && ((dy) > (box)->y) && ((dy) < (box)->y + (box)->h))

typedef struct rie_surface_s  rie_surface_t;
typedef struct rie_pattern_s  rie_pattern_t;
typedef struct rie_texture_s  rie_texture_t;

typedef struct {
    int32_t         x;
    int32_t         y;
    uint32_t        w;
    uint32_t        h;
} rie_rect_t;

typedef struct rie_clip_s  rie_clip_t;

struct rie_clip_s {
    rie_rect_t     *box;
    rie_clip_t     *parent;
};

typedef struct {
    double          r;
    double          g;
    double          b;
} rie_color_t;

struct rie_image_s {
    rie_rect_t      box;                   /* geometry */
    rie_surface_t  *tx;
};

#include "rie_font.h"

rie_gfx_t *rie_gfx_new(rie_xcb_t *xcb);
void rie_gfx_delete(rie_gfx_t *gc);

void rie_gfx_resize(rie_gfx_t *gc, int w, int h);

void rie_gfx_render_start(rie_gfx_t *gc);
void rie_gfx_render_done(rie_gfx_t *gc);

int rie_gfx_render_patch(rie_gfx_t *gc, rie_texture_t *tspec, rie_rect_t *dst,
    rie_rect_t *src, rie_clip_t *clip);
int rie_gfx_render_texture(rie_gfx_t *gc, rie_texture_t *tspec, rie_rect_t *dst,
    rie_clip_t *clip);
int rie_gfx_draw_text(rie_gfx_t *gc, rie_fc_t *fc, char *text, rie_rect_t *box,
    rie_clip_t *clip);
rie_rect_t rie_gfx_text_bounding_box(rie_gfx_t *gc, rie_fc_t *fc, char *text);

rie_surface_t *rie_gfx_surface_from_png(char *filename, int *w, int *h);
rie_surface_t *rie_gfx_surface_from_clip(rie_surface_t *surface, int x, int y,
    int w, int h);
rie_surface_t *rie_gfx_surface_from_icon(rie_gfx_t *gc, void *data,
    int w, int h);
rie_surface_t *rie_gfx_surface_from_zpixmap(rie_gfx_t *gc, uint32_t *data,
    int w, int h);
void rie_gfx_surface_free(rie_surface_t *surface);

rie_pattern_t *rie_gfx_pattern_from_surface(rie_surface_t *surface);
void rie_gfx_pattern_free(rie_pattern_t *pat);

#endif
