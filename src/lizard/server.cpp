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
#include <lizard/Version.h>
#include <lizard/server.hpp>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <utils/logger.hpp>

static lizard::Logger& slogger = lizard::getLog("lizard");
static lizard::Logger& sacclogger = lizard::getLog("access");

lizard::statistics stats;

//-----------------------------------------------------------------------------------------------------------
inline std::string events2string(int events)
{
    std::string rep;

    if (events & EPOLLIN)
    {
        if (!rep.empty())
            rep += " | EPOLLIN";
        else
            rep += " EPOLLIN";
    }

    if (events & EPOLLOUT)
    {
        if (!rep.empty())
            rep += " | EPOLLOUT";
        else
            rep += " EPOLLOUT";
    }

    /*if (events & EPOLLRDHUP)
    {
        if (!rep.empty())
            rep += " | EPOLLRDHUP";
        else
            rep += " EPOLLRDHUP";
    }*/

    if (events & EPOLLPRI)
    {
        if (!rep.empty())
            rep += " | EPOLLPRI";
        else
            rep += " EPOLLPRI";
    }

    if (events & EPOLLERR)
    {
        if (!rep.empty())
            rep += " | EPOLLERR";
        else
            rep += " EPOLLERR";
    }

    if (events & EPOLLHUP)
    {
        if (!rep.empty())
            rep += " | EPOLLHUP";
        else
            rep += " EPOLLHUP";
    }

    if (events & EPOLLET)
    {
        if (!rep.empty())
            rep += " | EPOLLET";
        else
            rep += " EPOLLET";
    }

    if (events & EPOLLONESHOT)
    {
        if (!rep.empty())
            rep += " | EPOLLONESHOT";
        else
            rep += " EPOLLONESHOT";
    }

    return  "[" + rep + " ]";
}

inline std::string events2string(const epoll_event& ev)
{
    std::string rep = "[";

    char buff[256];
    snprintf(buff, 256, "%d", ev.data.fd);
    rep += buff;

    rep += "]: " + events2string(ev.events);

    return rep;
}

//-----------------------------------------------------------------------------------------------------------

namespace lizard
{
    extern volatile sig_atomic_t    quit;
    extern volatile sig_atomic_t    hup;
    extern volatile sig_atomic_t    rotate;

    extern int             MSG_LIZARD_ID;

    //statistics stats;

    void * epoll_loop_function(void * ptr);
    void * easy_loop_function(void * ptr);
    void * hard_loop_function(void * ptr);
    void * stats_loop_function(void * ptr);
    void * idle_loop_function(void * ptr);
}

//-----------------------------------------------------------------------------------------------------------

lizard::server::lz_callback::lz_callback() : lz(0)
{

}

lizard::server::lz_callback::~lz_callback()
{
    lz = 0;
}


void lizard::server::lz_callback::init(server * srv)
{
    lz = srv;
}

namespace
{
    lizard::log_level rdev2lizard_lv[] = {
        lizard::ACCESS,
        lizard::CRIT,
        lizard::CRIT,
        lizard::CRIT,
        lizard::ERROR,
        lizard::WARN,
        lizard::NOTICE,
        lizard::INFO,
        lizard::DEBUG
    };
}

void lizard::server::lz_callback::log_message(plugin_log_levels log_level, const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    slogger.log(rdev2lizard_lv[log_level], fmt, ap);
}

void lizard::server::lz_callback::vlog_message(plugin_log_levels log_level, const char * fmt, va_list ap)
{
    slogger.log(rdev2lizard_lv[log_level], fmt, ap);
}

//-----------------------------------------------------------------------------------------------------------

lizard::server::server()
:   incoming_sock(-1)
,   stats_sock(-1)
,   epoll_sock(-1)
,   epoll_wakeup_isock(-1)
,   epoll_wakeup_osock(-1)
,   threads_num(0)
,   start_time(0)
{
    pthread_mutex_init(&done_mutex, 0);

    pthread_mutex_init(&easy_proc_mutex, 0);
    pthread_mutex_init(&hard_proc_mutex, 0);
    pthread_mutex_init(&stats_proc_mutex, 0);

    pthread_cond_init(&easy_proc_cond, 0);
    pthread_cond_init(&hard_proc_cond, 0);
    pthread_cond_init(&stats_proc_cond, 0);

    start_time = time(0);
}

lizard::server::~server()
{
    slogger.debug("~server()");

    quit = 1;
    join_threads();

    finalize();

    fire_all_threads();

    pthread_cond_destroy(&stats_proc_cond);
    pthread_cond_destroy(&hard_proc_cond);
    pthread_cond_destroy(&easy_proc_cond);

    pthread_mutex_destroy(&stats_proc_mutex);
    pthread_mutex_destroy(&hard_proc_mutex);
    pthread_mutex_destroy(&easy_proc_mutex);

    pthread_mutex_destroy(&done_mutex);

    slogger.debug("/~server()");
}

//-----------------------------------------------------------------------------------------------------------

void lizard::server::init_threads()
{
    if (0 == pthread_create(&epoll_th, NULL, &epoll_loop_function, this))
    {
        slogger.debug("epoll thread created");
        threads_num++;
    }
    else
    {
        throw std::logic_error("error creating epoll thread");
    }

    if (0 == pthread_create(&idle_th, NULL, &idle_loop_function, this))
    {
        threads_num++;
        slogger.debug("idle thread created");
    }
    else
    {
        throw std::logic_error("error creating idle thread");
    }

    if (0 == pthread_create(&stats_th, NULL, &stats_loop_function, this))
    {
        threads_num++;
        slogger.debug("stats thread created");
    }
    else
    {
        throw std::logic_error("error creating stats thread");
    }
    slogger.info("%d internal threads created", threads_num);

    slogger.info("requested worker threads {easy: %d, hard: %d}", config.root.plugin.easy_threads, config.root.plugin.hard_threads);

    for (int i = 0; i < config.root.plugin.easy_threads; i++)
    {
        pthread_t th;
        int r = pthread_create(&th, NULL, &easy_loop_function, this);
        if (0 == r)
        {
            slogger.debug("easy thread created");
            easy_th.push_back(th);

            threads_num++;
        }
        else
        {
            char s[256];
            snprintf(s, 256, "error creating easy thread #%d : %s", i, strerror(r));
            throw std::logic_error(s);
        }
    }

    for (int i = 0; i < config.root.plugin.hard_threads; i++)
    {
        pthread_t th;
        int r = pthread_create(&th, NULL, &hard_loop_function, this);
        if (0 == r)
        {
            slogger.debug("hard thread created");
            hard_th.push_back(th);

            threads_num++;
        }
        else
        {
            char s[256];
            snprintf(s, 256, "error creating hard thread #%d : %s", i, strerror(r));
            throw std::logic_error(s);
        }
    }

    slogger.info("all worker threads created");
}

void lizard::server::join_threads()
{
    if (0 == threads_num)
    {
        return;
    }

    pthread_join(epoll_th,  0);
    threads_num--;

    pthread_join(idle_th,  0);
    threads_num--;

    pthread_join(stats_th,  0);
    threads_num--;

    for (size_t i = 0; i < easy_th.size(); i++)
    {
        slogger.debug("pthread_join(easy_th[%d], 0)", (int)i);
        pthread_join(easy_th[i], 0);
        threads_num--;
    }

    easy_th.clear();

    for (size_t i = 0; i < hard_th.size(); i++)
    {
        slogger.debug("pthread_join(hard_th[%d], 0)", (int)i);
        pthread_join(hard_th[i], 0);
        threads_num--;
    }

    hard_th.clear();

    slogger.debug("%d threads left", (int)threads_num);
}

void lizard::server::fire_all_threads()
{
    pthread_mutex_lock(&easy_proc_mutex);
    pthread_cond_broadcast(&easy_proc_cond);
    pthread_mutex_unlock(&easy_proc_mutex);

    pthread_mutex_lock(&hard_proc_mutex);
    pthread_cond_broadcast(&hard_proc_cond);
    pthread_mutex_unlock(&hard_proc_mutex);

    slogger.debug("fire_all_threads");
}

void lizard::server::epoll_send_wakeup()
{
    slogger.debug("epoll_send_wakeup()");
    char b[1] = {'w'};

    int ret;
    do
    {
        ret = write(epoll_wakeup_osock, b, 1);
        if (ret < 0 && errno != EINTR)
        {
            slogger.error("epoll_send_wakeup(): write failure: '%s'", strerror(errno));
        }
    }
    while (ret < 0);
}

void lizard::server::epoll_recv_wakeup()
{
    slogger.debug("epoll_recv_wakeup()");
    char b[1024];

    int ret;
    do
    {
        ret = read(epoll_wakeup_isock, b, 1024);
        if (ret < 0 && errno != EAGAIN && errno != EINTR)
        {
            slogger.error("epoll_recv_wakeup(): read failure: '%s'", strerror(errno));
        }
    }
    while (ret == 1024 || errno == EINTR);
}

bool lizard::server::push_easy(http * el)
{
    bool res = false;

    pthread_mutex_lock(&easy_proc_mutex);

    size_t eq_sz = easy_queue.size();
    stats.report_easy_queue_len(eq_sz);

    if (config.root.plugin.easy_queue_limit == 0 || (eq_sz < (size_t)config.root.plugin.easy_queue_limit))
    {
        easy_queue.push_back(el);
        res = true;

        slogger.debug("push_easy %d", el->get_fd());

        pthread_cond_signal(&easy_proc_cond);
    }

    pthread_mutex_unlock(&easy_proc_mutex);

    return res;
}

bool lizard::server::pop_easy_or_wait(http** el)
{
    bool ret = false;

    pthread_mutex_lock(&easy_proc_mutex);

    size_t eq_sz = easy_queue.size();

    stats.report_easy_queue_len(eq_sz);

    if (eq_sz)
    {
        *el = easy_queue.front();

        slogger.debug("pop_easy %d", (*el)->get_fd());

        easy_queue.pop_front();

        ret = true;
    }
    else
    {
        slogger.debug("pop_easy : events empty");

        pthread_cond_wait(&easy_proc_cond, &easy_proc_mutex);
    }

    pthread_mutex_unlock(&easy_proc_mutex);

    return ret;
}

bool lizard::server::push_hard(http * el)
{
    bool res = false;

    pthread_mutex_lock(&hard_proc_mutex);

    size_t hq_sz = hard_queue.size();

    stats.report_hard_queue_len(hq_sz);

    if (config.root.plugin.hard_queue_limit == 0 || (hq_sz < (size_t)config.root.plugin.hard_queue_limit))
    {
        hard_queue.push_back(el);

        res = true;

        slogger.debug("push_hard %d", el->get_fd());

        pthread_cond_signal(&hard_proc_cond);
    }

    pthread_mutex_unlock(&hard_proc_mutex);

    return res;
}

bool lizard::server::pop_hard_or_wait(http** el)
{
    bool ret = false;

    pthread_mutex_lock(&hard_proc_mutex);

    size_t hq_sz = hard_queue.size();

    stats.report_hard_queue_len(hq_sz);

    if (hq_sz)
    {
        *el = hard_queue.front();

        slogger.debug("pop_hard %d", (*el)->get_fd());

        hard_queue.pop_front();

        ret = true;
    }
    else
    {
        slogger.debug("pop_hard : events empty");

        pthread_cond_wait(&hard_proc_cond, &hard_proc_mutex);
    }

    pthread_mutex_unlock(&hard_proc_mutex);

    return ret;
}
bool lizard::server::push_done(http * el)
{
    pthread_mutex_lock(&done_mutex);

    done_queue.push_back(el);

    stats.report_done_queue_len(done_queue.size());

    slogger.debug("push_done %d", el->get_fd());

    pthread_mutex_unlock(&done_mutex);

    epoll_send_wakeup();

    return true;
}

bool lizard::server::pop_done(http** el)
{
    bool ret = false;

    pthread_mutex_lock(&done_mutex);

        size_t dq_sz = done_queue.size();

    stats.report_done_queue_len(dq_sz);

    if (dq_sz)
    {
        *el = done_queue.front();

        slogger.debug("pop_done %d", (*el)->get_fd());

        done_queue.pop_front();

        ret = true;
    }

    pthread_mutex_unlock(&done_mutex);

    return ret;
}

//-----------------------------------------------------------------------------------------------------------

void lizard::server::load_config(const char *xml_in, const char* &pid_in)
{
    if (::access(xml_in, F_OK) < 0)
    {
        throw std::logic_error(std::string("Cannot access config file '") + xml_in + "' : " + strerror(errno));
    }

    config_path = xml_in;

    config.clear();
    config.load_from_file(xml_in);
    config.check();

    if (NULL == pid_in && !config.root.pid_file_name.empty())
    {
        pid_in = config.root.pid_file_name.c_str();
    }

    lizard::setLog("lizard", config.root.log_file_name, lizard::str2lv(config.root.log_level));
    lizard::setLogPrintSelfName("lizard", false);
    lizard::configureLoggerFromConfig(config.root);
}

void lizard::server::prepare()
{
    srv_callback.init(this);

    factory.load_module(config.root.plugin, config_path, &srv_callback);

    epoll_sock = init_epoll();

    //----------------------------
    //add incoming sock

     incoming_sock = lz_utils::add_listener(config.root.plugin.ip.c_str(), config.root.plugin.port.c_str(), LISTEN_QUEUE_SZ);
        if (-1 != incoming_sock)
    {
            slogger.info("lizard is bound to %s:%s", config.root.plugin.ip.c_str(), config.root.plugin.port.c_str());
    }

    lz_utils::set_nonblocking(incoming_sock);
    add_epoll_action(incoming_sock, EPOLL_CTL_ADD, EPOLLIN);

    //----------------------------
    //add stats sock

    stats_sock = lz_utils::add_listener(config.root.stats.ip.c_str(), config.root.stats.port.c_str());
    if (-1 != stats_sock)
    {
        slogger.info("lizard statistics is bound to %s:%s", config.root.stats.ip.c_str(), config.root.stats.port.c_str());
    }

    lz_utils::set_socket_timeout(stats_sock, 50000);

    //----------------------------
    //add epoll wakeup fd

    int pipefd[2];
    if (::pipe(pipefd) == -1)
    {
        throw std::logic_error((std::string)"server::prepare():pipe() failed : " + strerror(errno));
    }

    epoll_wakeup_osock = pipefd[1];
    epoll_wakeup_isock = pipefd[0];

    lz_utils::set_nonblocking(epoll_wakeup_isock);
    add_epoll_action(epoll_wakeup_isock, EPOLL_CTL_ADD, EPOLLIN | EPOLLET);

}

void lizard::server::finalize()
{
    if (-1 != incoming_sock)
    {
        lz_utils::close_connection(incoming_sock);
        incoming_sock = -1;
    }

    if (-1 != stats_sock)
    {
        lz_utils::close_connection(stats_sock);
        stats_sock = -1;
    }

    if (-1 != epoll_wakeup_isock)
    {
        close(epoll_wakeup_isock);
        epoll_wakeup_isock = -1;
    }

    if (-1 != epoll_wakeup_osock)
    {
        close(epoll_wakeup_osock);
        epoll_wakeup_osock = -1;
    }

    if (-1 != epoll_sock)
    {
        close(epoll_sock);
        epoll_sock = -1;
    }

    factory.unload_module();
}

//-----------------------------------------------------------------------------------------------------------

int lizard::server::init_epoll()
{
    slogger.debug("init_epoll");

    int res = epoll_create(HINT_EPOLL_SIZE);
    if (-1 == res)
    {
        throw std::logic_error((std::string)"epoll_create : " + strerror(errno));
    }

    slogger.debug("epoll inited: %d", res);

    return res;
}

void lizard::server::add_epoll_action(int fd, int action, uint32_t mask)
{
    slogger.debug("add_epoll_action %d", fd);

    struct epoll_event evt;
    memset(&evt, 0, sizeof(evt));

    evt.events = mask;
    evt.data.fd = fd;

    int r;
    do
    {
        r = epoll_ctl(epoll_sock, action, fd, &evt);
    }
    while (r < 0 && errno == EINTR);

    if (-1 == r)
    {
        std::string err_str = "add_epoll_action:epoll_ctl(";

        switch(action)
        {
            case EPOLL_CTL_ADD:
                err_str += "EPOLL_CTL_ADD";
                break;
            case EPOLL_CTL_MOD:
                err_str += "EPOLL_CTL_MOD";
                break;
            case EPOLL_CTL_DEL:
                err_str += "EPOLL_CTL_DEL";
                break;
        }
        err_str += ", " + events2string(evt.events) + ") : ";
        err_str += strerror(errno);

        throw std::logic_error(err_str);
    }
}

void lizard::server::epoll_processing_loop()
{
    http * done_task = 0;
    while (pop_done(&done_task))
    {
        done_task->unlock();

        if (-1 != done_task->get_fd())
        {
            slogger.debug("%d is still alive", done_task->get_fd());
            process(done_task);
        }
        else
        {
            slogger.debug("%d is already dead while travelling through queues", done_task->get_fd());
            fds.release(done_task);
        }
    }

    int nfds = 0;
    do
    {
        nfds = epoll_wait(epoll_sock, events, EPOLL_EVENTS, fds.min_timeout()/*EPOLL_TIMEOUT*/);
    }
    while (nfds == -1 && (errno == EINTR || errno == EAGAIN));

    if (-1 == nfds)
    {
        throw std::logic_error((std::string)"epoll_wait : " + strerror(errno));
    }

    if (nfds)
    {
        if (nfds > 1)
        {
            slogger.debug("==  epoll_processing %d messages ==", nfds);
        }
        else
        {
            slogger.debug("==  epoll_processing 1 message ==");
        }

        //slogger.info("==  %d messages ==", nfds);
    }

    for (int i = 0; i < nfds; i++)
    {
        if (events[i].data.fd == epoll_wakeup_isock)
        {
            epoll_recv_wakeup();
        }
        else if (events[i].data.fd == incoming_sock)
        {
            struct in_addr ip;

            int client = lz_utils::accept_new_connection(incoming_sock, ip);

            if (client >= 0)
            {
                slogger.debug("accept_new_connection: %d from %s", client, inet_ntoa(ip));

                lz_utils::set_nonblocking(client);

                if (true == fds.create(client, ip))
                {
                    add_epoll_action(client, EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT/* | EPOLLRDHUP*/ | EPOLLET);
                }
                else
                {
                    slogger.error("ERROR: fd[%d] is still in list of fds", client);
                }
            }
        }
        else
        {
            process_event(events[i]);
        }
    }

    fds.kill_oldest(1000 * config.root.plugin.connection_timeout);

    stats.process();

    if (rotate)
    {
        lizard::rotateLogs();
        rotate = 0;
    }
}

bool lizard::server::process_event(const epoll_event& ev)
{
    slogger.debug("query event: %s", events2string(ev).c_str());

    http * con = fds.acquire(ev.data.fd);

    if (con)
    {
        if (ev.events & (EPOLLHUP | EPOLLERR))
        {
            slogger.debug("closing connection: got HUP/ERR!");
            con->set_rdeof();
            con->set_wreof();

            fds.del(ev.data.fd);
        }
        else
        {
                        if (ev.events & EPOLLIN)
                {
                con->allow_read();
                   }

            if (ev.events & EPOLLOUT)
            {
                con->allow_write();
            }


            /*if (ev.events & EPOLLRDHUP)
            {
                slogger.debug("push_event: RDHUP!");

                con->set_rdeof();
            }*/

            process(con);
        }
    }
    else
    {
        slogger.error("ERROR: process_event: unregistered fd in epoll set: %d", ev.data.fd);
    }

    return true;
}

bool lizard::server::process(http * con)
{
    if (!con->is_locked())
    {
        con->process();

        if (con->state() == http::sReadyToHandle)
        {
            slogger.access("%s|%s?%s|", inet_ntoa(con->get_request_ip()),
                con->get_request_uri_path(), con->get_request_uri_params());

            slogger.debug("push_easy(%d)", con->get_fd());

            con->lock();

            if (false == push_easy(con))
            {
                slogger.debug("easy queue full: easy_queue_size == %d", config.root.plugin.easy_queue_limit);

                con->set_response_status(503);
                con->set_response_header("Content-type", "text/plain");
                con->append_response_body("easy queue filled!", strlen("easy queue filled!"));

                push_done(con);
            }
        }
        else if (con->state() == http::sDone || con->state() == http::sUndefined)
        {
            slogger.debug("%d is done, closing write side of connection", con->get_fd());

            fds.del(con->get_fd());
        }
    }

    return true;
}

void lizard::server::easy_processing_loop()
{
    lizard::plugin * plugin = factory.get_plugin();

    //lizard::statistics::reporter easy_reporter(stats, lizard::statistics::reporter::repEasy);

    http * task = 0;

    if (pop_easy_or_wait(&task))
    {
        slogger.debug("lizard::easy_loop_function.fd = %d", task->get_fd());

        switch(plugin->handle_easy(task))
        {
        case plugin::rSuccess:

            slogger.debug("easy_loop: processed %d", task->get_fd());

            push_done(task);

            break;

        case plugin::rHard:

            slogger.debug("easy thread -> hard thread");

            if (config.root.plugin.hard_threads)
            {
                bool ret = push_hard(task);
                if (false == ret)
                {
                    slogger.debug("hard queue full: hard_queue_size == %d", config.root.plugin.hard_queue_limit);

                    task->set_response_status(503);
                    task->set_response_header("Content-type", "text/plain");
                    task->append_response_body("hard queue filled!", strlen("hard queue filled!"));

                    push_done(task);
                }
            }
            else
            {
                slogger.error("easy-thread tried to enqueue hard-thread, but config::plugin::hard_threads = 0");

                task->set_response_status(503);
                task->set_response_header("Content-type", "text/plain");
                task->append_response_body("easy loop error", strlen("easy loop error"));

                push_done(task);
            }

            break;

        case plugin::rError:

            slogger.error("easy thread reports error");

            task->set_response_status(503);
            task->set_response_header("Content-type", "text/plain");
            task->append_response_body("easy loop error", strlen("easy loop error"));

            push_done(task);

            break;
        }

        //easy_reporter.commit();
    }
}

void lizard::server::hard_processing_loop()
{
     lizard::plugin * plugin = factory.get_plugin();

//    lizard::statistics::reporter hard_reporter(stats, lizard::statistics::reporter::repHard);

    http * task = 0;

    if (pop_hard_or_wait(&task))
    {

        slogger.debug("lizard::hard_loop_function.fd = %d", task->get_fd());

        switch(plugin->handle_hard(task))
        {
        case plugin::rSuccess:

            slogger.debug("hard_loop: processed %d", task->get_fd());

            push_done(task);

            break;

        case plugin::rHard:
        case plugin::rError:

            slogger.error("hard_loop reports error");

            task->set_response_status(503);
            task->set_response_header("Content-type", "text/plain");
            task->append_response_body("hard loop error", strlen("hard loop error"));

            push_done(task);

            break;
        }

        //easy_reporter.commit();
    }
}

void lizard::server::idle_processing_loop()
{
    if (0 == config.root.plugin.idle_timeout)
    {
        slogger.debug("idle_loop_function: timing once");

        factory.idle();

        while (!quit && !hup)
        {
            sleep(1);
        }
    }
    else
    {
        slogger.debug("idle_loop_function: timing every %d(ms)", (int)config.root.plugin.idle_timeout);

        long secs = (config.root.plugin.idle_timeout * 1000000LLU) / 1000000000LLU;
        long nsecs = (config.root.plugin.idle_timeout * 1000000LLU) % 1000000000LLU;

        while (!quit && !hup)
        {
            factory.idle();

            lz_utils::uwait(secs, nsecs);
        }
    }
}


//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------

void *lizard::epoll_loop_function(void *ptr)
{
    lizard::server *srv = (lizard::server *) ptr;

    try
    {
        while (!quit && !hup)
        {
            srv->epoll_processing_loop();
        }
    }
    catch (const std::exception &e)
    {
        quit = 1;
        slogger.crit("epoll_loop: exception: %s", e.what());
    }

    srv->fire_all_threads();
    pthread_exit(NULL);
}

//--------------------------------------------------------------------------------------------------------

void *lizard::easy_loop_function(void *ptr)
{
    lizard::server *srv = (lizard::server *) ptr;

    try
    {
        while (!quit && !hup)
        {
            srv->easy_processing_loop();
        }
    }
    catch (const std::exception &e)
    {
        quit = 1;
        slogger.crit("easy_loop: exception: %s", e.what());
    }

    srv->fire_all_threads();
    pthread_exit(NULL);
}

//--------------------------------------------------------------------------------------------------------

void *lizard::hard_loop_function(void *ptr)
{
     lizard::server *srv = (lizard::server *) ptr;

    try
    {
        while (!quit && !hup)
        {
             srv->hard_processing_loop();
        }
    }
    catch (const std::exception &e)
    {
        quit = 1;
        slogger.crit("hard_loop: exception: %s", e.what());
    }

    srv->fire_all_threads();
    pthread_exit(NULL);
}

//--------------------------------------------------------------------------------------------------------

void *lizard::idle_loop_function(void *ptr)
{
    lizard::server *srv = (lizard::server *) ptr;

    try
    {
        while (!quit && !hup)
        {
             srv->idle_processing_loop();
        }
    }
    catch (const std::exception &e)
    {
        quit = 1;
        slogger.crit("idle_loop: exception: %s", e.what());
    }

    srv->fire_all_threads();
    pthread_exit(NULL);
}

//--------------------------------------------------------------------------------------------------------

void *lizard::stats_loop_function(void *ptr)
{
    lizard::server *srv = (lizard::server *) ptr;
    lizard::http stats_parser;

    try
    {
        while (!quit && !hup)
        {
            if (srv->stats_sock != -1)
            {
                struct in_addr ip;
                int stats_client = lz_utils::accept_new_connection(srv->stats_sock, ip);

                if (stats_client >= 0)
                {

                    lz_utils::set_socket_timeout(stats_client, 50000);

                    slogger.debug("stats: accept_new_connection: %d from %s", stats_client, inet_ntoa(ip));

                    stats_parser.init(stats_client, ip);

                    while (true)
                    {
                        stats_parser.allow_read();
                        stats_parser.allow_write();

                        stats_parser.process();

                        if (stats_parser.state() == http::sReadyToHandle)
                        {
                            slogger.access("%s|%s?%s|stats",
                                    inet_ntoa(stats_parser.get_request_ip()),
                                    stats_parser.get_request_uri_path(),
                                    stats_parser.get_request_uri_params());

                            stats_parser.set_response_status(200);
                            stats_parser.set_response_header("Content-type", "text/plain");

                            std::string resp = "<lizard_stats>\n";

                            //------------------------------------------------------------------------------------
                            char buff[1024];

                            time_t up_time = time(0) - srv->start_time;

                            snprintf(buff, 1024, "\t<lizard_version>%s</lizard_version>\n", LIZARD_VERSION_STRING);
                            resp += buff;

                            snprintf(buff, 1024, "\t<plugin_version>%s</plugin_version>\n", srv->factory.get_plugin()->version_string());
                            resp += buff;

                            snprintf(buff, 1024, "\t<uptime>%d</uptime>\n", (int)up_time);
                            resp += buff;

                            snprintf(buff, 1024, "\t<rps>%.4f</rps>\n", stats.get_rps());
                            resp += buff;

                            snprintf(buff, 1024, "\t<fd_count>%d</fd_count>\n", (int)srv->fds.fd_count());
                            resp += buff;

                            snprintf(buff, 1024, "\t<queues>\n\t\t<easy>%d</easy>\n\t\t<max_easy>%d</max_easy>\n"
                                "\t\t<hard>%d</hard>\n\t\t<max_hard>%d</max_hard>\n\t\t<done>%d</done>\n"
                                "\t\t<max_done>%d</max_done>\n\t</queues>\n",
                                    (int)stats.easy_queue_len,
                                                                        (int)stats.easy_queue_max_len,
                                    (int)stats.hard_queue_len,
                                    (int)stats.hard_queue_max_len,
                                    (int)stats.done_queue_len,
                                    (int)stats.done_queue_max_len);
                            resp += buff;

                            snprintf(buff, 1024, "\t<conn_time>\n\t\t<min>%.4f</min>\n\t\t<avg>%.4f</avg>\n\t\t<max>%.4f</max>\n\t</conn_time>\n",
                                    stats.get_min_lifetime(), stats.get_mid_lifetime(), stats.get_max_lifetime());
                            resp += buff;

                            snprintf(buff, 1024, "\t<mem_allocator>\n\t\t<pages>%d</pages>\n\t\t<objects>%d</objects>\n\t</mem_allocator>\n",
                                    (int)stats.pages_in_http_pool, (int)stats.objects_in_http_pool);
                            resp += buff;


                            struct rusage usage;
                            ::getrusage(RUSAGE_SELF, &usage);

                            snprintf(buff, 1024, "\t<rusage>\n\t\t<utime>%d</utime>\n\t\t<stime>%d</stime>\n\t</rusage>\n",
                                    (int)usage.ru_utime.tv_sec, (int)usage.ru_stime.tv_sec);
                            resp += buff;

                            //------------------------------------------------------------------------------------
                            resp += "</lizard_stats>\n";

                            stats_parser.append_response_body(resp.data(), resp.size());

                        }
                        else if (stats_parser.state() == http::sDone)
                        {
                            slogger.debug("%d is done, closing write side of connection", stats_parser.get_fd());
                            break;
                        }
                        else if (stats_parser.get_rdeof())
                        {
                            slogger.debug("%d is done, EOF is reached", stats_parser.get_fd());
                            break;
                        }

                    }

                    stats_parser.destroy();

                    stats.process();
                }
            }
            else
            {
                sleep(1);
            }
        }
    }
    catch (const std::exception &e)
    {
        quit = 1;
        slogger.crit("stats_loop: exception: %s", e.what());
    }

    srv->fire_all_threads();
    pthread_exit(NULL);
}

//--------------------------------------------------------------------------------------------------------

