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

#ifndef __LIZARD_UTILS_HPP___
#define __LIZARD_UTILS_HPP___

#include <stdint.h>

struct in_addr;

namespace lz_utils {

void uwait(long N, long M = 0);
uint64_t fine_clock();

void close_connection(int fd);
void set_socket_timeout(int fd, long timeout);
int set_nonblocking(int fd);
int add_listener(const char * host_desc, const char * port_desc, int listen_q_sz = 1024);
int add_sender(const char * host_desc, const char * port_desc);
int accept_new_connection(int fd, struct in_addr& ip);

}

#endif
