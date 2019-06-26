
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
#include "rie_skin.h"
#include "rie_config.h"

#include <stdlib.h>
#include <limits.h>
#include <libgen.h> /* for dirname */
#include <string.h>
#include <stddef.h>


struct rie_skin_s {
    rie_conf_meta_t  meta;
    rie_font_ctx_t  *font_ctx;

    rie_texture_t    textures[RIE_TX_LAST];
    rie_border_t     borders[RIE_BORDER_LAST];
    rie_fc_t         fonts[RIE_FONT_LAST];

    double           icon_alpha;
};

static int rie_skin_hex_to_rgb(rie_conf_item_t *spec, void *value, void *res,
    char *key);


static rie_conf_map_t rie_texture_types[] = {
    { "none", RIE_TX_TYPE_NONE },
    { "color", RIE_TX_TYPE_COLOR },
    { "image", RIE_TX_TYPE_TEXTURE },
    { NULL, 0 }
};

static rie_conf_item_t rie_color_conf[] = {

   { "/@hex", RIE_CTYPE_STR, "0",
     0, rie_skin_hex_to_rgb, { NULL } },

    { NULL, 0, NULL, 0, NULL, { NULL } }
};

static rie_conf_ref_t rie_color_ref[] = {
    { "/rieman-skin/colors/colordef[@name='%s']", rie_color_conf }
};


#define rie_border_entry(dflt_clr)                                  \
    {                                                               \
        { "/@width", RIE_CTYPE_UINT32, "0",                         \
          offsetof(rie_border_t, w),  NULL, { NULL } },             \
                                                                    \
        { "/@color", RIE_CTYPE_REF, dflt_clr,                       \
          offsetof(rie_border_t, color),                            \
          NULL, { rie_color_ref } },                                \
                                                                    \
        { "/@alpha", RIE_CTYPE_DBL, "1.0",                          \
          offsetof(rie_border_t, alpha),  NULL, { NULL } },         \
                                                                    \
        { "/@src", RIE_CTYPE_STR, "",                               \
          offsetof(rie_border_t, tile_src),  NULL, { NULL } },      \
                                                                    \
        { "/@type", RIE_CTYPE_STR, "color",                         \
          offsetof(rie_border_t, type),                             \
          rie_conf_set_variants, { rie_texture_types } },           \
                                                                    \
        { NULL, 0, NULL, 0, NULL, { NULL } }                        \
    }

static rie_conf_item_t rie_tx_border_conf[][6] = {
        rie_border_entry("black"), /* RIE_BORDER_PAGER */
        rie_border_entry("black"), /* RIE_BORDER_DESKTOP_ACTIVE */
        rie_border_entry("black"), /* RIE_BORDER_WINDOW */
        rie_border_entry("black"), /* RIE_BORDER_WINDOW_FOCUSED */
        rie_border_entry("white"), /* RIE_BORDER_WINDOW_ATTENTION */
        rie_border_entry("white"), /* RIE_BORDER_VIEWPORT */
        rie_border_entry("white"), /* RIE_BORDER_VIEWPORT_ACTIVE */
};


#define rie_tx_entry(dflt_clr)                            \
    {                                                     \
        { "/@type", RIE_CTYPE_STR, "color",               \
          offsetof(rie_texture_t, type),                  \
          rie_conf_set_variants, { rie_texture_types } }, \
                                                          \
        { "/@color", RIE_CTYPE_REF, dflt_clr,             \
          offsetof(rie_texture_t, color),                 \
          NULL, { rie_color_ref } },                      \
                                                          \
        { "/@src", RIE_CTYPE_STR, "",                     \
          offsetof(rie_texture_t, image),                 \
          NULL, { NULL } },                               \
                                                          \
        { "/@alpha", RIE_CTYPE_DBL, "1.0",                \
          offsetof(rie_texture_t, alpha),                 \
          NULL, { NULL } },                               \
                                                          \
        { NULL, 0, NULL, 0, NULL, { NULL } }              \
    }


static rie_conf_item_t rie_textures_conf[][5] = {
    rie_tx_entry("black"), /* RIE_TX_BACKGROUND */
    rie_tx_entry("black"), /* RIE_TX_DESKTOP */
    rie_tx_entry("black"), /* RIE_TX_CURRENT_DESKTOP */
    rie_tx_entry("black"), /* RIE_TX_DESKTOP_NAME_BG */
    rie_tx_entry("black"), /* RIE_TX_WINDOW */
    rie_tx_entry("black"), /* RIE_TX_WINDOW_FOCUSED */
    rie_tx_entry("white"), /* RIE_TX_WINDOW_ATTENTION */
    rie_tx_entry("white"), /* RIE_TX_MISSING_ICON */
    rie_tx_entry("white"), /* RIE_TX_VIEWPORT */
    rie_tx_entry("white"), /* RIE_TX_VIEWPORT_ACTIVE */
};


#define rie_font_entry(fname, fsize, fclr)                \
    {                                                     \
        { "/@size", RIE_CTYPE_UINT32, fsize,              \
          offsetof(rie_fc_t, points), NULL, { NULL } },   \
                                                          \
        { "/@face", RIE_CTYPE_STR, fname,                 \
          offsetof(rie_fc_t, face), NULL, { NULL } },     \
                                                          \
        { "/@color", RIE_CTYPE_REF, fclr,                 \
          offsetof(rie_fc_t, color),                      \
          NULL, { rie_color_ref } },                      \
                                                          \
        { "/@alpha", RIE_CTYPE_DBL, "1.0",                \
          offsetof(rie_fc_t, alpha), NULL, { NULL } },    \
                                                          \
        { NULL, 0, NULL, 0, NULL, { NULL } }              \
    }

static rie_conf_item_t rie_skin_font_conf[][5] = {
    rie_font_entry("Droid Sans", "10", "black"), /* RIE_FONT_DESKTOP_NAME */
    rie_font_entry("Droid Sans", "10", "black"), /* RIE_FONT_WINDOW_NAME */
    rie_font_entry("Clockopia",  "12", "black"), /* RIE_FONT_DESKTOP_NUMBER */
    rie_font_entry("Droid Sans", "12", "black"), /* RIE_FONT_GUI */
};


static rie_conf_item_t rie_skin_conf[] = {

    /* Backgrounds */

    { "/rieman-skin/backgrounds/pager", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_BACKGROUND]),
      NULL, { rie_textures_conf[RIE_TX_BACKGROUND] } },

    { "/rieman-skin/backgrounds/viewport", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_VIEWPORT]),
      NULL, { rie_textures_conf[RIE_TX_VIEWPORT] } },

    { "/rieman-skin/backgrounds/viewport-active", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_VIEWPORT_ACTIVE]),
      NULL, { rie_textures_conf[RIE_TX_VIEWPORT_ACTIVE] } },

    { "/rieman-skin/backgrounds/desktop",
      RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_DESKTOP]),
      NULL, { rie_textures_conf[RIE_TX_DESKTOP] } },

    { "/rieman-skin/backgrounds/desktop-active",
      RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_CURRENT_DESKTOP]),
      NULL, { rie_textures_conf[RIE_TX_CURRENT_DESKTOP] } },

    { "/rieman-skin/backgrounds/desktop-pad", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_DESKTOP_NAME_BG]),
      NULL, { rie_textures_conf[RIE_TX_DESKTOP_NAME_BG] } },

    { "/rieman-skin/backgrounds/window",
      RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_WINDOW]),
      NULL, { rie_textures_conf[RIE_TX_WINDOW] } },

    { "/rieman-skin/backgrounds/window-active",
      RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_WINDOW_FOCUSED]),
      NULL, { rie_textures_conf[RIE_TX_WINDOW_FOCUSED] } },

    { "/rieman-skin/backgrounds/window-attention",
      RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, textures[RIE_TX_WINDOW_ATTENTION]),
      NULL, { rie_textures_conf[RIE_TX_WINDOW_ATTENTION] } },

    /* Borders */

    { "/rieman-skin/borders/pager", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, borders[RIE_BORDER_PAGER]),
      NULL, { rie_tx_border_conf[RIE_BORDER_PAGER] } },

    { "/rieman-skin/borders/desktop-active", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, borders[RIE_BORDER_DESKTOP_ACTIVE]),
      NULL, { rie_tx_border_conf[RIE_BORDER_DESKTOP_ACTIVE] } },

    { "/rieman-skin/borders/viewport", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, borders[RIE_BORDER_VIEWPORT]),
      NULL, { rie_tx_border_conf[RIE_BORDER_VIEWPORT] } },

    { "/rieman-skin/borders/viewport-active", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, borders[RIE_BORDER_VIEWPORT_ACTIVE]),
      NULL, { rie_tx_border_conf[RIE_BORDER_VIEWPORT_ACTIVE] } },

    { "/rieman-skin/borders/window",
      RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, borders[RIE_BORDER_WINDOW]),
      NULL, { rie_tx_border_conf[RIE_BORDER_WINDOW] } },

    { "/rieman-skin/borders/window-active", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, borders[RIE_BORDER_WINDOW_FOCUSED]),
      NULL, { rie_tx_border_conf[RIE_BORDER_WINDOW_FOCUSED] } },

    { "/rieman-skin/borders/window-attention", RIE_CTYPE_CHAIN, NULL,
      offsetof(struct rie_skin_s, borders[RIE_BORDER_WINDOW_ATTENTION]),
      NULL, { rie_tx_border_conf[RIE_BORDER_WINDOW_ATTENTION] } },

    /* Fonts */

    { "/rieman-skin/fonts/desktop-name", RIE_CTYPE_CHAIN, "small",
      offsetof(struct rie_skin_s, fonts[RIE_FONT_DESKTOP_NAME]),
      NULL, { rie_skin_font_conf[RIE_FONT_DESKTOP_NAME] }},

    { "/rieman-skin/fonts/window-name", RIE_CTYPE_CHAIN, "small",
      offsetof(struct rie_skin_s, fonts[RIE_FONT_WINDOW_NAME]),
      NULL, { rie_skin_font_conf[RIE_FONT_WINDOW_NAME] }},

    { "/rieman-skin/fonts/desktop-number", RIE_CTYPE_CHAIN, "small",
      offsetof(struct rie_skin_s, fonts[RIE_FONT_DESKTOP_NUMBER]),
      NULL, { rie_skin_font_conf[RIE_FONT_DESKTOP_NUMBER] } },

    /* Icons */

    { "/rieman-skin/icons/window/@alpha", RIE_CTYPE_DBL, "1.0",
      offsetof(struct rie_skin_s, icon_alpha), NULL, { NULL } },

    { "/rieman-skin/icons/window/@fallback", RIE_CTYPE_STR, "missing_icon.png",
      offsetof(struct rie_skin_s, textures[RIE_TX_MISSING_ICON].image),
      NULL, { NULL } },

    { NULL, 0, NULL, 0, NULL, { NULL } }
};


int
rie_skin_hex_to_rgb(rie_conf_item_t *spec, void *value, void *res, char *key)
{
    rie_color_t *color = res;

    int                rc;
    long               val;
    char              *str, *p, *endptr;
    uint32_t           number;

    str = p = *(char **) value;

    rc = RIE_ERROR;

    if (*p == '#') {
        if (p[1] == 0) {
            rie_log_error(0, "failed to convert color '%s' to hex in key '%s'",
                          str, key);
            goto cleanup;
        }
        p++;
    }

    errno = 0;
    val = strtol(p, &endptr, 16);

    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
        || (errno != 0 && val == 0))
    {
        rie_log_error0(errno, "conversion to hex failed: strtol()");
        goto cleanup;
    }

    if (endptr == p) {
        rie_log_error0(0, "conversion to hex failed: no digits");
        goto cleanup;
    }

    if (val < 0) {
        rie_log_error0(0, "conversion to hex failed: negative value");
        goto cleanup;
    }

    if (val > UINT32_MAX) {
        rie_log_error0(0, "conversion to hex failed: value too big");
        goto cleanup;
    }

    number = (uint32_t) val;

    color->r = ((number >> 16) & 0xFF);
    color->g = ((number >> 8)  & 0xFF);
    color->b = ((number) & 0xFF);

    color->r /= 255;
    color->g /= 255;
    color->b /= 255;

    rc = RIE_OK;

cleanup:

    free(str); /* no longer needed */

    return rc;
}


rie_texture_t *
rie_skin_texture(rie_skin_t *skin, rie_skin_texture_t elem)
{
    return &skin->textures[elem];
}


rie_border_t *
rie_skin_border(rie_skin_t *skin, rie_skin_border_t elem)
{
    return &skin->borders[elem];
}



rie_fc_t *
rie_skin_font(rie_skin_t *skin, rie_skin_font_t elem)
{
    return &skin->fonts[elem];
}

double
rie_skin_icon_alpha(rie_skin_t *skin)
{
    return skin->icon_alpha;
}


static int
rie_skin_load_border(char *skin_dir, rie_border_t *bspec)
{
    int    w, h;
    int    i, j, n;
    char  *p;

    rie_surface_t  *surface, *tx;
    rie_pattern_t *pat;

    if (bspec->type == RIE_TX_TYPE_TEXTURE) {

        p = rie_mkpath(skin_dir, bspec->tile_src, NULL);
        if (p == NULL) {
            return RIE_ERROR;
        }

        surface = rie_gfx_surface_from_png(p, &w, &h);

        free(p);

        if (surface == NULL) {
            return RIE_ERROR;
        }

        if (w < bspec->w * 4) {
            rie_log_error(0, "'%s' tiles are too small: expected width "
                          "is 4 x border_width(%d) = %d, real %d",
                          bspec->tile_src, bspec->w, 4 * bspec->w, w);
            return RIE_ERROR;
        }

        if (h < bspec->w * 3) {
            rie_log_error(0, "'%s' tiles are too small: expected width "
                          "is 4 x border_width(%d) = %d, real %d",
                          bspec->tile_src, bspec->w, 3 * bspec->w, h);
            return RIE_ERROR;
        }

        bspec->tx = surface;

        for (n = 0, i = 0; i < 4; i++) {
            for (j = 0; j < 4 && n < RIE_GRID_LAST; j++, n++) {

                tx = rie_gfx_surface_from_clip(surface,
                                               j * bspec->w,
                                               i * bspec->w,
                                               bspec->w, bspec->w);
                if (tx == NULL) {
                    return RIE_ERROR;
                }

                switch (n) {
                case RIE_GRID_HORIZONTAL_TOP:
                case RIE_GRID_VERTICAL_LEFT:
                case RIE_GRID_HORIZONTAL_BOTTOM:
                case RIE_GRID_VERTICAL_RIGHT:

                    pat = rie_gfx_pattern_from_surface(tx);

                    rie_gfx_surface_free(tx);

                    if (pat == NULL) {
                        return RIE_ERROR;
                    }

                    bspec->tiles[n].pat = pat;

                    break;

                default:
                    bspec->tiles[n].surf = tx;
                    break;
                }
            }
        }
        rie_gfx_surface_free(surface);
    }

    return RIE_OK;
}


rie_skin_t *
rie_skin_new(char *name, rie_gfx_t *gc)
{
    int    i;
    char  *p, *skin_dir;

    rie_skin_t     *skin;
    rie_surface_t  *surface;
    rie_texture_t  *tspec;

    char  conf_file[FILENAME_MAX];

    if (name == NULL) {
        name = "default";

    } else {

        if (rie_locate_skin(&conf_file, name) != RIE_OK) {
            rie_log_error0(0, "no usable skin configuration file found, "
                           "please install at least default one");
            return NULL;
        }
    }

    skin = malloc(sizeof(struct rie_skin_s));
    if (skin == NULL) {
        return NULL;
    }

    memset(skin, 0, sizeof(struct rie_skin_s));

    skin->meta.version_min = 10;
    skin->meta.version_max = 10;
    skin->meta.spec = rie_skin_conf;

    rie_log("using skin configuration file '%s'", conf_file);

    if (rie_conf_load(conf_file, &skin->meta, skin) != RIE_OK) {
        rie_log_error(0, "Failed to load skin configuration file '%s'",
                      conf_file[0] ? conf_file : "-");
        goto failed;
    }

    skin_dir = dirname(conf_file);

    /* load all images and convert them to textures */
    for (i = 0; i < RIE_TX_LAST; i++) {

        tspec = &skin->textures[i];

        if (i == RIE_TX_MISSING_ICON) {
            tspec->type = RIE_TX_TYPE_TEXTURE;
            tspec->alpha = 1.0;
        }

        if (tspec->type == RIE_TX_TYPE_TEXTURE) {

            /* load texture from file */

            if (strcmp(tspec->image, ":root:") == 0) {
                tspec->img_is_root = 1;
                surface = NULL;
                tspec->tag = ":root:";

            } else {

                p = rie_mkpath(skin_dir, tspec->image, NULL);
                if (p == NULL) {
                    goto failed;
                }

                surface = rie_gfx_surface_from_png(p, NULL, NULL);

                free(p);

                if (surface == NULL) {
                    goto failed;
                }

                tspec->tag = "skin_image";
            }

        } else if (tspec->type == RIE_TX_TYPE_COLOR) {

            /* no surface required, renderer will just draw using color */
            surface = NULL;
            tspec->tag = "skin_color";

        } else {
            /* none, i.e. no texture */
            surface = NULL;
            tspec->tag = "skip";
        }

        tspec->tx = surface;
    }


    for (i = 0; i < RIE_BORDER_LAST; i++) {
        if (rie_skin_load_border(skin_dir, &skin->borders[i])
            != RIE_OK)
        {
            goto failed;
        }
    }

    skin->textures[RIE_TX_WINDOW].border = &skin->borders[RIE_BORDER_WINDOW];
    skin->textures[RIE_TX_WINDOW_FOCUSED].border =
                                     &skin->borders[RIE_BORDER_WINDOW_FOCUSED];
    skin->textures[RIE_TX_WINDOW_ATTENTION].border =
                                   &skin->borders[RIE_BORDER_WINDOW_ATTENTION];

    skin->font_ctx = rie_font_ctx_new();
    if (skin->font_ctx == NULL) {
        goto failed;
    }

    for (i = 0; i < RIE_FONT_LAST; i++) {
        if (rie_font_init(skin->font_ctx, &skin->fonts[i]) != RIE_OK) {
            goto failed;
        }
    }

    return skin;

failed:

    rie_skin_delete(skin);

    return NULL;

}

static void
rie_skin_border_free(rie_border_t *bspec)
{
    int  j;

    for (j = 0; j < RIE_GRID_LAST; j++) {

        switch (j) {
        case RIE_GRID_HORIZONTAL_TOP:
        case RIE_GRID_VERTICAL_LEFT:
        case RIE_GRID_HORIZONTAL_BOTTOM:
        case RIE_GRID_VERTICAL_RIGHT:

            if (bspec->tiles[j].pat) {
                rie_gfx_pattern_free(bspec->tiles[j].pat);
                bspec->tiles[j].pat = NULL;
            }
            break;

        default:

            if (bspec->tiles[j].surf) {
                rie_gfx_surface_free(bspec->tiles[j].surf);
                bspec->tiles[j].surf = NULL;
            }
        }
    }
}


void
rie_skin_delete(rie_skin_t *skin)
{
    int  i;

    rie_texture_t  *tspec;

    for (i = 0; i < RIE_TX_LAST; i++) {
        tspec = &skin->textures[i];

        if (tspec->tx) {
            rie_gfx_surface_free(tspec->tx);
            tspec->tx = NULL;
        }
    }

    for (i = 0; i < RIE_BORDER_LAST; i++) {
        rie_skin_border_free(&skin->borders[i]);
    }

    for (i = 0; i < RIE_FONT_LAST; i++) {
        rie_font_cleanup(skin->fonts[i].font);
    }

    if (skin->font_ctx) {
        rie_font_ctx_cleanup(skin->font_ctx);
        skin->font_ctx = NULL;
    }

    rie_conf_cleanup(&skin->meta, skin);

    free(skin);
}
