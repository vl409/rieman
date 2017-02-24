
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

#ifndef __RIE_UTIL_H__
#define __RIE_UTIL_H__


/* get pointer to i-th element of array if present, or to first */
#define rie_array_get(arr, i, type)                     \
    (i) < (arr)->nitems ? &(((type *)(arr)->data)[i])   \
                        : ((type *)((arr)->data))

typedef struct rie_array_s  rie_array_t;

typedef void (*rie_array_free_pt)(void *data, size_t nitems);

struct rie_array_s {
    void                  *data;
    size_t                 nitems;
    rie_array_free_pt      xfree;
};

int rie_array_init(rie_array_t *array, size_t nitems, size_t item_len,
    rie_array_free_pt free_func);

void rie_array_wipe(rie_array_t *array);

int rie_str_list_to_array(char *in, size_t len, rie_array_t *res);

char *rie_mkpath(char *p1,...);

#if defined(RIE_DEBUG)
int rie_backtrace_save(rie_array_t *bt);
#endif

#endif
