
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
#include "rie_config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>     /* open */
#include <unistd.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>

#define rie_conf_log_error(key, err) \
    rie_conf_log_error_real( __FILE__, __LINE__, key, err)

typedef int (*rie_conf_type_parse_pt)(char *str, rie_conf_value_t *res);

void rie_conf_quiet_stub(void *ctx, const char *msg, ...);

/* low level type convertors */
static int rie_conf_to_str(char* str, rie_conf_value_t *res);
static int rie_conf_to_uint32(char* str, rie_conf_value_t *res);
static int rie_conf_to_dbl(char* str, rie_conf_value_t *res);
static int rie_conf_to_bool(char* str, rie_conf_value_t *res);

static int rie_conf_set_value(rie_conf_type_t type, rie_conf_value_t *val,
    void *res);

static int rie_locate_file(char (*found)[FILENAME_MAX],
    char (*paths)[FILENAME_MAX], size_t nelts);

static int rie_conf_process_item(rie_conf_item_t *spec, void *ctx,
    char *value);
static int rie_conf_init_defaults(rie_conf_item_t *cf, void *ctx);
static int rie_conf_handle_key(char *key, char*arg,
    rie_conf_item_t *cf, void *ctx);
static int rie_conf_parse(char *filename, rie_conf_item_t *spec, void *ctx);


rie_conf_type_parse_pt rie_conf_type_parser[] = {
    rie_conf_to_str,     /* RIE_CTYPE_STR     */
    rie_conf_to_uint32,  /* RIE_CTYPE_UINT32  */
    rie_conf_to_dbl,     /* RIE_CTYPE_DBL     */
    rie_conf_to_bool,    /* RIE_CTYPE_BOOL    */
};

static void
rie_conf_log_error_real(char *file, int line, char *key, char *error)
{
    if (key) {
        rie_log_sys_error_real(file, line, 0, "%s while processing key '%s'",
                               error, key);

    } else {
        rie_log_sys_error_real(file, line, 0, "%s", error);
    }
}


void
rie_conf_quiet_stub(void *ctx, const char *msg, ...)
{
    /* nothing here */
}


static int
rie_conf_to_str(char *str, rie_conf_value_t *res)
{
    char  *p;

    p = strdup(str);
    if (p == NULL) {
        rie_log_error0(errno, "strdup");
        return RIE_ERROR;
    }

    res->str = p;

    return RIE_OK;
}


static int
rie_conf_to_uint32(char* str, rie_conf_value_t *res)
{
    long   val;
    char  *endptr;

    errno = 0;
    val = strtol(str, &endptr, 0);

    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
        || (errno != 0 && val == 0))
    {
        rie_log_error0(errno, "conversion to uint32 failed: strtol()");
        return RIE_ERROR;
    }

    if (endptr == str) {
        rie_log_error0(0, "conversion to uint32 failed: no digits");
        return RIE_ERROR;
    }

    if (val < 0) {
        rie_log_error0(0, "conversion to uint32 failed: negative value");
        return RIE_ERROR;
    }

    if (val > UINT32_MAX) {
        rie_log_error0(0, "conversion to uint32 failed: value too big");
        return RIE_ERROR;
    }

    res->number = (uint32_t) val;

    return RIE_OK;
}


static int
rie_conf_to_dbl(char* str, rie_conf_value_t *res)
{
    double   val;
    char    *endptr;

    errno = 0;
    val = strtod(str, &endptr);

    if (errno) {
        rie_log_error(errno, "conversion of '%s' to double failed: strtod()",
                      str);
        return RIE_ERROR;
    }

    if (endptr == str) {
        rie_log_error(0, "conversion of '%s' to double failed: no digits", str);
        return RIE_ERROR;
    }

    res->dbl = val;

    return RIE_OK;
}


static int
rie_conf_to_bool(char *str, rie_conf_value_t *res)
{
    if (strcasecmp(str, "true") == 0 || strcmp(str, "1") ==0) {
        res->number = 1;

    } else if (strcasecmp(str, "false") == 0 || strcmp(str, "0") == 0) {
        res->number = 0;

    } else {
        rie_log_error(0, "conversion to bool failed: unknown value '%s'", str);
        return RIE_ERROR;
    }

    return RIE_OK;
}


static int
rie_conf_set_value(rie_conf_type_t type, rie_conf_value_t *val, void *res)
{
    switch (type) {
    case RIE_CTYPE_STR:
        *((char **) res) = val->str;
        break;
    case RIE_CTYPE_BOOL:     /* bool is stored as uint32 */
    case RIE_CTYPE_UINT32:
        *((uint32_t *) res) = val->number;
        break;
    case RIE_CTYPE_DBL:
        *((double *) res) = val->dbl;
        break;
    default:
        /* unknown type, should not happen */
        return RIE_ERROR;
    }

    return RIE_OK;
}


static int
rie_locate_file(char (*found)[FILENAME_MAX], char (*paths)[FILENAME_MAX],
    size_t nelts)
{
    int          i, rc;
    struct stat  fileinfo;

    for (i = 0; i < nelts; i++) {

        rie_debug("trying file %s", paths[i]);

        rc = stat(paths[i], &fileinfo);
        if (rc == -1) {
            continue;
        }

        if ((fileinfo.st_mode & S_IFMT) != S_IFREG) {
            rie_log_error(0, "file %s is not a regular file", paths[i]);
            return RIE_ERROR;
        }

        strcpy(*found, paths[i]);
        return RIE_OK;
    }

    return RIE_ERROR;
}


int
rie_conf_set_variants(rie_conf_item_t *spec, void *value, void *res,
    char *key)
{
    int        i;
    char      *s;
    uint32_t  *out;

    rie_conf_map_t *map;

    out = res;
    s = *(char **)value;
    map = (rie_conf_map_t *) spec->data.ptr;

    for (i = 0; map[i].str; i++) {
        if (strcmp(map[i].str, s) == 0) {
            *out = map[i].number;
            free(s); /* no longer needed */
            return RIE_OK;
        }
    }

    rie_log_error(0, "invalid variant '%s' of key '%s'", s, key);
    free(s);

    return RIE_ERROR;
}


int
rie_locate_config(char (*found)[FILENAME_MAX], char *fname)
{
    int    last = 0;
    char  *home;

#if defined(RIE_TESTS)
    char locations[5][FILENAME_MAX] = { {0}, {0}, {0}, {0}, {0} };
    sprintf(locations[last++], "./tests/conf/%s", fname);
#else
    char locations[4][FILENAME_MAX] = { {0}, {0}, {0}, {0} };
#endif

    sprintf(locations[last++], "./conf/%s", fname);

    home = getenv("XDG_CONFIG_HOME");
    if (home) {
        sprintf(locations[last++], "%s/rieman/%s", home, fname);
    }

    home = getenv("HOME");
    if (home) {
       sprintf(locations[last++], "%s/.config/rieman/%s", home, fname);
    }

    sprintf(locations[last++], "%s/%s", RIE_DATADIR, fname);

    return rie_locate_file(found, locations, last);
}


int
rie_locate_skin(char (*found)[FILENAME_MAX], char *fname)
{
    int    last;
    char  *home;

    char locations[4][FILENAME_MAX] = { {0}, {0}, {0} };

    last = 0;

    sprintf(locations[last++], "./skins/%s/rieman_skin.conf", fname);

    home = getenv("XDG_DATA_HOME");
    if (home) {
        sprintf(locations[last++], "%s/rieman/skins/%s/rieman_skin.conf",
                home, fname);
    }

    home = getenv("HOME");
    if (home) {
       sprintf(locations[last++],
               "%s/.local/share/rieman/skins/%s/rieman_skin.conf",
               home, fname);
    }

    sprintf(locations[last++], "%s/skins/%s/rieman_skin.conf", RIE_DATADIR,
            fname);

    return rie_locate_file(found, locations, last);
}


static int
rie_conf_process_item(rie_conf_item_t *spec, void *ctx, char *value)
{
    int                rc;
    void              *res;
    rie_conf_value_t   box;

    /* convert string value of key into destination type */
    if (rie_conf_type_parser[spec->type](value, &box) != RIE_OK) {
        rie_conf_log_error(spec->name, "failed to convert key value");
        return RIE_ERROR;
    }

    res = (char *) ctx + spec->offset;

    /* save converted value into destination according to type */
    if (spec->convert) {
        rc = spec->convert(spec, &box.data, res, spec->name);

    } else {
        rc = rie_conf_set_value(spec->type, &box, res);
    }

    if (rc != RIE_OK) {
        return RIE_ERROR;
    }

    spec->initialized = 1;

    return RIE_OK;

}


static int
rie_conf_init_defaults(rie_conf_item_t *cf, void *ctx)
{
    char             *value;
    rie_conf_item_t  *spec;

    for (spec = cf; spec->name; spec++) {
        if (spec->initialized) {
            continue;
        }

        if (spec->dflt == NULL) {
            rie_conf_log_error(spec->name,
                               "value not set in config and no default");
            return RIE_ERROR;
        }

        value = (char *) spec->dflt;

        if (rie_conf_process_item(spec, ctx, value) != RIE_OK) {
            return RIE_ERROR;
        }
    }

    return RIE_OK;
}


static int
rie_conf_handle_key(char *key, char *arg, rie_conf_item_t *cf, void *ctx)
{
    rie_conf_item_t  *spec;

    rie_debug("processing key \"%s\" arg \"%s\"", key, arg);

    for (spec = cf; spec->name; spec++) {
        if (strcmp(spec->name, key) == 0) {

            if (rie_conf_process_item(spec, ctx, arg) != RIE_OK) {
                return RIE_ERROR;
            }

            return RIE_OK;
        }
    }

    rie_log_error(0, "unknown configuration directive: \"%s\"", key);

    return RIE_ERROR;
}


static inline void
rie_conf_strip_tail(char *tail)
{
    char *p;
    p = tail - 1;

    while (isspace(*p)) {
        *p = 0;
        p--;
    }
}


static int
rie_conf_parse(char *filename, rie_conf_item_t *spec, void *ctx)
{
    int    fd;
    char  *buf, *p, *end, *key, *arg, *lpos, *errmsg;

    size_t       lnum;
    ssize_t      n;
    struct stat  sb;

    enum {
        ST_PRE,
        ST_KEY,
        ST_DIV,
        ST_ARG,
        ST_TAIL
    } state;

    buf = NULL;
    key = NULL;
    arg = NULL;
    errmsg = NULL;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        rie_log_error(errno, "open(\"%s\") failed", filename);
        return RIE_ERROR;
    }

    if (fstat(fd, &sb) == -1) {
        rie_log_error(errno, "fstat(\"%s\") failed", filename);
        goto failed;
    }

    buf = malloc(sb.st_size +1);
    if (buf == NULL) {
        rie_log_error(errno, "malloc");
        goto failed;
    }

    n = read(fd, buf, sb.st_size);
    if (n == -1) {
        rie_log_error(errno, "read(\"%s\") failed", filename);
        goto failed;
    }

    if (n != sb.st_size) {
        rie_log_error(0, "cannot read full file");
        goto failed;
    }

    p = buf;
    end = p + n;
    lnum = 1;
    lpos = p;

    state = ST_PRE;

    while (p < end) {

        switch (state) {
        case ST_PRE:

            if (*p == '#') {
                state = ST_TAIL;

            } else if (isalnum(*p) || ispunct(*p)) {
                state = ST_KEY;
                key = p;

            } else if (*p == '\n') {
                lnum++;
                lpos = p + 1;

            } else if (!(isspace(*p))) {
                errmsg = "unexpected symbol while looking for key";
                goto failed;
            }

            break;

        case ST_KEY:

            if (*p == '\n' || *p == '#') {
                errmsg = "key without value";
                goto failed;

            } else if (isspace(*p)) {
                *p = 0;
                state = ST_DIV;

            } else if (isalnum(*p) || ispunct(*p)) {
                /* consuming key... */

            } else {
                errmsg = "unexpected symbol while processing key";
                goto failed;
            }

            break;

        case ST_DIV:

            if (*p == '\n' || *p == '#') {
                errmsg = "key without value";
                goto failed;

            } else if (isalnum(*p) || ispunct(*p)) {
                state = ST_ARG;
                arg = p;

            } else if (!isspace(*p)) {
                errmsg = "unexpected symbol while looking for argument";
                goto failed;
            }

            break;

        case ST_ARG:

            if (*p == '\n') {
                state = ST_PRE;
                lnum++;
                lpos = p + 1;

                *p = 0;
                rie_conf_strip_tail(p);

                if (rie_conf_handle_key(key, arg, spec, ctx) != RIE_OK) {
                    goto failed;
                }

            } else if (*p == '#') {
                *p = 0;
                state = ST_TAIL;
                rie_conf_strip_tail(p);

                if (rie_conf_handle_key(key, arg, spec, ctx) != RIE_OK) {
                    goto failed;
                }
            } else if (isalnum(*p) || ispunct(*p) || isspace(*p)) {

                /* consuming key */

            } else {
                errmsg = "unexpected symbol while processing argument";
                goto failed;
            }

            break;

        case ST_TAIL:
            if (*p == '\n') {
                state = ST_PRE;
                lnum++;
                lpos = p + 1;
            }
            break;
        }

        p++;
    }

    switch (state) {
    case ST_PRE:
    case ST_TAIL:
        /* all good, nothing to do */
        break;

    case ST_KEY:
    case ST_DIV:
        errmsg = "key without value";
        goto failed;

    case ST_ARG:
        *p = 0;
        rie_conf_strip_tail(p);

        if (rie_conf_handle_key(key, arg, spec, ctx) != RIE_OK) {
            goto failed;
        }
        break;
    }

    (void) close(fd);
    free(buf);

    return RIE_OK;

failed:

    if (errmsg) {
        rie_log_error(0, "syntax error at line %ld pos %ld: %s",
                      lnum, p - lpos, errmsg);
    }

    close(fd);

    if (buf) {
        free(buf);
    }

    return RIE_ERROR;
}


int
rie_conf_load(char *conf_file, rie_conf_meta_t *meta, void *ctx)
{
    if (rie_conf_parse(conf_file, meta->spec, ctx) != RIE_OK) {
        rie_conf_cleanup(meta, ctx);
        return RIE_ERROR;
    }

    return rie_conf_init_defaults(meta->spec, ctx);
}


void
rie_conf_cleanup(rie_conf_meta_t *meta, void *ctx)
{
    char  **conf_key, *mem;
    void   *res;
    rie_conf_item_t  *spec;

    for (spec = meta->spec; spec->name; spec++) {
        if (!spec->initialized) {
            continue;
        }

        if (spec->type == RIE_CTYPE_STR && spec->convert == NULL) {
            /* converters free source string by themselves */

            res = (char *) ctx + spec->offset;

            conf_key = (char **) res;
            mem = *conf_key;

            if (mem) {
                free(mem);
                *conf_key = NULL;
            }
        }

        spec->initialized = 0;
    }
}
