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

#ifndef __LIZARD_PLUGIN_FACTORY_HPP___
#define __LIZARD_PLUGIN_FACTORY_HPP___

#include <lizard/config.hpp>
#include <lizard/plugin.hpp>
#include <pthread.h>

namespace lizard
{
//-------------------------------------------------------------------------
class plugin_factory
{
    void * loaded_module;

    lizard::plugin * plugin_handle;

public:

    plugin_factory();
    ~plugin_factory();

    void load_module(const lizard::lz_config::ROOT::PLUGIN& pd, const char * config_path, server_callback * srv);
    void unload_module();

    lizard::plugin * get_plugin()const;

    void idle();
};
//-------------------------------------------------------------------------
}
#endif
