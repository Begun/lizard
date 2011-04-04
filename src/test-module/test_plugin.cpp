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

#include <lizard/plugin.hpp>

class lz_test : public lizard::plugin
{
    lizard::server_callback *srv;

public:
     lz_test(lizard::server_callback *srv_cb);
    ~lz_test();

    int set_param(const char *xml_in);
    void idle();

    int handle_easy(lizard::task *task);
    int handle_hard(lizard::task *task);

    const char* version_string() const { return "version? nothing version!"; }
};

extern "C" lizard::plugin *get_plugin_instance(lizard::server_callback *srv_cb)
{
    srv_cb->log_message(lizard::log_info, "HELLO "
        "from get_plugin_instance");

    try
    {
        return new lz_test (srv_cb);
    }
    catch (const std::exception &e)
    {
        srv_cb->log_message(lizard::log_crit, "lz_test plugin "
            "load failed: %s", e.what());
    }

    return NULL;
}

lz_test:: lz_test(lizard::server_callback *srv_cb) : plugin(srv_cb), srv(srv_cb)
{
}

lz_test::~lz_test()
{
}

int lz_test::set_param(const char *xml_in)
{
    srv->log_message(lizard::log_info, "loading "
        "plugin param: %s", xml_in);

    return rSuccess;
}

int lz_test::handle_easy(lizard::task *task)
{
    switch (task->get_request_method())
    {
        case lizard::task::requestGET:   break;
        case lizard::task::requestHEAD:  break;
        case lizard::task::requestPOST:  return rHard;
        case lizard::task::requestUNDEF: return rError;
    }

    task->set_response_status  (200);
    task->append_response_body ("Hello, world!\n", 14);

    return rSuccess;
}

int lz_test::handle_hard(lizard::task* /*task*/)
{
    return rSuccess;
}

void lz_test::idle()
{
}

