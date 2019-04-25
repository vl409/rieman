
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

#include <errno.h>

#include <fontconfig/fontconfig.h>
#include <cairo/cairo-ft.h>


struct rie_font_ctx_s {
    FcConfig        *fontconf;
    FT_Library       ft_library;
};


rie_font_ctx_t *
rie_font_ctx_new(rie_fc_t *fc)
{
    rie_font_ctx_t  *font_ctx;

    font_ctx = malloc(sizeof(rie_font_ctx_t));
    if (font_ctx == NULL) {
        rie_log_error0(errno, "malloc()");
        return NULL;
    }

    font_ctx->fontconf = FcInitLoadConfigAndFonts();
    if (font_ctx->fontconf == NULL) {
        rie_log_error0(0, "FcInitLoadConfigAndFonts() failed");
        return NULL;
    }

    if (FT_Init_FreeType(&font_ctx->ft_library)) {
        rie_log_error0(0, "FT_Init_FreeType() failed");
        return NULL;
    }

    return font_ctx;
}


void
rie_font_ctx_cleanup(rie_font_ctx_t *font_ctx)
{
    FcConfigDestroy(font_ctx->fontconf);
    free(font_ctx);
}


int
rie_font_init(rie_font_ctx_t* font_ctx, rie_fc_t *fc)
{
    FT_Face     ft_face;
    FcChar8    *file;
    FcResult    r;
    FcPattern  *font;
    FcPattern  *pat;

    cairo_status_t      cs;
    cairo_font_face_t  *ff;

    pat = FcNameParse((const FcChar8*)(fc->face));
    if (pat == NULL) {
        rie_log_error(0, "FcNameParse(\"%s\") failed", fc->face);
        return RIE_ERROR;
    }

    if (FcConfigSubstitute(font_ctx->fontconf, pat, FcMatchPattern) != FcTrue) {
        rie_log_error(0, "FcConfigSubstitute() failed for face %s", fc->face);
        return RIE_ERROR;
    }

    FcDefaultSubstitute(pat);

    font = FcFontMatch(font_ctx->fontconf, pat, &r);
    if (font == NULL) {
        rie_log_error(0, "FcFontMatch() failed for face %s", fc->face);
        return RIE_ERROR;
    }

    if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch) {
        FcPatternDestroy(font);
        rie_log_error(0, "FcPatternGetString(\"%s\") failed", file);
        return RIE_ERROR;
    }

    FcPatternDestroy(pat);

    if (FT_New_Face(font_ctx->ft_library, (char *) file, 0, &ft_face)) {
        rie_log_error(0, "FT_New_Face() for file \"%s\" failed", file);
        return RIE_ERROR;
    }

    rie_log("Using \"%s\" for specified font \"%s\"", file, fc->face);

    /* 'file' is allocated somewhere inside 'font' */
    FcPatternDestroy(font);

    ff = cairo_ft_font_face_create_for_ft_face(ft_face, 0);

    cs = cairo_font_face_status(ff);
    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs),
                          "cairo_ft_font_face_create_for_ft_face()");
        return RIE_ERROR;
    }

    cs = cairo_font_face_set_user_data(ff,
                                 (cairo_user_data_key_t *) ft_face,
                                 ft_face, (cairo_destroy_func_t) FT_Done_Face);

    if (cs != CAIRO_STATUS_SUCCESS) {
        rie_log_str_error0(cairo_status_to_string(cs),
                          "cairo_font_face_set_user_data()");
        cairo_font_face_destroy(ff);
        FT_Done_Face(ft_face);
        return RIE_ERROR;
    }

    fc->font = (rie_font_t *) ff;

    return RIE_OK;
}


void
rie_font_cleanup(rie_font_t *font)
{
    cairo_font_face_destroy((cairo_font_face_t*)font);
}
