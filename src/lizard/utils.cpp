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

#include <errno.h>
#include <fcntl.h>
#include <lizard/config.hpp>
#include <lizard/utils.hpp>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <utils/logger.hpp>

#define DEFER_ACCEPT_TIME 200

static lizard::Logger& slogger = lizard::getLog("lizard");

//-----------------------------------------------------------------------------------------------------------
void lz_utils::uwait(long N, long M)
{
    struct timespec ts = {N, M};

    nanosleep(&ts, 0);
}

uint64_t lz_utils::fine_clock()
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    return (tv.tv_sec * 1000000LLU + tv.tv_usec);
}

//-----------------------------------------------------------------------------------------------------------

void lz_utils::close_connection(int fd)
{
    slogger.debug("close_connection %d", fd);
    shutdown(fd, SHUT_RDWR);

    close(fd);
}

void lz_utils::set_socket_timeout(int fd, long timeout)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv,sizeof(struct timeval)) < 0)
    {
        slogger.warn("setsockopt(SO_RCVTIMEO) - '%s'", strerror(errno));
    }
}

int lz_utils::set_nonblocking(int fd)
{
    slogger.debug("set_nonblocking %d", fd);

    int flags = fcntl(fd, F_GETFL, 0);
    if ((flags == -1) || (flags & O_NONBLOCK))
        return flags;
    else
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

//-----------------------------------------------------------------------------------------------------------

int lz_utils::add_listener(const char * host_desc, const char * port_desc, int listen_q_sz)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* addr = 0;
    int repl = getaddrinfo(host_desc, port_desc, &hints, &addr);

    if (0 != repl || 0 == addr)
    {
        throw std::logic_error((std::string)"getaddrinfo(" +
                    (std::string)host_desc + ":" +
                    (std::string)port_desc +
                    ") failed : " + gai_strerror(repl));
    }

    int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    if (fd < 0)
    {
        throw std::logic_error((std::string)"socket error: " + strerror(errno));
    }

    int is_true = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &is_true, sizeof(is_true)) < 0)
    {
        close_connection(fd);
        throw std::logic_error((std::string)"setsockopt - " + strerror(errno));
    }

    if (bind(fd, addr->ai_addr, addr->ai_addrlen) < 0)
    {
        close_connection(fd);
        throw std::logic_error((std::string)"bind(" +
                (std::string)host_desc + ":" +
                (std::string)port_desc +
                (std::string)") failed : " +
                (std::string)strerror(errno));

    }

#if DEFER_ACCEPT_TIME
    int defer_accept = DEFER_ACCEPT_TIME;
    setsockopt(fd, SOL_TCP, TCP_DEFER_ACCEPT, (char *) &defer_accept, sizeof(int));
#endif
    freeaddrinfo(addr);

    if (listen(fd, listen_q_sz) < 0)
    {
        close_connection(fd);

        throw lizard::error("listen(%s:%s) failed: %d: %s", host_desc,
            port_desc, errno, strerror(errno));
#if 0
        throw std::logic_error((std::string)"listen(" +
                (std::string)host_desc + ":" +
                (std::string)port_desc +
                (std::string)") failed : " +
                (std::string)strerror(errno));
#endif
    }

    return fd;
}

int lz_utils::add_sender(const char * host_desc, const char * port_desc)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* addr = 0;
    int repl = getaddrinfo(host_desc, port_desc, &hints, &addr);

    if (0 != repl || 0 == addr)
    {
        throw std::logic_error((std::string)"getaddrinfo(" +
                (std::string)host_desc + ":" +
                (std::string)port_desc +
                ") failed : " + gai_strerror(repl));
    }

    int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

    if (fd < 0)
    {
        throw std::logic_error((std::string)"socket error: " + strerror(errno));
    }

    if (connect(fd, addr->ai_addr, addr->ai_addrlen) < 0)
    {
        throw std::logic_error((std::string)"connect(" +
                (std::string)host_desc + ":" +
                (std::string)port_desc +
                (std::string)") failed : " +
                (std::string)strerror(errno));
    }

    freeaddrinfo(addr);

    return fd;
}

int lz_utils::accept_new_connection(int fd, struct in_addr& ip)
{
    int connection;
    struct sockaddr_in sa;
    socklen_t lsa = sizeof(sa);

    do
    {
        connection = accept(fd, (struct sockaddr *) &sa, &lsa);
    }
    while (connection < 0 && errno == EINTR);

    if (connection < 0 && errno != EAGAIN)
    {
        slogger.error("accept failure: '%s'", strerror(errno));
    }

    ip = sa.sin_addr;

    return connection;
}
