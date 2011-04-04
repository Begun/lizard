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

#ifndef __LIZARD_SERVER_HPP__
#define __LIZARD_SERVER_HPP__

#include <cstdarg>
#include <deque>
#include <lizard/config.hpp>
#include <lizard/fd_map.hpp>
#include <lizard/plugin_factory.hpp>
#include <lizard/statistics.hpp>
#include <lizard/utils.hpp>
#include <stdexcept>
#include <sys/epoll.h>

namespace lizard
{
//-----------------------------------------------------------------
class server
{
    class lz_callback : public server_callback
    {
        server * lz;

    public:
        lz_callback();
        ~lz_callback();

        void init(server * srv);
        void log_message(plugin_log_levels log_level, const char * param_str, ...);
        void vlog_message(plugin_log_levels log_lebel, const char * param_str, va_list ap);
    };

    enum {LISTEN_QUEUE_SZ = 1024};
    enum {HINT_EPOLL_SIZE = 10000};
    enum {EPOLL_EVENTS = 2000};

    struct epoll_event events[EPOLL_EVENTS];

    pthread_t epoll_th;
    std::vector<pthread_t> easy_th;
    std::vector<pthread_t> hard_th;
    pthread_t stats_th;
    pthread_t idle_th;

    mutable pthread_mutex_t     easy_proc_mutex;
    mutable pthread_cond_t      easy_proc_cond;

    mutable pthread_mutex_t     hard_proc_mutex;
    mutable pthread_cond_t      hard_proc_cond;

    mutable pthread_mutex_t     stats_proc_mutex;
    mutable pthread_cond_t      stats_proc_cond;


    mutable pthread_mutex_t     done_mutex;

    std::deque<http*>           easy_queue;
    std::deque<http*>           hard_queue;
    std::deque<http*>           done_queue;

    fd_map                      fds;

    plugin_factory              factory;

    const char *                config_path; // for passing to plugins
    lz_config                   config;

    lz_callback                 srv_callback;

    int incoming_sock;
    int stats_sock;
    int epoll_sock;

    int epoll_wakeup_isock;
    int epoll_wakeup_osock;

    int threads_num;

    time_t                      start_time;
    // network part

    int  init_epoll();

    void add_epoll_action(int fd, int action, uint32_t mask);

    void timeouts_kill_oldest();

    void epoll_processing_loop();
    void easy_processing_loop();
    void hard_processing_loop();
    void idle_processing_loop();

    //void stats_print();

    // pthreads part

    bool process_event(const epoll_event&);
    bool process(http *);

    void epoll_send_wakeup();
    void epoll_recv_wakeup();

    bool push_easy(http *);
    bool pop_easy_or_wait(http**);

    bool push_hard(http *);
    bool pop_hard_or_wait(http**);

    bool push_done(http *);
    bool pop_done(http**);

    void fire_all_threads();

    friend void * epoll_loop_function(void * ptr);
    friend void * easy_loop_function(void * ptr);
    friend void * hard_loop_function(void * ptr);
    friend void * stats_loop_function(void * ptr);
    friend void * idle_loop_function(void * ptr);

public:
    server();
    ~server();

    void load_config(const char *xml_in, const char* &pid_in);
    void prepare();
    void finalize();

    void init_threads();
    void join_threads();

    bool pid_file_init();
    bool pid_file_is_set()const;
    void pid_file_free();
};

//-----------------------------------------------------------------

}

extern lizard::statistics stats;

#endif
