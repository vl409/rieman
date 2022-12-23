
/*
 * Copyright (C) 2020-2022 Vladimir Homutov
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>


#define RIE_CMD_BUF  128
#define rie_cmd_name(str)  str, sizeof(str) - 1

struct rie_control_s {
    int                  fd;   /* socket descriptor */
    struct sockaddr_un   sa;
    char                *path;
    void                *data; /* context for pager */
};

typedef struct {
    const char          *name;
    size_t               len;
    rie_command_t        action;
} rie_cmd_desc_t;

static rie_cmd_desc_t  rie_control_messages[] = {
    { rie_cmd_name("reload"),                RIE_CMD_RELOAD},
    { rie_cmd_name("exit"),                  RIE_CMD_EXIT },
    { rie_cmd_name("switch_desktop_left"),   RIE_CMD_SWITCH_DESKTOP_LEFT },
    { rie_cmd_name("switch_desktop_right"),  RIE_CMD_SWITCH_DESKTOP_RIGHT },
    { rie_cmd_name("switch_desktop_up"),     RIE_CMD_SWITCH_DESKTOP_UP},
    { rie_cmd_name("switch_desktop_down"),   RIE_CMD_SWITCH_DESKTOP_DOWN },
    { rie_cmd_name("switch_desktop_prev"),   RIE_CMD_SWITCH_DESKTOP_PREV },
    { rie_cmd_name("switch_desktop_next"),   RIE_CMD_SWITCH_DESKTOP_NEXT },
    { rie_cmd_name("tile_current_desktop"),  RIE_CMD_TILE_CURRENT_DESKTOP },
    { NULL, 0, 0 }
};


rie_control_t *
rie_control_new(rie_settings_t *cfg, void *data)
{
    int             rc;
    size_t          len;
    rie_control_t  *ctl;

    len = strlen(cfg->control_socket_path);
    if (len - 1 >= sizeof(struct sockaddr_un)) {
        rie_log_error(0, "socket path is too long");
        return NULL;
    }

    ctl = malloc(sizeof(rie_control_t));
    if (ctl == NULL) {
        return NULL;
    }

    rie_memzero(ctl, sizeof(struct rie_control_s));

    ctl->data = data;
    ctl->path = cfg->control_socket_path;

    ctl->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (ctl->fd == -1) {
        rie_log_error(errno, "failed to create socket");
        free(ctl);
        return NULL;
    }

    rc = fcntl(ctl->fd, F_SETFL, O_NONBLOCK);
    if (rc == -1) {
        rie_log_error(errno, "failed set socket non-blocking");
        free(ctl);
        return NULL;
    }

    ctl->sa.sun_family = AF_UNIX;
    strcpy(ctl->sa.sun_path, ctl->path);

    (void) unlink(ctl->path);

    rc = bind(ctl->fd, (struct sockaddr *) &ctl->sa,
              sizeof(struct sockaddr_un));
    if (rc == -1) {
        rie_log_error(errno, "failed to bind socket");
        (void) close(ctl->fd);
        free(ctl);
        return NULL;
    }

    rie_log("listening for user commands at '%s'", ctl->path);

    return ctl;
}


/*
 * "-m" switch handler, exits immediately after this,
 * thus no cleanup, logs, etc
 */
int
rie_control_send_message(char *sockpath, char *msg)
{
    int        sock;
    ssize_t    n;
    socklen_t  slen;

    struct sockaddr_un  sa;

    if (strlen(msg) > sizeof(sa.sun_path) - 1) {
        fprintf(stderr, "socket name too long\n");
        return RIE_ERROR;
    }

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock == -1) {
        fprintf(stderr, "failed to create socket: %s\n",
                strerror(errno));
        return RIE_ERROR;
    }

    rie_memzero(&sa, sizeof(struct sockaddr_un));

    slen = sizeof(struct sockaddr_un);
    sa.sun_family = AF_UNIX;
    strcpy(sa.sun_path, sockpath);

    n = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *) &sa, slen);
    if (n == -1) {
        fprintf(stderr, "failed to send control message: %s\n",
                strerror(errno));
        return RIE_ERROR;
    }

    if (n != strlen(msg)) {
        fprintf(stderr, "short write while sending control message: %s\n",
               strerror(errno));
        return RIE_ERROR;
    }

    return RIE_OK;
}


int
rie_control_get_fd(rie_control_t *ctl)
{
    return ctl ? ctl->fd : -1;
}


int
rie_control_handle_socket_event(rie_control_t *ctl)
{
    int              found;
    ssize_t          n;
    rie_cmd_desc_t  *cmd;

    char  buf[RIE_CMD_BUF];

    do {

        errno = 0;
        n = recv(ctl->fd, buf, RIE_CMD_BUF, 0);

        if (errno == EAGAIN) {
            return RIE_OK;
        }

        if (n < 0) {
            rie_log_error(errno, "read() on control socket failed");
            return RIE_ERROR;
        }

        found = 0;

        for (cmd = rie_control_messages; cmd->name; cmd++) {
            if (n == cmd->len && strncmp(buf, cmd->name, n) == 0) {
                rie_log("remote command: \"%.*s\", running", (int) n, buf);
                rie_pager_run_cmd(ctl->data, cmd->action);
                found = 1;
                break;
            }
        }

        if (!found) {
            rie_log("unknown remote command: \"%.*s\", ignored", (int) n, buf);
        }

    } while (1);

    return RIE_OK;
}


void
rie_control_delete(rie_control_t *ctl, int final)
{
    if (ctl == NULL) {
        return;
    }

    if (ctl->fd != -1) {
        (void) close(ctl->fd);
    }

    if (final) {
        (void) unlink(ctl->path);
    }

    free(ctl);
}

