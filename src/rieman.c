
/*
 * Copyright (C) 2017-2020 Vladimir Homutov
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
#include "rie_config.h"
#include "rie_xcb.h"
#include "rie_skin.h"
#include "rie_external.h"

#include <stdio.h>
#include <limits.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>


static rie_conf_map_t rie_conf_layers[] = {
    { "below", RIE_WIN_STATE_BELOW },
    { "normal", 0 },
    { "above", RIE_WIN_STATE_ABOVE },
    { NULL, 0 }
};

static rie_conf_map_t rie_conf_orientations[] = {
    { "horizontal", XCB_EWMH_WM_ORIENTATION_HORZ },
    { "vertical", XCB_EWMH_WM_ORIENTATION_VERT },
    { NULL, 0 }
};

static rie_conf_map_t rie_conf_positions[] = {
    { "topleft", RIE_POS_TOPLEFT },
    { "topright", RIE_POS_TOPRIGHT },
    { "bottomleft", RIE_POS_BOTTOMLEFT },
    { "bottomright", RIE_POS_BOTTOMRIGHT },
    { NULL, 0 }
};

static rie_conf_map_t rie_conf_corners[] = {
    { "topleft", XCB_EWMH_WM_TOPLEFT },
    { "topright", XCB_EWMH_WM_TOPRIGHT },
    { "bottomleft", XCB_EWMH_WM_BOTTOMLEFT },
    { "bottomright", XCB_EWMH_WM_BOTTOMRIGHT },
    { NULL, 0 }
};

static rie_conf_map_t rie_conf_pad_positions[] = {
    { "below", RIE_PAD_POS_BELOW },
    { "above", RIE_PAD_POS_ABOVE },
    { NULL, 0 }
};

static rie_conf_map_t rie_conf_buttons[] = {
    { "left", XCB_BUTTON_MASK_1 },
    { "middle", XCB_BUTTON_MASK_2 },
    { "right", XCB_BUTTON_MASK_3 },
    { NULL, 0 }
};

static rie_conf_map_t rie_conf_labels[] = {
    { "number", RIE_DESKTOP_NUMBER },
    { "name",  RIE_DESKTOP_NAME },
    { NULL, 0 }
};


static rie_conf_item_t rie_conf[] = {

    { "/rieman-conf/geometry/@width", RIE_CTYPE_UINT32, "100",
      offsetof(rie_settings_t, desktop.w), NULL, { NULL } },

    { "/rieman-conf/geometry/@height", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, desktop.h), NULL, { NULL } },

    { "/rieman-conf/layout/@wrap", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, desktop.wrap), NULL, { NULL } },

    { "/rieman-conf/layout/@corner", RIE_CTYPE_STR, "topleft",
      offsetof(rie_settings_t, desktop.corner),
      rie_conf_set_variants, { &rie_conf_corners } },

    { "/rieman-conf/layout/@orientation", RIE_CTYPE_STR, "horizontal",
      offsetof(rie_settings_t, desktop.orientation),
      rie_conf_set_variants, { &rie_conf_orientations } },


    { "/rieman-conf/appearance/skin/@name", RIE_CTYPE_STR,  "default",
      offsetof(rie_settings_t, skin), NULL, { NULL } },

    { "/rieman-conf/appearance/desktop_pad/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, show_pad), NULL, { NULL } },

   { "/rieman-conf/appearance/desktop_pad/@position", RIE_CTYPE_STR, "below",
      offsetof(rie_settings_t, pad_position),
      rie_conf_set_variants, { &rie_conf_pad_positions } },

   { "/rieman-conf/appearance/desktop_pad/@margin", RIE_CTYPE_UINT32, "2",
      offsetof(rie_settings_t, pad_margin), NULL, { NULL } },

    { "/rieman-conf/appearance/desktop_text/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, show_text), NULL, { NULL } },

    { "/rieman-conf/appearance/desktop_text/@content", RIE_CTYPE_STR, "number",
      offsetof(rie_settings_t, desktop.content),
      rie_conf_set_variants, { &rie_conf_labels } },

    { "/rieman-conf/appearance/window_icon/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, show_window_icons), NULL, { NULL } },

    { "/rieman-conf/appearance/viewports/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, show_viewports), NULL, { NULL } },

    { "/rieman-conf/appearance/minitray/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, show_minitray), NULL, { NULL } },

    { "/rieman-conf/window/withdrawn/@enable", RIE_CTYPE_BOOL, "false",
      offsetof(rie_settings_t, withdrawn), NULL, { NULL } },

    { "/rieman-conf/window/skip_taskbar/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, skip_taskbar), NULL, { NULL } },

    { "/rieman-conf/window/skip_pager/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, skip_pager), NULL, { NULL } },

    { "/rieman-conf/window/sticky/@enable", RIE_CTYPE_BOOL, "true",
      offsetof(rie_settings_t, sticky), NULL, { NULL } },

    { "/rieman-conf/window/layer/@value", RIE_CTYPE_STR, "normal",
      offsetof(rie_settings_t, layer),
      rie_conf_set_variants, { &rie_conf_layers } },

    { "/rieman-conf/window/position/@value", RIE_CTYPE_STR, "topleft",
      offsetof(rie_settings_t, position),
      rie_conf_set_variants, { &rie_conf_positions } },


    { "/rieman-conf/actions/change_desktop/@mouse_button", RIE_CTYPE_STR, "left",
      offsetof(rie_settings_t, change_desktop_button),
      rie_conf_set_variants, { &rie_conf_buttons } },

    /* configuration schema 1.1 */

    { "/rieman-conf/actions/tile_windows/@mouse_button", RIE_CTYPE_STR, "right",
      offsetof(rie_settings_t, tile_button),
      rie_conf_set_variants, { &rie_conf_buttons } },

    /* configuration schema 1.2 */

    { "/rieman-conf/window/dock/@enable", RIE_CTYPE_BOOL, "false",
      offsetof(rie_settings_t, docked), NULL, { NULL } },

    { "/rieman-conf/window/struts/@enable", RIE_CTYPE_BOOL, "false",
      offsetof(rie_settings_t, struts.enabled), NULL, { NULL } },

    { "/rieman-conf/window/struts/@left", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.left), NULL, { NULL } },

    { "/rieman-conf/window/struts/@left_start_y", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.left_start_y), NULL, { NULL } },

    { "/rieman-conf/window/struts/@left_end_y", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.left_end_y), NULL, { NULL } },

    { "/rieman-conf/window/struts/@right", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.right), NULL, { NULL } },

    { "/rieman-conf/window/struts/@right_start_y", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.right_start_y), NULL, { NULL } },

    { "/rieman-conf/window/struts/@right_end_y", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.right_end_y), NULL, { NULL } },

    { "/rieman-conf/window/struts/@top", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.top), NULL, { NULL } },

    { "/rieman-conf/window/struts/@top_start_x", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.top_start_x), NULL, { NULL } },

    { "/rieman-conf/window/struts/@top_end_x", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.top_end_x), NULL, { NULL } },

    { "/rieman-conf/window/struts/@bottom", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.bottom), NULL, { NULL } },

    { "/rieman-conf/window/struts/@bottom_start_x", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.bottom_start_x), NULL, { NULL } },

    { "/rieman-conf/window/struts/@bottom_end_x", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, struts.bottom_end_x), NULL, { NULL } },

    { "/rieman-conf/control/@socket", RIE_CTYPE_STR, "",
      offsetof(rie_settings_t, control_socket_path), NULL, { NULL } },

    { "/rieman-conf/window/position/@dx", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, pos_x_offset), NULL, { NULL } },

    { "/rieman-conf/window/position/@dy", RIE_CTYPE_UINT32, "0",
      offsetof(rie_settings_t, pos_y_offset), NULL, { NULL } },

    { NULL, 0, NULL, 0, NULL, { NULL } }
};


#if defined(RIE_TESTS)
rie_t    *pagerp;
#endif

uint8_t   rie_withdraw;
uint8_t   rie_quit;
uint8_t   rie_reload;


int
rie_pager_init(rie_t *pager, rie_t *oldpager)
{
    pager->selected_desktop = -1;
    pager->selected_vp.x = -1;
    pager->selected_vp.y = -1;
    pager->m_x = -1;
    pager->m_y = -1;

    if (oldpager) {
        pager->xcb = oldpager->xcb;
        pager->log = oldpager->log;

    } else {
        pager->xcb = rie_xcb_new(pager->cfg);
        if (pager->xcb == NULL) {
            return RIE_ERROR;
        }
    }

    if (rie_xcb_set_desktop_layout(pager->xcb, pager->cfg) != RIE_OK) {
        return RIE_ERROR;
    }

    pager->gfx = rie_gfx_new(pager->xcb);
    if (pager->gfx == NULL) {
        return RIE_ERROR;
    }

    pager->skin = rie_skin_new(pager->cfg->skin, pager->gfx);
    if (pager->skin == NULL) {
        return RIE_ERROR;
    }

    if (rie_event_init(pager) != RIE_OK) {
        return RIE_ERROR;
    }

    if (rie_xcb_set_window_hints(pager->xcb, pager->cfg, pager->current_desktop)
        != RIE_OK)
    {
        return RIE_ERROR;
    }

    if (strlen(pager->cfg->control_socket_path)) {
        pager->ctl = rie_control_new(pager->cfg, pager);
        if (pager->ctl == NULL) {
            return RIE_ERROR;
        }
    }

    return RIE_OK;
}


static void
rie_switch_desktop(rie_t *pager, rie_command_t direction)
{
    rie_desktop_t  *desk;
    int32_t         left, right, up, down, curr;

    desk = pager->desktops.data;

    curr = pager->current_desktop;
    desk = &desk[curr];

    left = right = up = down = 0;

    if (direction == RIE_CMD_SWITCH_DESKTOP_PREV) {
        if (curr == 0) {
            return;
        }
        rie_xcb_set_desktop(pager->xcb, curr - 1);
        return;
    }

    if (direction == RIE_CMD_SWITCH_DESKTOP_NEXT) {
        if (curr == pager->desktops.nitems - 1) {
            return;
        }
        rie_xcb_set_desktop(pager->xcb, curr + 1);
        return;
    }

    if (pager->cfg->desktop.orientation == XCB_EWMH_WM_ORIENTATION_HORZ) {

        switch (pager->cfg->desktop.corner) {
        case XCB_EWMH_WM_TOPLEFT:
        case XCB_EWMH_WM_BOTTOMLEFT:
            left = curr - 1;
            right = curr + 1;
            break;

        case XCB_EWMH_WM_TOPRIGHT:
        case XCB_EWMH_WM_BOTTOMRIGHT:
            left = curr + 1;
            right = curr - 1;
            break;
        default:
            break;
        }

        switch (pager->cfg->desktop.corner) {
        case XCB_EWMH_WM_TOPLEFT:
        case XCB_EWMH_WM_TOPRIGHT:
            up = curr - pager->ncols;
            down = curr + pager->ncols;
            break;

        case XCB_EWMH_WM_BOTTOMRIGHT:
        case XCB_EWMH_WM_BOTTOMLEFT:
            up = curr + pager->ncols;
            down = curr - pager->ncols;
            break;
        default:
            break;
        }

    } else { /* XCB_EWMH_WM_ORIENTATION_VERT */

        switch (pager->cfg->desktop.corner) {
        case XCB_EWMH_WM_TOPLEFT:
        case XCB_EWMH_WM_TOPRIGHT:
            up = curr - 1;
            down = curr + 1;
            break;

        case XCB_EWMH_WM_BOTTOMRIGHT:
        case XCB_EWMH_WM_BOTTOMLEFT:
            up = curr + 1;
            down = curr - 1;
            break;
        default:
            break;
        }

        switch (pager->cfg->desktop.corner) {
        case XCB_EWMH_WM_TOPLEFT:
        case XCB_EWMH_WM_BOTTOMLEFT:
            left = curr - pager->nrows;
            right = curr + pager->nrows;
            break;

        case XCB_EWMH_WM_TOPRIGHT:
        case XCB_EWMH_WM_BOTTOMRIGHT:
            left = curr + pager->nrows;
            right = curr - pager->nrows;
            break;
        default:
            break;
        }
    }

    switch (direction) {
    case RIE_CMD_SWITCH_DESKTOP_LEFT:
        if (desk->lcol == 0) {
            return;
        }
        rie_xcb_set_desktop(pager->xcb, left);

        break;
    case RIE_CMD_SWITCH_DESKTOP_RIGHT:
        if (desk->lcol == pager->ncols - 1) {
            return;
        }
        rie_xcb_set_desktop(pager->xcb, right);

    break;

    case RIE_CMD_SWITCH_DESKTOP_UP:
        if (desk->lrow == 0) {
            return;
        }
        rie_xcb_set_desktop(pager->xcb, up);

        break;
    case RIE_CMD_SWITCH_DESKTOP_DOWN:
        if (desk->lrow == pager->nrows - 1) {
            return;
        }
        rie_xcb_set_desktop(pager->xcb, down);

        break;

    default:
        /* make compiler happy with incomplete enum switch */
        break;
    }
}


void
rie_pager_run_cmd(rie_t *pager, rie_command_t cmd)
{
    switch (cmd) {

    case RIE_CMD_RELOAD:
        rie_reload = 1; /* to be handled by event loop */
        break;

    case RIE_CMD_EXIT:
        rie_quit = 1; /* to be handled by event loop */
        break;

    case RIE_CMD_SWITCH_DESKTOP_LEFT:
    case RIE_CMD_SWITCH_DESKTOP_RIGHT:
    case RIE_CMD_SWITCH_DESKTOP_UP:
    case RIE_CMD_SWITCH_DESKTOP_DOWN:
    case RIE_CMD_SWITCH_DESKTOP_PREV:
    case RIE_CMD_SWITCH_DESKTOP_NEXT:
        rie_switch_desktop(pager, cmd);
        break;
    case RIE_CMD_TILE_CURRENT_DESKTOP:
        pager->render = 1;
        (void) rie_windows_tile(pager, pager->current_desktop);
        break;
    }
}


void
rie_pager_delete(rie_t *pager, int final)
{
    rie_event_cleanup(pager);
    rie_skin_delete(pager->skin);
    rie_gfx_delete(pager->gfx);
    rie_control_delete(pager->ctl);
    rie_conf_cleanup(&pager->cfg->meta, pager->cfg);

    if (final) {
        rie_xcb_delete(pager->xcb);
        rie_log_delete(pager->log);
    }

    free(pager->cfg);
    free(pager);
}


void
rie_usage(char *p)
{
    fprintf(stdout, "Usage: %s [-h] [-v[v]] [-c <config>]"
                    " [-w] [-l] [-m <sockpath> <msg>]\n", p);
}


void
rie_version(int verbose)
{
    if (!verbose) {
        fprintf(stdout, "rieman "RIEMAN_VERSION"\n");
        return;
    }

    fprintf(stdout, "Rieman " RIEMAN_VERSION
" Copyright (c) 2017-2019  Vladimir Homutov\n"
"\n"
"This program comes with ABSOLUTELY NO WARRANTY.\n"
"This is free software, and you are welcome to redistribute it\n"
"under certain conditions; see the file COPYING for details.\n\n"

"Configured options:\n"
                    " · prefix:  \"" RIE_PREFIX "\"\n"
                    " · bindir:  \"" RIE_BINDIR  "\"\n"
                    " · mandir:  \"" RIE_MANDIR  "\"\n"
                    " · docdir:  \"" RIE_DOCDIR  "\"\n"
                    " · datadir: \"" RIE_DATADIR  "\"\n"
#if defined(RIE_DEBUG)
                    " · debug:   yes\n"
#else
                    " · debug:   no\n"
#endif

#if defined(RIE_TESTS)
                    " · tests:   yes\n"
#else
                    " · tests:   no\n"
#endif

                    "\nSources used:\n"
#if defined(RIE_REV)
                    " · git revision: \"" RIE_REV  "\"\n"
#else
                    " · tarball with version: \"" RIEMAN_VERSION  "\"\n"
#endif
                    "\nBuilt with:\n"
                    " · compiler: \"" RIE_CC "\"\n"
#if defined(__VERSION__)
                    " · compiler version: \"" __VERSION__ "\"\n"
#endif
                    " · build host: \"" RIE_BUILD_OS "\"\n"
                    "\nCompilation flags:\n"
                    " · CFLAGS: \"" RIE_CFLAGS "\"\n"
                    " · LDLAGS: \"" RIE_LDFLAGS "\"\n"
           "\n");
}


static void
rie_signal_handler(int sig_num)
{
    switch (sig_num) {
    case SIGTERM:
    case SIGINT:   /* CTRL-C */
        rie_quit = 1;
        break;
    case SIGUSR1:
        rie_reload = 1;
        break;
    default:
        /* should never happen */
        break;
    }
}


static int
rie_init_signals(sigset_t *ss)
{
    int  rc;

    struct sigaction  sa;

    sa.sa_flags = 0;
    sa.sa_handler = rie_signal_handler;

    rc = sigaction(SIGINT, &sa, NULL);
    if (-1 == rc) {
        rie_log_error0(errno, "sigaction()");
        return RIE_ERROR;
    }

    rc = sigaction(SIGTERM, &sa, NULL);
    if (-1 == rc) {
        rie_log_error0(errno, "sigaction()");
        return RIE_ERROR;
    }

    rc = sigaction(SIGUSR1, &sa, NULL);
    if (-1 == rc) {
        rie_log_error0(errno, "sigaction()");
        return RIE_ERROR;
    }

    /* block all signals */
    rc = sigfillset(ss);
    if (-1 == rc) {
        rie_log_error0(errno, "sigfillset()");
        return RIE_ERROR;
    }

    rc = sigprocmask(SIG_BLOCK, ss, NULL);
    if (-1 == rc) {
        rie_log_error0(errno, "sigprocmask()");
        return RIE_ERROR;
    }

    /* prepare signal mask for pselect() */

    rc = sigdelset(ss, SIGTERM);
    if (-1 == rc) {
        rie_log_error0(errno, "sigdelset()");
        return RIE_ERROR;
    }

    rc = sigdelset(ss, SIGINT);
    if (-1 == rc) {
        rie_log_error0(errno, "sigdelset()");
        return RIE_ERROR;
    }

    rc = sigdelset(ss, SIGUSR1);
    if (-1 == rc) {
        rie_log_error0(errno, "sigdelset()");
        return RIE_ERROR;
    }

    return RIE_OK;
}


rie_t *
rie_pager_new(char *cfile, rie_log_t *log)
{
    rie_t           *pager;
    rie_settings_t  *cfg;

    cfg = malloc(sizeof(rie_settings_t));
    if (cfg == NULL) {
        return NULL;
    }

    pager = malloc(sizeof(rie_t));
    if (pager == NULL) {
        free(cfg);
        return NULL;
    }

    memset(cfg, 0, sizeof(rie_settings_t));
    memset(pager, 0, sizeof(rie_t));

    cfg->meta.version_min = 10;
    cfg->meta.version_max = 12;
    cfg->meta.spec = rie_conf;

    rie_log("using configuration file '%s'", cfile);

    if (rie_conf_load(cfile, &cfg->meta, cfg) != RIE_OK) {
        rie_log_error(0, "configuration load failed");
        free(cfg);
        free(pager);
        return NULL;
    }

    cfg->meta.conf_file = cfile;

    pager->cfg = cfg;
    pager->log = log;

#if defined(RIE_TESTS)
    pagerp = pager;
#endif

    return pager;
}


int
main(int argc, char *argv[])
{
    int         i;
    char       *cfile, *logfile, *sockpath, *msg;
    rie_t      *pager;
    sigset_t    sigmask;
    rie_log_t  *log;

    char conf_file[FILENAME_MAX];

    cfile = NULL;
    logfile = NULL;
    msg = NULL;
    sockpath = NULL;

    if (argc > 1) {

        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-h") == 0) {
                rie_usage(argv[0]);
                exit(EXIT_SUCCESS);
            }

            if (strcmp(argv[i], "-v") == 0) {
                rie_version(0);
                exit(EXIT_SUCCESS);
            }

            if (strcmp(argv[i], "-vv") == 0) {
                rie_version(1);
                exit(EXIT_SUCCESS);
            }

            if (strcmp(argv[i], "-l") == 0) {
                if (argc < (i + 2)) {
                    fprintf(stderr, "option \"-l\" requires argument\n");
                    rie_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                logfile = argv[i + 1];
                i++;
                continue;
            }

            if (strcmp(argv[i], "-c") == 0) {

                if (argc < (i + 2)) {
                    fprintf(stderr, "option \"-c\" requires argument\n");
                    rie_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }

                cfile = argv[i + 1];
                i++;
                continue;
            }

            if (strcmp(argv[i], "-w") == 0) {
                rie_withdraw = 1;
                continue;
            }

            if (strcmp(argv[i], "-m") == 0) {

                if (argc < (i + 3)) {
                    fprintf(stderr, "option \"-m\" requires 2 arguments\n");
                    rie_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }

                sockpath = argv[i + 1];
                msg = argv[i + 2];

                i += 2;

                continue;
            }

            fprintf(stderr, "Unknown argument \"%s\"\n", argv[i]);
            rie_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (msg) {
        if (rie_control_send_message(sockpath, msg) != RIE_OK) {
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }

    log = rie_log_new(logfile);
    if (log == NULL) {
        fprintf(stderr, "failed to create log\n");
        exit(EXIT_FAILURE);
    }

    rie_log("rieman ver.%s (%s) started...", RIEMAN_VERSION, RIE_REV);

    if (cfile == NULL) {
        if (rie_locate_config(&conf_file, "rieman.xml") != RIE_OK) {
            fprintf(stderr, "No usable configuration file found, exiting\n");
            goto failed;
        }
        cfile = conf_file;
    }

    pager = rie_pager_new(cfile, log);
    if (pager == NULL) {
        goto failed;
    }

    if (rie_withdraw) {
        /* commandline overrides any default or config file */
        pager->cfg->withdrawn = 1;
    }

    if (rie_init_signals(&sigmask) != RIE_OK) {
        goto failed;
    }

    if (rie_pager_init(pager, NULL) != RIE_OK) {
        fprintf(stderr, "configuration failed to initialize, exiting\n");
        goto failed;
    }

    rie_log(" *** rieman is running with PID %d ***", (int) getpid());

    rie_log(" *** starting event loop ***");

    return rie_event_loop(pager, &sigmask);

failed:

    fprintf(stderr, ">> rieman failed to start due to fatal error\n");
    rie_log_dump();

    return EXIT_FAILURE;
}
