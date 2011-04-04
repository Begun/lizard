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

#ifndef __FD_INFO_______
#define __FD_INFO_______

#include <Judy.h>
#include <lizard/http.hpp>
#include <lizard/plugin.hpp>
#include <lizard/pool/pool.hpp>
#include <lizard/statistics.hpp>
#include <lizard/timeline.hpp>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>

namespace lizard
{
//-----------------------------------------------------------------

class fd_map
{
    class container : public http
    {
        uint64_t first_access;
        uint64_t last_access;

    public:
        container();
        ~container();

        void init_time();
        void touch_time();

        uint64_t get_lifetime()const;
    };

    pool_ns::pool<container, 500> elements_pool;

    Pvoid_t map_handle;

    timeline timeouts;

    double min_lifetime, mid_lifetime, max_lifetime;
public:

    fd_map();
    ~fd_map();

    bool create(int fd, const in_addr& ip);
    http * acquire(int fd);

    bool release(http *);

    bool del(int fd);

    void kill_oldest(int timeout);

    int min_timeout()const;

    size_t fd_count()const;
};

//-----------------------------------------------------------------

}

extern lizard::statistics stats;

#endif
