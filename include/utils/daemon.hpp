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

#ifndef __LZ_DAEMON_H__
#define __LZ_DAEMON_H__

#include <signal.h>

namespace lizard {
/* daemon_getopt.c */

typedef struct getopt getopt_t;

struct getopt
{
    const char *config_file;
    const char *pid_file;

    unsigned no_daemon:1;
    unsigned sync_exit:1;
    unsigned version:1;
};

typedef struct getopt_mc getopt_mc_t;

/* Размер этой структуры зависит от ее поля config_files,
   которое определяет количество элементов в массиве config_file */
struct getopt_mc
{
    unsigned no_daemon:1;
    unsigned sync_exit:1;
    unsigned version:1;
    const char *pid_file;
    unsigned config_files;
    const char *config_file[1];
};

int getopt_usage(int argc, char **argv);
int getopt_parse(int argc, char **argv, getopt_t *opts);
getopt_mc_t* getopt_mc_parse(int argc, char ** argv);

/* daemon_signal.c */

extern volatile sig_atomic_t do_rotatelog;
extern volatile sig_atomic_t do_changebin;
extern volatile sig_atomic_t do_changecfg;
extern volatile sig_atomic_t do_terminate;
extern volatile sig_atomic_t do_processdb;

int signal_setup();
int signal_masqu(int how);

/* daemon_fork.c */

int daemon_start();

int pid_create(const char *pidfn);
int pid_unlink(const char *pidfn);

}// lizard

#endif /* __LZ_DAEMON_H__ */
