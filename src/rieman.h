
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

#ifndef __RIEMAN_H__
#define __RIEMAN_H__

#include "config.h"

/* SUS v3, POSIX 1003.1 2004 (POSIX 2001 + Corrigenda) */
#define _XOPEN_SOURCE 600

#include <stdint.h>
#include <string.h>
#include <errno.h>


#define RIEMAN_VERSION "1.0.0"
#define RIEMAN_TITLE   "Rieman"

typedef struct rie_settings_s  rie_settings_t;
typedef struct rie_window_s    rie_window_t;
typedef struct rie_image_s     rie_image_t;
typedef struct rie_skin_s      rie_skin_t;
typedef struct rie_xcb_s       rie_xcb_t;
typedef struct rie_gfx_s       rie_gfx_t;
typedef struct rie_s           rie_t;

#include "rie_util.h"
#include "rie_log.h"
#include "rie_gfx.h"
#include "rie_window.h"

enum rie_rc_e {
    RIE_OK = 0,
    RIE_ERROR = -1,
    RIE_NOTFOUND = -2,
};

enum rie_positions_e {
    RIE_POS_TOPRIGHT,
    RIE_POS_TOPLEFT,
    RIE_POS_BOTTOMLEFT,
    RIE_POS_BOTTOMRIGHT
};

enum rie_pad_positions_e {
    RIE_PAD_POS_ABOVE,
    RIE_PAD_POS_BELOW
};

enum rie_desktop_labels_e {
    RIE_DESKTOP_NUMBER,
    RIE_DESKTOP_NAME
};

typedef enum {
    RIE_TILE_MODE_FAIR_EAST,
    RIE_TILE_MODE_FAIR_WEST,
    RIE_TILE_MODE_LAST
} rie_tile_e;

typedef struct {
    uint32_t  version_min;      /* minimum supported version */
    uint32_t  version_max;      /* maximum supported version */
} rie_conf_meta_t;

struct rie_settings_s {
    rie_conf_meta_t  meta;                  /* must be first */

    char            *conf_file;
    char            *skin;                  /* skin name */

    uint8_t          withdrawn;
    uint8_t          show_text;
    uint8_t          show_window_icons;
    uint8_t          show_viewports;
    uint8_t          show_minitray;
    uint8_t          show_pad;
    uint32_t         pad_position;
    uint32_t         pad_margin;
    uint32_t         position;

    struct {
        uint32_t     wrap;

        uint32_t     w;                     /* pixels */
        uint32_t     h;                     /* pixels */

        uint32_t     orientation;           /* _NET_DESKTOP_LAYOUT */
        uint32_t     corner;
        uint32_t     content;
    } desktop;

    uint32_t         change_desktop_button;
    uint32_t         tile_button;
    uint32_t         wmhints;               /* initial window state flags */
};

typedef struct {
    rie_rect_t       cell;                  /* bounding box of main cell */
    rie_rect_t       dbox;                  /* desktop representation */
    rie_rect_t       pad;                   /* pad for desktop name/icons */
    uint32_t         nhidden;               /* number of hidden windows */
    uint32_t         nnormal;               /* number of normal windows */
    uint32_t         lrow;
    uint32_t         lcol;
} rie_desktop_t;


struct rie_s {

    rie_settings_t  *cfg;                   /* configuration */

    rie_log_t       *log;
    rie_xcb_t       *xcb;                   /* X11 connection context */
    rie_gfx_t       *gfx;                   /* graphics context       */
    rie_skin_t      *skin;                  /* loaded skin object     */

    rie_rect_t       desktop_geom;          /* full desktop size      */
    rie_image_t      root_bg;               /* root window background */
    rie_desktop_t    template;

    rie_array_t      windows;               /* of rie_window_t  */
    rie_array_t      desktop_names;         /* of char *        */
    rie_array_t      desktops;              /* of rie_desktop_t */
    rie_array_t      workareas;             /* of rie_rect_t    */
    rie_array_t      viewports;             /* of rie_rect_t    */
    rie_array_t      virtual_roots;         /* of xcb_window_t  */

    uint32_t         current_desktop;       /* active desktop number       */
    uint32_t         selected_desktop;      /* currently selected by mouse */
    rie_window_t    *fwindow;               /* currently focused window    */

    uint32_t         nrows;                 /* current pager geometry */
    uint32_t         ncols;

    uint32_t         vp_rows;               /* viewports inside desktop */
    uint32_t         vp_cols;
    rie_rect_t       vp;
    rie_rect_t       selected_vp;           /* currently selected by mouse */

    int32_t          m_x;                   /* current mouse position/state */
    int32_t          m_y;
    uint8_t          m_in;

    rie_tile_e       current_tile_mode;
};

rie_t *rie_pager_new(char *cfile, rie_log_t *log);
void rie_pager_delete(rie_t *pager, int final);
int rie_pager_init(rie_t *pager, rie_t *oldpager);

#endif
