
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


#define rie_border_entry(prefix, index, dflt_clr)               \
    { prefix".width", RIE_CTYPE_UINT32, "0",                    \
      offsetof(struct rie_skin_s, borders[index])               \
      + offsetof(rie_border_t, w),  NULL, { NULL } },           \
                                                                \
    { prefix".color", RIE_CTYPE_STR, dflt_clr,                  \
      offsetof(struct rie_skin_s, borders[index])               \
      + offsetof(rie_border_t, color),                          \
      rie_skin_hex_to_rgb, { NULL } },                          \
                                                                \
    { prefix".alpha", RIE_CTYPE_DBL, "1.0",                     \
      offsetof(struct rie_skin_s, borders[index])               \
      + offsetof(rie_border_t, alpha),  NULL, { NULL } },       \
                                                                \
    { prefix".src", RIE_CTYPE_STR, "",                          \
      offsetof(struct rie_skin_s, borders[index])               \
      + offsetof(rie_border_t, tile_src),  NULL, { NULL } },    \
                                                                \
    { prefix".type", RIE_CTYPE_STR, "color",                    \
      offsetof(struct rie_skin_s, borders[index])               \
      + offsetof(rie_border_t, type),                           \
      rie_conf_set_variants, { rie_texture_types } }            \


#define rie_tx_entry(prefix, index, dflt_clr)                   \
    { prefix".type", RIE_CTYPE_STR, "color",                    \
      offsetof(struct rie_skin_s, textures[index])              \
      + offsetof(rie_texture_t, type),                          \
      rie_conf_set_variants, { rie_texture_types } },           \
                                                                \
    { prefix".color", RIE_CTYPE_STR, dflt_clr,                  \
      offsetof(struct rie_skin_s, textures[index])              \
      + offsetof(rie_texture_t, color),                         \
      rie_skin_hex_to_rgb, { NULL } },                          \
                                                                \
    { prefix".src", RIE_CTYPE_STR, "",                          \
      offsetof(struct rie_skin_s, textures[index])              \
      + offsetof(rie_texture_t, image),                         \
      NULL, { NULL } },                                         \
                                                                \
    { prefix".alpha", RIE_CTYPE_DBL, "1.0",                     \
      offsetof(struct rie_skin_s, textures[index])              \
      + offsetof(rie_texture_t, alpha),                         \
      NULL, { NULL } }                                          \

#define rie_font_entry(prefix, index, fname, fsize, fclr)       \
    { prefix".size", RIE_CTYPE_UINT32, fsize,                   \
      offsetof(struct rie_skin_s, fonts[index])                 \
      + offsetof(rie_fc_t, points), NULL, { NULL } },           \
                                                                \
    { prefix".face", RIE_CTYPE_STR, fname,                      \
      offsetof(struct rie_skin_s, fonts[index])                 \
      + offsetof(rie_fc_t, face), NULL, { NULL } },             \
                                                                \
    { prefix".color", RIE_CTYPE_STR, fclr,                      \
      offsetof(struct rie_skin_s, fonts[index])                 \
      + offsetof(rie_fc_t, color),                              \
      rie_skin_hex_to_rgb, { NULL } },                          \
                                                                \
    { prefix".alpha", RIE_CTYPE_DBL, "1.0",                     \
      offsetof(struct rie_skin_s, fonts[index])                 \
      + offsetof(rie_fc_t, alpha), NULL, { NULL } }             \


static rie_conf_item_t rie_skin_conf[] = {

    /* Backgrounds */

    rie_tx_entry("backgrounds.pager", RIE_TX_BACKGROUND,
                 "#000000"),
    rie_tx_entry("backgrounds.desktop", RIE_TX_DESKTOP,
                 "#000000"),
    rie_tx_entry("backgrounds.desktop-active", RIE_TX_CURRENT_DESKTOP,
                 "#000000"),
    rie_tx_entry("backgrounds.desktop-pad", RIE_TX_DESKTOP_NAME_BG,
                 "#000000"),
    rie_tx_entry("backgrounds.window", RIE_TX_WINDOW,
                 "#000000"),
    rie_tx_entry("backgrounds.window-active", RIE_TX_WINDOW_FOCUSED,
                 "#000000"),
    rie_tx_entry("backgrounds.window-attention", RIE_TX_WINDOW_ATTENTION,
                 "#ffffff"),
    rie_tx_entry("backgrounds.viewport", RIE_TX_VIEWPORT,
                 "#ffffff"),
    rie_tx_entry("backgrounds.viewport-active", RIE_TX_VIEWPORT_ACTIVE,
                 "#ffffff"),

    /* Borders */

    rie_border_entry("borders.pager", RIE_BORDER_PAGER,
                     "#000000"),
    rie_border_entry("borders.desktop-active", RIE_BORDER_DESKTOP_ACTIVE,
                     "#000000"),
    rie_border_entry("borders.window", RIE_BORDER_WINDOW,
                     "#000000"),
    rie_border_entry("borders.window-active", RIE_BORDER_WINDOW_FOCUSED,
                     "#000000"),
    rie_border_entry("borders.window-attention", RIE_BORDER_WINDOW_ATTENTION,
                     "#ffffff"),
    rie_border_entry("borders.viewport", RIE_BORDER_VIEWPORT,
                     "#ffffff"),
    rie_border_entry("borders.viewport-active", RIE_BORDER_VIEWPORT_ACTIVE,
                     "#ffffff"),

    /* Fonts */

    rie_font_entry("fonts.desktop-name", RIE_FONT_DESKTOP_NAME,
                   "Droid Sans", "10", "#000000"),

    rie_font_entry("fonts.window-name", RIE_FONT_WINDOW_NAME,
                    "Droid Sans", "10", "#000000"),

    rie_font_entry("fonts.desktop-number", RIE_FONT_DESKTOP_NUMBER,
                    "Clockopia",  "12", "#000000"),

    /* Icons */

    { "icons.window.alpha", RIE_CTYPE_DBL, "1.0",
      offsetof(struct rie_skin_s, icon_alpha), NULL, { NULL } },

    { "icons.window.fallback", RIE_CTYPE_STR, "missing_icon.png",
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

    skin->meta.version_min = 11;
    skin->meta.version_max = 11;
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
