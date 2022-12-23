
/*
 * Copyright (C) 2017-2022 Vladimir Homutov
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
#include "rie_external.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


struct rie_log_s {
    char            *name;
    FILE            *file;
#if defined(RIE_DEBUG)
    rie_array_t      backtrace;
#endif
};


static rie_log_t  *rie_logp;

rie_log_t *
rie_log_new(char *filename)
{
    int  err;

    rie_logp = malloc(sizeof(rie_log_t));
    if (rie_logp == NULL) {
        return NULL;
    };

    rie_memzero(rie_logp, sizeof(rie_log_t));

    if (filename == NULL) {
        rie_logp->file = stdout;
        return rie_logp;
    }

    rie_logp->file = fopen(filename, "w");
    if (rie_logp->file == NULL) {
        err = errno;
        fprintf(stdout, "failed to open '%s' for logging: %s, using stdout\n",
                filename, strerror(err));

        free(rie_logp);
        return NULL;
    }

    rie_logp->name = filename;

    (void) setvbuf(rie_logp->file, NULL, _IOLBF, 0);

    return rie_logp;
}


void
rie_log_delete(rie_log_t *log)
{
    fflush(log->file);

    if (log->file != stdout) {
        fclose(log->file);
    }
#if defined(RIE_DEBUG)
    if (log->backtrace.nitems) {
        free(log->backtrace.data);
        log->backtrace.nitems = 0;
    }
#endif
    free(log);
}


static void
rie_log_cat(int out, char *filename)
{
    int     fd, err;
    char    buf[4096];
    size_t  n;

    fd = -1;

    fd = open(filename, 0);
    if (-1 == fd) {
        goto failed;
    }

    while ((n = read(fd, buf, 4096))) {

        if (-1 == n) {
            goto failed;
        }

        n = write(out, buf, n);
        if (-1 == n) {
            goto failed;
        }
    }

    (void) close(fd);

    return;

failed:

    err = errno;

    fprintf(stderr, ">> failed to display log file '%s' due to error '%s'\n",
                    filename, strerror(err));

    if (-1 != fd) {
        (void) close(fd);
    }
}


void
rie_log_dump()
{
    if (rie_logp->name) {
        fprintf(stderr, ">> error log below:\n");
        rie_log_cat(fileno(stderr), rie_logp->name);
    }
    fflush(stdout);
}


static inline void
rie_log_time_prefix(FILE *out)
{
    int         err;
    time_t      tm;
    struct tm  *ti;

    char buf[80];

    tm = time(NULL);
    ti = localtime(&tm);

    if (strftime(buf, sizeof(buf), "[%F %H:%M:%S %Z]", ti) == 0) {
        err = errno;
        fprintf(rie_logp->file, "[time fail: strftime(): %s]", strerror(err));
        return;
    }

    fprintf(out, "%s", buf);
}


void
rie_log_sys_error_real(char *file, int line, int err, char *fmt, ...)
{
    va_list  ap;

    rie_log_time_prefix(rie_logp->file);

    fprintf(rie_logp->file, " -err- ");

    va_start(ap, fmt);
    (void) vfprintf(rie_logp->file, fmt, ap);
    va_end(ap);

    if (err) {
        fprintf(rie_logp->file, " - %s", strerror(err));
    }

    fprintf(rie_logp->file, ", at %s:%d\n", file, line);

#if defined (RIE_DEBUG)
    (void) rie_backtrace_save(&rie_logp->backtrace);
#endif

}

void
rie_log_str_error_real_v(char *file, int line, const char *err, char *fmt,
    va_list ap)
{
    rie_log_time_prefix(rie_logp->file);

    fprintf(rie_logp->file, " -err- ");

    (void) vfprintf(rie_logp->file, fmt, ap);

    fprintf(rie_logp->file, " - %s, at %s:%d\n", err, file, line);

#if defined (RIE_DEBUG)
    (void) rie_backtrace_save(&rie_logp->backtrace);
#endif
}


void
rie_log_str_error_real(char *file, int line, const char *err,
    char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    rie_log_str_error_real_v(file, line, err, fmt, ap);
    va_end(ap);
}


void
rie_debug_real(char *fmt, ...)
{
    va_list  ap;

    rie_log_time_prefix(rie_logp->file);

    fprintf(rie_logp->file, " -dbg- ");

    va_start(ap, fmt);
    (void) vfprintf(rie_logp->file, fmt, ap);
    va_end(ap);

    fprintf(rie_logp->file, "\n");
}


void
rie_log(char *fmt, ...)
{
    va_list  ap;

    rie_log_time_prefix(rie_logp->file);

    fprintf(rie_logp->file, " -log- ");

    va_start(ap, fmt);
    (void) vfprintf(rie_logp->file, fmt, ap);
    va_end(ap);

    fprintf(rie_logp->file, "\n");
}


#if defined(RIE_DEBUG)

void
rie_log_backtrace()
{
    int      i;
    char   **frames;
    size_t   n;

    frames = rie_logp->backtrace.data;
    n = rie_logp->backtrace.nitems;

    if (n < 2) {
        rie_debug0("backtrace is too short, skipping");
        return;
    }

    /* skip this function itself and our caller - some kind of log_error() */
    for (i = 2; i < n; i++) {
        rie_debug("frame %2d : %s", n - i, frames[i]);
    }
}

#endif
