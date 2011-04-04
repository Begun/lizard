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

#ifndef __LIZARD_STATS_HPP___
#define __LIZARD_STATS_HPP___

#include <stdint.h>
#include <time.h>

namespace lizard {

//---------------------------------------------------------------------------------------------

struct statistics
{
    enum {TIME_DELTA = 4};

    time_t last_processed_time;

    volatile int requests_count;
    volatile uint64_t resp_time_total;
    volatile double resp_time_min, resp_time_mid, resp_time_max, min_t, max_t, avg_rps;
    volatile size_t eq_ml, hq_ml, dq_ml;
public:

    volatile size_t easy_queue_len;
    volatile size_t easy_queue_max_len;
    volatile size_t hard_queue_len;
    volatile size_t hard_queue_max_len;
    volatile size_t done_queue_len;
    volatile size_t done_queue_max_len;

    volatile size_t objects_in_http_pool;
    volatile size_t pages_in_http_pool;

    statistics();

    void process();
    void report_response_time(uint64_t time);

    void report_easy_queue_len(size_t len);
    void report_hard_queue_len(size_t len);
    void report_done_queue_len(size_t len);

    double get_min_lifetime()const;
    double get_mid_lifetime()const;
    double get_max_lifetime()const;

    double get_rps()const;
};

//---------------------------------------------------------------------------------------------
}

#endif

