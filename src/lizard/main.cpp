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

#include <getopt.h>
#include <lizard/Version.h>
#include <lizard/server.hpp>
#include <signal.h>
#include <sys/time.h>
#include <utils/daemon.hpp>
#include <utils/logger.hpp>

static lizard::Logger& slogger = lizard::getLog("lizard");

namespace lizard {

volatile sig_atomic_t quit   = 0;
volatile sig_atomic_t hup    = 0;
volatile sig_atomic_t rotate = 0;

} /* namespace lizard */

static void onPipe(int /*v*/)
{
}

static void onTerm(int /*v*/)
{
    lizard::quit = 1;
    slogger.warn("TERM received");
}

static void onInt(int /*v*/)
{
    lizard::quit = 1;
    slogger.warn("INT received");
}

static void onHUP(int /*v*/)
{
    lizard::hup = 1;
}

static void onUSR1(int /*v*/)
{
    lizard::rotate = 1;
}

int main(int argc, char * argv[])
{
    //-----------------------------------------------
    //begin parsing incoming parameters
    lizard::getopt_t opts;
    if (0 != lizard::getopt_parse(argc, argv, &opts))
    {
        lizard::getopt_usage(argc, argv);
        return EXIT_FAILURE;
    }

    if (1 == opts.version)
    {
        printf("lizard/%s\n", LIZARD_VERSION_STRING);
        return EXIT_SUCCESS;
    }

    //-----------------------------------------------
    //dealing with params extracted

    signal(SIGTERM, onTerm);
    signal(SIGINT,  onInt);
    signal(SIGHUP,  onHUP);
    signal(SIGPIPE, onPipe);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, onUSR1);
    signal(SIGUSR2, SIG_IGN);

    try
    {
        lizard::server server;

        int rc;
        if (!opts.no_daemon && 0 != (rc = lizard::daemon_start()))
            throw lizard::error("main: 0 != lizard::daemon_start(): %d: %s",
                             rc, lizard::strerror(rc));

        while (!lizard::quit)
        {
            slogger.info("loading config...");
            server.load_config(opts.config_file, opts.pid_file);

            slogger.info("prepare...");
            server.prepare();

            slogger.info("init lizard...");

            if (NULL != opts.pid_file && 0 != (rc = lizard::pid_create(opts.pid_file)))
            {
                slogger.error("create pid-file failed: %s", lizard::strerror(rc));

                exit(EXIT_FAILURE);
            }

            //---------------------------------------
            server.init_threads();

            slogger.info("all threads started!");

            //~~~

            server.join_threads();

            slogger.info("all threads ended!");
            //---------------------------------------

            if (NULL != opts.pid_file && 0 != (rc = lizard::pid_unlink(opts.pid_file)))
            {
                slogger.error("free pid-file failed: %s", lizard::strerror(rc));

                exit(EXIT_FAILURE);
            }

            slogger.info("finalizing...");
            server.finalize();

            lizard::hup = 0;
        }
    }
    catch (const std::exception &e)
    {
        lizard::quit = 1;

        if (opts.no_daemon) fprintf(stderr, "main: exception: %s\n", e.what());
        slogger.crit("main: exception: %s", e.what());

        int rc;
        if (NULL != opts.pid_file && 0 != (rc = lizard::pid_unlink(opts.pid_file)))
        {
            slogger.error("free pid-file failed: %s", lizard::strerror(rc));

            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
