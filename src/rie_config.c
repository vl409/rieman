
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
#include "rie_config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>

#define rie_conf_log_error(key, err) \
    rie_conf_log_error_real( __FILE__, __LINE__, key, err)


typedef int (*rie_conf_type_parse_pt)(char *str, rie_conf_value_t *res);
typedef int (*rie_conf_process_key_pt)(xmlDocPtr doc, xmlChar *key,
    rie_conf_item_t *spec, void *res);

static void rie_conf_quiet_stub(void *ctx, const char *msg, ...);

/* low level type convertors */
static int rie_conf_to_str(char* str, rie_conf_value_t *res);
static int rie_conf_to_uint32(char* str, rie_conf_value_t *res);
static int rie_conf_to_dbl(char* str, rie_conf_value_t *res);
static int rie_conf_to_bool(char* str, rie_conf_value_t *res);

static int rie_conf_get_value(xmlDocPtr doc, unsigned char *xpath,
    xmlChar **rv);
static int rie_conf_set_value(rie_conf_type_t type, rie_conf_value_t *val,
    void *res);

static int rie_conf_process_key(xmlDocPtr doc, xmlChar *key,
    rie_conf_item_t *spec, void *res);
static int rie_conf_free_key(xmlDocPtr doc, xmlChar *key,
    rie_conf_item_t *spec, void *res);

static int rie_conf_iterate(xmlDocPtr doc, rie_conf_item_t *specs,
    xmlChar *prefix, void *ctx, rie_conf_process_key_pt handler);

static int rie_locate_file(char (*found)[FILENAME_MAX],
    char (*paths)[FILENAME_MAX], size_t nelts);
static int rie_locate_config_schema(char (*found)[FILENAME_MAX],
    unsigned char *version);
static int rie_locate_skin_schema(char (*found)[FILENAME_MAX],
    unsigned char *version);

rie_conf_type_parse_pt rie_conf_type_parser[] = {
    rie_conf_to_str,     /* RIE_CTYPE_STR     */
    rie_conf_to_uint32,  /* RIE_CTYPE_UINT32  */
    rie_conf_to_dbl,     /* RIE_CTYPE_DBL     */
    rie_conf_to_bool,    /* RIE_CTYPE_BOOL    */
};


static void
rie_conf_log_error_real(char *file, int line, unsigned char *key, char *error)
{
    size_t       len;
    xmlErrorPtr  ex;

    ex = xmlGetLastError();
    if (ex != NULL) {

        /* libxml message ends with newline, remove it */
        len = strlen(ex->message) + 1;

        char buf[len];

        strcpy(buf, ex->message);
        if (buf[len - 1] == '\n') {
            buf[len - 1] = 0;
        }

        if (ex->file) {
            rie_log_sys_error_real(file, line, 0,
                                   "libxml: in '%s' line %d: "
                                   "%s (domain %d code %d)",
                                   ex->file, ex->line, buf, ex->domain,
                                   ex->code, ex->message);
        } else {
            rie_log_sys_error_real(file, line, 0,
                                   "libxml: %s (domain %d code %d)",
                                   buf, ex->domain, ex->code, ex->message);
        }
    }

    if (key) {
        rie_log_sys_error_real(file, line, 0, "%s while processing key '%s'",
                               error, key);

    } else {
        rie_log_sys_error_real(file, line, 0, "%s", error);
    }
}


static void
rie_conf_quiet_stub(void *ctx, const char *msg, ...)
{
    /* nothing here */
}


static int
rie_conf_to_str(char* str, rie_conf_value_t *res)
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
rie_conf_to_bool(char* str, rie_conf_value_t *res)
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
rie_conf_get_value(xmlDocPtr doc, unsigned char *xpath, xmlChar **rv)
{
    xmlNodeSetPtr        nodeset;
    xmlXPathObjectPtr    result;
    xmlXPathContextPtr   ctx;

    ctx = xmlXPathNewContext(doc);
    if (ctx == NULL) {
        rie_conf_log_error(xpath, "xmlXPathNewContext()");
        return RIE_ERROR;
    }

    result = xmlXPathEvalExpression(xpath, ctx);
    xmlXPathFreeContext(ctx);
    if (result == NULL) {
        rie_conf_log_error(xpath, "xmlXPathEvalExpression()");
        return RIE_ERROR;
    }

    if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        xmlXPathFreeObject(result);
        return RIE_NOTFOUND;
    }

    nodeset = result->nodesetval;

    /* all expressions are expected to return single node */
    *rv = xmlNodeListGetString(doc,
                               nodeset->nodeTab[0]->xmlChildrenNode, 1);
    xmlXPathFreeObject(result);

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
rie_conf_process_key(xmlDocPtr doc, xmlChar *key, rie_conf_item_t *spec,
    void *res)
{
    int       rc;
    xmlChar   prefix[FILENAME_MAX];
    xmlChar  *value, *p;

    rie_conf_ref_t    *ref;
    rie_conf_value_t   box;

    value = NULL;

    rc = rie_conf_get_value(doc, key, &value);

    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    if (rc == RIE_NOTFOUND) {

        if (spec->dflt == NULL) {
            rie_conf_log_error(key, "value not set in config and no default");
            return RIE_ERROR;
        }

        if (spec->type == RIE_CTYPE_REF) {
            ref = spec->data.ptr;

            rie_debug("%s : missing, using default '%s'", key, spec->dflt);

            sprintf((char *) prefix, ref->xpath, spec->dflt);

            return rie_conf_iterate(doc, ref->spec, prefix, res,
                                    rie_conf_process_key);
        }

        p = (xmlChar *) spec->dflt;

    } else {

        if (spec->type == RIE_CTYPE_REF) {
            ref = spec->data.ptr;

            /* instantiate a template referring to actual values */
            sprintf((char *) prefix, ref->xpath, value);

            xmlFree(value);

            return rie_conf_iterate(doc, ref->spec, prefix, res,
                                    rie_conf_process_key);
        }

        p = value;
    }

    /* convert string value of key into destination type */
    if (rie_conf_type_parser[spec->type]((char *) p, &box) != RIE_OK) {
        rie_conf_log_error(key, "failed to convert key value");
        rc = RIE_ERROR;
        goto cleanup;
    }

    /* save converted value into destination according to type */
    if (spec->convert) {
        rc = spec->convert(spec, &box.data, res, (char *) key);

    } else {
        rc = rie_conf_set_value(spec->type, &box, res);
    }

    rie_debug("%s = %s%s", key, p, (p == value) ? "" : " [default]");

cleanup:

    if (value) {
        xmlFree(value);
    }

    return rc;
}


static int
rie_conf_free_key(xmlDocPtr doc, xmlChar *key, rie_conf_item_t *spec,
    void *res)
{
    int        rc;
    char     **conf_key, *mem;
    xmlChar   *value;
    xmlChar    prefix[FILENAME_MAX];

    rie_conf_ref_t  *ref;

    value = NULL;

    rc = rie_conf_get_value(doc, key, &value);

    if (rc == RIE_ERROR) {
        return RIE_ERROR;
    }

    if (spec->type == RIE_CTYPE_STR && spec->convert == NULL) {
        /* converters free source string by themselves */

        conf_key = (char **) res;
        mem = *conf_key;

        if (mem) {
            free(mem);
            *conf_key = NULL;
        }
    }

    if (spec->type == RIE_CTYPE_REF) {
        ref = spec->data.ptr;

        /* instantiate a template referring to actual values */
        sprintf((char *) prefix, ref->xpath, value);

        xmlFree(value);

        return rie_conf_iterate(doc, ref->spec, prefix, res,
                                rie_conf_free_key);
    }

    return RIE_OK;
}


static int
rie_conf_iterate(xmlDocPtr doc, rie_conf_item_t *specs, xmlChar *prefix,
    void *ctx, rie_conf_process_key_pt handler)
{
    void             *res;
    rie_conf_item_t  *spec;

    xmlChar           key[FILENAME_MAX];

    for (spec = specs; spec->name; spec++) {

        if (prefix) {
            (void) sprintf((char *) key, "%s%s", prefix, spec->name);

        } else {
            (void) sprintf((char *) key, "%s", spec->name);
        }

        res = (char *) ctx + spec->offset;

        if (spec->type == RIE_CTYPE_CHAIN) {
            if (rie_conf_iterate(doc, spec->data.ptr, key, res, handler)
                != RIE_OK)
            {
                return RIE_ERROR;
            }
            continue;
        }

        if (handler(doc, key, spec, res) != RIE_OK) {
            return RIE_ERROR;
        }
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


static int
rie_locate_config_schema(char (*found)[FILENAME_MAX], unsigned char *version)
{
    int   last = 0;

#if defined(RIE_TESTS)
    char  locations[3][FILENAME_MAX] = { {0}, {0}, {0} };
    sprintf(locations[last++], "./tests/conf/rieman-%s.xsd", version);
#else
    char  locations[2][FILENAME_MAX] = { {0}, {0} };
#endif

    sprintf(locations[last++], "./conf/rieman-%s.xsd", version);

    sprintf(locations[last++], "%s/rieman-%s.xsd", RIE_DATADIR, version);

    return rie_locate_file(found, locations, last);
}


int
rie_locate_skin(char (*found)[FILENAME_MAX], char *fname)
{
    int    last;
    char  *home;

    char locations[4][FILENAME_MAX] = { {0}, {0}, {0} };

    last = 0;

    sprintf(locations[last++], "./skins/%s/rieman_skin.xml", fname);

    home = getenv("XDG_DATA_HOME");
    if (home) {
        sprintf(locations[last++], "%s/rieman/skins/%s/rieman_skin.xml",
                home, fname);
    }

    home = getenv("HOME");
    if (home) {
       sprintf(locations[last++],
               "%s/.local/share/rieman/skins/%s/rieman_skin.xml",
               home, fname);
    }

    sprintf(locations[last++], "%s/skins/%s/rieman_skin.xml", RIE_DATADIR,
            fname);

    return rie_locate_file(found, locations, last);
}


static int
rie_locate_skin_schema(char (*found)[FILENAME_MAX], unsigned char *version)
{
    int   last = 0;
    char  locations[2][FILENAME_MAX] = { {0}, {0} };

    sprintf(locations[last++], "./skins/rieman-skin-%s.xsd", version);

    sprintf(locations[last++], "%s/skins/rieman-skin-%s.xsd",
                               RIE_DATADIR, version);

    return rie_locate_file(found, locations, last);
}


int
rie_conf_load(char *conf_file, rie_conf_item_t *specs, void *ctx)
{
    int  rc, vact, is_conf = 0;

    rie_conf_meta_t   *meta;
    rie_conf_value_t   cv;

    xmlChar      *vers;
    xmlDocPtr     doc;
    xmlNodePtr    root;
    xmlSchemaPtr  schema;

    xmlSchemaValidCtxtPtr  validator;
    xmlSchemaParserCtxtPtr schema_parser;

    char schema_file[FILENAME_MAX];

    schema = NULL;
    validator = NULL;
    schema_parser = NULL;

    doc = xmlReadFile(conf_file, NULL, XML_PARSE_PEDANTIC);
    if (doc == NULL) {
        rie_log_error(0, "failed to parse configuration file \"%s\"", conf_file);
        return RIE_ERROR;
    }

    root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        rc = RIE_ERROR;
        goto cleanup;
    }

    if (strcmp((char *) root->name, "rieman-conf") == 0) {
        rc = rie_conf_get_value(doc, (xmlChar*) "/rieman-conf/@version", &vers);
        is_conf = 1;

    } else if (strcmp((char *) root->name, "rieman-skin") == 0) {
        rc = rie_conf_get_value(doc, (xmlChar*) "/rieman-skin/@version", &vers);

    } else {
        rie_log_error(0, "unknown root element \"%s\"", root->name);
        rc = RIE_ERROR;
        goto cleanup;
    }

    if (rc == RIE_ERROR) {
        rc = RIE_ERROR;
        goto cleanup;
    }

    if (rc != RIE_NOTFOUND) {

        /* if config or skin specifies version, it will be validated */

        if (rie_conf_to_dbl((char *) vers, &cv) != RIE_OK) {
            xmlFree(vers);
            rc = RIE_ERROR;
            goto cleanup;
        }

        vact = cv.dbl * 10;

        meta = (rie_conf_meta_t *) ctx;

        if (vact < meta->version_min || vact > meta->version_max) {
#if !defined(RIE_TESTS)
            rie_log_error(0, "unsupported version: \"%s\", "
                          "supported are: %.1f..%.1f", vers,
                          (float) meta->version_min / 10,
                          (float) meta->version_max / 10);
            xmlFree(vers);
            rc = RIE_ERROR;
            goto cleanup;
#endif
        }

        if (is_conf) {

            if (rie_locate_config_schema(&schema_file, vers) != RIE_OK) {
                rie_log_error0(0, "No config schema found, exiting");
                xmlFree(vers);
                rc = RIE_ERROR;
                goto cleanup;
            }

        } else {

            if (rie_locate_skin_schema(&schema_file, vers) != RIE_OK) {
                rie_log_error0(0, "skin schema not found");
                xmlFree(vers);
                rc = RIE_ERROR;
                goto cleanup;
            }
        }

        xmlFree(vers);

        rie_log("validating configuration using schema '%s'", schema_file);

        schema_parser = xmlSchemaNewParserCtxt(schema_file);
        if (schema_parser == NULL) {
            rie_conf_log_error(NULL, "failed to create schema parser");
            rc = RIE_ERROR;
            goto cleanup;
        }

        schema = xmlSchemaParse(schema_parser);
        if (schema == NULL) {
            rie_conf_log_error(NULL, "failed to parse schema");
            rc = RIE_ERROR;
            goto cleanup;
        }

        validator = xmlSchemaNewValidCtxt(schema);
        if (validator == NULL) {
            rie_conf_log_error(NULL, "failed to create validator context");
            rc = RIE_ERROR;
            goto cleanup;
        }

        rc = xmlSchemaValidateDoc(validator, doc);
        if (rc != 0) {
            rie_conf_log_error(NULL, "configuration is not valid");
            rc = RIE_ERROR;
            goto cleanup;
        }
    }

    xmlSetGenericErrorFunc(doc, rie_conf_quiet_stub);

    rc = rie_conf_iterate(doc, specs, NULL, ctx, rie_conf_process_key);

    if (rc != RIE_OK) {
        (void) rie_conf_cleanup(specs, ctx);
    }

cleanup:

    xmlSetGenericErrorFunc(NULL, NULL);

    xmlFreeDoc(doc);
    xmlCleanupParser();

    if (schema_parser) {
        xmlSchemaFreeParserCtxt(schema_parser);
    }

    if (schema) {
        xmlSchemaFree(schema);
    }

    if (validator) {
        xmlSchemaFreeValidCtxt(validator);
    }

    return rc;
}


void
rie_conf_cleanup(rie_conf_item_t *specs, void *ctx)
{
    (void) rie_conf_iterate(NULL, specs, NULL, ctx, rie_conf_free_key);
}
