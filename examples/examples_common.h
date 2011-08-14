/**
 * @file
 * @author Matt Miller <matt@matthewjmiller.net>
 *
 * @section LICENSE
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 *
 * These are common functionalities shared by the examples.
 */
#ifndef __EXAMPLES_COMMON_H__
#define __EXAMPLES_COMMON_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include "../mkavl.h"

/**
 * Determine the number of elements in an array.
 */
#ifndef NELEMS
#define NELEMS(x) (sizeof(x) / sizeof(x[0]))
#endif

/**
 * Compile time assert macro from:
 * http://www.pixelbeat.org/programming/gcc/static_assert.html 
 */
#ifndef CT_ASSERT
#define CT_ASSERT(e) extern char (*CT_ASSERT(void)) [sizeof(char[1 - 2*!(e)])]
#endif

/**
 * Sanity check for infinite loops.
 */
#define EXAMPLES_RUNAWAY_SANITY 100000

/**
 * Assert utility to crash (via abort()) if the condition is not met regardless
 * of whether NDEBUG is defined.
 *
 * @param condition The condition for which a crash will happen if false.
 */
static inline void
assert_abort (bool condition)
{
    if (!condition) {
        abort();
    }
}

static inline double
timeval_to_seconds (struct timeval *tv)
{
    if (NULL == tv) {
        return (0.0);
    }

    return (tv->tv_sec + (tv->tv_usec / 1000000.0));
}

static size_t 
my_strlcpy(char *dst, const char *src, size_t siz)
{
    char *d = dst;
    const char *s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0) {
        do {
        if ((*d++ = *s++) == 0)
            break;
        } while (--n != 0);
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0';              /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return(s - src - 1);    /* count does not include NUL */
}

#endif
