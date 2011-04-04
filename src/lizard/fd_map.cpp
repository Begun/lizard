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

#include <lizard/fd_map.hpp>
#include <lizard/utils.hpp>
#include <utils/logger.hpp>

static lizard::Logger& slogger = lizard::getLog("lizard");

enum {EPOLL_TIMEOUT = 100};

//TODO: min_timeout(): we should calculate next timeout for epoll here but I decided to set it to 10ms manually for now

//--------------------------------------------------------------------------------
lizard::fd_map::container::container() : first_access(0), last_access(0)
{

}

lizard::fd_map::container::~container()
{

}
//--------------------------------------------------------------------------------------------------------
void lizard::fd_map::container::init_time()
{
    last_access = first_access = lz_utils::fine_clock();
}
//--------------------------------------------------------------------------------------------------------
void lizard::fd_map::container::touch_time()
{
    last_access = lz_utils::fine_clock();
}
//--------------------------------------------------------------------------------------------------------
uint64_t lizard::fd_map::container::get_lifetime()const
{
    return (last_access > first_access) ? (last_access - first_access) : 0;
}
//--------------------------------------------------------------------------------
lizard::fd_map::fd_map() : map_handle(0), timeouts(10)
{
}
//--------------------------------------------------------------------------------------------------------
lizard::fd_map::~fd_map()
{
    if (map_handle)
    {
        JudyLFreeArray(&map_handle, 0);
    }
}
//--------------------------------------------------------------------------------------------------------
bool lizard::fd_map::create(int fd, const in_addr& ip)
{
    bool ret = true;

    //message("fds.create(%d)", fd);

    PPvoid_t h = JudyLIns(&map_handle, (Word_t)fd, 0);

    if (h)
    {
        if (0 == *h)
        {
            container * new_el = elements_pool.allocate();

            new_el->init(fd, ip);
            new_el->init_time();

            *h = new_el;

            timeouts.reg(fd, lz_utils::fine_clock());
        }
        else
        {
            ret = false;
        }
    }

    //rdev_ns::log_message_r(LOG_DEBUG, "fds.create(%d)", fd);

    return ret;
}
//--------------------------------------------------------------------------------------------------------
lizard::http * lizard::fd_map::acquire(int fd)
{
    //rdev_ns::log_message_r(LOG_DEBUG, "fds.acquire(%d)", fd);
    //message("fds.acquire(%d)", fd);


    http * ret = 0;

    PPvoid_t h = JudyLGet(map_handle, (Word_t)fd, 0);

    if (h && *h)
    {
        container * c = (container*)(*h);
        // touch_time вызывается в del тоже, т.ч здесь необязательно
        // c->touch_time();

        timeouts.reg(fd, lz_utils::fine_clock());

        ret = c;
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------------
bool lizard::fd_map::release(http * el)
{
    container * c = static_cast<container*>(el);

    if (false == c->is_locked())
    {
        //rdev_ns::log_message_r(LOG_DEBUG, "fds.release(%d)", el->get_fd());

        //message("fds.release(%d).livetime=%llu", el->get_fd(), (long long unsigned)c->get_lifetime());
        c->destroy();
        elements_pool.free(c);

        return true;
    }
    else
    {
        //message("fds.release(%d) deferred", el->get_fd());
        c->destroy();

        return false;
    }
}
//--------------------------------------------------------------------------------------------------------
bool lizard::fd_map::del(int fd)
{
    bool ret = false;

    PWord_t h = (PWord_t)JudyLGet(map_handle, fd, 0);
    if (h)
    {
        Word_t key = *h;

        if (key)
        {
            container * ob = (container *)key;
            ob->touch_time();
            stats.report_response_time(ob->get_lifetime());
            slogger.debug("%d lifetime is %ld mcs", fd, ob->get_lifetime());

            ret = release(ob);
        }

        ret &= JudyLDel(&map_handle, (Word_t)fd, 0);

        timeouts.del(fd);
    }

    //rdev_ns::log_message_r(LOG_DEBUG, "fds.del(%d)", fd);
    //rdev_ns::log_message_r(LOG_DEBUG, "===============================================================================");

    return ret;
}
//--------------------------------------------------------------------------------------------------------
void lizard::fd_map::kill_oldest(int timeout)
{
    timeline::iterator it;

    Word_t obj = 0;
    Word_t time = lz_utils::fine_clock();

    while (timeouts.enumerate(it, obj, time - timeout))
    {
        //rdev_ns::log_message_r(LOG_DEBUG, "timeout %d for %d", timeout, obj);
        //    message("timeout %d for %d", timeout, obj);
        del(obj);
    }

    timeouts.erase_oldest(time - timeout);

    stats.objects_in_http_pool = elements_pool.allocated_objects();
    stats.pages_in_http_pool = elements_pool.allocated_pages();

}
//--------------------------------------------------------------------------------------------------------
int lizard::fd_map::min_timeout()const
{
    return EPOLL_TIMEOUT;
}
//--------------------------------------------------------------------------------------------------------
size_t lizard::fd_map::fd_count()const
{
     return (size_t)JudyLCount(map_handle, 0, -1, 0);
}
