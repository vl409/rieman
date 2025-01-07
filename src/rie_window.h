
/*
 * Copyright (C) 2017-2025 Vladimir Homutov
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

#ifndef __RIE_WINDOW_H__
#define __RIE_WINDOW_H__

#include "rieman.h"

typedef enum {
    RIE_TILE_FAIR_EAST,
    RIE_TILE_FAIR_WEST,
} rie_tile_fair_orientation_e;


struct  rie_window_s {
    rie_rect_t       box;        /* real window corrdinates/size  */
    rie_rect_t       sbox;       /* scaled window inside pager    */
    rie_rect_t       hbox;       /* box of a hidden window on pad */
    rie_rect_t       frame;      /* window manager decorations (real) */

    char            *name;
    char            *title;
    uint32_t         state;
    uint32_t         desktop;
    uint32_t         winid;
    uint32_t         types;
    uint32_t         hidden_idx; /* index in the list of hidden windows */
    uint8_t          focused;
    uint8_t          m_in;       /* mouse is over window *in pager* */
    uint8_t          dead;
    rie_array_t     *icons;
};

int rie_window_init_list(rie_array_t *windows, size_t len);

rie_window_t *rie_window_lookup(rie_t *pager, uint32_t winid);

int rie_window_update_geometry(rie_t *pager);
int rie_window_query(rie_t *pager, rie_window_t *window, uint32_t winid);

void rie_window_update_pager_focus(rie_t *pager);
int rie_windows_tile(rie_t *pager, int desk);

#endif
