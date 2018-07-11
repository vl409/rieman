
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

#ifndef __RIE_XCB_H_
#define __RIE_XCB_H_

#include "rieman.h"

#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <signal.h>


/* order must match atom names enum */
#define RIE_WIN_STATE_STICKY          0x1
#define RIE_WIN_STATE_SKIP_TASKBAR    0x2
#define RIE_WIN_STATE_SKIP_PAGER      0x4
#define RIE_WIN_STATE_ABOVE           0x8
#define RIE_WIN_STATE_BELOW          0x10
#define RIE_WIN_STATE_HIDDEN         0x20
#define RIE_WIN_STATE_SHADED         0x40
#define RIE_WIN_STATE_MODAL          0x80
#define RIE_WIN_STATE_MAXVERT       0x100
#define RIE_WIN_STATE_MAXHOR        0x200
#define RIE_WIN_STATE_FULLSCREEN    0x400
#define RIE_WIN_STATE_ATTENTION     0x800


#define RIE_WINDOW_TYPE_DESKTOP       0x1
#define RIE_WINDOW_TYPE_DOCK          0x2
#define RIE_WINDOW_TYPE_TOOLBAR       0x4
#define RIE_WINDOW_TYPE_MENU          0x8
#define RIE_WINDOW_TYPE_UTILITY      0x10
#define RIE_WINDOW_TYPE_SPLASH       0x20
#define RIE_WINDOW_TYPE_DIALOG       0x40
#define RIE_WINDOW_TYPE_NORMAL       0x80

#define rie_xcb_handle_error(err, fmt, ...)                                   \
    rie_xcb_handle_error_real(__FILE__, __LINE__, err, fmt, ##__VA_ARGS__)

#define rie_xcb_handle_error0(err, msg)  rie_xcb_handle_error(err, "%s", msg)


/* atom names are indexed by this enum */
typedef enum {
    RIE_UTF8_STRING = 0,
    RIE_COMPOUND_TEXT,
    RIE_STRING,

    RIE_WM_NAME,
    RIE_WM_CLASS,
    RIE_WM_DELETE_WINDOW,

    RIE_NET_DESKTOP_LAYOUT,
    RIE_NET_DESKTOP_NAMES,
    RIE_NET_NUMBER_OF_DESKTOPS,
    RIE_NET_CURRENT_DESKTOP,
    RIE_NET_CLIENT_LIST,
    RIE_NET_CLIENT_LIST_STACKING,
    RIE_NET_DESKTOP_GEOMETRY,
    RIE_NET_WORKAREA,
    RIE_NET_DESKTOP_VIEWPORT,
    RIE_NET_VIRTUAL_ROOTS,
    RIE_NET_ACTIVE_WINDOW,
    RIE_NET_WM_NAME,
    RIE_NET_WM_DESKTOP,

    RIE_NET_WM_WINDOW_TYPE,             /* 1st WINDOW_TYPE - contigious list */

    RIE_NET_WM_WINDOW_TYPE_DESKTOP,
    RIE_NET_WM_WINDOW_TYPE_DOCK,
    RIE_NET_WM_WINDOW_TYPE_TOOLBAR,
    RIE_NET_WM_WINDOW_TYPE_MENU,
    RIE_NET_WM_WINDOW_TYPE_UTILITY,
    RIE_NET_WM_WINDOW_TYPE_SPLASH,
    RIE_NET_WM_WINDOW_TYPE_DIALOG,
    RIE_NET_WM_WINDOW_TYPE_NORMAL,      /* last WINDOW_TYPE */

    RIE_NET_WM_STATE,

    RIE_NET_WM_STATE_STICKY,            /* 1st WM_STATE - contigious list */
    RIE_NET_WM_STATE_SKIP_TASKBAR,
    RIE_NET_WM_STATE_SKIP_PAGER,
    RIE_NET_WM_STATE_ABOVE,
    RIE_NET_WM_STATE_BELOW,
    RIE_NET_WM_STATE_HIDDEN,
    RIE_NET_WM_STATE_SHADED,
    RIE_NET_WM_STATE_MODAL,
    RIE_NET_WM_STATE_MAXIMIZED_VERT,
    RIE_NET_WM_STATE_MAXIMIZED_HORZ,
    RIE_NET_WM_STATE_FULLSCREEN,
    RIE_NET_WM_STATE_DEMANDS_ATTENTION, /* last WM_STATE */

    RIE_NET_WM_ICON,
    RIE_XROOTPMAP_ID,
    RIE_MOTIF_WM_HINTS,
    /* when adding, don't forget to update rie_atom_names[] */
    RIE_ATOM_LAST
} rie_atom_name_t;


xcb_connection_t *rie_xcb_get_connection(rie_xcb_t *xcb);
xcb_window_t rie_xcb_get_root(rie_xcb_t *xcb);
xcb_window_t rie_xcb_get_window(rie_xcb_t *xcb);
xcb_visualtype_t *rie_xcb_root_visual(rie_xcb_t *xcb);
xcb_ewmh_connection_t *rie_xcb_ewmh(rie_xcb_t *xcb);
int rie_xcb_screen(rie_xcb_t *xcb);
rie_rect_t rie_xcb_root_geom(rie_xcb_t *xcb);
int rie_xcb_update_root_geom(rie_xcb_t *xcb);

xcb_screen_t *rie_xcb_get_screen(xcb_connection_t *c, int screen);
xcb_visualtype_t *rie_xcb_find_visual(xcb_connection_t *c,
    xcb_visualid_t visual);

int rie_xcb_create_window(rie_xcb_t *xcb);

int rie_xcb_get_window_geometry(rie_xcb_t *xcb, xcb_window_t *win,
    xcb_window_t *vroot, rie_rect_t *box, rie_rect_t *viewport);

rie_xcb_t *rie_xcb_new(rie_settings_t *cfg);
void rie_xcb_delete(rie_xcb_t *xcb);

int rie_xcb_property_notify_atom(rie_xcb_t *xcb,
    xcb_property_notify_event_t *ev);

xcb_generic_event_t *rie_xcb_next_event(rie_xcb_t *xcb, int *etype);
int rie_xcb_wait_for_event(rie_xcb_t *xcb, sigset_t *sigmask);

int rie_xcb_property_get(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, xcb_atom_t type, void *value);

int rie_xcb_property_get_array(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, xcb_atom_t type, rie_array_t *array);

int rie_xcb_property_get_utftext(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, char **value);

int rie_xcb_property_get_array_utftext(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, rie_array_t *array);

int rie_xcb_property_set_array(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, xcb_atom_t xtype, rie_array_t *array);

#if defined(RIE_TESTS)
int rie_xcb_property_delete(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property);
#endif

int rie_xcb_client_message(rie_xcb_t *xcb, xcb_window_t target,
    xcb_window_t win, unsigned int atom, uint32_t *value, int cnt);

int rie_xcb_update_event_mask(rie_xcb_t *xcb, xcb_window_t win,
    unsigned long mask);

int rie_xcb_configure_window(rie_xcb_t *xcb, int x, int y, int w, int h);

int rie_xcb_configure_window_ext(rie_xcb_t *xcb, xcb_window_t target,
    int x, int y, int w, int h);

int rie_xcb_moveresize_window(rie_xcb_t *xcb, xcb_window_t target,
    int x, int y, int w, int h);

int rie_xcb_get_window_frame(rie_xcb_t *xcb, xcb_window_t win,
    rie_rect_t *frame);

void rie_xcb_flush(rie_xcb_t *xcb);

int rie_xcb_get_root_pixmap(rie_xcb_t *xcb, rie_gfx_t *gc, rie_image_t *img);

int rie_xcb_get_screen_pixmap(rie_xcb_t *xcb, rie_gfx_t *gc, xcb_drawable_t obj,
    rie_rect_t *box, rie_image_t *img);

int rie_xcb_get_window_state(rie_xcb_t *xcb, rie_window_t *window,
    xcb_window_t xwin);

int rie_xcb_get_window_type(rie_xcb_t *xcb, rie_window_t *window,
    xcb_window_t xwin);

int rie_xcb_set_desktop(rie_xcb_t *xcb, uint32_t new_desk);

int rie_xcb_set_desktop_viewport(rie_xcb_t *xcb, int x, int y);

int rie_xcb_restore_hidden(rie_xcb_t *xcb, xcb_window_t win);

int rie_xcb_set_focus(rie_xcb_t *xcb, xcb_window_t win);

int rie_xcb_set_desktop_layout(rie_xcb_t *xcb, rie_settings_t *cfg);

int rie_xcb_handle_error_real(char *file, int line, void *err, char *fmt, ...);

#endif
