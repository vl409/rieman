
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

#include <stdarg.h>
#include <stdio.h>
#include <xcb/xcb_util.h>
#include <sys/select.h>

/* #define RIE_XCB_DBG */


struct rie_xcb_s {
    Display                *display;
    int                     screen;
    xcb_window_t            root;
    xcb_window_t            window;
    xcb_connection_t       *xc;
    xcb_screen_t           *xs;
    xcb_ewmh_connection_t   ewmh;
    rie_rect_t              resolution;
    xcb_atom_t              atoms[RIE_ATOM_LAST];
};

static int rie_xcb_set_window_borderless(rie_xcb_t *xcb);
static int rie_xcb_set_window_title(rie_xcb_t *xcb);
static int rie_xcb_set_initial_state(rie_xcb_t *xcb, rie_settings_t *cfg);


static const char *rie_atom_names[] = {
    /* basic X11 types */
    "UTF8_STRING",
    "COMPOUND_TEXT",
    "STRING",

    /* X11 WM properties */
    "WM_NAME",
    "WM_CLASS",
    /* client message */
    "WM_DELETE_WINDOW",

    /* NET specification properties */
    "_NET_DESKTOP_LAYOUT",
    "_NET_DESKTOP_NAMES",
    "_NET_NUMBER_OF_DESKTOPS",
    "_NET_CURRENT_DESKTOP",
    "_NET_CLIENT_LIST",
    "_NET_CLIENT_LIST_STACKING",
    "_NET_DESKTOP_GEOMETRY",
    "_NET_WORKAREA",
    "_NET_DESKTOP_VIEWPORT",
    "_NET_VIRTUAL_ROOTS",
    "_NET_ACTIVE_WINDOW",
    "_NET_WM_NAME",
    "_NET_WM_DESKTOP",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DESKTOP",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_TOOLBAR",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_UTILITY",
    "_NET_WM_WINDOW_TYPE_SPLASH",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_STATE",
    "_NET_WM_STATE_STICKY",
    "_NET_WM_STATE_SKIP_TASKBAR",
    "_NET_WM_STATE_SKIP_PAGER",
    "_NET_WM_STATE_ABOVE",
    "_NET_WM_STATE_BELOW",
    "_NET_WM_STATE_HIDDEN",
    "_NET_WM_STATE_SHADED",
    "_NET_WM_STATE_MODAL",
    "_NET_WM_STATE_MAXIMIZED_VERT",
    "_NET_WM_STATE_MAXIMIZED_HORZ",
    "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_STATE_DEMANDS_ATTENTION",
    "_NET_WM_ICON",

    "_XROOTPMAP_ID",
    "_MOTIF_WM_HINTS"
};


rie_xcb_t *
rie_xcb_new(rie_settings_t *cfg)
{
    int  rc;

    rie_xcb_t  *xcb;

    xcb_void_cookie_t          vcookie;
    xcb_generic_error_t       *err;
    xcb_icccm_wm_hints_t       hints;
    xcb_intern_atom_cookie_t  *cookie;

    xcb = malloc(sizeof(rie_xcb_t));
    if (xcb == NULL) {
        rie_log_error0(errno, "malloc()");
        return NULL;
    }

    memset(xcb, 0, sizeof(rie_xcb_t));

    xcb->xc = xcb_connect (NULL, NULL);
    if (xcb->xc == NULL) {
        rie_log_error0(0, "failed to connect to X11 Display");
        return NULL;
    }

    xcb->screen = 0; /* note: multiple screens have never been tested */

    xcb->xs = rie_xcb_get_screen(xcb->xc, xcb->screen);
    if (xcb->xs == NULL) {
        rie_log_error0(0, "failed to get XCB screen");
        return NULL;
    }

    xcb->root = xcb->xs->root;

    cookie = xcb_ewmh_init_atoms(xcb->xc, &xcb->ewmh);

    if (xcb_ewmh_init_atoms_replies(&xcb->ewmh, cookie, &err) == 0) {
        (void) rie_xcb_handle_error0(err, "xcb_ewmh_init_atoms_replies");
        return NULL;
    }

    if (rie_xcb_create_window(xcb) != RIE_OK) {
        return NULL;
    }

    if (rie_xcb_init_atoms(xcb) != RIE_OK) {
        return NULL;
    }

    /* expected to be set by WM, but this is not always true */
    rc = rie_xcb_update_event_mask(xcb, xcb->root, PropertyChangeMask);
    if (rc != RIE_OK) {
        return NULL;
    }

    if (cfg->withdrawn) {
        xcb_icccm_wm_hints_set_withdrawn(&hints);

        vcookie = xcb_icccm_set_wm_hints(xcb->xc, xcb->window, &hints);
        err = xcb_request_check(xcb->xc, vcookie);
        if (err != NULL) {
            (void) rie_xcb_handle_error0(err, "xcb_icccm_set_wm_hints");
            return NULL;
        }
    }

    /* consider root window size identical to screen resolution */
    rc = rie_xcb_get_window_geometry(xcb, &xcb->root, NULL, &xcb->resolution,
                                     NULL);
    if (rc != RIE_OK) {
        return NULL;
    }


    if (rie_xcb_set_window_borderless(xcb) != RIE_OK) {
        return NULL;
    }

    if (rie_xcb_set_window_title(xcb) != RIE_OK) {
        return NULL;
    }

    xcb_map_window(xcb->xc, xcb->window);

    if (rie_xcb_set_initial_state(xcb, cfg) != RIE_OK) {
        return NULL;
    }

    xcb_flush(xcb->xc);

    return xcb;
}

void
rie_xcb_delete(rie_xcb_t *xcb)
{
    xcb_ewmh_connection_wipe(&xcb->ewmh);
    xcb_disconnect(xcb->xc);

    free(xcb);
}


xcb_connection_t *
rie_xcb_get_connection(rie_xcb_t *xcb)
{
    return xcb->xc;
}


xcb_ewmh_connection_t *
rie_xcb_ewmh(rie_xcb_t *xcb)
{
    return &xcb->ewmh;
}

int
rie_xcb_screen(rie_xcb_t *xcb)
{
    return xcb->screen;
}

rie_rect_t
rie_xcb_resolution(rie_xcb_t *xcb)
{
    return xcb->resolution;
}

xcb_window_t
rie_xcb_get_root(rie_xcb_t *xcb)
{
    return xcb->root;
}

xcb_window_t
rie_xcb_get_window(rie_xcb_t *xcb)
{
    return xcb->window;
}


void
rie_xcb_flush(rie_xcb_t *xcb)
{
    xcb_flush(xcb->xc);
}


static int
rie_xcb_set_window_borderless(rie_xcb_t *xcb)
{
    xcb_void_cookie_t     vcookie;
    xcb_generic_error_t  *err;

    struct
    {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } MWMHints = {
        (1L << 1), 0, 0 /* border*/ , 0, 0
    };


    vcookie = xcb_change_property(xcb->xc, XCB_PROP_MODE_REPLACE, xcb->window,
                                  xcb->atoms[RIE_MOTIF_WM_HINTS],
                                  xcb->atoms[RIE_MOTIF_WM_HINTS], 32,
                                  sizeof(MWMHints) / sizeof(long), &MWMHints);

    err = xcb_request_check(xcb->xc, vcookie);
    if (err != NULL) {
        return rie_xcb_handle_error0(err, "xcb_change_property(MOTIF_WM_HINTS)");
    }

    return RIE_OK;
}


static int
rie_xcb_set_window_title(rie_xcb_t *xcb)
{
    xcb_void_cookie_t     vcookie;
    xcb_generic_error_t  *err;

    vcookie = xcb_change_property(xcb->xc, XCB_PROP_MODE_REPLACE, xcb->window,
                                  xcb->atoms[RIE_NET_WM_NAME],
                                  xcb->atoms[RIE_UTF8_STRING], 8,
                                  sizeof(RIEMAN_TITLE) - 1, RIEMAN_TITLE);

    err = xcb_request_check(xcb->xc, vcookie);
    if (err != NULL) {
        return rie_xcb_handle_error0(err, "xcb_change_property(_NET_WM_NAME)");
    }

    vcookie = xcb_change_property(xcb->xc, XCB_PROP_MODE_REPLACE, xcb->window,
                                  xcb->atoms[RIE_WM_NAME],
                                  xcb->atoms[RIE_STRING], 8,
                                  sizeof(RIEMAN_TITLE) - 1, RIEMAN_TITLE);

    err = xcb_request_check(xcb->xc, vcookie);
    if (err != NULL) {
        return rie_xcb_handle_error0(err, "xcb_change_property(WM_NAME)");
    }

    vcookie = xcb_change_property(xcb->xc, XCB_PROP_MODE_REPLACE, xcb->window,
                                  xcb->atoms[RIE_WM_CLASS],
                                  xcb->atoms[RIE_STRING], 8,
                                  sizeof(RIEMAN_TITLE) - 1, RIEMAN_TITLE);

    err = xcb_request_check(xcb->xc, vcookie);
    if (err != NULL) {
        return rie_xcb_handle_error0(err, "xcb_change_property(WM_CLASS)");
    }

    return RIE_OK;
}


xcb_screen_t *
rie_xcb_get_screen(xcb_connection_t *c, int screen)
{
    int  i;
    xcb_screen_iterator_t iter;

    iter = xcb_setup_roots_iterator(xcb_get_setup(c));

    for (i = 0; iter.rem; xcb_screen_next(&iter), i++) {
        if (i == screen) {
            return iter.data;
        }
    }

    return NULL;
}


xcb_visualtype_t *
rie_xcb_root_visual(rie_xcb_t *xcb)
{
    xcb_depth_iterator_t       di;
    xcb_screen_iterator_t      si;
    xcb_visualtype_iterator_t  vi;

    si = xcb_setup_roots_iterator(xcb_get_setup(xcb->xc));

    for (; si.rem; xcb_screen_next(&si)) {
        di = xcb_screen_allowed_depths_iterator(si.data);

        for (; di.rem; xcb_depth_next(&di)) {
            vi = xcb_depth_visuals_iterator(di.data);

            for (; vi.rem; xcb_visualtype_next(&vi)) {
                if (xcb->xs->root_visual == vi.data->visual_id) {
                    return vi.data;
                }
            }
        }
    }

    rie_log_error0(0, "xcb visual not found");

    return NULL;
}


int
rie_xcb_create_window(rie_xcb_t *xcb)
{
    uint32_t  mask;

    xcb_window_t          window;
    xcb_void_cookie_t     cookie;
    xcb_generic_error_t  *error;

    mask = XCB_EVENT_MASK_EXPOSURE
           | XCB_EVENT_MASK_BUTTON_PRESS
           | XCB_EVENT_MASK_BUTTON_RELEASE
           | XCB_EVENT_MASK_POINTER_MOTION
           | XCB_EVENT_MASK_ENTER_WINDOW
           | XCB_EVENT_MASK_LEAVE_WINDOW;

    window = xcb_generate_id(xcb->xc);

    cookie = xcb_create_window(xcb->xc, XCB_COPY_FROM_PARENT, window, xcb->root,
                               0, 0, 150, 150, 1,
                               XCB_WINDOW_CLASS_INPUT_OUTPUT,
                               xcb->xs->root_visual, XCB_CW_EVENT_MASK,
                               &mask);

    error = xcb_request_check(xcb->xc, cookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error, "xcb_create_window");
    }

    xcb->window = window;

    return RIE_OK;
}


int
rie_xcb_configure_window(rie_xcb_t *xcb, int x, int y, int w, int h)
{
    return rie_xcb_configure_window_ext(xcb, xcb->window, x, y, w, h);
}


int
rie_xcb_configure_window_ext(rie_xcb_t *xcb, xcb_window_t target, int x, int y,
    int w, int h)
{
    uint16_t  mask;
    uint32_t  values[4];

    xcb_void_cookie_t     cookie;
    xcb_generic_error_t  *error;

    mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
           | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;

    values[0] = x;
    values[1] = y;
    values[2] = w;
    values[3] = h;

    cookie = xcb_configure_window(xcb->xc, target, mask, values);

    error = xcb_request_check(xcb->xc, cookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error, "xcb_configure_window");
    }

    return RIE_OK;
}


int
rie_xcb_moveresize_window(rie_xcb_t *xcb, xcb_window_t target, int x, int y,
    int w, int h)
{
    xcb_void_cookie_t       cookie;
    xcb_generic_error_t    *error;
    xcb_ewmh_connection_t  *ec;

    ec = rie_xcb_ewmh(xcb);

    cookie = xcb_ewmh_request_moveresize_window(
        ec, xcb->screen, target, XCB_GRAVITY_STATIC,
        XCB_EWMH_CLIENT_SOURCE_TYPE_OTHER,
        XCB_EWMH_MOVERESIZE_WINDOW_X
        | XCB_EWMH_MOVERESIZE_WINDOW_Y
        | XCB_EWMH_MOVERESIZE_WINDOW_WIDTH
        | XCB_EWMH_MOVERESIZE_WINDOW_HEIGHT,
        x, y, w, h);

    error = xcb_request_check(xcb->xc, cookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error,
                                     "xcb_ewmh_request_moveresize_window");
    }

    return RIE_OK;
}


int
rie_xcb_get_window_geometry(rie_xcb_t *xcb, xcb_window_t *winp,
    xcb_window_t *vrootp, rie_rect_t *box, rie_rect_t *viewport)
{
    int                                 x, y;
    xcb_window_t                        win, root;
    xcb_generic_error_t                *error;
    xcb_get_geometry_reply_t           *geom;
    xcb_translate_coordinates_reply_t  *trans;

    win = winp ? *winp : xcb->window;
    root = vrootp ? *vrootp : xcb->root; /* root may be reparented to window */

    geom = xcb_get_geometry_reply(xcb->xc, xcb_get_geometry(xcb->xc, win),
                                                            &error);
    if (geom == NULL) {
        return rie_xcb_handle_error0(error, "xcb_get_geometry");
    }

    if (viewport) {
        x = viewport->x;
        y = viewport->y;

    } else {
        x = 0;
        y = 0;
    }

    box->x = geom->x;
    box->y = geom->y;
    box->w = geom->width;
    box->h = geom->height;

    free (geom);

    /* we need coordinates in the root window coordinate system */
    trans = xcb_translate_coordinates_reply(xcb->xc,
                          xcb_translate_coordinates(xcb->xc, win, root,
                                                    box->x, box->y), &error);
    if (trans == NULL) {
        return rie_xcb_handle_error0(error, "xcb_translate_coordinates");
    }

    /*
     * returned coordinates are absolute to desktop;
     *
     * box->x and y are non-zero in case of our window is reparented
     * (i.e. inside some dock window and not in a first position)
     *
     */
    box->x = trans->dst_x + x - box->x;
    box->y = trans->dst_y + y - box->y;

    free (trans);

    /*
     * TODO: window borders visually are a part of window and thus should be
     *       treated by pager as window area;
     */

    return RIE_OK;
}


int
rie_xcb_get_window_frame(rie_xcb_t *xcb, xcb_window_t win, rie_rect_t *frame)
{
    int  rc;

    xcb_generic_error_t           *error;
    xcb_ewmh_connection_t         *ec;
    xcb_get_property_cookie_t      cookie;
    xcb_ewmh_get_extents_reply_t   reply;

    ec = rie_xcb_ewmh(xcb);

    cookie = xcb_ewmh_get_frame_extents(ec, win);

    rc = xcb_ewmh_get_frame_extents_reply(ec, cookie, &reply, &error);

    if (rc == 0) {

        if (error) {
            return rie_xcb_handle_error0(error, "xcb_ewmh_get_frame_extents");
        }

        frame->x = frame->y = frame->w = frame->h = 0;

        return RIE_OK;
    }

    frame->x = reply.left;
    frame->y = reply.top;
    frame->w = reply.right;
    frame->h = reply.bottom;

    return RIE_OK;
}


int
rie_xcb_init_atoms(rie_xcb_t *xcb)
{
    int                        i, rc;
    xcb_generic_error_t       *error;
    xcb_intern_atom_reply_t   *r;
    xcb_intern_atom_cookie_t  *cs;

    cs = malloc(sizeof(xcb_intern_atom_cookie_t) * RIE_ATOM_LAST);
    if (cs == NULL) {
        rie_log_error0(errno, "malloc");
        return RIE_ERROR;
    }

    for (i = 0; i < RIE_ATOM_LAST; i++) {
        cs[i] = xcb_intern_atom(xcb->xc, 0, strlen(rie_atom_names[i]),
                                rie_atom_names[i]);
    }

    for (i = 0; i < RIE_ATOM_LAST; i++) {

        r = xcb_intern_atom_reply(xcb->xc, cs[i], &error);
        if (r == NULL) {
            rc = rie_xcb_handle_error0(error, "xcb_intern_atom_reply");
            free(cs);
            return rc;
        }

        xcb->atoms[i] = r->atom;
        free(r);
    }

    free(cs);

    return RIE_OK;
}


int
rie_xcb_property_notify_atom(rie_xcb_t *xcb, xcb_property_notify_event_t *ev)
{
    int  i;

    for (i = 0; i < RIE_ATOM_LAST; i++) {
        if (ev->atom == xcb->atoms[i]) {
            break;
        }
    }

    return i;
}


xcb_generic_event_t *
rie_xcb_next_event(rie_xcb_t *xcb, int *etype)
{
    xcb_generic_event_t  *event;
    xcb_generic_error_t  *err;

    event = xcb_poll_for_event(xcb->xc);

    if (event == NULL) {
        return NULL;
    }

    if (event->response_type == 0) {
        err = (xcb_generic_error_t *) event;
        rie_xcb_handle_error0(err, "xcb_poll_for_event()");
        return NULL;
    }

    *etype = event->response_type & ~0x80;

    return event;
}


/* interruptable event waiter */
int
rie_xcb_wait_for_event(rie_xcb_t *xcb, sigset_t *sigmask)
{
    int     fd, rc, err, xerr;
    fd_set  fds;

    fd = xcb_get_file_descriptor(xcb->xc);

    while (1) {

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        rc = pselect(fd + 1, &fds, NULL, NULL, NULL, sigmask);

        err = errno;

        xerr = xcb_connection_has_error(xcb->xc);
        if (xerr) {
            rie_log_error(0, "xcb connection error %d", xerr);
            return RIE_ERROR;
        }

        if (rc > 0) {
            return RIE_OK;
        }

        if (rc == 0) {
            /* should never happen since no timeout */
            continue;
        }


        if (err == EINTR) {
            return RIE_OK;
        }

        rie_log_error0(errno, "select()");

        return RIE_ERROR;
    }

    /* unreachable */
    return RIE_OK;
}


int
rie_xcb_property_get(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, xcb_atom_t type, void *value)
{
    int           rc;
    rie_array_t   res;
    union {
        uint32_t        u32;
        xcb_atom_t      atom;
        xcb_drawable_t  dwb;
    } *u;

    memset(&res, 0, sizeof(rie_array_t));

    rc = rie_xcb_property_get_array(xcb, win, property, type, &res);
    if (rc != RIE_OK) {
        return rc;
    }

    if (res.nitems < 1) {
        rie_log_error0(0, "no properties returned");
        return RIE_ERROR;
    }

    u = value;

    switch (type) {
    case XCB_ATOM_ATOM:
        u->atom = ((xcb_atom_t *) res.data)[0];
        break;
    case XCB_ATOM_PIXMAP:
        u->dwb = ((xcb_drawable_t *) res.data)[0];
        break;
    case XCB_ATOM_CARDINAL:
    default:
        u->u32 = ((uint32_t *) res.data)[0];
    }

    rie_array_wipe(&res);

    return RIE_OK;
}


int
rie_xcb_property_get_array(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, xcb_atom_t type, rie_array_t *res)
{
    int          i;
    size_t       len, is;
    uint32_t    *u32, *a32;
    xcb_atom_t  *atom, *aa;

    xcb_generic_error_t        *error;
    xcb_get_property_reply_t   *reply;
    xcb_get_property_cookie_t   cookie;

    cookie = xcb_get_property(xcb->xc, 0, win, xcb->atoms[property], type,
                              0, 0xFFFFFF);

    reply = xcb_get_property_reply(xcb->xc, cookie, &error);
    if (reply == NULL) {
        rie_xcb_handle_error0(error, "xcb_get_property_reply");
        return RIE_NOTFOUND;
    }

    if (reply->type == XCB_ATOM_NONE) {
        rie_debug("xcb_get_property(%s): property not found",
                  rie_atom_names[property]);
        free(reply);
        return RIE_NOTFOUND;
    }

    if (reply->type != type) {
        rie_log_error(0, "xcb_get_property(%s): unexpected type: %d",
                      rie_atom_names[property], reply->type);
        return RIE_ERROR;
    }

    len = xcb_get_property_value_length(reply);
    if (len == 0) {
        /* property found, but it is empty */
        rie_debug("xcb_get_property_value_length(%s): zero items",
                  rie_atom_names[property]);
        free(reply);
        return RIE_NOTFOUND;
    }

    switch (property) {
    case XCB_ATOM_ATOM:
        is = sizeof(xcb_atom_t);
        break;
    default:
        is = sizeof(uint32_t);
        break;
    }

    res->nitems = len / is;

    if (res->nitems == 0) {
        rie_log_error(0, "xcb_get_property_value_length(%s): bad item sizing",
                      rie_atom_names[property]);
        return RIE_ERROR;
    }

    res->data = malloc(len);
    if (res->data == NULL) {
        rie_log_error0(errno, "malloc");
        free(reply);
        return RIE_ERROR;
    }

    switch (property) {
    case XCB_ATOM_ATOM:

        atom = xcb_get_property_value(reply);
        aa = res->data;
        for (i = 0; i < res->nitems; i++) {
            aa[i] = atom[i];
        }

        break;

    default:

        u32 = xcb_get_property_value(reply);
        a32 = res->data;
        for (i = 0; i < res->nitems; i++) {
            a32[i] = u32[i];
        }
        break;
    }

    free(reply);

    /* caller must free array */

    return RIE_OK;
}


int
rie_xcb_property_set_array(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, xcb_atom_t xtype, rie_array_t *array)
{
    xcb_void_cookie_t     cookie;
    xcb_generic_error_t  *error;

#if defined(RIE_XCB_DBG)
    {
        int       i;
        uint32_t *data;

        data = array->data;
        rie_debug("xcb_change_property %s[%ld]:",
                  rie_atom_names[property], array->nitems);
        for (i = 0; i < array->nitems; i++) {
            rie_debug("\telement #%d = %d", i + 1, data[i]);
        }
    }
#endif

    cookie = xcb_change_property(xcb->xc, XCB_PROP_MODE_REPLACE, win,
                                 xcb->atoms[property], xtype, 32,
                                 array->nitems, array->data);

    error = xcb_request_check(xcb->xc, cookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error, "xcb_change_property");
    }

    return RIE_OK;
}


#if defined(RIE_TESTS)

int
rie_xcb_property_delete(rie_xcb_t *xcb, xcb_window_t win, unsigned int property)
{
    xcb_void_cookie_t     vcookie;
    xcb_generic_error_t  *err;

    vcookie = xcb_delete_property(xcb->xc, win, xcb->atoms[property]);

    err = xcb_request_check(xcb->xc, vcookie);
    if (err != NULL) {
        return rie_xcb_handle_error(err, "xcb_delete_property(%s)",
                                    rie_atom_names[property]);
    }

    return RIE_OK;
}

#endif


int
rie_xcb_property_get_utftext(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, char **value)
{
    int    rc;
    char  *p;

    rie_array_t array;

    memset(&array, 0, sizeof(rie_array_t));

    rc = rie_xcb_property_get_array_utftext(xcb, win, property, &array);

    if (rc != RIE_OK) {
        return rc;
    }

    /* caller must free */
    p = strdup(((char **) array.data)[0]);

    rie_array_wipe(&array);

    if (p == NULL) {
        return RIE_ERROR;
    }

    *value = p;

    return RIE_OK;
}


int
rie_xcb_property_get_array_utftext(rie_xcb_t *xcb, xcb_window_t win,
    unsigned int property, rie_array_t *arr)
{
    int                         len, rc;
    char                       *val;
    xcb_generic_error_t        *error;
    xcb_get_property_reply_t   *reply;
    xcb_get_property_cookie_t   cookie;


    cookie = xcb_get_property(xcb->xc, 0, win, xcb->atoms[property],
                              XCB_GET_PROPERTY_TYPE_ANY, 0, 0xFFFFFF);

    reply = xcb_get_property_reply(xcb->xc, cookie, &error);
    if (reply == NULL) {
        return rie_xcb_handle_error(error, "xcb_get_property(%s)",
                                    rie_atom_names[property]);
    }

    if (reply->type == XCB_ATOM_NONE) {
        /* no such property (yet), retry */
        rie_debug("xcb_get_property(%s): property not found",
                  rie_atom_names[property]);
        free(reply);
        return RIE_NOTFOUND;
    }

    if (!(reply->type == xcb->atoms[RIE_UTF8_STRING]
          || reply->type == xcb->atoms[RIE_COMPOUND_TEXT]
          || reply->type == xcb->atoms[RIE_STRING]))
    {
        rie_log_error(0, "xcb_get_property(%s): unexpected type: %d",
                      rie_atom_names[property], reply->type);
        free(reply);
        return RIE_ERROR;
    }

    len = xcb_get_property_value_length(reply);
    if (len == 0) {
        rie_log_error(0, "xcb_get_property(%s) returned zero items",
                      rie_atom_names[property]);

        free(reply);
        return RIE_NOTFOUND;
    }


    val = xcb_get_property_value(reply);
    if (val == NULL) {
        rie_log_error(0, "xcb_get_property_value(%s)",
                      rie_atom_names[property]);
        free(reply);
        return RIE_ERROR;
    }

    rc = rie_str_list_to_array(val, len, arr);

    free(reply);

    return rc;
}


int
rie_xcb_client_message(rie_xcb_t *xcb, xcb_window_t target,
    xcb_window_t win, unsigned int atom, uint32_t *value, int cnt)
{
    int       i;
    uint32_t  event_mask;

    xcb_void_cookie_t            cookie;
    xcb_generic_error_t         *error;
    xcb_client_message_event_t   event;

    if (cnt > 5) {
        rie_log_error0(0, "Client messages only takes 5 data arguments");
        return RIE_ERROR;
    }

    memset(&event, 0, sizeof(xcb_client_message_event_t));

    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = win;
    event.type = xcb->atoms[atom];

    for (i = 0; i < cnt; i++) {
        event.data.data32[i] = value[i];
    }

    event_mask = SubstructureNotifyMask  | SubstructureRedirectMask;

    cookie = xcb_send_event(xcb->xc, 0, target, event_mask,
                            (const char *) &event);

    error = xcb_request_check(xcb->xc, cookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error, "xcb_send_event");
    }

    return RIE_OK;
}


int
rie_xcb_update_event_mask(rie_xcb_t *xcb, xcb_window_t win,
    unsigned long mask)
{
    uint32_t res_mask;

    xcb_void_cookie_t                    vcookie;
    xcb_generic_error_t                 *error;
    xcb_get_window_attributes_reply_t   *reply;
    xcb_get_window_attributes_cookie_t   cookie;

    /* read current attributes */

    cookie = xcb_get_window_attributes(xcb->xc, win);

    reply = xcb_get_window_attributes_reply(xcb->xc, cookie, &error);
    if (reply == NULL) {
        return rie_xcb_handle_error0(error, "xcb_get_window_attributes_reply");
    }

    /* update current attributes */

    res_mask = reply->your_event_mask | mask;

    free(reply);

    /* set new attributes */

    vcookie =  xcb_change_window_attributes_checked(xcb->xc, win,
                                                    XCB_CW_EVENT_MASK,
                                                    &res_mask);
    error = xcb_request_check(xcb->xc, vcookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error, "xcb_change_window_attributes_checked");
    }

    return RIE_OK;
}


int
rie_xcb_get_root_pixmap(rie_xcb_t *xcb, rie_gfx_t *gc, rie_image_t *img)
{
    int             rc;
    xcb_drawable_t  pixmap;

    /* locate ID of root window pixmap */
    rc = rie_xcb_property_get(xcb, xcb->root, RIE_XROOTPMAP_ID,
                              XCB_ATOM_PIXMAP, &pixmap);
    if (rc != RIE_OK) {
        return RIE_ERROR;
    }

    return rie_xcb_get_screen_pixmap(xcb, gc, pixmap, &xcb->resolution, img);
}


int
rie_xcb_get_screen_pixmap(rie_xcb_t *xcb, rie_gfx_t *gc, xcb_drawable_t obj,
    rie_rect_t *box, rie_image_t *img)
{
    int        w, h;
    size_t     len;
    uint32_t  *data;

    rie_surface_t           *surface;
    xcb_generic_error_t     *err;
    xcb_get_image_reply_t   *reply;
    xcb_get_image_cookie_t   cookie;

    w = box->w;
    h = box->h;

    /* step 1: copy image data to client */
    cookie = xcb_get_image(xcb->xc, XCB_IMAGE_FORMAT_Z_PIXMAP, obj,
                           box->x, box->y, w, h, 0xffffffff);

    reply = xcb_get_image_reply(xcb->xc, cookie, &err);
    if (err != NULL) {
        rie_xcb_handle_error0(err, "xcb_get_image_reply");
        return RIE_ERROR;
    }

    /* step 2: couple of sanity checks, since it is not clear if experimentally
     * found format of image is always the same. Ideally, locate some
     * document describing format in details and implement all possibilities.
     */

    if (reply->depth != 24) {
        rie_log_error(0, "expecting 24-bit depth root image, but found %d",
                      reply->depth);
        free(reply);
        return RIE_ERROR;
    }

    len = xcb_get_image_data_length(reply);

    if (len != (w * h * sizeof(uint32_t))) {
        rie_log_error(0, "expecting %ld pixels, but %ld returned",
                      w * h * sizeof(uint32_t), len);
        free(reply);
        return RIE_ERROR;
    }

    data = (uint32_t *) xcb_get_image_data(reply);

    /* step 3: create ARGB surface and fill its pixels */
    surface = rie_gfx_surface_from_zpixmap(gc, data, w, h);

    free(reply);

    if (surface == NULL) {
        return RIE_ERROR;
    }

    if (img->tx) {
        /* previous root image, if any */
        rie_gfx_surface_free(img->tx);
    }

    img->tx = surface;

    /* width and height are cached, for scaling purposes */
    img->box = *box;

    return RIE_OK;
}


int
rie_xcb_get_window_state(rie_xcb_t *xcb, rie_window_t *window,
    xcb_window_t xwin)
{
    int              rc, i;
    uint32_t         mask;
    xcb_atom_t      *atoms;
    rie_array_t      res;
    rie_atom_name_t  atom;

    char  buf[512], *p; /* enough to fit all states names + separators */

    memset(&res, 0, sizeof(rie_array_t));

    rc = rie_xcb_property_get_array(xcb, xwin, RIE_NET_WM_STATE, XCB_ATOM_ATOM,
                                    &res);
    if (rc == RIE_ERROR) {
        return rc;
    }

    window->state = 0;

    if (rc == RIE_NOTFOUND) {
        /* empty list in property or property unset */
        return RIE_OK;
    }

    p = buf;
    *p = 0;
    atoms = res.data;

    for (i = 0; i < res.nitems; i++) {

        for (atom = RIE_NET_WM_STATE_STICKY, mask = RIE_WIN_STATE_STICKY;
             atom <= RIE_NET_WM_STATE_DEMANDS_ATTENTION;
             atom++, mask <<= 1)
        {
            if (xcb->atoms[atom] == atoms[i]) {
                window->state |= mask;
                p += sprintf(p, "%s ", rie_atom_names[atom]
                                       + sizeof("_NET_WM_STATE_") - 1);
            }
        }
    }

    rie_debug("window %s state: %s", window->name, buf);

    rie_array_wipe(&res);

    return RIE_OK;
}


int
rie_xcb_get_window_type(rie_xcb_t *xcb, rie_window_t *window, xcb_window_t
    xwin)
{
    int  rc, i;

    uint32_t         mask;
    rie_atom_name_t  atom;

    char  buf[512], *p; /* enough to fit all window types names + separators */

    xcb_generic_error_t         *err;
    xcb_ewmh_connection_t       *ec;
    xcb_get_property_cookie_t    cookie;
    xcb_ewmh_get_atoms_reply_t   atoms;

    ec = rie_xcb_ewmh(xcb);

    cookie = xcb_ewmh_get_wm_window_type(ec, xwin);

    rc = xcb_ewmh_get_wm_window_type_reply(ec, cookie, &atoms, &err);
    if (rc == 0) {

        if (err) {
            return rie_xcb_handle_error0(err, "xcb_ewmh_get_wm_window_type");
        }
        return RIE_NOTFOUND;
    }

    window->types = 0;
    p = buf;
    *p = 0;

    for (i = 0; i < atoms.atoms_len; i++) {

        for (atom = RIE_NET_WM_WINDOW_TYPE_DESKTOP,
                mask = RIE_WINDOW_TYPE_DESKTOP;
             atom <= RIE_NET_WM_WINDOW_TYPE_NORMAL;
             atom++, mask <<= 1)
        {
            if (xcb->atoms[atom] == atoms.atoms[i]) {
                window->types |= mask;
                p += sprintf(p, "%s ", rie_atom_names[atom]
                                       + sizeof("_NET_WM_WINDOW_TYPE") - 1);
            }
        }
    }

    xcb_ewmh_get_atoms_reply_wipe(&atoms);

    rie_debug("window %s type: %s", window->name, buf);

    return RIE_OK;
}


static int
rie_xcb_set_initial_state(rie_xcb_t *xcb, rie_settings_t *cfg)
{
    uint32_t  mask;
    uint32_t  cmd[5];

    rie_atom_name_t  atom;

    cmd[0] = XCB_EWMH_WM_STATE_ADD;
    cmd[2] = 0;
    cmd[3] = 1;
    cmd[4] = 0;

    /* iterate through all known states, checking corresponding masks */
    for (atom = RIE_NET_WM_STATE_STICKY, mask = RIE_WIN_STATE_STICKY;
         atom <= RIE_NET_WM_STATE_DEMANDS_ATTENTION;
         atom++, mask <<= 1)
    {
        if (cfg->wmhints & mask) {
            cmd[1] = xcb->atoms[atom];
            if (rie_xcb_client_message(xcb, xcb->root, xcb->window,
                                       RIE_NET_WM_STATE, cmd, 5)
                != RIE_OK)
            {
                return RIE_ERROR;
            }
        }
    }

    /* sticky requires additional setting for desktop number */
    if (cfg->wmhints & RIE_WIN_STATE_STICKY) {
        cmd[0] = 0xFFFFFFFF;
        return rie_xcb_client_message(xcb, xcb->root, xcb->window,
                                      RIE_NET_WM_DESKTOP, cmd, 1);
    }

    return RIE_OK;
}


int
rie_xcb_set_desktop(rie_xcb_t *xcb, uint32_t new_desk)
{
    return rie_xcb_client_message(xcb, rie_xcb_get_root(xcb),
                                  rie_xcb_get_root(xcb),
                                  RIE_NET_CURRENT_DESKTOP, &new_desk, 1);
}


int
rie_xcb_set_desktop_viewport(rie_xcb_t *xcb, int x, int y)
{
    uint32_t  cmd[2];

    cmd[0] = x;
    cmd[1] = y;

    return rie_xcb_client_message(xcb, rie_xcb_get_root(xcb),
                                  rie_xcb_get_root(xcb),
                                  RIE_NET_DESKTOP_VIEWPORT, cmd, 2);
}


int
rie_xcb_restore_hidden(rie_xcb_t *xcb, xcb_window_t w)
{

#if 0

    /* this method does not work in Ubuntu and E16,
     * but works in fluxbox/openbox/gnome
     *
     * window manager MAY reject request for removal of hidden state
     */

    uint32_t  cmd[5];

    cmd[0] = XCB_EWMH_WM_STATE_REMOVE;
    cmd[1] = xcb->atoms[RIE_NET_WM_STATE_HIDDEN];
    cmd[2] = 0;
    cmd[3] = 1;
    cmd[4] = 0;

    return rie_xcb_client_message(xcb, xcb->root, w, RIE_NET_WM_STATE, cmd, 5);

#else

    /* this looks more compatible, but has the drawback: you are switched
     * to the desktop of the hidden window.
     *
     * Probably this is ok, since you probably don't want to restore window
     * on another desktop without using it.
     */

    return rie_xcb_set_focus(xcb, w);

#endif
}


int
rie_xcb_set_focus(rie_xcb_t *xcb, xcb_window_t winid)
{
    uint32_t data[3];

    data[0] = 2;                       /* source indication: pager */
    data[1] = 0;                       /* timestamp (unset, but seem working) */
    data[2] = rie_xcb_get_window(xcb); /* currently active window */

    return rie_xcb_client_message(xcb, rie_xcb_get_root(xcb), winid,
                                  RIE_NET_ACTIVE_WINDOW, data, 3);
}


int
rie_xcb_set_desktop_layout(rie_xcb_t *xcb, rie_settings_t *cfg)
{
    uint32_t  cols, rows;

    xcb_void_cookie_t     cookie;
    xcb_generic_error_t  *error;

    /* _NET_DESKTOP_LAYOUT:
     *
     *  Either rows or columns (but not both) may be specified as 0 in which
     *  case its actual value will be derived from _NET_NUMBER_OF_DESKTOPS.
     */

    if (cfg->desktop.orientation == XCB_EWMH_WM_ORIENTATION_HORZ) {
        cols = cfg->desktop.wrap;
        rows = 0;

    } else { /* XCB_EWMH_WM_ORIENTATION_VERT */
        rows = cfg->desktop.wrap;
        cols = 0;
    }

    cookie = xcb_ewmh_set_desktop_layout_checked(rie_xcb_ewmh(xcb),
                                                 rie_xcb_screen(xcb),
                                                 cfg->desktop.orientation,
                                                 cols, rows,
                                                 cfg->desktop.corner);
    error = xcb_request_check(rie_xcb_get_connection(xcb), cookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error, "xcb_ewmh_set_desktop_layout");
    }

    return RIE_OK;
}


int
rie_xcb_handle_error_real(char *file, int line, void *xerr, char *fmt, ...)
{
    va_list  ap;

    xcb_generic_error_t *err = xerr;

    int  rc;
    char buf[128];

    *buf = 0;

    rc = RIE_ERROR;

    if (err) {
        sprintf(buf, "request \"%s\" error %d (%s)",
                xcb_event_get_request_label(err->major_code),
                err->error_code,
                xcb_event_get_error_label(err->error_code));

        switch (err->error_code) {
        case XCB_DRAWABLE:
        case XCB_WINDOW:
            /* indicate as a separate type of errors: pager may often
             * fall into situations when it refers to closed window
             */
            rc = RIE_NOTFOUND;
            break;
        }
    }

    va_start(ap, fmt);
    rie_log_str_error_real_v(file, line, buf, fmt, ap);
    va_end(ap);

    return rc;
}
