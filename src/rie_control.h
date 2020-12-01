
/*
 * Copyright (C) 2020 Vladimir Homutov
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

#ifndef __RIE_CONTROL_H__
#define __RIE_CONTROL_H__

rie_control_t *rie_control_new(rie_settings_t *cfg, void *data);

void rie_control_delete(rie_control_t *ctl, int final);

int rie_control_get_fd(rie_control_t *ctl);

int rie_control_handle_socket_event(rie_control_t *ctl);

int rie_control_send_message(char *sockpath, char *msg);

#endif
