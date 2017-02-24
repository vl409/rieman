
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

#ifndef __RIE_LOG_H_
#define __RIE_LOG_H_

#include "rieman.h"

#include <stdarg.h>


#define rie_log_error(err, fmt, ...)                                          \
    rie_log_sys_error_real( __FILE__, __LINE__, err, fmt, ##__VA_ARGS__)

#define rie_log_str_error(str, fmt, ...)                                      \
    rie_log_str_error_real(__FILE__, __LINE__, str, fmt, ##__VA_ARGS__)

#define rie_log_error0(err, msg)  rie_log_error(err, "%s", msg)

#define rie_log_str_error0(str, msg)  rie_log_str_error(str, "%s", msg)

#if defined(RIE_DEBUG)

#define rie_debug(fmt, ...)  rie_debug_real(fmt, ##__VA_ARGS__)

#define rie_debug0(msg)  rie_debug("%s", msg)

void rie_log_backtrace();

#else

#define rie_debug(fmt, ...)

#endif

typedef struct  rie_log_s  rie_log_t;

rie_log_t *rie_log_new(char *filename);
void rie_log_delete(rie_log_t *log);
void rie_log_dump();

void rie_log(char *fmt, ...);

void rie_debug_real(char *fmt, ...);

void rie_log_sys_error_real(char *file, int line, int err,
    char *fmt, ...);

void rie_log_str_error_real(char *file, int line, const char *err,
    char *fmt, ...);

void rie_log_str_error_real_v(char *file, int line, const char *err, char *fmt,
    va_list ap);

#endif
