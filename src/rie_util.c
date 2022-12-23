
/*
 * Copyright (C) 2017-2022 Vladimir Homutov
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
#include <stdarg.h>
#include <limits.h>
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>

#define RIE_BT_BUF_SIZE  64


int
rie_array_init(rie_array_t *array, size_t nitems, size_t item_len,
    rie_array_free_pt free_func)
{
    array->data = malloc(nitems * item_len);
    if (array->data == NULL) {
        return RIE_ERROR;
    }

    array->nitems = nitems;

    rie_memzero(array->data, nitems * item_len);

    array->xfree = free_func;

    return RIE_OK;
}

void
rie_array_wipe(rie_array_t *array)
{
    if (array->xfree) {
        array->xfree(array->data, array->nitems);
    }

    free(array->data);
    array->data = NULL;
}


static void
rie_util_free_str_list(void *data, size_t nitems)
{
    char **strings = data;
    free(strings[0]);
}


int
rie_str_list_to_array(char *in, size_t len, rie_array_t *res)
{
    int i, j, n;
    char *data, *p;
    char **items;

    n = 0;

    data = malloc(len + 1);
    if (data == NULL) {
        rie_log_error0(errno, "malloc");
        return RIE_ERROR;
    }

    memcpy(data, in, len);
    data[len] = 0;

    for (n = 0, i = 0; i < len; i++) {
        if (data[i] == 0) {
            n++;
        }
    }

    if (data[len - 1] != 0) {
        n++;
    }

    items = malloc(n * sizeof(char*));
    if (items == NULL) {
        rie_log_error0(errno, "malloc");
        return RIE_ERROR;
    }

    p = data;

    for (i = 0, j = 0; i < len; i++) {
        if (data[i] == 0) {
            items[j++] = p;
            p = &data[i + 1];
        }
    }

    if (data[len - 1] != 0) {
        items[j] = p;
    }

    res->data = items;
    res->nitems = n;

    res->xfree = rie_util_free_str_list;

    return RIE_OK;
}


char *
rie_mkpath(char *p1,...)
{
    char *item, *res, *p;
    size_t len;

    va_list  ap;

    va_start(ap, p1);

    len = sizeof("/") + strlen(p1);

    while ((item = va_arg(ap, char*))) {
        len += strlen(item) + 1; /* for slashes */
    }
    va_end(ap);

    if (len > FILENAME_MAX) {
        rie_log_error0(0, "resulting string is too big");
        return NULL;
    }

    res = malloc(len);
    if (res == NULL) {
        rie_log_error0(errno, "malloc");
        return NULL;
    }

    p = res;

    len = strlen(p1);
    p = (char *) memcpy(p, p1, len) + len;

    va_start(ap, p1);

    while ((item = va_arg(ap, char*))) {
        *p++ = '/';
        len = strlen(item);
        p = (char *) memcpy(p, item, len) + len;
    }
    va_end(ap);

    *p++ = 0;

    return res;
}


#if defined(RIE_DEBUG)

int
rie_backtrace_save(rie_array_t *bt)
{
    void  *buffer[RIE_BT_BUF_SIZE];

    if (bt->nitems) {
        free(bt->data);
    }

    bt->nitems = backtrace(buffer, RIE_BT_BUF_SIZE);
    bt->data = backtrace_symbols(buffer, bt->nitems);
    if (bt->data == NULL) {
        rie_log_error0(errno, "backtrace_symbols");
        bt->nitems = 0;
        return RIE_ERROR;
    }

    return RIE_OK;
}

#endif
