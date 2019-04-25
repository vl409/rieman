
/*
 * Copyright (C) 2017-2019 Vladimir Homutov
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
#include "rie_event.h"
#include "rie_xcb.h"
#include "rie_gfx.h"
#include "rie_render.h"
#include "rie_external.h"

#define RIE_PAGER_EVENT  0x2


typedef int (*rie_event_handler_pt)(rie_t *pager, xcb_generic_event_t *ev);

typedef struct {
    int                    id;
    char                  *evname;
    rie_event_handler_pt   handler;
    uint8_t                loggable;
} rie_event_t;


static uint32_t rie_event_mask(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_handle_pager_event(rie_t *pager, xcb_generic_event_t *ev);
static void rie_event_reload(rie_t **ppager);
static int rie_event_xcb_expose(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xcb_enter_notify(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xcb_leave_notify(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xcb_motion_notify(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xcb_button_release(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xcb_configure_notify(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xcb_destroy_notify(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xcb_property_notify(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_number_of_desktops(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_desktop_names(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_current_desktop(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_client_list(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_desktop_geometry(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_wm_state(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_wm_window_type(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_wm_desktop(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_xrootpmap_id(rie_t *pager,  xcb_generic_event_t *ev);
static int rie_event_active_window(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_workarea(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_desktop_viewport(rie_t *pager, xcb_generic_event_t *ev);
static int rie_event_virtual_roots(rie_t *pager, xcb_generic_event_t *ev);

#define named(val)  val, #val

rie_event_t rie_event_handlers[] = {
    { named(XCB_EXPOSE),           rie_event_xcb_expose,           0 },
    { named(XCB_ENTER_NOTIFY),     rie_event_xcb_enter_notify,     1 },
    { named(XCB_LEAVE_NOTIFY),     rie_event_xcb_leave_notify,     1 },
    { named(XCB_MOTION_NOTIFY),    rie_event_xcb_motion_notify,    0 },
    { named(XCB_BUTTON_RELEASE),   rie_event_xcb_button_release,   1 },
    { named(XCB_CONFIGURE_NOTIFY), rie_event_xcb_configure_notify, 1 },
    { named(XCB_DESTROY_NOTIFY),   rie_event_xcb_destroy_notify,   0 },
    { named(XCB_PROPERTY_NOTIFY),  rie_event_xcb_property_notify,  0 }
};

rie_event_t rie_property_event_handlers[] ={
    { named(RIE_NET_NUMBER_OF_DESKTOPS),   rie_event_number_of_desktops, 1 },
    { named(RIE_NET_DESKTOP_NAMES),        rie_event_desktop_names,      1 },
    { named(RIE_NET_CURRENT_DESKTOP),      rie_event_current_desktop,    1 },
    { named(RIE_NET_CLIENT_LIST_STACKING), rie_event_client_list,        1 },
    { named(RIE_NET_DESKTOP_GEOMETRY),     rie_event_desktop_geometry,   1 },
    { named(RIE_NET_WM_STATE),             rie_event_wm_state,           1 },
    { named(RIE_NET_WM_WINDOW_TYPE),       rie_event_wm_window_type,     1 },
    { named(RIE_NET_WM_DESKTOP),           rie_event_wm_desktop,         1 },
    { named(RIE_XROOTPMAP_ID),             rie_event_xrootpmap_id,       1 },
    { named(RIE_NET_ACTIVE_WINDOW),        rie_event_active_window,      1 },
    { named(RIE_NET_WORKAREA),             rie_event_workarea,           1 },
    { named(RIE_NET_DESKTOP_VIEWPORT),     rie_event_desktop_viewport,   1 },
    { named(RIE_NET_VIRTUAL_ROOTS),        rie_event_virtual_roots,      1 }
};


extern uint8_t  rie_quit;
extern uint8_t  rie_reload;


int
rie_event_init(rie_t *pager)
{
    int  i;

    static rie_event_handler_pt  init_handlers[] = {
        rie_event_desktop_geometry,
        rie_event_workarea,
        rie_event_desktop_viewport,
        rie_event_virtual_roots,
        rie_event_number_of_desktops,
        rie_event_desktop_names,
        rie_event_current_desktop,
        rie_event_client_list,
        rie_event_xrootpmap_id,
        NULL
    };

    /* trigger fake events to populate initial settings */
    for (i = 0; init_handlers[i]; i++) {
        if (init_handlers[i](pager, NULL) != RIE_OK) {
            return RIE_ERROR;
        }
    }

    /* initial render with window positioning and resize */
    return rie_render(pager);
}


void
rie_event_cleanup(rie_t *pager)
{
    if (pager->windows.data) {
        rie_array_wipe(&pager->windows);
        pager->fwindow = NULL;
    }

    rie_array_wipe(&pager->desktops);
    rie_array_wipe(&pager->desktop_names);

    rie_array_wipe(&pager->workareas);
    rie_array_wipe(&pager->viewports);
    rie_array_wipe(&pager->virtual_roots);

    if (pager->root_bg.tx) {
        rie_gfx_surface_free(pager->root_bg.tx);
        pager->root_bg.tx = NULL;
    }
}


int
rie_event_loop(rie_t *pager, sigset_t *sigmask)
{
    int       rc;
    uint32_t  mask;

    xcb_generic_event_t  *ev;

    do {

        while ((ev = rie_xcb_next_event(pager->xcb))) {
            rc = RIE_OK;

            rie_debug("event: #%d", rie_xcb_event_type(ev));

            mask = rie_event_mask(pager, ev);

            if (mask & RIE_PAGER_EVENT) {
                if (rie_event_handle_pager_event(pager, ev) != RIE_OK) {
                    rc = RIE_ERROR;
                }
            }

            if (rc != RIE_OK) {

#if defined (RIE_DEBUG)
                rie_debug("leaving event loop during processing event %d "
                          "due to error, backtrace of the last one is below",
                          rie_xcb_event_type(ev));
                rie_log_backtrace();
#endif
                free(ev);
                goto done;
            }

            free(ev);

            if (pager->render) {
                /* render errors are ignored in hope they are not permanent */
                (void) rie_render(pager);
                pager->render = 0;
            }
        }

        if (rie_xcb_wait_for_event(pager->xcb, sigmask) != RIE_OK) {
            goto done;
        }

        /* handle received signals */

        if (rie_quit) {
            rie_quit = 0;

            rie_log(" *** terminate signal received ***");
            break;
        }

        if (rie_reload) {
            rie_reload = 0;

            rie_log(" *** reload signal received ***");
            rie_event_reload(&pager);
        }

    } while (1);

done:

    rie_log("rieman ver. %s (%s) exiting...", RIEMAN_VERSION, RIE_REV);

    rie_pager_delete(pager, 1);

    return EXIT_SUCCESS;

}


static uint32_t
rie_event_mask(rie_t *pager, xcb_generic_event_t *ev)
{
    return RIE_PAGER_EVENT;
}


static int
rie_event_handle_pager_event(rie_t *pager, xcb_generic_event_t *ev)
{
    int  i;

    static size_t nevents =
                    sizeof(rie_event_handlers) / sizeof(rie_event_handlers[0]);

    /* lookup known events */
    for (i = 0; i < nevents; i++) {
        if (rie_xcb_event_type(ev) == rie_event_handlers[i].id) {
            if (rie_event_handlers[i].loggable) {
                rie_debug("event %s", rie_event_handlers[i].evname);
            }
            break;
        }
    }

    if (i == nevents) {
        /* unknown event */
        rie_debug("unknown event #%d", rie_xcb_event_type(ev));
        return RIE_OK;
    }

    return rie_event_handlers[i].handler(pager, ev);
}


static void
rie_event_reload(rie_t **ppager)
{
    rie_t  *newpager, *oldpager;

    oldpager = *ppager;

    newpager = rie_pager_new(oldpager->cfg->conf_file, oldpager->log);
    if (newpager == NULL) {
        goto failed;
    }

    if (rie_pager_init(newpager, oldpager) != RIE_OK) {
        goto failed;
    }

    if (rie_event_init(newpager) != RIE_OK) {
        goto failed;
    }

    rie_pager_delete(oldpager, 0);

    *ppager = newpager;

    return;

failed:

    if (newpager) {
        rie_pager_delete(newpager, 0);
    }

    rie_log_error0(0, "reload failed, keeping old configuration");
}


static int
rie_event_xcb_expose(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_expose_event_t  *expose = (xcb_expose_event_t *) ev;

    /* Avoid extra redraws by checking if this is
     * the last expose event in the sequence
     */
    if (expose->count == 0) {
        pager->render = 1;
    }

    return RIE_OK;
}


static int
rie_event_xcb_enter_notify(rie_t *pager, xcb_generic_event_t *ev)
{
    pager->m_in = 1;
    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_xcb_leave_notify(rie_t *pager, xcb_generic_event_t *ev)
{
    pager->m_in = 0;
    pager->m_x = -1;
    pager->m_y = -1;
    pager->selected_desktop = -1;
    pager->selected_vp.x = -1;
    pager->selected_vp.y = -1;

    if (pager->fwindow) {
        pager->fwindow->m_in = 0;
        pager->fwindow = NULL;
    }

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_xcb_motion_notify(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_motion_notify_event_t  *motion = (xcb_motion_notify_event_t *) ev;

    int            new_desk, new_x, new_y;
    rie_window_t  *fwindow;

    pager->m_x = motion->event_x;
    pager->m_y = motion->event_y;

    fwindow = pager->fwindow;

    rie_window_update_pager_focus(pager);

    if (pager->cfg->show_viewports && (pager->vp_rows > 1 || pager->vp_cols > 1)) {
        new_desk = rie_viewport_by_coords(pager, motion->event_x,
                                          motion->event_y, &new_x, &new_y);

    } else {
        new_desk = rie_desktop_by_coords(pager, motion->event_x,
                                         motion->event_y);
    }

    if (new_desk == -1) {
        /* not a desktop/viewport */
        return RIE_OK;
    }

    if (new_desk != pager->selected_desktop || fwindow != pager->fwindow) {

        pager->selected_desktop = new_desk;
        pager->render = 1;
    }

    if (pager->cfg->show_viewports && (pager->vp_rows > 1 || pager->vp_cols > 1)) {

        if (pager->selected_vp.x != new_x || pager->selected_vp.y != new_y) {
            pager->selected_vp.x = new_x;
            pager->selected_vp.y = new_y;
            pager->render = 1;
        }
    }

    return RIE_OK;
}


static uint32_t
rie_hidden_window_by_coords(rie_t *pager, int x, int y)
{
    int  i;

    rie_window_t   *win;

    win = pager->windows.data;

    for (i = 0; i < pager->windows.nitems; i++) {

        if (!(win[i].state & RIE_WIN_STATE_HIDDEN)) {
            continue;
        }

        if (rie_gfx_xy_inside_rect(x, y, &win[i].hbox)) {
            return win[i].winid;
        }
    }

    return 0;
}


static int
rie_event_xcb_button_release(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_button_release_event_t  *button = (xcb_button_release_event_t *) ev;

    int  new_desk, new_x, new_y;

    uint32_t wid;

    new_x = new_y = 0;

    if (!(button->state & pager->cfg->tile_button
          || button->state & pager->cfg->change_desktop_button))
    {
        return RIE_OK;
    }

    if (pager->cfg->show_viewports
        && (pager->vp_rows > 1 || pager->vp_cols > 1))
    {
        new_desk = rie_viewport_by_coords(pager, button->event_x,
                                          button->event_y, &new_x, &new_y);

    } else {
        new_desk = rie_desktop_by_coords(pager, button->event_x,
                                         button->event_y);
    }

    wid = rie_hidden_window_by_coords(pager, button->event_x, button->event_y);
    if (wid) {
        return rie_xcb_restore_hidden(pager->xcb, wid);
    }

    if (new_desk == -1) {
        /* ignore event, not a desktop hit: border/labels... */
        return RIE_OK;
    }

    if (button->state & pager->cfg->tile_button) {

        pager->render = 1;

        return rie_windows_tile(pager, new_desk);
    }

    /* button->state & pager->cfg->change_desktop_button */

    if (new_desk != pager->current_desktop) {
        if (rie_xcb_set_desktop(pager->xcb, new_desk) != RIE_OK) {
            return RIE_ERROR;
        }
    }

    if (pager->cfg->show_viewports) {
        if (rie_xcb_set_desktop_viewport(pager->xcb, new_x, new_y) != RIE_OK) {
            return RIE_ERROR;
        }
        pager->vp.x = new_x;
        pager->vp.y = new_y;
    }

    if (pager->fwindow) {

        if (rie_xcb_set_focus(pager->xcb, pager->fwindow->winid) != RIE_OK) {
            return RIE_ERROR;
        }

        rie_log("user hit desktop %d, window 0x%x", new_desk,
                pager->fwindow->winid);

    } else {
        rie_log("user hit desktop %d", new_desk);
    }

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_xcb_configure_notify(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_configure_request_event_t *xce = (xcb_configure_request_event_t *) ev;

    int            rc;
    rie_window_t  *win;
    xcb_window_t   root;

    root = rie_xcb_get_root(pager->xcb);

    if (xce->window == root) {
        pager->render = 1;
        return rie_xcb_update_root_geom(pager->xcb);
    }

    win = rie_window_lookup(pager, xce->window);
    if (win == NULL) {
        return RIE_OK;
    }

    rc = rie_window_query(pager, win, xce->window);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    if (rc == RIE_NOTFOUND) {
        /* we failed to obtain information about this window, ignore it */
        win->dead = 1;
        win->desktop = 0;
    }

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_xcb_destroy_notify(rie_t *pager, xcb_generic_event_t *ev)
{
    /* tracking this event allows to avoid some errors in log */

    xcb_destroy_notify_event_t *dn = (xcb_destroy_notify_event_t *) ev;

    rie_window_t  *win;

    win = rie_window_lookup(pager, dn->window);

    if (win) {
        win->dead = 1;    /* ignore this window since now */
        win->desktop = 0; /* consider it not to occupy any desktop */
    }

    return RIE_OK;
}


static int
rie_event_xcb_property_notify(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_property_notify_event_t *notify = (xcb_property_notify_event_t *) ev;

    int  atom, i;

    static size_t nevents = sizeof(rie_property_event_handlers)
                            / sizeof(rie_property_event_handlers[0]);

    atom = rie_xcb_property_notify_atom(pager->xcb, notify);

    for (i = 0; i < nevents; i++) {
        if (atom == rie_property_event_handlers[i].id) {
            break;
        }
    }

    if (i == nevents) {
        /* ignore unknown atoms */
        rie_debug("unknown property notify event, atom #%d", atom);
        return RIE_OK;
    }


    if (rie_property_event_handlers[i].loggable) {
        rie_debug("event property notify: %s",
                  rie_property_event_handlers[i].evname);
    }

    return rie_property_event_handlers[i].handler(pager, ev);
}


static int
rie_event_number_of_desktops(rie_t *pager, xcb_generic_event_t *ev)
{
    int       screen, rc, len, oldcount;
    uint32_t  ndesktops;

    rie_array_t                 desktops;
    xcb_generic_error_t        *err;
    xcb_ewmh_connection_t      *ec;
    xcb_get_property_cookie_t   cookie;

    ec = rie_xcb_ewmh(pager->xcb);
    screen =  rie_xcb_screen(pager->xcb);

    oldcount = pager->desktops.nitems;

    cookie = xcb_ewmh_get_number_of_desktops(ec, screen);

    rc = xcb_ewmh_get_number_of_desktops_reply(ec, cookie, &ndesktops, &err);
    if (rc == 0) {
        if (err) {
            return rie_xcb_handle_error0(err, "xcb_ewmh_get_number_of_desktops_reply");
        }

        /* property was not found, consider single desktop */
        ndesktops = 1;
    }

    if (oldcount == ndesktops) {
        return RIE_OK;
    }

    rc = rie_array_init(&desktops, ndesktops, sizeof(rie_desktop_t), NULL);
    if (rc != RIE_OK) {
        return RIE_ERROR;
    }

    if (pager->desktops.data) {

        len = (oldcount < ndesktops) ? oldcount: ndesktops;

        memcpy(desktops.data, pager->desktops.data, len * sizeof(rie_desktop_t));

        rie_array_wipe(&pager->desktops);
    }

    pager->desktops = desktops;

    pager->resize = 1;
    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_desktop_names(rie_t *pager, xcb_generic_event_t *ev)
{
    int  rc, screen;

    rie_array_t                 names;
    xcb_generic_error_t        *err;
    xcb_ewmh_connection_t      *ec;
    xcb_get_property_cookie_t   cookie;

    xcb_ewmh_get_utf8_strings_reply_t   reply;

    ec = rie_xcb_ewmh(pager->xcb);
    screen =  rie_xcb_screen(pager->xcb);

    cookie = xcb_ewmh_get_desktop_names(ec, screen);

    rc = xcb_ewmh_get_desktop_names_reply(ec, cookie, &reply, &err);
    if (rc == 0) {
        if (err) {
            return rie_xcb_handle_error0(err, "xcb_ewmh_get_desktop_names");
        }

        return RIE_OK;
    }

    rc = rie_str_list_to_array(reply.strings, reply.strings_len, &names);

    xcb_ewmh_get_utf8_strings_reply_wipe(&reply);

    if (rc != RIE_OK) {
        return RIE_ERROR;
    }

    if (pager->desktop_names.data) {
        rie_array_wipe(&pager->desktop_names);
    }

    pager->desktop_names = names;

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_current_desktop(rie_t *pager, xcb_generic_event_t *ev)
{
    int  rc, screen;

    xcb_generic_error_t        *err;
    xcb_ewmh_connection_t      *ec;
    xcb_get_property_cookie_t   cookie;

    ec = rie_xcb_ewmh(pager->xcb);
    screen =  rie_xcb_screen(pager->xcb);

    cookie = xcb_ewmh_get_current_desktop(ec, screen);

    rc = xcb_ewmh_get_current_desktop_reply(ec, cookie,
                                            &pager->current_desktop, &err);
    if (rc == 0) {
        if (err) {
            return rie_xcb_handle_error0(err, "xcb_ewmh_get_current_desktop");
        }
        return RIE_OK;
    }

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_client_list(rie_t *pager, xcb_generic_event_t *ev)
{
    int  rc, screen, i;

    rie_array_t     winlist;
    rie_window_t   *win;

    xcb_generic_error_t           *err;
    xcb_ewmh_connection_t         *ec;
    xcb_get_property_cookie_t      cookie;
    xcb_ewmh_get_windows_reply_t   clients;

    ec = rie_xcb_ewmh(pager->xcb);
    screen = rie_xcb_screen(pager->xcb);

    cookie = xcb_ewmh_get_client_list_stacking(ec, screen);

    rc = xcb_ewmh_get_client_list_stacking_reply(ec, cookie, &clients, &err);
    if (rc == 0) {
        if (err) {
            return rie_xcb_handle_error0(err, "xcb_ewmh_get_client_list_stacking");
        }

        return RIE_OK;
    }

    rc = rie_window_init_list(&winlist, clients.windows_len);

    if (rc != RIE_OK) {
        xcb_ewmh_get_windows_reply_wipe(&clients);
        return RIE_ERROR;
    }

    win = (rie_window_t *) winlist.data;

    for (i = 0; i < clients.windows_len; i++) {
        rc = rie_window_query(pager, &win[i], clients.windows[i]);

        if (rc == RIE_ERROR) {
            rie_array_wipe(&winlist);
            xcb_ewmh_get_windows_reply_wipe(&clients);
            return RIE_ERROR;
        }

        if (rc == RIE_NOTFOUND) {
            /* we failed to obtain information about this window, ignore it */
            win[i].dead = 1;
            win[i].desktop = 0;
            continue;
        }

        rc = rie_xcb_get_window_state(pager->xcb, &win[i], clients.windows[i]);

        if (rc == RIE_ERROR) {
            rie_array_wipe(&winlist);
            xcb_ewmh_get_windows_reply_wipe(&clients);
            return RIE_ERROR;
        }
    }

    if (pager->windows.data != NULL) {
        rie_array_wipe(&pager->windows);
        pager->fwindow = NULL;
    }

    pager->windows = winlist;

    /* trigger NET_ACTIVE_WINDOW lookup - it does not change with client list */
    (void) rie_event_active_window(pager, ev);
    /* focused window inside pager also needs to be updated */
    rie_window_update_pager_focus(pager);

    xcb_ewmh_get_windows_reply_wipe(&clients);

    pager->render = 1;
    return RIE_OK;
}


static int
rie_event_desktop_geometry(rie_t *pager, xcb_generic_event_t *ev)
{
    int       rc, screen;
    uint32_t  w, h;

    xcb_generic_error_t        *err;
    xcb_ewmh_connection_t      *ec;
    xcb_get_property_cookie_t   cookie;

    ec = rie_xcb_ewmh(pager->xcb);
    screen = rie_xcb_screen(pager->xcb);

    cookie = xcb_ewmh_get_desktop_geometry(ec, screen);

    if (xcb_ewmh_get_desktop_geometry_reply(ec, cookie, &w, &h, &err) == 0) {
        /* may fail if NET_DESKTOP_GEOMETRY is not set */
        rie_xcb_handle_error0(err, "xcb_ewmh_get_desktop_geometry");

        rc = rie_external_get_desktop_geometry(&w, &h);
        if (rc != RIE_OK) {
            return rc;
        }
    }

    pager->desktop_geom.w = w;
    pager->desktop_geom.h = h;

    rie_log("desktop geometry is %ld x %ld", w, h);

    pager->render = 1;
    pager->resize = 1;

    return RIE_OK;
}


static int
rie_event_wm_state(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_property_notify_event_t *xpe = (xcb_property_notify_event_t *) ev;

    int            rc;
    rie_window_t  *win;

    win = rie_window_lookup(pager, xpe->window);
    if (win == NULL) {
        return RIE_OK;
    }

    if (win->dead) {
        pager->render = 1;
        return RIE_OK;
    }

    rc = rie_xcb_get_window_state(pager->xcb, win, xpe->window);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_wm_window_type(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_property_notify_event_t *xpe = (xcb_property_notify_event_t *) ev;

    int            rc;
    rie_window_t  *win;

    win = rie_window_lookup(pager, xpe->window);
    if (win == NULL) {
        return RIE_OK;
    }

    rc = rie_xcb_get_window_type(pager->xcb, win, xpe->window);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_wm_desktop(rie_t *pager, xcb_generic_event_t *ev)
{
    xcb_property_notify_event_t *xpe = (xcb_property_notify_event_t *) ev;

    int            rc;
    rie_window_t  *window;

    window = rie_window_lookup(pager, xpe->window);
    if (window == NULL) {
        return RIE_OK;
    }

    if (window->dead) {
        return RIE_OK;
    }

    rc = rie_xcb_property_get(pager->xcb, xpe->window, RIE_NET_WM_DESKTOP,
                              XCB_ATOM_CARDINAL, &window->desktop);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;

    } else if (rc == RIE_NOTFOUND) {
        /* window has no yet desktop assigned */
        window->desktop = 0;
    }

    pager->render = 1;
    return RIE_OK;
}


static int
rie_event_xrootpmap_id(rie_t *pager, xcb_generic_event_t *ev)
{
    int  rc;

    rc = rie_xcb_get_root_pixmap(pager->xcb, pager->gfx, &pager->root_bg);
    if (rc == RIE_ERROR) {
        /* do not fail hardly if there is not root window */
        pager->root_bg.tx = NULL;
        rie_log("WARNING: can't get root window pixmap,"
                " continuing with color background");

        /* return RIE_ERROR; */
    }

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_active_window(rie_t *pager, xcb_generic_event_t *ev)
{
    int       rc, i;
    uint32_t  focused;

    rie_window_t  *win;

    rc = rie_xcb_property_get(pager->xcb, rie_xcb_get_root(pager->xcb),
                              RIE_NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, &focused);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;

    } else if (rc == RIE_NOTFOUND) {
        /* window no more exists, just ignore this event;
         * client list event will trigger pager update
         */
        return RIE_OK;
    }

    win = pager->windows.data;

    /* only single window can be focused */
    for (i = 0; i < pager->windows.nitems; i++) {
        if (win[i].winid == focused) {
            win[i].focused = 1;

        } else {
            win[i].focused = 0;
        }
    }

    rie_window_update_pager_focus(pager);

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_workarea(rie_t *pager, xcb_generic_event_t *ev)
{
    int  rc, i, screen;

    rie_rect_t             *area;
    rie_array_t             workareas;
    xcb_generic_error_t    *err;
    xcb_ewmh_connection_t  *ec;

    xcb_get_property_cookie_t      cookie;
    xcb_ewmh_get_workarea_reply_t  reply;

    ec = rie_xcb_ewmh(pager->xcb);
    screen = rie_xcb_screen(pager->xcb);

    cookie = xcb_ewmh_get_workarea(ec, screen);

    if (xcb_ewmh_get_workarea_reply(ec, cookie, &reply, &err) == 0) {
        rie_xcb_handle_error0(err, "xcb_ewmh_get_workarea");
        goto fallback;
    }

    if (reply.workarea_len == 0) {
        xcb_ewmh_get_workarea_reply_wipe(&reply);
        goto fallback;
    }

    rc = rie_array_init(&workareas, reply.workarea_len,
                        sizeof(rie_rect_t), NULL);
    if (rc != RIE_OK) {
        xcb_ewmh_get_workarea_reply_wipe(&reply);
        return RIE_ERROR;
    }

    area = workareas.data;

    for (i = 0; i < workareas.nitems; i++) {
        area[i].x = reply.workarea->x;
        area[i].y = reply.workarea->y;
        area[i].w = reply.workarea->width;
        area[i].h = reply.workarea->height;
    }

    xcb_ewmh_get_workarea_reply_wipe(&reply);

    if (pager->workareas.data) {
        rie_array_wipe(&pager->workareas);
    }

    pager->workareas = workareas;

    pager->render = 1;
    return RIE_OK;

fallback:

    /* for those WMs that don't set WORKAREA (maybe use DESKTOP_GEOMETRY) */
    rc = rie_array_init(&workareas, 1, sizeof(rie_rect_t), NULL);
    if (rc != RIE_OK) {
        return RIE_ERROR;
    }

    area = workareas.data;

    area[0] = pager->desktop_geom;

    if (pager->workareas.data) {
        rie_array_wipe(&pager->workareas);
    }

    pager->workareas = workareas;

    pager->render = 1;
    return RIE_OK;
}


static int
rie_event_desktop_viewport(rie_t *pager, xcb_generic_event_t *ev)
{
    int           rc, i, screen;
    rie_rect_t   *viewport, root_geom;
    rie_array_t   viewports;

    xcb_generic_error_t        *err;
    xcb_ewmh_connection_t      *ec;
    xcb_get_property_cookie_t   cookie;

    xcb_ewmh_get_desktop_viewport_reply_t   reply;

    ec = rie_xcb_ewmh(pager->xcb);
    screen = rie_xcb_screen(pager->xcb);
    root_geom = rie_xcb_root_geom(pager->xcb);

    cookie = xcb_ewmh_get_desktop_viewport(ec, screen);

    if (xcb_ewmh_get_desktop_viewport_reply(ec, cookie, &reply, &err) == 0) {
        rie_xcb_handle_error0(err, "xcb_ewmh_get_desktop_viewport");
        goto fallback;
    }

    if (reply.desktop_viewport_len == 0) {
        xcb_ewmh_get_desktop_viewport_reply_wipe(&reply);
        goto fallback;
    }

    rc = rie_array_init(&viewports, reply.desktop_viewport_len,
                        sizeof(rie_rect_t), NULL);
    if (rc != RIE_OK) {
        xcb_ewmh_get_desktop_viewport_reply_wipe(&reply);
        return RIE_ERROR;
    }

    viewport = viewports.data;

    for (i = 0; i < viewports.nitems; i++) {
        viewport[i].x = reply.desktop_viewport[i].x;
        viewport[i].y = reply.desktop_viewport[i].y;

        /* .w is column, .h is row of a desktop's viewport */
        viewport[i].w = viewport[i].x / root_geom.w;
        viewport[i].h = viewport[i].y / root_geom.h;
    }

    xcb_ewmh_get_desktop_viewport_reply_wipe(&reply);

    if (pager->viewports.data) {
        rie_array_wipe(&pager->viewports);
    }
    pager->viewports = viewports;

    /* viewport change means all windows changed their coordinates */
    rc = rie_window_update_geometry(pager);

    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    pager->render = 1;

    return RIE_OK;

fallback:

    /* for those WMs that don't set _NET_DESKTOP_VIEWPORT, set to zero,
     * so that rendering code always could obtain viewport
     */

    rc = rie_array_init(&viewports, 1, sizeof(rie_rect_t), NULL);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    viewport = viewports.data;

    viewport[0].x = 0;
    viewport[0].y = 0;
    viewport[0].w = root_geom.w;
    viewport[0].h = root_geom.h;

    if (pager->viewports.data) {
        rie_array_wipe(&pager->viewports);
    }
    pager->viewports = viewports;

    pager->render = 1;

    return RIE_OK;
}


static int
rie_event_virtual_roots(rie_t *pager, xcb_generic_event_t *ev)
{
    int           rc, i, screen;
    xcb_window_t *window;
    rie_array_t   virtual_roots;

    xcb_generic_error_t        *err;
    xcb_ewmh_connection_t      *ec;
    xcb_get_property_cookie_t   cookie;

    xcb_ewmh_get_windows_reply_t   reply;

    ec = rie_xcb_ewmh(pager->xcb);
    screen = rie_xcb_screen(pager->xcb);

    cookie = xcb_ewmh_get_virtual_roots(ec, screen);

    if (xcb_ewmh_get_virtual_roots_reply(ec, cookie, &reply, &err) == 0) {
#if defined(RIE_DEBUG)
        rie_xcb_handle_error0(err, "xcb_ewmh_get_virtual_roots");
#endif
        goto fallback;
    }

    if (reply.windows_len == 0) {
        xcb_ewmh_get_windows_reply_wipe(&reply);
        goto fallback;
    }

    rc = rie_array_init(&virtual_roots, reply.windows_len,
                        sizeof(xcb_window_t), NULL);
    if (rc != RIE_OK) {
        xcb_ewmh_get_windows_reply_wipe(&reply);
        return RIE_ERROR;
    }

    window = virtual_roots.data;

    for (i = 0; i < virtual_roots.nitems; i++) {
        window[i] = reply.windows[i];
    }

    xcb_ewmh_get_windows_reply_wipe(&reply);

    if (pager->virtual_roots.data) {
        rie_array_wipe(&pager->virtual_roots);
    }
    pager->virtual_roots = virtual_roots;

    pager->render = 1;

    return RIE_OK;

fallback:

    /* for those WMs that don't set VIRTUAL_ROOTS, set to real root */

    rc = rie_array_init(&virtual_roots, 1, sizeof(xcb_window_t), NULL);
    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    window = virtual_roots.data;

    window[0] = rie_xcb_get_root(pager->xcb);

    if (pager->virtual_roots.data) {
        rie_array_wipe(&pager->virtual_roots);
    }
    pager->virtual_roots = virtual_roots;

    pager->render = 1;

    return RIE_OK;
}
