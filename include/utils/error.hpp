/* Copyright 2011 ZAO "Begun".
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LZ_ERROR_HPP__
#define __LZ_ERROR_HPP__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <exception>

#if __GNUC__ >= 3
#   define LZ_FORMAT(n, f, e) __attribute__ ((format (n, f, e)))
#else
#   define LZ_FORMAT(n, f, e) /* void */
#endif

namespace lizard
{

const size_t LZ_ERROR_BUFSZ = 4096;

inline const char *strerror(int errnum)
{
    static __thread char errbuf [LZ_ERROR_BUFSZ];

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
    if (0 != ::strerror_r(errnum, errbuf, LZ_ERROR_BUFSZ))
    {
        return "XSI-compliant strerror_r returned "
            "error. Happy Hours!";
    }

    return errbuf;
#else
    return ::strerror_r(errnum, errbuf, LZ_ERROR_BUFSZ);
#endif /* _POSIX_C_SOURCE >= ... */
}

class error : public std::exception
{
    char msg [LZ_ERROR_BUFSZ];

public:
    error(const char *fmt, ...) throw() LZ_FORMAT(printf, 2, 3)
    {
        va_list ap; va_start(ap, fmt);
        vsnprintf(msg, LZ_ERROR_BUFSZ, fmt, ap);
    }

    error(int errnum, const char *s) throw()
    {
        snprintf(msg, LZ_ERROR_BUFSZ, "%s: %d: %s",
            s, errnum, strerror(errnum));
    }

    virtual ~error() throw() {}
    virtual const char *what() const throw() { return msg; }
};

}// lizard

#endif
