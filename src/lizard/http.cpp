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
#include <lizard/Version.h>
#include <lizard/http.hpp>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <utils/logger.hpp>

lizard::Logger& lizard::mem_block::slogger = lizard::getLog("lizard");
static lizard::Logger& slogger = lizard::getLog("lizard");

//TODO: Error messages support etc
//TODO: 'Expect: 100-continue' header support for POST requests

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ FOR DEBUG PURPOSES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::string print_errno()
{
    return strerror(errno);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct HTTP_CODES_DESC
{
    int code;
    const char * desc;
}
codes_table[] = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Moved Temporarily"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Long"},
    {415, "Unsupported Media Type"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"}
};

#define codes_table_sz (sizeof(codes_table) / sizeof(codes_table[0]))


enum {HTTP_CODES_MAX_SIZE = 1024};
const char * http_codes_array[HTTP_CODES_MAX_SIZE];

const char ** lizard::http::http_codes = 0;
int lizard::http::http_codes_num = 0;

lizard::http::http() :
    fd(-1),
    want_read(false),
    want_write(false),
    can_read(false),
    can_write(false),
    stop_reading(false),
    stop_writing(false),
    locked(false),
    state_(sUndefined),
    header_items_num(0),
    protocol_major(0),
    protocol_minor(0),
    keep_alive(false),
    cache(false),
    uri_path(0),
    uri_params(0),
    response_status(0)
{
    memset(&in_ip, 0, sizeof(in_ip));

    out_post.set_expand(true);

        if (0 == http_codes)
    {
                for (uint32_t i = 0; i < HTTP_CODES_MAX_SIZE; i++)
        {
                     http_codes_array[i] = "Unknown";
        }

        for (uint32_t j = 0; j < codes_table_sz; j++)
        {
            int key = codes_table[j].code;

            if (key < HTTP_CODES_MAX_SIZE)
            {
                http_codes_array[key] = codes_table[j].desc;

                if (http_codes_num < key)
                {
                     http_codes_num = key;
                }
            }
        }

        http_codes = http_codes_array;
    }
}

lizard::http::~http()
{
    if (-1 != fd)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);

        fd = -1;
    }

    locked = false;

    state_ = sUndefined;
}

void lizard::http::init(int new_fd, const struct in_addr& ip)
{
    if (-1 == fd)
    {
        fd = new_fd;
    }
    else
    {
        slogger.warn("lizard::http::[%d] tried to double-init on %d", fd, new_fd);
    }

    in_ip = ip;

    want_read = false;
    want_write = false;
    can_read = false;
    can_write = false;
    stop_reading = false;
    stop_writing = false;

    locked = false;

    state_ = sUndefined;
    header_items_num = 0;
    protocol_major = 0;
    protocol_minor = 0;
    keep_alive = false;
    cache = false;

    uri_path = 0;
    uri_params = 0;
    response_status = 0;

    in_headers.reset();
    in_post.reset();
    out_title.reset();
    out_headers.reset();
    out_post.reset();

    out_post.set_expand(true);

    state_ = sUndefined;

    //slogger.debug("lizard::http::init(%d, %s)", new_fd, inet_ntoa(in_ip));
}

void lizard::http::destroy()
{
    //slogger.debug("lizard::http::destroy(%d)", fd);

    if (-1 != fd)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);

        fd = -1;
    }

    in_headers.reset();
    in_post.reset();
    out_title.reset();
    out_headers.reset();
    out_post.reset();

    state_ = sUndefined;
}


bool lizard::http::ready_read()const
{
    return can_read/* && want_read*/;// && !stop_reading;
}

bool lizard::http::ready_write()const
{
    return can_write && want_write;// && !stop_writing;
}

//-------------------------------------------------------------------------------------------------------------------

void lizard::http::allow_read()
{
    can_read = true;
    //slogger.debug("lizard::http::process::allow_read(%d)", fd);
}

void lizard::http::allow_write()
{
    can_write = true;
    //slogger.debug("lizard::http::process::allow_write(%d)", fd);
}

bool lizard::http::ready()const
{
    //slogger.debug("lizard::http::ready() : read{c:%d w:%d st:%d}, write{c:%d w:%d st:%d}",
    //    can_read,  want_read, stop_reading,
      //    can_write, want_write, stop_writing);

    return ready_read() || ready_write();
}

bool lizard::http::get_rdeof()
{
    return stop_reading;
}

void lizard::http::set_rdeof()
{
    stop_reading = true;
    //can_read = false;
    //slogger.debug("set_rdeof()");
}

bool lizard::http::get_wreof()
{
    return stop_writing;
}

void lizard::http::set_wreof()
{
    stop_writing = true;
    can_write = false;
    //slogger.debug("set_wreof()");
}

int lizard::http::get_fd()const
{
    return fd;
}

void lizard::http::lock()
{
    //slogger.debug("http(%d)::lock()", fd);
    locked = true;
}

void lizard::http::unlock()
{
    //slogger.debug("http(%d)::unlock()", fd);
    locked = false;
}

bool lizard::http::is_locked()const
{
    return locked;
}

lizard::http::http_state lizard::http::state()const
{
    return state_;
}

lizard::task::request_method_t lizard::http::get_request_method()const
{
    return method;
}

int lizard::http::get_version_major()const
{
    return protocol_major;
}
int lizard::http::get_version_minor()const
{
    return protocol_minor;
}

bool lizard::http::get_keepalive()const
{
    return keep_alive;
}

bool lizard::http::get_cache()const
{
    return cache;
}

struct in_addr lizard::http::get_request_ip()const
{
    return in_ip;
}

const char * lizard::http::get_request_uri_path()const
{
    return uri_path;
}

const char * lizard::http::get_request_uri_params()const
{
    return uri_params;
}

size_t lizard::http::get_request_body_len()const
{
    return in_post.size();
}

const uint8_t * lizard::http::get_request_body()const
{
    return (const uint8_t *)in_post.get_data();
}

const char * lizard::http::get_request_header(const char * hk)const
{
    for (size_t i = 0; i < get_request_headers_num(); i++)
    {
        if (!strcasecmp(header_items[i].key, hk))
            return header_items[i].value;
    }

    return 0;
}

size_t lizard::http::get_request_headers_num()const
{
    return header_items_num;
}

const char * lizard::http::get_request_header_key(int sz)const
{
    return header_items[sz].key;
}

const char * lizard::http::get_request_header_value(int sz)const
{
    return header_items[sz].value;
}

void lizard::http::set_keepalive(bool st)
{
    keep_alive = st;
}

void lizard::http::set_cache(bool ch)
{
    cache = ch;
}

void lizard::http::set_response_status(int st)
{
    response_status = st;
}

void lizard::http::set_response_header(const char * header_nm, const char * val)
{
    out_headers.append_data(header_nm, strlen(header_nm));
    out_headers.append_data(": ", 2);
    out_headers.append_data(val, strlen(val));
    out_headers.append_data("\r\n", 2);
}

void lizard::http::append_response_body(const char * data, size_t sz)
{
    out_post.append_data(data, sz);
}

void lizard::http::process()
{
    //slogger.debug("lizard::http::process()");

    bool quit = false;
    int res = 0;

    while (!quit)
    {
        switch(state_)
        {
        case sUndefined:

            want_read = true;
            want_write = false;

            state_ = sReadingHead;

            break;

        case sReadingHead:
            //slogger.debug("lizard::http::process(): sReadingHead");
            res = parse_title();

            if (res > 0)
            {
                //slogger.debug("process():  parse_title() error %d", res);
                state_ = sDone;
                quit = true;
            }
            else if (res < 0)
            {
                quit = true;
            }

            break;

        case sReadingHeaders:
            //slogger.debug("lizard::http::process(): sReadingHeaders");
            res = parse_header_line();

            if (res > 0)
            {
                //slogger.debug("process():  parse_header_line() error %d", res);
                state_ = sDone;
                quit = true;
            }
            else if (res < 0)
            {
                quit = true;
            }

            if (state_ == sReadyToHandle)
            {
                quit = true;
            }

            break;

        case sReadingPost:
            //in_post.print();
            res = parse_post();
            //in_post.print();
            if (res < 0)
            {
                quit = true;
            }

            break;

        case sReadyToHandle:
            //slogger.debug("lizard::http::process(): sReadyToHandle");

            commit();
            state_ = sWriting;
//            commit();

            break;

        case sWriting:
            //slogger.debug("sWriting:");
            want_write = true;

            res = write_data();

            if (res < 0)
            {
                quit = true;
            }

            break;

        case sDone:
            //slogger.debug("lizard::http::process(): sDone reached!");
            quit = true;

            break;

        default:

            break;
        }
    }
}

bool lizard::http::network_tryread()
{
    if (-1 != fd)
    {
           // slogger.debug("lizard::http::process::ready_read(%d)", fd);
           // slogger.debug("read{c:%d w:%d st:%d}, write{c:%d w:%d st:%d}",
           //                     can_read,  want_read, stop_reading,
           //                 can_write, want_write, stop_writing);

        while (can_read && !stop_reading/* && want_read*/)
        {
            if (!in_headers.read_from_fd(fd, can_read, want_read, stop_reading))
            {
                state_ = sDone;
                response_status = 400;

                return false;
            }
        }
    }

    return true;
}


bool lizard::http::network_trywrite()
{
    if (-1 != fd)
    {
        //slogger.debug("lizard::http::process::ready_write(%d)", fd);
        //slogger.debug("read{c:%d w:%d st:%d}, write{c:%d w:%d st:%d}",
         //                   can_read,  want_read, stop_reading,
          //                  can_write, want_write, stop_writing);

        bool want_write = true;

        if (can_write && !stop_writing)
        {
            //slogger.debug("out_title.write_to_fd");
            out_title.write_to_fd(fd, can_write, want_write, stop_writing);
        }

/*        want_write = true;

        if (can_write && !stop_writing)
        {
            slogger.debug("out_headers.write_to_fd");
            out_headers.write_to_fd(fd, can_write, want_write, stop_writing);
        }
*/
        want_write = true;

        if (out_post.get_data_size() && can_write && !stop_writing)
        {
            //slogger.debug("out_post.write_to_fd");
            out_post.write_to_fd(fd, can_write, want_write, stop_writing);
        }

        //slogger.debug("all writings done");
        set_wreof();
    }

    return 0;
}

int lizard::http::write_data()
{
    while (can_write)
    {
        network_trywrite();
    }

    if (false == get_wreof())
    {
        state_ = sWriting;

        return -1;
    }
    else
    {
        state_ = sDone;

        return 0;
    }
}

char * lizard::http::read_header_line()
{
    while (true)
    {
        want_read = true;

        if (!network_tryread())
        {
            break;
        }

        char * headers_data = (char*)in_headers.get_data();
        headers_data[in_headers.get_data_size()] = 0;

        char * begin = headers_data + in_headers.marker();

        char * nl = strchr(begin, '\n');

        if (nl)
        {
            in_headers.marker() = nl - headers_data + 1;

            *nl = 0;
            nl--;
            if (*nl == '\r')
            {
                *nl = 0;
            }

            return begin;
        }
        else if (!can_read || stop_reading)
        {
            break;
        }
        else //can read && !want_read && !stop_reading
        {
            //slogger.message_r(LOG_ERROR, "header is larger than %d", (int)in_headers.page_size());
            state_ = sDone;
            break;
        }
    }

    return 0;
}

int lizard::http::parse_title()
{
    //slogger.debug("parse_title()");
    char * line = read_header_line();
    if (!line)
    {
        //slogger.debug("got 0");
        return -1;
    }

    //slogger.debug("read_header_line() = '%s'", line);

    state_ = sDone;

    char * mthd = line;

    while (*mthd == ' ')mthd++;

    char * url = strchr(mthd, ' ');
    if (!url)
    {
        return 400;
    }

    while (*url == ' ')*url++ = 0;

    char * version = strchr(url, ' ');
    if (!version)
    {
        return 400;
    }

    while (*version == ' ')*version++ = 0;

    if (strncasecmp(version, "HTTP/", 5))
    {
        return 400;
    }

    version += 5;
    char * mnr = strchr(version, '.');
    if (!mnr)
    {
        return 400;
    }

    char * delim = strchr(url, '?');
    if (!delim)
    {
        delim = url - 1;
    }
    else
    {
        *delim++ = 0;
    }

    switch(mthd[0])
    {
        case 'g':
        case 'G':

            method = requestGET;

            break;

        case 'h':
        case 'H':

            method = requestHEAD;

            break;

        case 'p':
        case 'P':

            if (mthd[1] == 'o' || mthd[1] == 'O')
            {
                method = requestPOST;
            }

            break;
        default:

            method = requestUNDEF;

            return 501;
    }

    protocol_major = atoi(version);
    protocol_minor = atoi(mnr + 1);

    uri_path = url;
    uri_params = delim;

    state_ = sReadingHeaders;

    return 0;
}

int lizard::http::parse_header_line()
{
    //slogger.debug("parse_header_line()");
    char * key = read_header_line();
    if (!key)
    {
        return -1;
    }

    slogger.debug("read_header_line() = '%s'", key);

    if (0 == *key)
    {
        if (method == requestPOST)
        {
            state_ = sReadingPost;
            slogger.debug("->sReadingPost");

            in_post.append_data((char*)in_headers.get_data() + in_headers.marker(), in_headers.get_data_size() - in_headers.marker());
        }
        else
        {
            state_ = sReadyToHandle;

            slogger.debug("->sReadyToHandle");
        }

        return 0;
    }

    state_ = sDone;

    while (*key == ' ')key++;
    char * val = strchr(key, ':');
    if (!val)
    {
        return 400;
    }

    *val++ = 0;
    while (*val == ' ')val++;

    if (header_items_num < MAX_HEADER_ITEMS && *key)
    {
         header_items[header_items_num].key = key;
        header_items[header_items_num].value = val;

        header_items_num++;

        slogger.debug("header_items['%s']='%s'", key, val);

    }

    if (!strcasecmp(key, "connection") && !strcmp(val, "keep-alive"))
    {
        keep_alive = false;
    }
    else if (!strncasecmp(key, "content-len", 11))
    {
        int sz = atoi(val);
        in_post.resize(sz);

        slogger.debug("post body found (%d bytes)", sz);
    }
    else if (!strcasecmp(key, "expect") && !strcasecmp(val, "100-continue")) //EVIL HACK for answering on "Expect: 100-continue"
    {
        const char * ret_str = "HTTP/1.1 100 Continue\r\n\r\n";
        int ret_str_sz = 25;//strlen(ret_str);

                ssize_t wr = write(fd, ret_str, ret_str_sz);
        if (wr == -1 || wr < ret_str_sz)
        {
                      slogger.warn("client didn't receive '100 Continue'");
        }
    }

    state_ = sReadingHeaders;

    return 0;
}

int lizard::http::parse_post()
{
    slogger.debug("parse_post() (%d bytes)", (int)in_post.size());

    in_post.read_from_fd(fd, can_read, want_read, stop_reading);

    if (in_post.size() == in_post.capacity())
    {
        state_ = sReadyToHandle;
    }

    if (stop_reading && state_ != sReadyToHandle)
    {
        response_status = 400;
        state_ = sDone;
    }

    return -1;
}

int lizard::http::commit()
{
    char buff[1024];

    char now_str[128];
    time_t now_time;
    time(&now_time);
    memset(now_str, 0, 128);
    strftime(now_str, 127, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now_time));

        const char * resp_status_str = "Unknown";

    if (response_status >= http_codes_num || response_status == 0)
    {
        response_status = 404;
    }
    else
    {
             resp_status_str = http_codes[response_status];
    }


    int l = snprintf(buff, 1023,
            "HTTP/%d.%d %d %s\r\n"
            "Server: lizard/" LIZARD_VERSION_STRING "\r\n"
            "Date: %s\r\n",
               protocol_major, protocol_minor, response_status, resp_status_str, now_str);

    out_title.append_data(buff, l);

    if (!cache)
    {
        const char m[] = "Pragma: no-cache\r\nCache-control: no-cache\r\n";
        out_headers.append_data(m, sizeof(m) - 1);

        //set_response_header("Pragma", "no-cache");
        //set_response_header("Cache-control", "no-cache");
    }

    if (keep_alive)
    {
        set_response_header("Connection", "keep-alive");
    }
    else
    {
        set_response_header("Connection", "close");
    }

    if (out_post.get_data_size())
    {
        set_response_header("Accept-Ranges", "bytes");

        l = snprintf(buff, 1023, "Content-Length: %d\r\n", (int)out_post.get_total_data_size());
        out_headers.append_data(buff, l);
    }

    out_headers.append_data("\r\n", 2);

    out_title.append_data(out_headers.get_data(), out_headers.get_data_size());

    //slogger.debug("out_headers:---\n%s\n---", (char*)out_headers.get_data());

    return 0;
}

//-------------------------------------------------------------------------------------------------------------------
