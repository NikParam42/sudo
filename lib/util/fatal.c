/*
 * Copyright (c) 2004-2005, 2010-2014 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# include "compat/stdbool.h"
#endif /* HAVE_STDBOOL_H */

#define DEFAULT_TEXT_DOMAIN	"sudo"
#include "gettext.h"		/* must be included before missing.h */

#include "missing.h"
#include "sudo_alloc.h"
#include "fatal.h"
#include "queue.h"
#include "sudo_plugin.h"

struct sudo_fatal_callback {
    SLIST_ENTRY(sudo_fatal_callback) entries;
    void (*func)(void);
};
SLIST_HEAD(sudo_fatal_callback_list, sudo_fatal_callback);

static struct sudo_fatal_callback_list callbacks;

static void _warning(int errnum, const char *fmt, va_list ap);

static void
do_cleanup(void)
{
    struct sudo_fatal_callback *cb;

    /* Run callbacks, removing them from the list as we go. */
    while ((cb = SLIST_FIRST(&callbacks)) != NULL) {
	SLIST_REMOVE_HEAD(&callbacks, entries);
	cb->func();
	free(cb);
    }
}

void
sudo_fatal_nodebug(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warning(errno, fmt, ap);
    va_end(ap);
    do_cleanup();
    exit(EXIT_FAILURE);
}

void
sudo_fatalx_nodebug(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warning(0, fmt, ap);
    va_end(ap);
    do_cleanup();
    exit(EXIT_FAILURE);
}

void
sudo_vfatal_nodebug(const char *fmt, va_list ap)
{
    _warning(errno, fmt, ap);
    do_cleanup();
    exit(EXIT_FAILURE);
}

void
sudo_vfatalx_nodebug(const char *fmt, va_list ap)
{
    _warning(0, fmt, ap);
    do_cleanup();
    exit(EXIT_FAILURE);
}

void
sudo_warn_nodebug(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _warning(errno, fmt, ap);
    va_end(ap);
}

void
sudo_warnx_nodebug(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _warning(0, fmt, ap);
    va_end(ap);
}

void
sudo_vwarn_nodebug(const char *fmt, va_list ap)
{
    _warning(errno, fmt, ap);
}

void
sudo_vwarnx_nodebug(const char *fmt, va_list ap)
{
    _warning(0, fmt, ap);
}

static void
_warning(int errnum, const char *fmt, va_list ap)
{
    char *str;

    sudo_evasprintf(&str, fmt, ap);
    if (errnum) {
	if (fmt != NULL) {
	    sudo_printf(SUDO_CONV_ERROR_MSG,
		_("%s: %s: %s\n"), getprogname(), str,
		sudo_warn_strerror(errnum));
	} else {
	    sudo_printf(SUDO_CONV_ERROR_MSG,
		_("%s: %s\n"), getprogname(),
		sudo_warn_strerror(errnum));
	}
    } else {
	sudo_printf(SUDO_CONV_ERROR_MSG,
	    _("%s: %s\n"), getprogname(), str ? str : "(null)");
    }
    efree(str);
}

/*
 * Register a callback to be run when sudo_fatal()/sudo_fatalx() is called.
 */
int
sudo_fatal_callback_register(void (*func)(void))
{
    struct sudo_fatal_callback *cb;

    /* Do not register the same callback twice.  */
    SLIST_FOREACH(cb, &callbacks, entries) {
	if (func == cb->func)
	    return -1;		/* dupe! */
    }

    /* Allocate and insert new callback. */
    cb = malloc(sizeof(*cb));
    if (cb == NULL)
	return -1;
    cb->func = func;
    SLIST_INSERT_HEAD(&callbacks, cb, entries);

    return 0;
}

/*
 * Deregister a sudo_fatal()/sudo_fatalx() callback.
 */
int
sudo_fatal_callback_deregister(void (*func)(void))
{
    struct sudo_fatal_callback *cb, **prev;

    /* Search for callback and remove if found, dupes are not allowed. */
    SLIST_FOREACH_PREVPTR(cb, prev, &callbacks, entries) {
	if (cb->func == func) {
	    if (cb == SLIST_FIRST(&callbacks))
		SLIST_REMOVE_HEAD(&callbacks, entries);
	    else
		SLIST_REMOVE_AFTER(*prev, entries);
	    free(cb);
	    return 0;
	}
    }

    return -1;
}