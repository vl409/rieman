
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

#ifndef __RIE_FONT_H__
#define __RIE_FONT_H__

typedef struct rie_font_ctx_s  rie_font_ctx_t;
typedef struct rie_font_s      rie_font_t;

#include "rie_gfx.h"

typedef struct {
    char          *face;
    int            points;
    double         alpha;
    rie_color_t    color;
    rie_font_t    *font;
} rie_fc_t;

rie_font_ctx_t *rie_font_ctx_new();
void rie_font_ctx_cleanup(rie_font_ctx_t *font_ctx);
int rie_font_init(rie_font_ctx_t* font_ctx, rie_fc_t *fc);
void rie_font_cleanup(rie_font_t *font);

#endif
