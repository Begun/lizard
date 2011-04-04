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

#ifndef __LIZARD__PLUGIN_HPP__
#define __LIZARD__PLUGIN_HPP__

#include <arpa/inet.h>
#include <cstdarg>
#include <stdint.h>
#include <sys/types.h>
#include <utils/error.hpp>

namespace lizard
{

//-----------------------------------------------------------------

class task
{
public:

    enum request_method_t {requestUNDEF, requestGET, requestPOST, requestHEAD};


    virtual ~task(){};
    virtual request_method_t get_request_method()const = 0;

    virtual int              get_version_major()const = 0;
    virtual int              get_version_minor()const = 0;
    virtual bool             get_keepalive()const = 0;
    virtual bool             get_cache()const = 0;

    virtual struct in_addr   get_request_ip()const = 0;

    virtual const char *     get_request_uri_path()const = 0;
    virtual const char *     get_request_uri_params()const = 0;

    virtual size_t           get_request_body_len()const = 0;
    virtual const uint8_t *  get_request_body()const = 0;

    virtual const char *     get_request_header(const char *)const = 0;
    virtual size_t           get_request_headers_num()const = 0;
    virtual const char *     get_request_header_key(int)const = 0;
    virtual const char *     get_request_header_value(int)const = 0;


    virtual void set_response_status(int) = 0;
    virtual void set_keepalive(bool) = 0;
    virtual void set_cache(bool) = 0;
    virtual void set_response_header(const char * header_nm, const char * val) = 0;
    virtual void append_response_body(const char * data, size_t sz) = 0;
};

enum plugin_log_levels
{
    log_access = 0,  /* special level for access log       */
    log_emerg  = 1,  /* system is unusable                 */
    log_alert  = 2,  /* action must be taken immediately   */
    log_crit   = 3,  /* critical conditions                */
    log_error  = 4,  /* error conditions                   */
    log_warn   = 5,  /* warning conditions                 */
    log_notice = 6,  /* normal, but significant, condition */
    log_info   = 7,  /* informational message              */
    log_debug  = 8   /* debug-level message                */
};

class server_callback
{
public:

    virtual ~server_callback(){};

    virtual void  log_message(plugin_log_levels log_level, const char * param_str, ...) LZ_FORMAT(printf, 3, 4) = 0;
    virtual void vlog_message(plugin_log_levels log_level, const char * param_str, va_list ap) = 0;
};


class plugin
{
public:
    enum {rSuccess, rHard, rError};

    plugin(server_callback* /*srv*/){}
    virtual ~plugin(){}

    virtual int set_param(const char * xml_in) = 0;

    virtual int handle_easy(lizard::task * tsk) = 0;
    virtual int handle_hard(lizard::task * tsk) = 0;

    virtual void idle(){}

    virtual const char* version_string() const = 0;
};
//-----------------------------------------------------------------
}

extern "C" lizard::plugin * get_plugin_instance(lizard::server_callback * srv);

#endif
