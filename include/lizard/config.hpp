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

#ifndef __LIZARD_CONFIG___
#define __LIZARD_CONFIG___

#include <inttypes.h>
#include <stdio.h>
#include <string>
#include <utils/error.hpp>
#include <utils/parxml.hpp>

#define SRV_BUF 4096

#define DET_MEMB(m) p->determineMember(#m, m)

namespace lizard {

struct lz_config : public xmlobject
{
    struct ROOT : public xmlobject
    {
        std::string pid_file_name;
        std::string log_file_name;
        std::string log_level;
        std::string access_log_file_name;
        std::string log_config_str;

        struct STATS : public xmlobject
        {
            std::string ip;
            std::string port;

            STATS(){}

            void determine(xmlparser *p)
            {
                DET_MEMB(ip);
                DET_MEMB(port);
            }

            void clear()
            {
                ip.clear();
                port.clear();
            }

            void check(const char *par, const char *ns)
            {
                char curns [SRV_BUF];
                snprintf(curns, SRV_BUF, "%s:%s", par, ns);

                if (ip  .empty()) throw error ("<%s:ip> is empty in config", curns);
                if (port.empty()) throw error ("<%s:port> is empty in config", curns);
            }
        };

        struct PLUGIN : public xmlobject
        {
            std::string ip;
            std::string port;
            int connection_timeout;
            int idle_timeout;

            std::string library;
            std::string params;

            int easy_threads;
            int hard_threads;

            int easy_queue_limit;
            int hard_queue_limit;

            PLUGIN() : connection_timeout(0), idle_timeout(0), easy_threads(1), hard_threads(0), easy_queue_limit(0), hard_queue_limit(0){}

            void determine(xmlparser *p)
            {
                DET_MEMB(ip);
                DET_MEMB(port);
                DET_MEMB(connection_timeout);
                DET_MEMB(idle_timeout);

                DET_MEMB(library);
                DET_MEMB(params);

                DET_MEMB(easy_threads);
                DET_MEMB(hard_threads);

                DET_MEMB(easy_queue_limit);
                DET_MEMB(hard_queue_limit);
            }

            void clear()
            {
                ip.clear();
                port.clear();
                connection_timeout = 0;
                idle_timeout = 0;

                library.clear();
                params.clear();

                easy_threads = 1;
                hard_threads = 0;

                easy_queue_limit = 0;
                hard_queue_limit = 0;
            }

            void check(const char *par, const char *ns)
            {
                char curns [SRV_BUF];
                snprintf(curns, SRV_BUF, "%s:%s", par, ns);

                if (ip     .empty()) throw error ("<%s:ip> is empty in config", curns);
                if (port   .empty()) throw error ("<%s:port> is empty in config", curns);
                if (library.empty()) throw error ("<%s:library> is empty in config", curns);

                if (0 == connection_timeout) throw error ("<%s:connection_timeout> is not set or set to 0", curns);
                if (0 == easy_threads) throw error ("<%s:easy_threads> is set to 0", curns);
            }
        };

        STATS stats;
        PLUGIN plugin;

        void determine(xmlparser *p)
        {
            DET_MEMB(pid_file_name);
            DET_MEMB(log_file_name);
            DET_MEMB(log_level);
            DET_MEMB(access_log_file_name);
            DET_MEMB(log_config_str);

            DET_MEMB(stats);
            DET_MEMB(plugin);
        }

        void clear()
        {
            pid_file_name.clear();
            log_file_name.clear();
            log_level.clear();
            access_log_file_name.clear();
            log_config_str.clear();

            stats.clear();
            plugin.clear();
        }

        void check()
        {
            const char *curns = "lizard";

            if (log_level.empty()) throw error ("<%s:log_level> is empty in config", curns);

            stats .check(curns, "stats");
            plugin.check(curns, "plugin");
        }
    } root;

    void determine(xmlparser *p)
    {
        p->determineMember("lizard", root);
    }

    void clear()
    {
        root.clear();
    }

    void check()
    {
        root.check();
    }
};

//----------------------------------------------------------------------------------
}

#undef DET_MEMB

#endif
