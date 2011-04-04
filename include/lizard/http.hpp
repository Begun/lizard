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

#ifndef __LIZARD_CONNECTION_HPP
#define __LIZARD_CONNECTION_HPP

#include <lizard/mem_chunk.hpp>
#include <lizard/plugin.hpp>
#include <lizard/utils.hpp>
#include <stdint.h>

namespace lizard
{

//---------------------------------------------------------------------------------------

class http : public lizard::task
{
public:
    enum http_state {sUndefined, sReadingHead, sReadingHeaders, sReadingPost, sReadyToHandle, sWriting, sDone};

protected:

    static int http_codes_num;

    static const char ** http_codes;


    enum {MAX_HEADER_ITEMS = 16};

    enum {READ_HEADERS_SZ = 8192};
    enum {WRITE_TITLE_SZ = 8192};
    enum {WRITE_HEADERS_SZ = 4096};
    enum {WRITE_BODY_SZ = 32768};

    int fd;

    bool want_read;
    bool want_write;
    bool can_read;
    bool can_write;

    // Требуется остановить чтение
    // На самом деле флаг устанавливается, если в read_from_fd достигнут конец файла
    bool stop_reading;
    bool stop_writing;

    volatile bool locked;

    mem_chunk<READ_HEADERS_SZ>    in_headers;
    mem_block                     in_post;

    mem_chunk<WRITE_TITLE_SZ>     out_title;
    mem_chunk<WRITE_HEADERS_SZ>   out_headers;
    mem_chunk<WRITE_BODY_SZ>      out_post;

    http_state state_;

    struct header_item
    {
        const char *key;
        const char *value;
    }
    header_items[MAX_HEADER_ITEMS];

    int header_items_num;

    task::request_method_t method;

    int protocol_major;
    int protocol_minor;
    bool keep_alive;
    bool cache;

    struct in_addr in_ip;

    const char *uri_path;
    const char *uri_params;

    int response_status;

    bool ready_read()const;
    bool ready_write()const;

    bool network_tryread();
    bool network_trywrite();

    char * read_header_line();
    int parse_title();
    int parse_header_line();
    int parse_post();

    int commit();
    int write_data();

public:
    http();
    ~http();

    void init(int fd, const struct in_addr& ip);
    void destroy();

    bool ready()const;

    void allow_read();
    void allow_write();

    bool get_rdeof();
    void set_rdeof();
    bool get_wreof();
    void set_wreof();

    void lock();
    void unlock();
    bool is_locked()const;

    int get_fd()const;

    // Качает данные из сокета. О результатах работы можно судить по изменению state
    void process();

    http_state state()const;

    request_method_t get_request_method()const;

    int              get_version_major()const;
    int              get_version_minor()const;
    bool             get_keepalive()const;
    bool             get_cache()const;

    struct in_addr   get_request_ip()const;

    const char *     get_request_uri_path()const;
    const char *     get_request_uri_params()const;

    size_t           get_request_body_len()const;
    const uint8_t *  get_request_body()const;

    const char *     get_request_header(const char *)const;
    size_t           get_request_headers_num()const;
    const char *     get_request_header_key(int)const;
    const char *     get_request_header_value(int)const;

    void set_response_status(int);
    void set_keepalive(bool);
    void set_cache(bool);
    void set_response_header(const char * header_nm, const char * val);
    void append_response_body(const char * data, size_t sz);
};

//---------------------------------------------------------------------------------------
}

#endif
