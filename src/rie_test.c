
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
#include "rie_event.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>


/*
 * X application to execute for testing windows operations
 * killall is used during test cleanup, so don't use something important
 */
#define TEST_APP "xterm"
#define TEST_APP_CLASS "XTerm"


#define rie_test_poll_cond(tc, cond, ms)       \
    do {                                       \
        uint32_t  cnt = 0;                     \
        struct timespec ts  = { 0, 10000000 }; \
                                               \
        while (cond) {                         \
            nanosleep(&ts, NULL);              \
            if (cnt++ > ms / 10 ) {            \
                break;                         \
            }                                  \
        }                                      \
    } while (0)

#define rie_tc_failed(tc)                                      \
    do { tc->file = __FILE__; tc->line = __LINE__; } while(0)


typedef struct rie_testcase_s  rie_testcase_t;
typedef int (*rie_testcase_pt)(rie_t *pager, rie_testcase_t *tc);

struct rie_testcase_s {
    char             *name;
    rie_testcase_pt   run;
    uint8_t           passed;
    uint8_t           completed;
    char             *file;
    uint32_t          line;
};

typedef struct {
    rie_t           **pagerp;
    rie_t            *pager;
    uint32_t          passed;
    uint32_t          failed;
    uint32_t          completed;
    uint32_t          total;
} rie_test_ctx_t;


/* Tests execution */
static void rie_test_init() __attribute__ ((constructor));
static void *rie_testing_thread(void *data);

/* Helper functions */
static int rie_test_exec(char *fmt, ...);

/* Testcases */
static int rie_testcase_ndesktops(rie_t *pager, rie_testcase_t *tc);
static int rie_testcase_current_desktop(rie_t *pager, rie_testcase_t *tc);
static int rie_testcase_switch_desktop(rie_t *pager, rie_testcase_t *tc);
static int rie_testcase_screenetry(rie_t *pager, rie_testcase_t *tc);
static int rie_testcase_desktop_name_change(rie_t *pager, rie_testcase_t *tc);
static int rie_testcase_window_states(rie_t *pager, rie_testcase_t *tc);
static int rie_testcase_geometry_fallback(rie_t *pager, rie_testcase_t *tc);
static int rie_testcase_window_change_desktop(rie_t *pager, rie_testcase_t *tc);

static rie_testcase_t rie_testcases[] = {
    { "desktops number changes", rie_testcase_ndesktops, },
    { "current desktop changes", rie_testcase_current_desktop, },
    { "switching desktops", rie_testcase_switch_desktop, },
    { "switching resolution", rie_testcase_screenetry, },
    { "desktop name change", rie_testcase_desktop_name_change, },
    { "minimized window", rie_testcase_window_states, },
    { "geometry fallback", rie_testcase_geometry_fallback, },
    { "window desktop change", rie_testcase_window_change_desktop, },
    { NULL, NULL, }
};

static pthread_t rie_test_tid;
static char *rie_test_enabled;

extern rie_t *pagerp;


void
rie_test_init()
{
    rie_test_ctx_t *ctx;

    rie_test_enabled = getenv("RIE_TEST_ENABLE");
    if (rie_test_enabled == NULL) {
        /* only perform testing, if requested */
        return;
    }

    ctx = malloc(sizeof(rie_test_ctx_t));
    if (ctx == NULL) {
        rie_log_error0(errno, "malloc");
        exit(EXIT_FAILURE);
    }
    memset(ctx, 0, sizeof(rie_test_ctx_t));

    ctx->pagerp = &pagerp;

    if (pthread_create(&rie_test_tid, NULL, rie_testing_thread, ctx) != 0) {
        rie_log_error0(errno, "failed to create test thread");
        exit(EXIT_FAILURE);
    }
}


static void *
rie_testing_thread(void *data)
{
    rie_test_ctx_t  *ctx = data;

    rie_testcase_t *tc;

    /* wait till main thread will start and initialize pager */
    while (*ctx->pagerp == NULL) {
        sched_yield();
    }

    /* XXX: also, give some time for events to be initialized */
    sleep(1);

    ctx->pager = *ctx->pagerp;

    while (ctx->pager->desktops.nitems == 0) {
        sched_yield();
    }

    for (tc = rie_testcases; tc->name; tc++) {

        if (tc->run(ctx->pager, tc) != RIE_OK) {
            tc->completed = 0;

        } else {
            tc->completed = 1;
            if (tc->passed) {
                ctx->passed++;

            } else {
                ctx->failed++;
            }
            ctx->completed++;
        }

        ctx->total++;
    }

    printf("Testcases:\n");
    for (tc = rie_testcases; tc->name; tc++) {
        printf("\t%s: ", tc->name);
        if (tc->completed && tc->passed) {
            printf("ok");
        } else {
            if (tc->completed) {
                printf("fail at %s:%d", tc->file, tc->line);
            } else {
                printf("incomplete");
            }
        }
        printf("\n");
    }

    printf("Testing results:\n\tTotal: %d\n\t"
           "Completed: %d\n\tPassed: %d\n\tFailed:%d\n",
           ctx->total,ctx->completed, ctx->passed, ctx->failed);

    free(ctx);

    exit(EXIT_SUCCESS);

    return NULL;
}


static int
rie_test_exec(char *fmt, ...)
{
    int       size, rc;
    char     *p;
    va_list   ap;

    p = NULL;
    size = 0;

    /* Determine required size */

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (size < 0) {
        rie_log_error0(errno, "vsnprintf");
        return RIE_ERROR;
    }

    size++; /* for terminating zero */
    p = malloc(size);
    if (p == NULL) {
        rie_log_error0(errno, "malloc");
        return RIE_ERROR;
    }

    va_start(ap, fmt);
    size = vsnprintf(p, size, fmt, ap);
    if (size < 0) {
        free(p);
        rie_log_error0(errno, "vsnprintf");
        return RIE_ERROR;
    }
    va_end(ap);

    rc = system(p);

    if (rc == -1) {
        rie_log_error0(errno, "system() failed to create child process");
        free(p);
        return RIE_ERROR;
    }

    if (WEXITSTATUS(rc) == 127) {
        rie_log_error(0, "system() failed to execute command '%s'", p);
        free(p);
        return RIE_ERROR;
    }

    free(p);
    return RIE_OK;
}


/* verify that changed number of desktops is noted */
static int
rie_testcase_ndesktops(rie_t *pager, rie_testcase_t *tc)
{
    uint32_t  n;

    /* current number of desktops */
    n = pager->desktops.nitems;

    /* add one more desktop using wmctrl */
    /* XXX: this method doesn't seem work in e16 - nothing happens */
    if (rie_test_exec("wmctrl -n %d", pager->desktops.nitems + 1) != RIE_OK) {
        return RIE_ERROR;
    }

    /* ensure number has changed (poll value for 1 second) */
    rie_test_poll_cond(tc, pager->desktops.nitems != n + 1, 1000);
    if (pager->desktops.nitems != n + 1) {
        rie_tc_failed(tc);
        return RIE_OK;
    }

    /* now switch number of desktops back to original */
    if (rie_test_exec("wmctrl -n %d", n) != RIE_OK) {
        return RIE_ERROR;
    }

    /* ensure number is changed accordingly */
    rie_test_poll_cond(tc, pager->desktops.nitems != n, 1000);
    if (pager->desktops.nitems != n) {
        rie_tc_failed(tc);
        return RIE_OK;
    }

    tc->passed = 1;
    return RIE_OK;
}


/* verify that current desktop change is noted */
static int
rie_testcase_current_desktop(rie_t *pager, rie_testcase_t *tc)
{
    int       i, rc;
    uint32_t  n, c;

    n = pager->desktops.nitems;
    c = pager->current_desktop;

    /* need at least 2 desktops */
    if (rie_test_exec("wmctrl -n %d", pager->desktops.nitems + 1) != RIE_OK) {
        return RIE_ERROR;
    }

    /* wait for change to happen */
    rie_test_poll_cond(tc, pager->desktops.nitems != n + 1, 1000);

    /* now try to switch to all desktops and ensure current desktop changed */
    for (i = 0; i < pager->desktops.nitems; i++) {
        if (rie_test_exec("wmctrl -s %d", i) != RIE_OK) {
            rc = RIE_ERROR;
            goto restore;
        }

        rie_test_poll_cond(tc, pager->current_desktop != i, 1000);
        if (pager->current_desktop != i) {
            rie_tc_failed(tc);
            rc = RIE_OK;
            goto restore;
        }
    }

    tc->passed = 1;
    rc = RIE_OK;

restore:

    /* now switch number of desktops back to original */
    if (rie_test_exec("wmctrl -n %d", n) != RIE_OK) {
        rc = RIE_ERROR;
    }

    /* and return current desktop */
    if (rie_test_exec("wmctrl -s %d", c) != RIE_OK) {
        rc = RIE_ERROR;
    }

    /* do not start other tests untill event will come back from X */
    rie_test_poll_cond(tc, pager->current_desktop != c, 1000);

    return rc;
}


/* move mouse and change desktops, triggering move and click events */
static int
rie_testcase_switch_desktop(rie_t *pager, rie_testcase_t *tc)
{
    int       i, rc;
    uint32_t  n, c, k;

    rie_rect_t      box, wbox, inner, *viewport;
    rie_desktop_t  *desk;

    xcb_window_t win, *vroot;

    n = pager->desktops.nitems;
    c = pager->current_desktop;

    /* need at least 2 desktops */
    if (rie_test_exec("wmctrl -n %d", pager->desktops.nitems + 1) != RIE_OK) {
        return RIE_ERROR;
    }

    /* wait for change to happen */
    rie_test_poll_cond(tc, pager->desktops.nitems != n + 1, 1000);

    viewport = rie_array_get(&pager->viewports, c, rie_rect_t);
    vroot = rie_array_get(&pager->virtual_roots, c, xcb_window_t);

    win = rie_xcb_get_window(pager->xcb);

    /* get coordinates of rieman window */
    if (rie_xcb_get_window_geometry(pager->xcb, &win , vroot, &wbox, viewport)
        != RIE_OK)
    {
        rc = RIE_ERROR;
        goto restore;
    }

    desk = pager->desktops.data;

    for (i = 0; i < pager->desktops.nitems; i++) {

        box = desk[i].dbox;

        /* middle of a current desktop */
        inner.x = wbox.x + box.x + box.w / 2;
        inner.y = wbox.y + box.y + box.h / 2;

        if (inner.x >= pager->desktop_geom.w
            || inner.y >= pager->desktop_geom.h)
        {
            /* we can't move mouse outside of screen, so this will have
             * no effect => just ignore desktops outside the screen
             */
            continue;
        }


        /* move mouse to center and click */
        if (rie_test_exec("xdotool mousemove --sync %d %d click 1",
                          inner.x, inner.y)
            != RIE_OK)
        {
            rc = RIE_ERROR;
            goto restore;
        }

        /* expecting desktop to change */
        rie_test_poll_cond(tc, pager->current_desktop != i, 1000);
        if (pager->current_desktop != i) {
            rie_log_error(0, "failed to switch to desktop %d "
                          "at (%d,%d) with mouse", i, inner.x, inner.y);
            rie_tc_failed(tc);
            rc = RIE_OK;
            goto restore;
        }

        /* pager must detect mouse in */
        if (pager->m_in != 1) {
            rie_tc_failed(tc);
            rc = RIE_OK;
            goto restore;
        }
    }

    k = pager->current_desktop;

    /* hit the border */
    if (rie_test_exec("xdotool mousemove --sync %d %d click 1",
                      wbox.x, wbox.y)
        != RIE_OK)
    {
        rc = RIE_ERROR;
        goto restore;
    }

    /* expecting desktop did not change */
    rie_test_poll_cond(tc, pager->current_desktop != k, 1000);
    if (pager->current_desktop != k) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }


    /* mouse leaves the window */
    if (rie_test_exec("xdotool mousemove --sync %d %d",
                      wbox.x + wbox.w + 10, wbox.y + wbox.h + 10)
        != RIE_OK)
    {
        rc = RIE_ERROR;
        goto restore;
    }

    /* pager must detect that mouse left */
    rie_test_poll_cond(tc, pager->m_in != 0, 1000);
    if (pager->m_in != 0) {
        rie_tc_failed(tc);
        rc = RIE_ERROR;
        goto restore;
    }

    tc->passed = 1;
    rc = RIE_OK;

restore:

    /* now switch number of desktops back to original */
    if (rie_test_exec("wmctrl -n %d", n) != RIE_OK) {
        rc = RIE_ERROR;
    }

    /* and return current desktop */
    if (rie_test_exec("wmctrl -s %d", c) != RIE_OK) {
        rc = RIE_ERROR;
    }

    /* do not start other tests untill event will come back from X */
    rie_test_poll_cond(tc, pager->current_desktop != c, 1000);

    return rc;
}


/* screen resolution reaction test */
static int
rie_testcase_screenetry(rie_t *pager, rie_testcase_t *tc)
{
    int       rc;
    uint32_t  w,h;

    w = pager->desktop_geom.w;
    h = pager->desktop_geom.h;

    /* change resolution to other (and hope it doesn't match and available) */
    if (rie_test_exec("xrandr -s 800x600") != RIE_OK) {
        return RIE_ERROR;
    }

    /* expecting geometry to change */
    /* XXX: test won't work in cases where NET_DESKTOP_GEOMETRY
     *      is not tied to resolution (e16)
     *
     *      another method to change desktop geometry is needed, not just
     *      switch resolution
     */
    rie_test_poll_cond(tc, pager->desktop_geom.w != 800, 1000);
    if (pager->desktop_geom.w != 800) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }

    rie_test_poll_cond(tc, pager->desktop_geom.h != 600, 1000);
    if (pager->desktop_geom.h != 600) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }

    tc->passed = 1;
    rc = RIE_OK;

restore:

    /* restore original */
    if (rie_test_exec("xrandr -s %dx%d", w, h) != RIE_OK) {
        return RIE_ERROR;
    }

    /* do not start other tests untill event will come back from X */
    rie_test_poll_cond(tc, pager->desktop_geom.w != w, 1000);

    return rc;
}


int
rie_test_get_desktop_names(rie_xcb_t *xcb, rie_array_t *names)
{
    int  rc;

    rie_array_t  res;

    xcb_generic_error_t                *err;
    xcb_get_property_cookie_t           cookie;
    xcb_ewmh_get_utf8_strings_reply_t   reply;

    cookie = xcb_ewmh_get_desktop_names(rie_xcb_ewmh(xcb), rie_xcb_screen(xcb));

    rc = xcb_ewmh_get_desktop_names_reply(rie_xcb_ewmh(xcb), cookie, &reply, &err);
    if (rc == 0) {
        return rie_xcb_handle_error0(err, "xcb_ewmh_get_desktop_names");
    }

    rc = rie_str_list_to_array(reply.strings, reply.strings_len, &res);

    xcb_ewmh_get_utf8_strings_reply_wipe(&reply);

    if (rc != RIE_OK) {
        return RIE_ERROR;
    }

    if (names->data) {
        rie_array_wipe(names);
    }

    *names = res;

    return rc;
}


static int
rie_testcase_desktop_name_change(rie_t *pager, rie_testcase_t *tc)
{
    int         rc;
    char      **str;
    uint32_t    i, len, n;

    rie_array_t           desktop_names;
    xcb_void_cookie_t     cookie;
    xcb_generic_error_t  *error;

#define RIE_TC_DESKTOP_NAMES  "one\0two\0"

    const char strings[] = RIE_TC_DESKTOP_NAMES;

    n = pager->desktops.nitems;

    /* need at least 2 desktops */
    if (rie_test_exec("wmctrl -n %d", pager->desktops.nitems + 1) != RIE_OK) {
        return RIE_ERROR;
    }

    /* wait for change to happen */
    rie_test_poll_cond(tc, pager->desktops.nitems != n + 1, 1000);

    memset(&desktop_names, 0, sizeof(rie_array_t));

    /* save original desktop names */
    if (rie_test_get_desktop_names(pager->xcb, &desktop_names) != RIE_OK) {
        return RIE_ERROR;
    }

    /* set desktop names to test strings */
    cookie = xcb_ewmh_set_desktop_names(rie_xcb_ewmh(pager->xcb),
                                        rie_xcb_screen(pager->xcb),
                                        sizeof(RIE_TC_DESKTOP_NAMES) - 1,
                                        strings);

    error = xcb_request_check(rie_xcb_get_connection(pager->xcb), cookie);
    if (error != NULL) {
        return rie_xcb_handle_error0(error, "xcb_ewmh_set_desktop_names");
    }

    /* wait until rieman will receive the event in event loop and process it */
    sleep(1);

    str = (char **) pager->desktop_names.data;
    if (strcmp(str[0], "one") != 0) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }

    if (strcmp(str[1], "two") != 0) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }

    tc->passed = 1;
    rc = RIE_OK;

restore:

    /* now switch number of desktops back to original */
    if (rie_test_exec("wmctrl -n %d", n) != RIE_OK) {
        rc = RIE_ERROR;
    }

    str = (char **) desktop_names.data;
    for (i = 0, len = 0; i < desktop_names.nitems; i++) {
        len += strlen(str[i]) + 1;
    }

    cookie = xcb_ewmh_set_desktop_names(rie_xcb_ewmh(pager->xcb), rie_xcb_screen(pager->xcb),
                                        len, str[0]);

    error = xcb_request_check(rie_xcb_get_connection(pager->xcb), cookie);
    if (error != NULL) {
        (void) rie_xcb_handle_error0(error, "xcb_ewmh_set_desktop_names");
        rc = RIE_ERROR;
    }

    rie_array_wipe(&desktop_names);

    return rc;
}


static int
rie_testcase_window_states(rie_t *pager, rie_testcase_t *tc)
{
    int             rc;
    uint32_t        n;
    rie_rect_t      box, wbox, inner, *viewport;
    xcb_window_t    win, *vroot;
    rie_desktop_t  *desk;

    desk = rie_array_get(&pager->desktops, pager->current_desktop, rie_desktop_t);

    /* window to manipulate */
    if (rie_test_exec(TEST_APP" &") != RIE_OK) {
        return RIE_ERROR;
    }

    /* let the app to start */
    sleep(1);

    n = desk->nhidden;

    /* minimize */
    if (rie_test_exec("xdotool search --onlyvisible --classname "
                      TEST_APP_CLASS" windowminimize")
        != RIE_OK)
    {
        return RIE_ERROR;
    }

    /* wait for change to happen */
    rie_test_poll_cond(tc, desk->nhidden != n + 1, 1000);
    if (desk->nhidden != n + 1) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }

    /* now imitate click on hidden window icon */

    viewport = rie_array_get(&pager->viewports, pager->current_desktop, rie_rect_t);
    vroot = rie_array_get(&pager->virtual_roots, pager->current_desktop, xcb_window_t);

    win = rie_xcb_get_window(pager->xcb);

    /* get coordinates of rieman window */
    if (rie_xcb_get_window_geometry(pager->xcb, &win , vroot, &wbox, viewport)
        != RIE_OK)
    {
        rc = RIE_ERROR;
        goto restore;
    }

    /* mouse move/click to restore window */
    box = desk->pad;

    /* 1st hidden window icon coords */
    inner.x = wbox.x + box.x + 2; /* TODO: skin margin here */
    inner.y = wbox.y + box.y + box.h / 2;

    if (inner.x >= pager->desktop_geom.w
        || inner.y >= pager->desktop_geom.h)
    {
        /* we can't move mouse outside of screen, so this will have
         * no effect => just ignore desktops outside the screen
         */
        rc = RIE_ERROR;
        goto restore;
    }

    /* move mouse to center and click */
    if (rie_test_exec("xdotool mousemove --sync %d %d click 1",
                      inner.x, inner.y)
        != RIE_OK)
    {
        rc = RIE_ERROR;
        goto restore;
    }

    /* wait for window to restore */
    rie_test_poll_cond(tc, desk->nhidden != n, 1000);
    if (desk->nhidden != n) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }

    tc->passed = 1;
    rc = RIE_OK;

restore:

    (void) rie_test_exec("killall "TEST_APP);

    return rc;
}


static int
rie_testcase_geometry_fallback(rie_t *pager, rie_testcase_t *tc)
{
    int           rc;
    uint32_t     *data;
    rie_array_t   res;

    /* preserve original settings of property */
    memset(&res, 0, sizeof(rie_array_t));

    rc = rie_xcb_property_get_array(pager->xcb, rie_xcb_get_root(pager->xcb),
                                     RIE_NET_DESKTOP_GEOMETRY,
                                     XCB_ATOM_CARDINAL, &res);
    if (rc != RIE_OK) {
        return RIE_ERROR;
    }

    data = res.data;

    /* delete the property and cleanup previously obtained settings */
    pager->desktop_geom.w = 0;
    pager->desktop_geom.h = 0;

    rc = rie_xcb_property_delete(pager->xcb, rie_xcb_get_root(pager->xcb),
                                 RIE_NET_DESKTOP_GEOMETRY);
    if (rc != RIE_OK) {
        rie_array_wipe(&res);
        return RIE_ERROR;
    }

    /* wait for pager to be triggered by property change */
    rie_test_poll_cond(tc, pager->desktop_geom.w != data[0], 1000);
    if (pager->desktop_geom.w != data[0]) {
        rie_tc_failed(tc);
        rc = RIE_OK;
        goto restore;
    }

    tc->passed = 1;
    rc = RIE_OK;

restore:

    (void) rie_xcb_property_set_array(pager->xcb, rie_xcb_get_root(pager->xcb),
                                      RIE_NET_DESKTOP_GEOMETRY,
                                      XCB_ATOM_CARDINAL, &res);
    rie_array_wipe(&res);

    return rc;
}


static int
rie_testcase_window_change_desktop(rie_t *pager, rie_testcase_t *tc)
{
    int             rc, i;
    uint32_t        newd;
    rie_window_t   *win;

    /* window to manipulate */
    if (rie_test_exec(TEST_APP" &") != RIE_OK) {
        return RIE_ERROR;
    }

    /* let the app to start */
    sleep(1);

    win = pager->windows.data;
    for (i = 0; i < pager->windows.nitems; i++) {
        if (strcmp(win[i].name, TEST_APP) == 0) {

            if (win[i].desktop != pager->current_desktop) {
                rie_log_error0(errno, "executed "TEST_APP" window is "
                               "not on current desktop");
                rc = RIE_ERROR;
                goto restore;

            } else {
                goto found;
            }
        }
    }

    rie_log_error0(errno, "executed "TEST_APP" window not found");
    return RIE_ERROR;

found:

    newd = pager->current_desktop + 1;

    /* minimize */
    rc = rie_test_exec("xdotool search --onlyvisible --classname "
                      TEST_APP_CLASS" set_desktop_for_window %d", newd);
    if (rc != RIE_OK) {
        goto restore;
    }

    /* wait for change to happen */
    sleep(1);

    /* need to find window again, because it could be re-allocated */
    win = pager->windows.data;
    for (i = 0; i < pager->windows.nitems; i++) {
        if (strcmp(win[i].name, TEST_APP) == 0) {
            if (win[i].desktop != newd) {
                rie_tc_failed(tc);
                rc = RIE_OK;

            } else {
                tc->passed = 1;
                rc = RIE_OK;
            }
        }
    }

restore:

    (void) rie_test_exec("killall "TEST_APP);

    return rc;
}
