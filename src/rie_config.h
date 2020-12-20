
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

#ifndef __RIE_CONFIG_H__
#define __RIE_CONFIG_H__

#include "rieman.h"

#include <stdio.h>
#include <sys/types.h>          /* for off_t */

#define RIE_CONF_MAIN      0x1  /* this is the main configuration file */
#define RIE_CONF_SKIN      0x2  /* this is the skin configuration file */

typedef enum {
    RIE_CTYPE_STR,
    RIE_CTYPE_UINT32,
    RIE_CTYPE_DBL,
    RIE_CTYPE_BOOL,
} rie_conf_type_t;

typedef union {
    char     *str;
    uint32_t  number;
    double    dbl;
    void     *data;
} rie_conf_value_t;

typedef struct rie_conf_item_s rie_conf_item_t;

typedef int (*rie_conf_convert_pt)(rie_conf_item_t *spec, void *value,
                                   void *res, char *key);

struct rie_conf_item_s {
    char                  *name;    /* key, i.e. /root/some/thing  */
    rie_conf_type_t        type;    /* destination type */
    char                  *dflt;    /* default value as string */
    off_t                  offset;  /* memory offset in context */
    rie_conf_convert_pt    convert; /* convertor function for custom types */
    union {                         /* extra args for convertor */
        void              *ptr;
        uint32_t           u32;
    } data;
    int                    initialized;
};

typedef struct {
    char                  *str;
    uint32_t               number;
} rie_conf_map_t;

typedef struct {
    char                  *xpath;
    rie_conf_item_t       *spec;
} rie_conf_ref_t;


int rie_conf_byte_to_double(rie_conf_item_t *spec, void *value, void *res,
    char *key);
int rie_conf_set_variants(rie_conf_item_t *spec, void *value, void *res,
    char *key);
int rie_conf_set_variants_mask(rie_conf_item_t *spec, void *value, void *res,
    char *key);
int rie_conf_set_mask(rie_conf_item_t *spec, void *value, void *res,
    char *key);

int rie_locate_config(char (*found)[FILENAME_MAX], char *fname);
int rie_locate_skin(char (*found)[FILENAME_MAX], char *fname);

int rie_conf_load(char *conf_file, rie_conf_meta_t *meta, void *ctx);
void rie_conf_cleanup(rie_conf_meta_t *meta, void *ctx);

#endif
