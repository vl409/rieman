
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

#include <stdlib.h>
#include <stdio.h>


int
rie_external_get_desktop_geometry(uint32_t *w, uint32_t *h)
{
    /* assume xdpyinfo is in PATH, runnable, etc... */
    static char *cmd = "xdpyinfo |awk '/dimension/ { print $2\"x\" }'";

    FILE  *fp;
    char   buf[16], *token;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        rie_log_error0(errno, "failed to run xdpyinfo");
        return RIE_ERROR;
    }

    if (fgets(buf, 16, fp) == NULL) {
        pclose(fp);
        return RIE_ERROR;
    }

    pclose(fp);

    /* simplistic parsing, assuming result is like '800x600x' */

    token = strtok(buf, "x");
    if (token == NULL) {
        rie_log_error0(0, "failed to parse results of xdpyinfo");
        return RIE_ERROR;
    }

    *w = atoi(token);

    token = strtok(NULL, "x");
    if (token == NULL) {
        rie_log_error0(0, "failed to parse results of xdpyinfo");
        return RIE_ERROR;
    }

    *h = atoi(token);

    return RIE_OK;
}
