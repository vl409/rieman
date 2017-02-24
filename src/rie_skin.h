
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

#ifndef __RIE_SKIN_H__
#define __RIE_SKIN_H__

#include "rie_font.h"


typedef enum {
    RIE_TX_BACKGROUND = 0,
    RIE_TX_DESKTOP,
    RIE_TX_CURRENT_DESKTOP,
    RIE_TX_DESKTOP_NAME_BG,
    RIE_TX_WINDOW,
    RIE_TX_WINDOW_FOCUSED,
    RIE_TX_WINDOW_ATTENTION,
    RIE_TX_MISSING_ICON,
    RIE_TX_VIEWPORT,
    RIE_TX_VIEWPORT_ACTIVE,
    RIE_TX_LAST
} rie_skin_texture_t;

typedef enum {
    RIE_FONT_DESKTOP_NAME = 0,
    RIE_FONT_WINDOW_NAME,
    RIE_FONT_DESKTOP_NUMBER,
    RIE_FONT_LAST,
} rie_skin_font_t;

typedef enum {
    RIE_TX_TYPE_NONE = 0,
    RIE_TX_TYPE_COLOR,
    RIE_TX_TYPE_TEXTURE,
    RIE_TX_TYPE_PATTERN,
    RIE_TX_TYPE_LAST
} rie_skin_type_t;

typedef enum {
    RIE_BORDER_PAGER,
    RIE_BORDER_DESKTOP_ACTIVE,
    RIE_BORDER_WINDOW,
    RIE_BORDER_WINDOW_FOCUSED,
    RIE_BORDER_WINDOW_ATTENTION,
    RIE_BORDER_VIEWPORT,
    RIE_BORDER_VIEWPORT_ACTIVE,
    RIE_BORDER_LAST
} rie_skin_border_t;


typedef enum {                 /* Frame layout: */
    RIE_FRAME_TOP,
    RIE_FRAME_BOTTOM,          /*  +--+---+--+  */
    RIE_FRAME_LEFT,            /*  |TL| T |TR|  */
    RIE_FRAME_RIGHT,           /*  |--+---+--+  */
    RIE_FRAME_TOPLEFT,         /*  | L| * | R|  */
    RIE_FRAME_BOTTOMLEFT,      /*  +--+---+--+  */
    RIE_FRAME_TOPRIGHT,        /*  |BL| B |BR|  */
    RIE_FRAME_BOTTOMRIGHT,     /*  +--+---+--+  */
    RIE_FRAME_LAST
} rie_frame_t;


/* order corresponds to 4x4 grid position in image file */
typedef enum {
    RIE_GRID_CORNER_TOPLEFT,        /* ┌  */
    RIE_GRID_HORIZONTAL_TOP,        /* ─  */
    RIE_GRID_MIDDLE_TOP,            /*  ┬ */
    RIE_GRID_CORNER_TOPRIGHT,       /*  ┐ */

    RIE_GRID_VERTICAL_LEFT,         /* │  */
    RIE_GRID_CROSS,                 /*  ┼ */
    RIE_GRID_RESERVED1,
    RIE_GRID_VERTICAL_RIGHT,        /* │  */

    RIE_GRID_MIDDLE_LEFT,           /*  ├ */
    RIE_GRID_RESERVED2,
    RIE_GRID_RESERVED3,
    RIE_GRID_MIDDLE_RIGHT,          /* ┤  */

    RIE_GRID_CORNER_BOTTOMLEFT,     /*  └ */
    RIE_GRID_HORIZONTAL_BOTTOM,     /* ─  */
    RIE_GRID_MIDDLE_BOTTOM,         /* ┴  */
    RIE_GRID_CORNER_BOTTOMRIGHT,    /* ┘  */

    RIE_GRID_LAST
} rie_grid_t;


typedef struct {
    rie_rect_t       box;
    rie_grid_t       type;
} rie_grid_elem_t;


typedef union {
    rie_surface_t   *surf;
    rie_pattern_t   *pat;
} rie_tile_t;

typedef struct {
    rie_skin_type_t  type;
    uint32_t         w;
    rie_color_t      color;
    rie_surface_t   *tx;
    char            *tile_src;
    double           alpha;
    rie_tile_t       tiles[RIE_GRID_LAST];
} rie_border_t;


struct rie_texture_s {
    rie_skin_type_t  type;
    rie_color_t      color;
    rie_surface_t   *tx;
    rie_pattern_t   *pat;
    char            *image;
    uint8_t          img_is_root;
    rie_border_t    *border;
    double           alpha;
    char            *tag;        /* for debugging purposes */
};


rie_skin_t *rie_skin_new(char *name, rie_gfx_t *gc);
void rie_skin_delete(rie_skin_t *skin);

rie_texture_t *rie_skin_texture(rie_skin_t *skin, rie_skin_texture_t elem);
rie_border_t  *rie_skin_border(rie_skin_t *skin, rie_skin_border_t elem);
rie_fc_t      *rie_skin_font(rie_skin_t *skin, rie_skin_font_t elem);
double         rie_skin_icon_alpha(rie_skin_t *skin);

#endif
