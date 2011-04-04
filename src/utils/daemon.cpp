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

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <malloc.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utils/daemon.hpp>

/* XXX : add error diagnostics to lizard::getopt_usage() */
/* XXX : add lizard::getopt_error() */

static struct option options [] =
{
    { "config-file"  , 1, NULL, 'c' },
    { "pid-file"     , 1, NULL, 'p' },
    { "no-daemon"    , 0, NULL, 'D' },
    { "sync-exit"    , 0, NULL, 's' },
    { "version"      , 0, NULL, 'v' },
    {  NULL          , 0, NULL,  0  },
};

int lizard::getopt_usage(int argc, char **argv)
{
    fprintf(stderr,
        "Usage:                                                 \n"
        "  %s <opts>                                            \n"
        "                                                       \n"
    , argv[0]);
    fprintf(stderr,
        "Options:                                               \n"
        "  -c, --config-file <path> : path to config file       \n"
        "  -p, --pid-file    <path> : path to pid file          \n"
        "  -D, --no-daemon          : do not daemonize          \n"
        "  -s, --sync-exit          : handle exit signals       \n"
        "  -v, --version            : print version and exit    \n"
        "                                                       \n"
        );

    return 0;
}

int lizard::getopt_parse(int argc, char **argv, lizard::getopt_t *opts)
{
    int c;
    opterr = 0;
    memset(opts, 0, sizeof(*opts));

    while (-1 != (c = getopt_long(argc, argv, "c:p:Dsv", options, NULL)))
    {
        switch (c)
        {
            case 'c': opts->config_file = optarg; break;
            case 'p': opts->pid_file    = optarg; break;
            case 'D': opts->no_daemon   = 1;      break;
            case 's': opts->sync_exit   = 1;      break;
            case 'v': opts->version     = 1;      break;

            default:
                return -1;
        }
    }

    if (NULL == opts->config_file && 0 == opts->version)
    {
        return -1;
    }

    return 0;
}

lizard::getopt_mc_t* lizard::getopt_mc_parse(int argc, char ** argv)
{
    int c;
    unsigned no_daemon = 0;
    unsigned sync_exit = 0;
    unsigned version = 0;
    const char* pid_file = NULL;
    int cur_config = 0;
    const char** config_file = (const char**)alloca(sizeof(char*) * argc);
    lizard::getopt_mc_t* opts = NULL;

    opterr = 0;

    while (-1 != (c = getopt_long(argc, argv, "c:p:Dsv", options, NULL)))
    {
        switch (c)
        {
            case 'c': config_file[cur_config++]  = optarg; break;
            case 'p': pid_file                   = optarg; break;
            case 'D': no_daemon                  = 1;      break;
            case 's': sync_exit                  = 1;      break;
            case 'v': version                    = 1;      break;

            default:  return NULL;
        }
    }

    if (0 == cur_config && 0 == version) return NULL;

    opts = (lizard::getopt_mc_t*) malloc (sizeof(lizard::getopt_mc_t) + sizeof(char*) * (cur_config-1));
    opts->no_daemon = no_daemon;
    opts->sync_exit = sync_exit;
    opts->version = version;
    opts->pid_file = pid_file;
    opts->config_files = cur_config;
    memcpy(opts->config_file, config_file, sizeof(char*) * cur_config);

    return opts;
}

int lizard::daemon_start()
{
    int rc;

    if (0 != close( STDIN_FILENO)) return errno;
    if (0 != close(STDOUT_FILENO)) return errno;
    if (0 != close(STDERR_FILENO)) return errno;

    rc = open("/dev/null", O_RDWR);
    if (-1 == rc) return errno;

    if (-1 == dup2(rc, 0)) return errno;
    if (-1 == dup2(rc, 1)) return errno;
    if (-1 == dup2(rc, 2)) return errno;

    rc = fork();

    if (0 < rc) exit(EXIT_SUCCESS);
    if (0 > rc) exit(EXIT_FAILURE);

    if ((pid_t) -1 == setsid())
    {
        return errno;
    }

    return 0;
}

int lizard::pid_create(const char *pidfn)
{
    char buf [32];
    int fd, sz, errnum;

    sz = snprintf(buf, 32, "%d\n", (int) getpid());

    if (-1 == (fd = open(pidfn, O_CREAT|O_EXCL|O_WRONLY|O_TRUNC, 0644)))
    {
        return errno;
    }

    if (-1 == write(fd, buf, sz))
    {
        errnum = errno;
        close(fd);

        return errnum;
    }

    if (0 != close(fd))
    {
        return errno;
    }

    return 0;
}

int lizard::pid_unlink(const char *pidfn)
{
    if (0 != unlink(pidfn))
    {
        return errno;
    }

    return 0;
}
